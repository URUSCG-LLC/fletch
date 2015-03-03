// Copyright (c) 2015, the Fletch project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.md file.

// Generated file. Do not edit.

package fletch;

public class Small extends Reader {
  public Small(byte[] memory) {
    super(memory);
  }

  public Small(byte[][] segments) {
    super(segments);
  }

  public int getX() { return getIntAt(0); }
}