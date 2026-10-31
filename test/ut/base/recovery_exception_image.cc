// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// A private image compiled with the core recovery options and production-like
// hidden visibility. Unit builds normally expose internals, which can conceal
// exception type-identity failures at a caller's shared-library boundary.
#include <new>
#include <stdexcept>

#if defined(_WIN32)
#define EXCEPTION_TEST_EXPORT __declspec(dllexport)
#else
#define EXCEPTION_TEST_EXPORT __attribute__((visibility("default")))
#endif

extern "C" EXCEPTION_TEST_EXPORT void SkityTestThrowBadAlloc() {
  throw std::bad_alloc();
}
extern "C" EXCEPTION_TEST_EXPORT void SkityTestThrowLengthError() {
  throw std::length_error("refused length");
}
extern "C" EXCEPTION_TEST_EXPORT bool SkityTestCatchWithinImage() {
  bool allocation = false, length = false;
  try {
    SkityTestThrowBadAlloc();
  } catch (const std::bad_alloc&) {
    allocation = true;
  }
  try {
    SkityTestThrowLengthError();
  } catch (const std::length_error&) {
    length = true;
  }
  return allocation && length;
}
