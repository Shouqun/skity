// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <skity/effect/mask_filter.hpp>

#include "src/render/hw/filters/hw_filters.hpp"

namespace skity {
namespace {

TEST(GaussianBounds, IncludesBothPassesAtTheDeviceLimit) {
  const auto bounds = Rect::MakeWH(64.f, 48.f);
  // Sigma one adds three pixels on each side, giving 70 by 54.
  EXPECT_TRUE(HWFilters::GaussianBlurIsRepresentable(1.f, bounds, Vec2{1.f},
                                                     70));
  EXPECT_FALSE(HWFilters::GaussianBlurIsRepresentable(1.f, bounds, Vec2{1.f},
                                                      69));
  EXPECT_FALSE(HWFilters::GaussianBlurIsRepresentable(1.f, bounds, Vec2{1.f},
                                                      0));
  const auto tall_bounds = Rect::MakeWH(48.f, 64.f);
  EXPECT_FALSE(HWFilters::GaussianBlurIsRepresentable(
      1.f, tall_bounds, Vec2{1.f}, 69));
}

TEST(GaussianBounds, UsesActualSigmaDownsamplingBeforeExpandedPasses) {
  const auto bounds = Rect::MakeWH(64.f, 48.f);
  // Sigma30 selects half scale: source32x24, support45, output122x114.
  // Rejecting the un-downsampled 244x228 would discard a valid layer.
  EXPECT_TRUE(HWFilters::GaussianBlurIsRepresentable(30.f, bounds, Vec2{1.f},
                                                     122));
  EXPECT_FALSE(HWFilters::GaussianBlurIsRepresentable(30.f, bounds, Vec2{1.f},
                                                      121));
}

TEST(GaussianBounds, ScalesSupportAndRoundsPassDimensionsOutward) {
  const auto bounds = Rect::MakeWH(32.f, 24.f);
  // At scale2,3 the source is64x72 and each axis gains12,18 respectively.
  EXPECT_TRUE(HWFilters::GaussianBlurIsRepresentable(
      1.f, bounds, Vec2{2.f, 3.f}, 90));
  EXPECT_FALSE(HWFilters::GaussianBlurIsRepresentable(
      1.f, bounds, Vec2{2.f, 3.f}, 89));

  EXPECT_TRUE(HWFilters::GaussianBlurIsRepresentable(
      0.375f, Rect::MakeWH(10.49f, 10.f), Vec2{1.f}, 14));
  EXPECT_FALSE(HWFilters::GaussianBlurIsRepresentable(
      0.375f, Rect::MakeWH(10.49f, 10.f), Vec2{1.f}, 13));
  EXPECT_TRUE(HWFilters::GaussianBlurIsRepresentable(
      0.375f, Rect::MakeWH(10.51f, 10.f), Vec2{1.f}, 15));
  EXPECT_FALSE(HWFilters::GaussianBlurIsRepresentable(
      0.375f, Rect::MakeWH(10.51f, 10.f), Vec2{1.f}, 14));
}

TEST(GaussianBounds, RejectsFiniteHugeSupportWithoutIntegerConversions) {
  const float sigma = std::numeric_limits<float>::max() / 4.f;
  ASSERT_NE(MaskFilter::MakeGaussianBlur(sigma), nullptr);
  EXPECT_FALSE(HWFilters::GaussianBlurIsRepresentable(
      sigma, Rect::MakeWH(8.f, 8.f), Vec2{1.f},
      std::numeric_limits<uint32_t>::max()));
  EXPECT_FALSE(HWFilters::GaussianBlurIsRepresentable(
      sigma, Rect::MakeWH(8.f, 8.f), Vec2{4.f}, 16384));
}

TEST(GaussianBounds, RejectsTransformOverflowAndUnrepresentableSourceSize) {
  const float huge = std::numeric_limits<float>::max();
  const float infinity = std::numeric_limits<float>::infinity();
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const auto bounds = Rect::MakeWH(8.f, 8.f);
  for (Vec2 scale : {Vec2{huge, 1.f}, Vec2{1.f, huge}, Vec2{infinity, 1.f},
                     Vec2{1.f, nan}, Vec2{0.f, 1.f}, Vec2{-1.f, 1.f}}) {
    EXPECT_FALSE(HWFilters::GaussianBlurIsRepresentable(1.f, bounds, scale,
                                                        16384));
  }
  // This finite float is2^32, just outside the texture descriptor range.
  EXPECT_FALSE(HWFilters::GaussianBlurIsRepresentable(
      1.f, Rect::MakeWH(4294967296.f, 8.f), Vec2{1.f},
      std::numeric_limits<uint32_t>::max()));
}

TEST(GaussianBounds, RejectsLocalOutsetOverflowEvenWithSmallDeviceScale) {
  const float huge = std::numeric_limits<float>::max();
  // The scaled dimensions can be small although adding local support to the
  // origin overflows. Keeping only the dimension checks would miss this.
  const Rect bounds{huge * 0.5f, huge * 0.5f, huge * 0.75f, huge * 0.75f};
  EXPECT_FALSE(HWFilters::GaussianBlurIsRepresentable(
      huge / 4.f, bounds, Vec2{1.e-36f}, 16384));
}

TEST(GaussianBounds, RejectsInvalidBoundsButKeepsTinyPositiveSigma) {
  const float infinity = std::numeric_limits<float>::infinity();
  const float nan = std::numeric_limits<float>::quiet_NaN();
  for (Rect bounds : {Rect::MakeWH(0.f, 8.f), Rect::MakeWH(8.f, 0.f),
                      Rect::MakeWH(infinity, 8.f), Rect::MakeWH(8.f, nan)}) {
    EXPECT_FALSE(HWFilters::GaussianBlurIsRepresentable(1.f, bounds, Vec2{1.f},
                                                        16384));
  }
  EXPECT_TRUE(HWFilters::GaussianBlurIsRepresentable(
      std::numeric_limits<float>::denorm_min(), Rect::MakeWH(8.f, 8.f),
      Vec2{1.f}, 10));
  // Every positive fractional support includes the outward neighboring texel.
  EXPECT_FALSE(HWFilters::GaussianBlurIsRepresentable(
      std::numeric_limits<float>::denorm_min(), Rect::MakeWH(8.f, 8.f),
      Vec2{1.f}, 9));
  for (float sigma : {0.f, -1.f, infinity, nan}) {
    EXPECT_FALSE(HWFilters::GaussianBlurIsRepresentable(
        sigma, Rect::MakeWH(8.f, 8.f), Vec2{1.f}, 16384));
  }
}

}  // namespace
}  // namespace skity
