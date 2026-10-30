// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "allocation_failure.hpp"

#include <cassert>
#include <new>

namespace skity {
namespace testing {
namespace {
thread_local int remaining = -1;
}
void BeginAllocationFailure(int allowance) {
  assert(remaining == -1 && allowance >= 0);
  remaining = allowance;
}
void EndAllocationFailure() { remaining = -1; }
bool ConsumeAllocationAllowance() {
  if (remaining == 0) return false;
  if (remaining > 0) --remaining;
  return true;
}
#if defined(SKITY_TEST_DARWIN_ASAN_RUNTIME)
namespace {
thread_local bool in_runtime = false;
}
void* AllocateFromRuntime(std::size_t bytes) {
  assert(!in_runtime);
  struct Scope {
    Scope() { in_runtime = true; }
    ~Scope() { in_runtime = false; }
  } scope;
  return ::operator new(bytes);
}
void FreeToRuntime(void* pointer) noexcept {
  assert(!in_runtime);
  struct Scope {
    Scope() { in_runtime = true; }
    ~Scope() { in_runtime = false; }
  } scope;
  ::operator delete(pointer);
}
#endif
}  // namespace testing
}  // namespace skity
