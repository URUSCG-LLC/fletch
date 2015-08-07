// Copyright (c) 2014, the Fletch project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.md file.

#include "src/vm/object_memory.h"

#include <stdlib.h>
#include <stdio.h>

#include "src/shared/assert.h"
#include "src/shared/platform.h"
#include "src/shared/utils.h"

#include "src/vm/heap.h"
#include "src/vm/object.h"
#include "src/vm/storebuffer.h"
#include "src/vm/stack_walker.h"

namespace fletch {

static Smi* chunk_end_sentinel() { return Smi::zero(); }

static bool HasSentinelAt(uword address) {
  return *reinterpret_cast<Object**>(address) == chunk_end_sentinel();
}

Chunk::~Chunk() {
  free(reinterpret_cast<void*>(base()));
}

Space::Space(int maximum_initial_size)
    : first_(NULL),
      last_(NULL),
      used_(0),
      top_(0),
      limit_(0),
      no_allocation_nesting_(0) {
  if (maximum_initial_size > 0) {
    int size = Utils::Minimum(maximum_initial_size, kDefaultChunkSize);
    Chunk* chunk = ObjectMemory::AllocateChunk(this, size);
    Append(chunk);
    top_ = chunk->base();
    limit_ = chunk->limit();
  }
}

Space::~Space() {
  Chunk* current = first();
  while (current != NULL) {
    Chunk* next = current->next();
    ObjectMemory::FreeChunk(current);
    current = next;
  }
}

void Space::Flush() {
  if (!is_empty()) {
    // Set sentinel at allocation end.
    ASSERT(top_ < limit_);
    *reinterpret_cast<Object**>(top_) = chunk_end_sentinel();
  }
}

uword Space::TryAllocate(int size) {
  uword new_top = top_ + size;
  // Make sure there is room for chunk end sentinel.
  if (new_top < limit_) {
    uword result = top_;
    top_ = new_top;
    return result;
  }

  if (!is_empty()) {
    // Update the accounting.
    used_ += top() - last()->base();
    // Make the last chunk consistent with a sentinel.
    Flush();
  }

  return 0;
}

uword Space::AllocateInNewChunk(int size) {
  // Allocate new chunk that is big enough to fit the object.
  int chunk_size = size >= kDefaultChunkSize
      ? (size + kPointerSize)  // Make sure there is room for sentinel.
      : kDefaultChunkSize;

  Chunk* chunk = ObjectMemory::AllocateChunk(this, chunk_size);
  if (chunk != NULL) {
    allocation_budget_ -= chunk->size();
    top_ = chunk->base();
    limit_ = chunk->limit();

    // Link it into the space.
    Append(chunk);

    // Allocate.
    uword result = TryAllocate(size);
    if (result != 0) return result;
  }

  FATAL1("Failed to allocate memory of size %d\n", size);
  return 0;
}

uword Space::Allocate(int size) {
  ASSERT(size >= HeapObject::kSize);
  ASSERT(Utils::IsAligned(size, kPointerSize));
  uword result = TryAllocate(size);
  if (result != 0) return result;
  if (!in_no_allocation_failure_scope() && needs_garbage_collection()) return 0;
  return AllocateInNewChunk(size);
}

void Space::TryDealloc(uword location, int size) {
  if (top_ == location) top_ -= size;
}

void Space::AdjustAllocationBudget() {
  allocation_budget_ = Utils::Maximum(512 * KB, Used());
}

void Space::PrependSpace(Space* space) {
  if (space->is_empty()) return;

  Chunk* first = space->first();
  Chunk* chunk = first;
  while (chunk != NULL) {
    ObjectMemory::SetSpaceForPages(chunk->base(), chunk->limit(), this);
    chunk->set_owner(this);
    chunk = chunk->next();
  }

  space->last()->set_next(this->first());
  first_ = first;
  used_ += space->Used();

  // NOTE: The destructor of [Space] will use some of the fields, so we just
  // reset all of them.
  space->first_ = NULL;
  space->last_ = NULL;
  space->used_ = 0;
  space->top_ = 0;
  space->limit_ = 0;
  delete space;
}

void Space::Append(Chunk* chunk) {
  ASSERT(chunk->owner() == this);
  if (is_empty()) {
    first_ = last_ = chunk;
  } else {
    last_->set_next(chunk);
    last_ = chunk;
  }
  chunk->set_next(NULL);
}

void Space::IterateObjects(HeapObjectVisitor* visitor) {
  if (is_empty()) return;
  Flush();
  for (Chunk* chunk = first(); chunk != NULL; chunk = chunk->next()) {
    uword current = chunk->base();
    while (!HasSentinelAt(current)) {
      HeapObject* object = HeapObject::FromAddress(current);
      current += object->Size();
      visitor->Visit(object);
    }
  }
}

void Space::CompleteScavenge(PointerVisitor* visitor) {
  Flush();
  for (Chunk* chunk = first(); chunk != NULL; chunk = chunk->next()) {
    uword current = chunk->base();
    // TODO(kasperl): I don't like the repeated checks to see if p is
    // the last chunk. Can't we just make sure to write the sentinel
    // whenever we've copied over an object, so this check becomes
    // simpler like in IterateObjects?
    while ((chunk == last()) ? (current < top()) : !HasSentinelAt(current)) {
      HeapObject* object = HeapObject::FromAddress(current);
      object->IteratePointers(visitor);
      current += object->Size();
    }
  }
}

void Space::CompleteScavengeMutable(PointerVisitor* visitor,
                                    Space* immutable_space,
                                    StoreBuffer* store_buffer) {
  ASSERT(store_buffer->is_empty());

  Flush();

  FindImmutablePointerVisitor finder(immutable_space);

  for (Chunk* chunk = first(); chunk != NULL; chunk = chunk->next()) {
    uword current = chunk->base();
    // TODO(kasperl): I don't like the repeated checks to see if p is
    // the last chunk. Can't we just make sure to write the sentinel
    // whenever we've copied over an object, so this check becomes
    // simpler like in IterateObjects?
    while ((chunk == last()) ? (current < top()) : !HasSentinelAt(current)) {
      HeapObject* object = HeapObject::FromAddress(current);
      object->IteratePointers(visitor);

      // We build up a new StoreBuffer, containing all mutable heap objects
      // pointing to the immutable space.
      if (finder.ContainsImmutablePointer(object)) {
        store_buffer->Insert(object);
      }

      current += object->Size();
    }
  }
}

void Space::CompleteTransformations(PointerVisitor* visitor, Process* process) {
  Flush();
  for (Chunk* chunk = first(); chunk != NULL; chunk = chunk->next()) {
    uword current = chunk->base();
    // TODO(kasperl): I don't like the repeated checks to see if p is
    // the last chunk. Can't we just make sure to write the sentinel
    // whenever we've copied over an object, so this check becomes
    // simpler like in IterateObjects?
    while ((chunk == last()) ? (current < top()) : !HasSentinelAt(current)) {
      HeapObject* object = HeapObject::FromAddress(current);
      if (object->forwarding_address() != NULL) {
        current += Instance::kSize;
        while (*reinterpret_cast<uword*>(current) == HeapObject::kTag) {
          current += kPointerSize;
        }
      } else if (object->IsStack()) {
        // We haven't cooked stacks when we perform object transformations.
        // Therefore, we cannot simply iterate pointers in the stack because
        // that would look at the raw bytecode pointers as well. Instead we
        // iterate the actual pointers in each frame directly.
        ASSERT(process != NULL);
        StackWalker stack_walker(process, Stack::cast(object));
        while (stack_walker.MoveNext()) {
          stack_walker.VisitPointersInFrame(visitor);
        }
        current += object->Size();
      } else {
        object->IteratePointers(visitor);
        current += object->Size();
      }
    }
  }
}

void PageDirectory::Clear() {
  memset(&tables_, 0, kPointerSize * ARRAY_SIZE(tables_));
}

void PageDirectory::Delete() {
  for (unsigned i = 0; i < ARRAY_SIZE(tables_); i++) {
    delete tables_[i];
  }
  Clear();
}

Mutex* ObjectMemory::mutex_;
#ifdef FLETCH32
PageDirectory ObjectMemory::page_directory_;
#else
PageDirectory* ObjectMemory::page_directories_[1 << 13];
#endif


void ObjectMemory::Setup() {
  mutex_ = Platform::CreateMutex();
#ifdef FLETCH32
  page_directory_.Clear();
#else
  memset(&page_directories_, 0, kPointerSize * ARRAY_SIZE(page_directories_));
#endif
}

void ObjectMemory::TearDown() {
#ifdef FLETCH32
  page_directory_.Delete();
#else
  for (unsigned i = 0; i < ARRAY_SIZE(page_directories_); i++) {
    PageDirectory* directory = page_directories_[i];
    if (directory == NULL) continue;
    directory->Delete();
    page_directories_[i] = NULL;
    delete directory;
  }
#endif
  delete mutex_;
}

#ifdef DEBUG
void Chunk::Scramble() {
  void* p = reinterpret_cast<void*>(base());
  memset(p, 0xab, limit() - base());
}
#endif


Chunk* ObjectMemory::AllocateChunk(Space* owner, int size) {
  ASSERT(owner != NULL);

  size = Utils::RoundUp(size, kPageSize);
  void* memory;
#if defined(ANDROID) || defined(FLETCH_BAREMETAL)
  // posix_memalign doesn't exist on Android. We fallback to
  // memalign.
  memory = memalign(kPageSize, size);
  if (memory == NULL) return NULL;
#else
  if (posix_memalign(&memory, kPageSize, size) != 0) return NULL;
#endif

  uword base = reinterpret_cast<uword>(memory);
  Chunk* chunk = new Chunk(owner, base, size);
#ifdef DEBUG
  chunk->Scramble();
#endif
  SetSpaceForPages(chunk->base(), chunk->limit(), owner);
  return chunk;
}

void ObjectMemory::FreeChunk(Chunk* chunk) {
#ifdef DEBUG
  chunk->Scramble();
#endif
  SetSpaceForPages(chunk->base(), chunk->limit(), NULL);
  delete chunk;
}

bool ObjectMemory::IsAddressInSpace(uword address, const Space* space) {
  PageTable* table = GetPageTable(address);
  return (table != NULL)
      ? table->Get((address >> 12) & 0x3ff) == space
      : false;
}

PageTable* ObjectMemory::GetPageTable(uword address) {
#ifdef FLETCH32
  return page_directory_.Get(address >> 22);
#else
  PageDirectory* directory = page_directories_[address >> 35];
  if (directory == NULL) return NULL;
  return directory->Get((address >> 22) & 0x1fff);
#endif
}

void ObjectMemory::SetPageTable(uword address, PageTable* table) {
  ASSERT(mutex_->IsLocked());
#ifdef FLETCH32
  page_directory_.Set(address >> 22, table);
#else
  int index = address >> 35;
  PageDirectory* directory = page_directories_[index];
  if (directory == NULL) {
    page_directories_[index] = directory = new PageDirectory();
    directory->Clear();
  }
  return directory->Set((address >> 22) & 0x1fff, table);
#endif
}

void ObjectMemory::SetSpaceForPages(uword base, uword limit, Space* space) {
  ASSERT(Utils::IsAligned(base, kPageSize));
  ASSERT(Utils::IsAligned(limit, kPageSize));
  for (uword address = base; address < limit; address += kPageSize) {
    PageTable* table = GetPageTable(address);
    if (table == NULL) {
      ASSERT(space != NULL);
      ScopedLock scope(mutex_);
      // Fetch the table again while locked to make sure only one thread
      // gets to initialize the directory entry.
      table = GetPageTable(address);
      if (table == NULL) {
        table = new PageTable(address & ~0x3fffff);
        SetPageTable(address, table);
      }
    }
    table->Set((address >> 12) & 0x3ff, space);
  }
}

}  // namespace fletch
