// Copyright (c) 2014, the Fletch project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.md file.

#ifndef SRC_VM_PROCESS_H_
#define SRC_VM_PROCESS_H_

#include "src/shared/atomic.h"
#include "src/shared/random.h"

#include "src/vm/heap.h"
#include "src/vm/lookup_cache.h"
#include "src/vm/program.h"
#include "src/vm/storebuffer.h"
#include "src/vm/thread.h"

namespace fletch {

class Engine;
class Interpreter;
class Port;
class PortQueue;
class ProcessQueue;
class DebugInfo;

class ThreadState {
 public:
  ThreadState();
  ~ThreadState();

  int thread_id() const { return thread_id_; }
  void set_thread_id(int thread_id) {
    ASSERT(thread_id_ == -1);
    thread_id_ = thread_id;
  }

  const ThreadIdentifier* thread() const { return &thread_; }

  // Update the thread field to point to the current thread.
  void AttachToCurrentThread();

  ProcessQueue* queue() { return queue_; }

  LookupCache* cache() const { return cache_; }
  LookupCache* EnsureCache();

  Monitor* idle_monitor() const { return idle_monitor_; }

  ThreadState* next_idle_thread() const { return next_idle_thread_; }
  void set_next_idle_thread(ThreadState* value) { next_idle_thread_ = value; }

 private:
  int thread_id_;
  ThreadIdentifier thread_;
  ProcessQueue* const queue_;
  LookupCache* cache_;
  Monitor* idle_monitor_;
  Atomic<ThreadState*> next_idle_thread_;
};

class Process {
 public:
  enum State {
    kSleeping,
    kReady,
    kRunning,
    kYielding,
    kBreakPoint,
    kCompileTimeError,
    kUncaughtException,
    kBlocked,
    kTerminated,
  };

  enum ProgramGCState {
    kUnknown,
    kFound,
    kProcessed,
  };

  enum StackCheckResult {
    kStackCheckContinue,
    kStackCheckInterrupt,
    kStackCheckOverflow
  };

  Function* entry() { return program_->entry(); }
  int main_arity() { return program_->main_arity(); }
  Program* program() { return program_; }
  Array* statics() const { return statics_; }
  Heap* heap() { return &heap_; }
  Heap* immutable_heap() { return &immutable_heap_; }

  Coroutine* coroutine() const { return coroutine_; }
  void UpdateCoroutine(Coroutine* coroutine);

  Stack* stack() const { return coroutine_->stack(); }
  Object** stack_limit() const { return stack_limit_.load(); }

  Port* ports() const { return ports_; }
  void set_ports(Port* port) { ports_ = port; }

  void SetupExecutionStack();
  StackCheckResult HandleStackOverflow(int addition);

  inline LookupCache::Entry* LookupEntry(Object* receiver, int selector);

    // Lookup and update the primary cache entry.
  LookupCache::Entry* LookupEntrySlow(LookupCache::Entry* primary,
                                      Class* clazz,
                                      int selector);

  Object* NewArray(int length);
  Object* NewDouble(double value);
  Object* NewInteger(int64 value);

  // Attempt to deallocate the large integer object. If the large integer
  // was the last allocated object the allocation top is moved back so
  // the memory can be reused.
  void TryDeallocInteger(LargeInteger* object);

  // NewString allocates a string of the given length and fills the payload
  // with zeroes.
  Object* NewString(int length);

  // NewStringUninitialized allocates a string of the given length and
  // leaves the payload uninitialized. The payload contains whatever
  // was in that heap space before. Only use this if you intend to
  // immediately overwrite the payload with something else.
  Object* NewStringUninitialized(int length);
  Object* NewStringFromAscii(List<const char> value);
  Object* NewBoxed(Object* value);
  Object* NewStack(int length);

  Object* NewInstance(Class* klass, bool immutable = false);

  // Returns either a Smi or a LargeInteger.
  Object* ToInteger(int64 value);

  Object* Concatenate(String* x, String* y);

  void CollectGarbage();
  void CollectImmutableGarbage();
  void CollectMutableGarbage();

