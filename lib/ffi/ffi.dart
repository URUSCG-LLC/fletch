// Copyright (c) 2014, the Fletch project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.md file.

// The dart:ffi library is a low-level 'foreign function interface'
// library that allows Dart code to call arbitrary native platform
// code defined outside the VM.
library dart.ffi;

class Foreign {
  static const int UNKNOWN = 0;
  static const int LINUX   = 1;
  static const int MACOS   = 2;

  static final Foreign NULL = new Foreign();

  int _value;
  int _size;

  Foreign() : _value = 0, _size = 0;

  Foreign.allocated(this._size) {
    _value = _allocate(_size);
  }

  Foreign.allocatedFinalize(this._size) {
    _value = _allocate(_size);
    _markForFinalization();
  }

  Foreign.fromAddress(this._value, this._size);

  Foreign.fromAddressFinalize(this._value, this._size) {
    _markForFinalization();
  }

  factory Foreign.fromString(String str) {
    var foreign = new Foreign.allocated(str.length + 1);
    for (int i = 0; i < str.length; i++) {
      foreign.setUint8(i, str.codeUnitAt(i));
    }
    return foreign;
  }

  static Foreign lookup(String name, {String library}) {
    if (library != null && library.isEmpty) library = null;
    return new Foreign.fromAddress(_lookup(name, library), 0);
  }

  // TODO(kasperl): Not quite sure where this fits in.
  static final int bitsPerMachineWord = _bitsPerMachineWord();
  static final int platform = _platform();

  int get value => _value;
  int get size => _size;

  int getInt8(int offset)
      => _getInt8(_computeAddress(offset, 1));
  int getInt16(int offset)
      => _getInt16(_computeAddress(offset, 2));
  int getInt32(int offset)
      => _getInt32(_computeAddress(offset, 4));
  int getInt64(int offset)
      => _getInt64(_computeAddress(offset, 8));

  int setInt8(int offset, int value)
      => _setInt8(_computeAddress(offset, 1), value);
  int setInt16(int offset, int value)
      => _setInt16(_computeAddress(offset, 2), value);
  int setInt32(int offset, int value)
      => _setInt32(_computeAddress(offset, 4), value);
  int setInt64(int offset, int value)
      => _setInt64(_computeAddress(offset, 8), value);

  int getUint8(int offset)
      => _getUint8(_computeAddress(offset, 1));
  int getUint16(int offset)
      => _getUint16(_computeAddress(offset, 2));
  int getUint32(int offset)
      => _getUint32(_computeAddress(offset, 4));
  int getUint64(int offset)
      => _getUint64(_computeAddress(offset, 8));

  int setUint8(int offset, int value)
      => _setUint8(_computeAddress(offset, 1), value);
  int setUint16(int offset, int value)
      => _setUint16(_computeAddress(offset, 2), value);
  int setUint32(int offset, int value)
      => _setUint32(_computeAddress(offset, 4), value);
  int setUint64(int offset, int value)
      => _setUint64(_computeAddress(offset, 8), value);

  void copyBytesToList(List<int> list, int from, int to, int listOffset) {
    int length = to - from;
    for (int i = 0; i < length; i++) {
      list[listOffset + i] = getUint8(from + i);
    }
  }

  void copyBytesFromList(List<int> list, int from, int to, int listOffset) {
    int length = to - from;
    for (int i = 0; i < length; i++) {
      setUint8(from + i, list[listOffset + i]);
    }
  }

  void free() {
    if (_size > 0) _free(_value);
    _value = 0;
    _size = 0;
  }

  // Support for calling foreign functions that return
  // machine words as immediate integer values.
  int icall$0() => _call$0(_value);
  int icall$1(a0) => _call$1(_value, _convert(a0));
  int icall$2(a0, a1) => _call$2(_value, _convert(a0), _convert(a1));
  int icall$3(a0, a1, a2) {
    return _call$3(_value, _convert(a0), _convert(a1), _convert(a2));
  }
  int icall$4(a0, a1, a2, a3) {
    return _call$4(_value,
                   _convert(a0),
                   _convert(a1),
                   _convert(a2),
                   _convert(a3));
  }
  int icall$5(a0, a1, a2, a3, a4) {
    return _call$5(_value,
                   _convert(a0),
                   _convert(a1),
                   _convert(a2),
                   _convert(a3),
                   _convert(a4));
  }
  int icall$6(a0, a1, a2, a3, a4, a5) {
    return _call$6(_value,
                   _convert(a0),
                   _convert(a1),
                   _convert(a2),
                   _convert(a3),
                   _convert(a4),
                   _convert(a5));
  }

  // Support for calling foreign functions that return
  // machine words -- typically pointers -- encapulated in
  // the given foreign object arguments.
  Foreign pcall$0(Foreign foreign) {
    foreign._value = _call$0(_value);
    return foreign;
  }

  Foreign pcall$1(Foreign foreign, a0) {
    foreign._value = _call$1(_value, _convert(a0));
    return foreign;
  }

  Foreign pcall$2(Foreign foreign, a0, a1) {
    foreign._value = _call$2(_value, _convert(a0), _convert(a1));
    return foreign;
  }

  // Helper for checking bounds and computing derived
  // address for memory address functionality.
  int _computeAddress(int offset, int n) {
    if (offset < 0 || offset + n > _size) throw new IndexError(offset, this);
    return _value + offset;
  }

  // Helper for converting the argument to a machine word.
  int _convert(argument) {
    if (argument is Foreign) return argument._value;
    if (argument is Port) return _convertPort(argument);
    if (argument is int) return argument;
    throw new ArgumentError();
  }

  // Natives needed for FFI support.
  static int _lookup(String name, String library) native catch (error) {
    throw (error != _indexOutOfBounds) ? error : new ArgumentError();
  }

  static int _call$0(int address) native;
  static int _call$1(int address, a0) native;
  static int _call$2(int address, a0, a1) native;
  static int _call$3(int address, a0, a1, a2) native;
  static int _call$4(int address, a0, a1, a2, a3) native;
  static int _call$5(int address, a0, a1, a2, a3, a4) native;
  static int _call$6(int address, a0, a1, a2, a3, a4, a5) native;

  static int _allocate(int size) native;
  static void _free(int address) native;
  void _markForFinalization() native;

  static int _getInt8(int address) native;
  static int _getInt16(int address) native;
  static int _getInt32(int address) native;
  static int _getInt64(int address) native;

  static int _setInt8(int address, int value) native;
  static int _setInt16(int address, int value) native;
  static int _setInt32(int address, int value) native;
  static int _setInt64(int address, int value) native;

  static int _getUint8(int address) native;
  static int _getUint16(int address) native;
  static int _getUint32(int address) native;
  static int _getUint64(int address) native;

  static int _setUint8(int address, int value) native;
  static int _setUint16(int address, int value) native;
  static int _setUint32(int address, int value) native;
  static int _setUint64(int address, int value) native;

  static int _bitsPerMachineWord() native;
  static int _errno() native;
  static int _platform() native;
  static int _convertPort(Port port) native;
}
