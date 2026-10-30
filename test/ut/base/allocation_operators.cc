// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Only linked into noninstalled recovery test images. Never a product
// allocator.
#include <cstdlib>
#include <new>

#include "allocation_failure.hpp"
#if defined(_WIN32)
#include <malloc.h>
#endif

namespace {
void* Allocate(size_t bytes, size_t alignment = 0) {
  if (!skity::testing::ConsumeAllocationAllowance()) throw std::bad_alloc();
  if (!bytes) bytes = 1;
#if defined(SKITY_TEST_DARWIN_ASAN_RUNTIME)
  if (!alignment) return skity::testing::AllocateFromRuntime(bytes);
#endif
  for (;;) {
    void* pointer = nullptr;
    if (alignment && alignment < sizeof(void*)) alignment = sizeof(void*);
    if (!alignment)
      pointer = std::malloc(bytes);
    else {
#if defined(_WIN32)
      pointer = _aligned_malloc(bytes, alignment);
#else
      if (posix_memalign(&pointer, alignment, bytes) != 0) pointer = nullptr;
#endif
    }
    if (pointer) return pointer;
    const auto handler = std::get_new_handler();
    if (!handler) throw std::bad_alloc();
    handler();
  }
}

void FreeAligned(void* p, std::size_t alignment) noexcept {
  (void)alignment;
#if defined(_WIN32)
  _aligned_free(p);
#else
  std::free(p);
#endif
}
void Free(void* p) noexcept {
#if defined(SKITY_TEST_DARWIN_ASAN_RUNTIME)
  skity::testing::FreeToRuntime(p);
#else
  std::free(p);
#endif
}
}  // namespace
void* operator new(size_t n) { return Allocate(n); }
void* operator new[](size_t n) { return Allocate(n); }
void* operator new(size_t n, const std::nothrow_t&) noexcept {
  try {
    return Allocate(n);
  } catch (...) {
    return nullptr;
  }
}
void* operator new[](size_t n, const std::nothrow_t& tag) noexcept {
  return ::operator new(n, tag);
}
void operator delete(void* p) noexcept { Free(p); }
void operator delete[](void* p) noexcept { Free(p); }
void operator delete(void* p, size_t) noexcept { Free(p); }
void operator delete[](void* p, size_t) noexcept { Free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { Free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { Free(p); }
void* operator new(size_t n, std::align_val_t a) {
  return Allocate(n, static_cast<size_t>(a));
}
void* operator new[](size_t n, std::align_val_t a) {
  return Allocate(n, static_cast<size_t>(a));
}
void* operator new(size_t n, std::align_val_t a,
                   const std::nothrow_t&) noexcept {
  try {
    return Allocate(n, static_cast<size_t>(a));
  } catch (...) {
    return nullptr;
  }
}
void* operator new[](size_t n, std::align_val_t a,
                     const std::nothrow_t& tag) noexcept {
  return ::operator new(n, a, tag);
}
void operator delete(void* p, std::align_val_t alignment) noexcept {
  FreeAligned(p, static_cast<std::size_t>(alignment));
}
void operator delete[](void* p, std::align_val_t alignment) noexcept {
  FreeAligned(p, static_cast<std::size_t>(alignment));
}
void operator delete(void* p, size_t, std::align_val_t alignment) noexcept {
  FreeAligned(p, static_cast<std::size_t>(alignment));
}
void operator delete[](void* p, size_t, std::align_val_t alignment) noexcept {
  FreeAligned(p, static_cast<std::size_t>(alignment));
}
void operator delete(void* p, std::align_val_t alignment,
                     const std::nothrow_t&) noexcept {
  FreeAligned(p, static_cast<std::size_t>(alignment));
}
void operator delete[](void* p, std::align_val_t alignment,
                       const std::nothrow_t&) noexcept {
  FreeAligned(p, static_cast<std::size_t>(alignment));
}
