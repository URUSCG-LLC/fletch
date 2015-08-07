
// Copyright (c) 2015, the Fletch project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.md file.

#if defined(FLETCH_TARGET_OS_LK)

#include "src/vm/event_handler.h"

#include "src/vm/thread.h"

namespace fletch {

int EventHandler::Create() {
  return 0;
}

void EventHandler::Run() {
}

}  // namespace fletch

#endif  // defined(FLETCH_TARGET_OS_LK)
