// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_UT_BASE_ALLOCATION_FAILURE_HPP
#define TEST_UT_BASE_ALLOCATION_FAILURE_HPP

#if defined(_WIN32)
#if defined(SKITY_ALLOCATION_TEST_SUPPORT_IMPL)
#define SKITY_ALLOCATION_TEST_API __declspec(dllexport)
#else
#define SKITY_ALLOCATION_TEST_API __declspec(dllimport)
#endif
#else
#define SKITY_ALLOCATION_TEST_API __attribute__((visibility("default")))
#endif

#include <cstddef>

#if defined(__APPLE__) && defined(__clang__)
#if __has_feature(address_sanitizer)
#define SKITY_TEST_DARWIN_ASAN_RUNTIME 1
#endif
#endif

namespace skity {
namespace testing {
// Private, noninstalled test support. One shared counter drives the replacement
// operators compiled into each tested image, including sanitized shared cores.
SKITY_ALLOCATION_TEST_API void BeginAllocationFailure(int allowance);
SKITY_ALLOCATION_TEST_API void EndAllocationFailure();
SKITY_ALLOCATION_TEST_API bool ConsumeAllocationAllowance();
#if defined(SKITY_TEST_DARWIN_ASAN_RUNTIME)
// This support image defines no replacement operators. Its standard-library
// calls retain Darwin ASan's native C++ allocation/deallocation family tags.
SKITY_ALLOCATION_TEST_API void* AllocateFromRuntime(std::size_t bytes);
SKITY_ALLOCATION_TEST_API void FreeToRuntime(void* pointer) noexcept;
#endif

class AllocationFailure final {
 public:
  explicit AllocationFailure(int allowance) {
    BeginAllocationFailure(allowance);
  }
  ~AllocationFailure() { EndAllocationFailure(); }
  AllocationFailure(const AllocationFailure&) = delete;
  AllocationFailure& operator=(const AllocationFailure&) = delete;
};
}  // namespace testing
}  // namespace skity
#endif