  // Perform garbage collection and chain all stack objects. Additionally,
  // locate all processes in ports in the heap that are not yet known
  // by the program GC and link them in the argument list. Returns the
  // number of stacks found in the heap.
  int CollectMutableGarbageAndChainStacks();
  int CollectGarbageAndChainStacks();

  void ValidateHeaps();

  // Iterate all pointers reachable from this process object.
  void IterateRoots(PointerVisitor* visitor);

  // Iterate all pointers in the process heap and stack. Used for
  // program garbage collection.
  void IterateProgramPointers(PointerVisitor* visitor);

  // Iterate over, and find pointers in the port queue.
  void IteratePortQueuesPointers(PointerVisitor* visitor);

  // Iterate over, and find processes (via ports) in the port queue.
  void IteratePortQueuesProcesses(ProcessVisitor* visitor);

  void Preempt();

  void Profile();

  // Debugging support.
  void AttachDebugger(DebugInfo* debug_info);
  int PrepareStepOver();
  int PrepareStepOut();

  DebugInfo* debug_info() { return debug_info_; }
  bool is_debugging() const { return debug_info_ != NULL; }

  Process* next() const { return next_; }
  void set_next(Process* process) { next_ = process; }

  void TakeLookupCache();
  void ReleaseLookupCache() { primary_lookup_cache_ = NULL; }

  // Program GC support. Cook the stack to rewrite bytecode pointers
  // to a pair of a function pointer and a delta. Uncook the stack to
  // rewriting the (now potentially moved) function pointer and the
  // delta into a direct bytecode pointer again.
  void CookStacks(int number_of_stacks);
  void UncookAndUnchainStacks();

  bool stacks_are_cooked() { return !cooked_stack_deltas_.is_empty(); }

  // Program GC support. Update breakpoints after having moved function.
  // Bytecode pointers need to be updated.
  void UpdateBreakpoints();

  // Change the state from 'from' to 'to. Return 'true' if the operation was
  // successful.
  inline bool ChangeState(State from, State to);
  State state() const { return state_; }

  ThreadState* thread_state() const { return thread_state_; }
  void set_thread_state(ThreadState* thread_state) {
    ASSERT(thread_state == NULL || thread_state_ == NULL);
    thread_state_ = thread_state;
  }

  // Thread-safe way of adding a 'message' at the end of the process'
  // message queue. Returns false if the object is of wrong type.
  bool Enqueue(Port* port, Object* message);
  bool EnqueueForeign(Port* port, void* foreign, int size, bool finalized);
  void EnqueueExit(Process* sender, Port* port, Object* message);

  // Determine if it's possible to enqueue the given 'object' in the
  // message queue of some process. If it's valid for this process, it's valid
  // for all processes.
  bool IsValidForEnqueue(Object* message);

  // Thread-safe way of asking if the message queue of [this] process is empty.
  bool IsQueueEmpty() const { return last_message_ == NULL; }

  // Queue iteration. These methods should only be called from the thread
  // currently owning the process.
  void TakeQueue();
  PortQueue* CurrentMessage();
  void AdvanceCurrentMessage();

  void TakeChildHeaps();

  void RegisterFinalizer(HeapObject* object, WeakPointerCallback callback);
  void UnregisterFinalizer(HeapObject* object);

  static void FinalizeForeign(HeapObject* foreign);

  // This is used in order to return a retry after gc failure on every other
  // call to the GC native that is used for testing only.
  bool TrueThenFalse();

  ProcessQueue* process_queue() const { return queue_; }

  static uword CoroutineOffset() { return OFFSET_OF(Process, coroutine_); }
  static uword StackLimitOffset() { return OFFSET_OF(Process, stack_limit_); }
  static uword ProgramOffset() { return OFFSET_OF(Process, program_); }
  static uword StaticsOffset() { return OFFSET_OF(Process, statics_); }
  static uword PrimaryLookupCacheOffset() {
    return OFFSET_OF(Process, primary_lookup_cache_);
  }

  void StoreErrno();
  void RestoreErrno();

  void IncrementBlocked() {
    ++block_count_;
  }

