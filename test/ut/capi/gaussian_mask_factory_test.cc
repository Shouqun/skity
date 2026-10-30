// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <skity/c/skity_mask_filter.h>

#include <limits>

namespace skity {
namespace {
TEST(GaussianMaskSerialization, CFactoryRejectsInvalidAndOwnsValidFilter) {
  EXPECT_EQ(skity_mask_filter_create_gaussian_blur(0.f), nullptr);
  EXPECT_EQ(skity_mask_filter_create_gaussian_blur(-1.f), nullptr);
  EXPECT_EQ(skity_mask_filter_create_gaussian_blur(
                std::numeric_limits<float>::quiet_NaN()),
            nullptr);
  const auto filter = skity_mask_filter_create_gaussian_blur(0.75f);
  ASSERT_NE(filter, nullptr);
  skity_mask_filter_destroy(filter);
}

}  // namespace
}  // namespace skity
