// Copyright (c) 2014, the Fletch project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.md file.

#ifndef SRC_VM_THREAD_H_
#define SRC_VM_THREAD_H_

#include "src/shared/globals.h"
#include "src/shared/platform.h"

#if defined(FLETCH_TARGET_OS_LK)
#include "src/vm/thread_lk.h"
#endif
#if defined(FLETCH_TARGET_OS_POSIX)
#include "src/vm/thread_posix.h"
#endif

namespace fletch {

// A ThreadIdentifier represents a thread identifier for a thread.
// The ThreadIdentifier does not own the underlying OS handle.
// Thread handles can be used for referring to threads and testing equality.
class ThreadIdentifier;

// Thread are started using the static Thread::Run method.
class Thread {
 public:
  // Returns true if 'thread' is the current thread.
  static bool IsCurrent(const ThreadIdentifier* thread);

  typedef void* (*RunSignature)(void*);
  static void Run(RunSignature run, void* data = NULL);

 private:
  DISALLOW_ALLOCATION();
};

}  // namespace fletch


#endif  // SRC_VM_THREAD_H_
