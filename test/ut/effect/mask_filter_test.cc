// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <wgsl_cross.h>

#include <limits>
#include <utility>
#include <skity/effect/mask_filter.hpp>
#include <skity/geometry/matrix.hpp>
#include <skity/graphic/paint.hpp>

#include "src/render/hw/draw/fragment/wgsl_blur_filter.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/filters/hw_filters.hpp"

namespace skity {
namespace {

TEST(MaskFilterOpacityTest, IsConservativelyTranslucent) {
  Paint paint;
  paint.SetMaskFilter(MaskFilter::MakeBlur(kNormal, 2.f));
  EXPECT_FALSE(IsPaintSourceOpaque(paint));
}

TEST(GaussianMaskFilter, KeepsSigmaAndThreeSigmaSupportExplicit) {
  const auto filter = MaskFilter::MakeGaussianBlur(0.375f);
  ASSERT_NE(filter, nullptr);
  EXPECT_EQ(filter->GetBlurType(), BlurType::kGaussianSigma);
  EXPECT_EQ(filter->GetBlurStyle(), kNormal);
  EXPECT_FLOAT_EQ(filter->GetBlurSigma(), 0.375f);
  EXPECT_FLOAT_EQ(filter->GetBlurRadius(), 1.125f);

  const auto legacy = MaskFilter::MakeBlur(kOuter, 1.125f);
  ASSERT_NE(legacy, nullptr);
  EXPECT_EQ(legacy->GetBlurType(), BlurType::kRadius);
  EXPECT_EQ(legacy->GetBlurStyle(), kOuter);
  EXPECT_FLOAT_EQ(legacy->GetBlurRadius(), 1.125f);
}

TEST(GaussianMaskFilter, RejectsInvalidSigmaAndOverflowingSupport) {
  for (float sigma : {0.f, -0.25f, std::numeric_limits<float>::infinity(),
                      -std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::max()}) {
    EXPECT_EQ(MaskFilter::MakeGaussianBlur(sigma), nullptr);
  }
}

TEST(GaussianMaskFilter, KeepsPositiveSubnormalSigma) {
  const float sigma = std::numeric_limits<float>::denorm_min();
  const auto filter = MaskFilter::MakeGaussianBlur(sigma);
  ASSERT_NE(filter, nullptr);
  EXPECT_EQ(filter->GetBlurType(), BlurType::kGaussianSigma);
  EXPECT_EQ(filter->GetBlurSigma(), sigma);
  EXPECT_EQ(filter->GetBlurRadius(), 3.f * sigma);
}

TEST(GaussianMaskFilter, PaintBoundsExpandByThreeSigmaBeforeScaling) {
  Paint paint;
  paint.SetMaskFilter(MaskFilter::MakeGaussianBlur(1.25f));
  const Rect source = Rect::MakeLTRB(8.f, 10.f, 12.f, 15.f);
  const Rect filtered = paint.ComputeFastBounds(source);
  EXPECT_EQ(filtered, Rect::MakeLTRB(4.25f, 6.25f, 15.75f, 18.75f));

  Rect scaled;
  ASSERT_TRUE(Matrix::Scale(2.f, 3.f).MapRect(&scaled, filtered));
  EXPECT_EQ(scaled, Rect::MakeLTRB(8.5f, 18.75f, 31.5f, 56.25f));
  EXPECT_FALSE(IsPaintSourceOpaque(paint));
}

TEST(GaussianMaskFilter, HardwareMaskUsesSigmaForDownsampling) {
  // ConvertPaintToHWFilter returns the vertical blur pass for a lone mask.
  // Inspect the real owner output, rather than reconstructing that owner here.
  // These values distinguish sigma from the legacy support-to-sigma mapping.
  for (const auto& sample : {std::pair<float, float>{0.375f, 1.f},
                             {16.f, 1.f}, {20.f, 1.f},
                             {24.f, 0.5f}, {30.f, 0.5f},
                             {64.f, 0.25f}, {128.f, 0.125f},
                             {256.f, 0.0625f}}) {
    Paint paint;
    paint.SetMaskFilter(MaskFilter::MakeGaussianBlur(sample.first));
    const auto filter = HWFilters::ConvertPaintToHWFilter(paint, Vec2{1.f});
    ASSERT_NE(filter, nullptr);
    const auto blur = std::static_pointer_cast<HWBlurFilter>(filter);
    EXPECT_EQ(blur->blur_type_, BlurType::kGaussianSigma);
    EXPECT_FLOAT_EQ(blur->sigma_, sample.first);
    EXPECT_FLOAT_EQ(blur->radius_, 3.f * sample.first);
    EXPECT_EQ(blur->direction_, (Vec2{0.f, 1.f}));
    EXPECT_FLOAT_EQ(blur->downsample_scale_, sample.second);
  }

  Paint legacy;
  legacy.SetMaskFilter(MaskFilter::MakeBlur(kNormal, 90.f));
  const auto legacy_filter =
      HWFilters::ConvertPaintToHWFilter(legacy, Vec2{1.f});
  ASSERT_NE(legacy_filter, nullptr);
  const auto legacy_blur =
      std::static_pointer_cast<HWBlurFilter>(legacy_filter);
  EXPECT_EQ(legacy_blur->blur_type_, BlurType::kRadius);
  EXPECT_FLOAT_EQ(legacy_blur->radius_, 90.f);
  EXPECT_FLOAT_EQ(legacy_blur->downsample_scale_, 0.25f);
}

TEST(GaussianMaskFilter, HardwareDownsamplingUsesLargestScaledSigma) {
  // Horizontal and vertical scale must each contribute to the shared
  // downsample decision; local sigma/support remain unscaled in the owner.
  for (const auto scale : {Vec2{3.f, 1.f}, Vec2{1.f, 3.f}}) {
    Paint paint;
    paint.SetMaskFilter(MaskFilter::MakeGaussianBlur(10.f));
    const auto filter = HWFilters::ConvertPaintToHWFilter(paint, scale);
    ASSERT_NE(filter, nullptr);
    const auto blur = std::static_pointer_cast<HWBlurFilter>(filter);
    EXPECT_EQ(blur->blur_type_, BlurType::kGaussianSigma);
    EXPECT_FLOAT_EQ(blur->sigma_, 10.f);
    EXPECT_FLOAT_EQ(blur->radius_, 30.f);
    EXPECT_FLOAT_EQ(blur->downsample_scale_, 0.5f);
  }

  // The private per-axis entry point also retains the vertical sigma/support
  // when the horizontal sigma alone selects the downsample.
  const auto filter = HWFilters::Blur(90.f, 6.f, Vec2{1.f}, nullptr,
                                      BlurType::kGaussianSigma,
                                      Vec2{30.f, 2.f});
  ASSERT_NE(filter, nullptr);
  const auto blur = std::static_pointer_cast<HWBlurFilter>(filter);
  EXPECT_FLOAT_EQ(blur->sigma_, 2.f);
  EXPECT_FLOAT_EQ(blur->radius_, 6.f);
  EXPECT_FLOAT_EQ(blur->downsample_scale_, 0.5f);
}

class BlurSourceTexture final : public GPUTexture {
 public:
  explicit BlurSourceTexture(const GPUTextureDescriptor& descriptor)
      : GPUTexture(descriptor) {}
  ~BlurSourceTexture() override = default;
  size_t GetBytes() const override { return desc_.width * desc_.height * 4; }
  void UploadData(uint32_t, uint32_t, uint32_t, uint32_t, void*) override {}
};

TEST(GaussianMaskFilter, SharedHardwareShaderKeyHasOneStableSource) {
  GPUTextureDescriptor descriptor{};
  descriptor.width = 8;
  descriptor.height = 8;
  descriptor.format = GPUTextureFormat::kRGBA8Unorm;
  const auto texture = std::make_shared<BlurSourceTexture>(descriptor);
  const WGSLBlurFilter legacy(texture, Vec2{1.f, 0.f}, 3.f, Vec2{1.f},
                               Vec2{0.f});
  const WGSLBlurFilter gaussian(texture, Vec2{0.f, 1.f}, 6.f, Vec2{1.f},
                                 Vec2{0.f}, BlurType::kGaussianSigma, 2.f);
  EXPECT_EQ(legacy.GetMainKey(), gaussian.GetMainKey());
  EXPECT_FALSE(legacy.GenSourceWGSL().empty());
  EXPECT_EQ(legacy.GenSourceWGSL(), gaussian.GenSourceWGSL());
  EXPECT_EQ(legacy.NextBindingIndex(), gaussian.NextBindingIndex());
}

TEST(GaussianMaskFilter, SharedShaderCompilesForEveryBackend) {
  const WGSLBlurFilter fragment(nullptr, Vec2{1.f, 0.f}, 3.f, Vec2{1.f},
                                Vec2{0.f});
  const auto program = wgx::Program::Parse(fragment.GenSourceWGSL());
  ASSERT_NE(program, nullptr);
  const auto diagnosis = program->GetDiagnosis();
  ASSERT_FALSE(diagnosis.has_value())
      << diagnosis->message << " at " << diagnosis->line << ":"
      << diagnosis->column;
  EXPECT_TRUE(program->WriteToMsl("fs_main", wgx::MslOptions{}).success);
  EXPECT_TRUE(program->WriteToGlsl("fs_main", wgx::GlslOptions{}).success);
#if defined(WGX_SPIRV)
  EXPECT_TRUE(program->WriteToSpirv("fs_main", wgx::SpirvOptions{}).success);
#endif
}

}  // namespace
}  // namespace skity
