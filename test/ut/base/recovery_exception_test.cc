// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <new>
#include <stdexcept>

extern "C" void SkityTestThrowBadAlloc();
extern "C" void SkityTestThrowLengthError();
extern "C" bool SkityTestCatchWithinImage();

TEST(RecoveryExceptionBoundary, SameImageTypedCatchesWork) {
  EXPECT_TRUE(SkityTestCatchWithinImage());
}
TEST(RecoveryExceptionBoundary, BadAllocRetainsTypeAcrossSharedImage) {
  EXPECT_THROW(SkityTestThrowBadAlloc(), std::bad_alloc);
}
TEST(RecoveryExceptionBoundary, LengthErrorRetainsTypeAcrossSharedImage) {
  EXPECT_THROW(SkityTestThrowLengthError(), std::length_error);
}
