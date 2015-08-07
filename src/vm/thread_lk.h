// Copyright (c) 2014, the Fletch project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.md file.

#ifndef SRC_VM_THREAD_LK_H_
#define SRC_VM_THREAD_LK_H_

#ifndef SRC_VM_THREAD_H_
#error "Don't include thread_lk.h directly, include thread.h."
#endif

#include <kernel/thread.h>

namespace fletch {

// A ThreadIdentifier represents a thread identifier for a thread.
// The ThreadIdentifier does not own the underlying OS handle.
// Thread handles can be used for referring to threads and testing equality.
class ThreadIdentifier {
 public:
  ThreadIdentifier() : thread_(get_current_thread()) { }

  // Test for thread running.
  bool IsSelf() const {
    return thread_ == get_current_thread();
  }

 private:
  thread_t *thread_;
};

}  // namespace fletch


#endif  // SRC_VM_THREAD_POSIX_H_