  bool DecrementBlocked() {
    return (--block_count_) == 0;
  }

  bool IsBlocked() const { return block_count_ > 0; }

  void set_blocked(Process* value) { blocked_ = value; }
  Process* blocked() const { return blocked_; }

  RandomLCG* random() { return &random_; }

  StoreBuffer* store_buffer() { return &store_buffer_; }

  void RecordStore(HeapObject* object, Object* value) {
    if (value->IsHeapObject() && value->IsImmutable()) {
      ASSERT(!program()->heap()->space()->Includes(
          object->address()));
      ASSERT(heap()->space()->Includes(
          object->address()));
      store_buffer_.Insert(object);
    }
  }

 private:
  friend class Interpreter;
  friend class Engine;
  friend class Program;

  // Creation and deletion of processes is managed by a [Program].
  explicit Process(Program* program);
  virtual ~Process();

  void UpdateStackLimit();

  // Put 'entry' at the end of the port's queue. This function is thread safe.
  void EnqueueEntry(PortQueue* entry);

  void set_process_list_next(Process* process) { process_list_next_ = process; }
  Process* process_list_next() { return process_list_next_; }
  void set_process_list_prev(Process* process) { process_list_prev_ = process; }
  Process* process_list_prev() { return process_list_prev_; }

  RandomLCG random_;

  Heap heap_;
  Heap immutable_heap_;
  StoreBuffer store_buffer_;
  Program* program_;
  Array* statics_;

  Coroutine* coroutine_;
  Atomic<Object**> stack_limit_;

  Atomic<State> state_;
  Atomic<ThreadState*> thread_state_;

  // We need extremely fast access to the primary lookup cache, so we
  // store a reference to it in the process whenever we're interpreting
  // code in this process.
  LookupCache::Entry* primary_lookup_cache_;

  List<List<int>> cooked_stack_deltas_;

  // Next pointer used by the Scheduler.
  Process* next_;

  // Fields used by ProcessQueue, when holding the Process.
  friend class ProcessQueue;
  Atomic<ProcessQueue*> queue_;
  // While the ProcessQueue is lock-free, we have an 'atomic lock' on the
  // head_ element. That will ensure we have the right memory order on
  // queue_next_/queue_previous_, as they are always read/modified while
  // head_ is 'locked'.
  Process* queue_next_;
  Process* queue_previous_;

  // Linked list of ports owned by this process.
  Port* ports_;

  // Pointer to the last PortQueue element of this process.
  Atomic<PortQueue*> last_message_;

  // Process-local list of PortQueue elements currently being processed.
  PortQueue* current_message_;

  // Used for chaining all processes of a program. It is protected by a lock
  // in the program.
  Process* process_list_next_;
  Process* process_list_prev_;

  int errno_cache_;

  DebugInfo* debug_info_;

  Atomic<int> block_count_;
  Process* blocked_;

#ifdef DEBUG
  bool true_then_false_;
#endif
};

inline LookupCache::Entry* Process::LookupEntry(Object* receiver,
                                                int selector) {
  ASSERT(!program()->is_compact());

  Class* clazz = receiver->IsSmi()
      ? program()->smi_class()
      : HeapObject::cast(receiver)->get_class();
  ASSERT(primary_lookup_cache_ != NULL);

  uword index = LookupCache::ComputePrimaryIndex(clazz, selector);
  LookupCache::Entry* primary = &(primary_lookup_cache_[index]);
  return (primary->clazz == clazz && primary->selector == selector)
      ? primary
      : LookupEntrySlow(primary, clazz, selector);
}

inline bool Process::ChangeState(State from, State to) {
  if (from == kRunning || from == kYielding || from == kBlocked) {
    ASSERT(thread_state_ == NULL);
    ASSERT(state_ == from);
    state_ = to;
    return true;
  }
  State value = state_;
  while (true) {
    if (value == kYielding) {
      value = state_;
      continue;
    }
    if (value != from) break;
    if (state_.compare_exchange_weak(value, to)) return true;
  }
  return false;
}

}  // namespace fletch

#endif  // SRC_VM_PROCESS_H_
