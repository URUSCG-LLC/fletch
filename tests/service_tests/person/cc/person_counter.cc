// Copyright (c) 2015, the Fletch project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.md file.

// Generated file. Do not edit.

#include "person_counter.h"
#include "include/service_api.h"
#include <stdlib.h>

static ServiceId _service_id = kNoServiceId;

void PersonCounter::Setup() {
  _service_id = ServiceApiLookup("PersonCounter");
}

void PersonCounter::TearDown() {
  ServiceApiTerminate(_service_id);
  _service_id = kNoServiceId;
}

static const MethodId _kGetAgeId = reinterpret_cast<MethodId>(1);

int32_t PersonCounter::GetAge(PersonBuilder person) {
  return person.InvokeMethod(_service_id, _kGetAgeId);
}

static const MethodId _kGetBoxedAgeId = reinterpret_cast<MethodId>(2);

int32_t PersonCounter::GetBoxedAge(PersonBoxBuilder box) {
  return box.InvokeMethod(_service_id, _kGetBoxedAgeId);
}

static const MethodId _kGetAgeStatsId = reinterpret_cast<MethodId>(3);

AgeStats PersonCounter::GetAgeStats(PersonBuilder person) {
  int64_t result = person.InvokeMethod(_service_id, _kGetAgeStatsId);
  char* memory = reinterpret_cast<char*>(result);
  Segment* segment = MessageReader::GetRootSegment(memory, 8);
  return AgeStats(segment, 0);
}

static const MethodId _kCreateAgeStatsId = reinterpret_cast<MethodId>(4);

AgeStats PersonCounter::CreateAgeStats(int32_t averageAge, int32_t sum) {
  static const int kSize = 40;
  char _bits[kSize];
  char* _buffer = _bits;
  *reinterpret_cast<int32_t*>(_buffer + 32) = averageAge;
  *reinterpret_cast<int32_t*>(_buffer + 36) = sum;
  ServiceApiInvoke(_service_id, _kCreateAgeStatsId, _buffer, kSize);
  int64_t result = *reinterpret_cast<int64_t*>(_buffer + 32);
  char* memory = reinterpret_cast<char*>(result);
  Segment* segment = MessageReader::GetRootSegment(memory, 8);
  return AgeStats(segment, 8);
}

static const MethodId _kCreatePersonId = reinterpret_cast<MethodId>(5);

Person PersonCounter::CreatePerson(int32_t children) {
  static const int kSize = 40;
  char _bits[kSize];
  char* _buffer = _bits;
  *reinterpret_cast<int32_t*>(_buffer + 32) = children;
  ServiceApiInvoke(_service_id, _kCreatePersonId, _buffer, kSize);
  int64_t result = *reinterpret_cast<int64_t*>(_buffer + 32);
  char* memory = reinterpret_cast<char*>(result);
  Segment* segment = MessageReader::GetRootSegment(memory, 16);
  return Person(segment, 8);
}

static const MethodId _kCreateNodeId = reinterpret_cast<MethodId>(6);

Node PersonCounter::CreateNode(int32_t depth) {
  static const int kSize = 40;
  char _bits[kSize];
  char* _buffer = _bits;
  *reinterpret_cast<int32_t*>(_buffer + 32) = depth;
  ServiceApiInvoke(_service_id, _kCreateNodeId, _buffer, kSize);
  int64_t result = *reinterpret_cast<int64_t*>(_buffer + 32);
  char* memory = reinterpret_cast<char*>(result);
  Segment* segment = MessageReader::GetRootSegment(memory, 16);
  return Node(segment, 8);
}

static const MethodId _kCountId = reinterpret_cast<MethodId>(7);

int32_t PersonCounter::Count(PersonBuilder person) {
  return person.InvokeMethod(_service_id, _kCountId);
}

static const MethodId _kDepthId = reinterpret_cast<MethodId>(8);

int32_t PersonCounter::Depth(NodeBuilder node) {
  return node.InvokeMethod(_service_id, _kDepthId);
}

List<PersonBuilder> PersonBuilder::NewChildren(int length) {
  Reader result = NewList(8, length, 16);
  return List<PersonBuilder>(result, length);
}

PersonBuilder PersonBoxBuilder::NewPerson() {
  Builder result = NewStruct(0, 16);
  return PersonBuilder(result);
}

Person PersonBox::person() const { return ReadStruct<Person>(0); }

ConsBuilder NodeBuilder::NewCons() {
  set_tag(2);
  Builder result = NewStruct(8, 16);
  return ConsBuilder(result);
}

Cons Node::cons() const { return ReadStruct<Cons>(8); }

NodeBuilder ConsBuilder::NewFst() {
  Builder result = NewStruct(0, 16);
  return NodeBuilder(result);
}

NodeBuilder ConsBuilder::NewSnd() {
  Builder result = NewStruct(8, 16);
  return NodeBuilder(result);
}

Node Cons::fst() const { return ReadStruct<Node>(0); }

Node Cons::snd() const { return ReadStruct<Node>(8); }
