// Copyright (c) 2015, the Fletch project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.md file.

// Generated file. Do not edit.

package fletch;

import java.util.List;

public class AgeStats extends Reader {
  public AgeStats() { }

  public AgeStats(byte[] memory, int offset) {
    super(memory, offset);
  }

  public AgeStats(Segment segment, int offset) {
    super(segment, offset);
  }

  public AgeStats(byte[][] segments, int offset) {
    super(segments, offset);
  }

  public static AgeStats create(Object rawData) {
    if (rawData instanceof byte[]) {
      return new AgeStats((byte[])rawData, 8);
    }
    return new AgeStats((byte[][])rawData, 8);
  }

  public int getAverageAge() { return segment.buffer().getInt(base + 0); }

  public int getSum() { return segment.buffer().getInt(base + 4); }
}
