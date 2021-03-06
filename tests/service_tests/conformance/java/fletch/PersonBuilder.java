// Copyright (c) 2015, the Fletch project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.md file.

// Generated file. Do not edit.

package fletch;

import java.util.List;
public class PersonBuilder extends Builder {
  public static int kSize = 24;
  public PersonBuilder(BuilderSegment segment, int offset) {
    super(segment, offset);
  }

  public PersonBuilder() {
    super();
  }

  public void setName(String value) {
    newString(0, value);
  }

  public Uint16ListBuilder initNameData(int length) {
    ListBuilder builder = new ListBuilder();
    newList(builder, 0, length, 2);
    return new Uint16ListBuilder(builder);
  }

  public PersonListBuilder initChildren(int length) {
    ListBuilder builder = new ListBuilder();
    newList(builder, 8, length, 24);
    return new PersonListBuilder(builder);
  }

  public void setAge(int value) {
    segment.buffer().putInt(base + 16, (int)value);
  }
}
