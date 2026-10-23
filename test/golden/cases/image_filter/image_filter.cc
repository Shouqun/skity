// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <skity/effect/image_filter.hpp>
#include <skity/effect/mask_filter.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/recorder/picture_recorder.hpp>
#include <vector>

#include "common/golden_test_check.hpp"
#include "common/golden_test_env.hpp"

constexpr const char* kGoldenTestDir = CASE_DIR;
constexpr const char* kGoldenTestCoverageAADir = CASE_DIR "coverage_aa_images/";

namespace {

struct PathListContext {
  explicit PathListContext(const char* name)
      : expected_path(kGoldenTestDir),
        coverage_aa_path(kGoldenTestCoverageAADir) {
    expected_path.append(name);
    coverage_aa_path.append(name);
  }

  skity::testing::PathList ToPathList() const {
    return {
        .cpu_tess_path = expected_path.c_str(),
        .gpu_tess_path = expected_path.c_str(),
        .coverage_aa_path = coverage_aa_path.c_str(),
    };
  }

  std::filesystem::path expected_path;
  std::filesystem::path coverage_aa_path;
};

}  // namespace

TEST(ImageFilterGolden, BlurFilter_10_5) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::Blur(10, 5));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  PathListContext context("blur_filter_10_5.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ImageFilterGolden, BlurFilter_10_5_Perspective) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::Blur(10, 5));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->Concat(skity::Matrix{
      2000, 0, 0,
      0,  //
      0, 2000, 0,
      0,  //
      800, 1200, 1,
      1,  //
      0, 0, 2000,
      2000,  //
  });
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  PathListContext context("blur_filter_10_5_perspective.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ImageFilterGolden, BlurFilter_10_10) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::Blur(10, 10));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->Concat(skity::Matrix::RotateDeg(45.f, skity::Vec2{50.f, 50.f}));

  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  PathListContext context("blur_filter_10_10.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ImageFilterGolden, BlurFilter_10_0) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::Blur(10, 0));

  auto canvas = recorder.GetRecordingCanvas();
  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  PathListContext context("blur_filter_10_0.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

// sigma = 0.5 must still produce a minimal-neighborhood blur instead of
// collapsing to an unfiltered draw.
TEST(ImageFilterGolden, BlurFilter_0_5) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::Blur(0.5f, 0.5f));

  auto canvas = recorder.GetRecordingCanvas();
  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  PathListContext context("blur_filter_0_5.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ImageFilterGolden, DropShadow_0_0_10_10) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::DropShadow(
      0, 0, 10, 10, skity::Color_GREEN, nullptr));

  auto canvas = recorder.GetRecordingCanvas();
  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();
  PathListContext context("drop_shadow_0_0_10_10.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ImageFilterGolden, DropShadow_10_n10_5_5) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::DropShadow(
      10, -10, 5, 5, skity::Color_GREEN, nullptr));

  auto canvas = recorder.GetRecordingCanvas();
  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  PathListContext context("drop_shadow_10_n10_5_5.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

// sigma = 0 only skips the blur pass; the tinted, offset hard-edged shadow
// and the source-over composite must still render.
TEST(ImageFilterGolden, DropShadow_10_n10_0_0) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::DropShadow(
      10, -10, 0, 0, skity::Color_GREEN, nullptr));

  auto canvas = recorder.GetRecordingCanvas();
  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  PathListContext context("drop_shadow_10_n10_0_0.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ImageFilterGolden, Matrix_translate_50_50) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(
      skity::ImageFilters::MatrixTransform(skity::Matrix::Translate(50, 50)));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);

  {
    skity::Paint bound_paint;
    bound_paint.SetAntiAlias(true);
    bound_paint.SetStyle(skity::Paint::kStroke_Style);
    bound_paint.SetColor(skity::Color_CYAN);
    bound_paint.SetStrokeWidth(1);

    canvas->DrawRect(skity::Rect::MakeWH(100, 100), bound_paint);
  }

  PathListContext context("matrix_translate_50_50.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ImageFilterGolden, RotateMatrix_10_deg_50_50) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::MatrixTransform(
      skity::Matrix::RotateRad(skity::FloatDegreesToRadians(10), {100, 100})));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->DrawRect(skity::Rect::MakeXYWH(50, 50, 100, 100), paint);

  {
    skity::Paint bound_paint;
    bound_paint.SetAntiAlias(true);
    bound_paint.SetStyle(skity::Paint::kStroke_Style);
    bound_paint.SetColor(skity::Color_CYAN);
    bound_paint.SetStrokeWidth(1);

    canvas->DrawRect(skity::Rect::MakeXYWH(50, 50, 100, 100), bound_paint);
  }

  PathListContext context("rotate_matrix_10_deg_50_50.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ImageFilterGolden, ComposeBlurMatrix) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);

  auto blur_filter = skity::ImageFilters::Blur(10, 10);
  auto matrix_filter =
      skity::ImageFilters::MatrixTransform(skity::Matrix::Translate(50, 50));

  auto filter = skity::ImageFilters::Compose(matrix_filter, blur_filter);
  paint.SetImageFilter(filter);

  auto canvas = recorder.GetRecordingCanvas();

  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);

  {
    skity::Paint bound_paint;
    bound_paint.SetAntiAlias(true);
    bound_paint.SetStyle(skity::Paint::kStroke_Style);
    bound_paint.SetColor(skity::Color_CYAN);
    bound_paint.SetStrokeWidth(1);

    canvas->DrawRect(skity::Rect::MakeWH(100, 100), bound_paint);
  }

  PathListContext context("compose_blur_matrix.png");

  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

namespace {

constexpr uint32_t kGaussianTestSize = 48;
constexpr int kGaussianSourceLeft = 20;
constexpr int kGaussianSourceTop = 20;
using GaussianPixel = std::array<uint8_t, 4>;
using GaussianPixels = std::vector<GaussianPixel>;

// Keep this environment-wide setting restored even on a fatal assertion.
class GaussianSingleSample {
 public:
  explicit GaussianSingleSample(skity::testing::GoldenTestEnv* env)
      : env_(env), previous_(env->GetSampleCount()) {
    env_->SetSampleCount(1);
  }
  ~GaussianSingleSample() { env_->SetSampleCount(previous_); }

 private:
  skity::testing::GoldenTestEnv* env_;
  uint32_t previous_;
};

enum class GaussianSource { kRect, kPath, kImage };

GaussianPixels RenderGaussianSource(
    skity::testing::GoldenTestEnv* env, GaussianSource source, int width,
    int height, float scale, const GaussianPixel& rgba,
    const std::shared_ptr<skity::MaskFilter>& filter) {
  auto texture = env->RenderToTexture(
      kGaussianTestSize, kGaussianTestSize, [&](skity::Canvas* canvas) {
        canvas->Scale(scale, scale);
        const auto bounds = skity::Rect::MakeXYWH(
            kGaussianSourceLeft / scale, kGaussianSourceTop / scale,
            width / scale, height / scale);
        skity::Paint paint;
        paint.SetAntiAlias(false);
        paint.SetMaskFilter(filter);
        if (source == GaussianSource::kImage) {
          // Public premultiplied RGBA input; no shader/test-internal texture.
          skity::Bitmap bitmap(1, 1, skity::AlphaType::kPremul_AlphaType);
          bitmap.SetPixel(0, 0,
                          skity::ColorSetARGB(rgba[3], rgba[0], rgba[1],
                                              rgba[2]));
          canvas->DrawImage(skity::Image::MakeImage(bitmap.GetPixmap()), bounds,
                            &paint);
        } else {
          // These geometry cases use opaque colors, so there is no ambiguity
          // between the public unpremultiplied paint and readback RGBA.
          paint.SetColor(skity::ColorSetARGB(rgba[3], rgba[0], rgba[1], rgba[2]));
          if (source == GaussianSource::kPath) {
            skity::Path path;
            path.AddRect(bounds);
            canvas->DrawPath(path, paint);
          } else {
            canvas->DrawRect(bounds, paint);
          }
        }
      });
  EXPECT_NE(texture, nullptr);
  if (!texture) {
    return {};
  }
  auto pixels = texture->ReadPixels();
  EXPECT_NE(pixels, nullptr);
  if (!pixels) {
    return {};
  }
  EXPECT_EQ(pixels->Width(), kGaussianTestSize);
  EXPECT_EQ(pixels->Height(), kGaussianTestSize);
  if (pixels->Width() != kGaussianTestSize ||
      pixels->Height() != kGaussianTestSize) {
    return {};
  }
  skity::Bitmap bitmap(pixels);
  GaussianPixels result;
  result.reserve(kGaussianTestSize * kGaussianTestSize);
  for (uint32_t y = 0; y < kGaussianTestSize; ++y) {
    for (uint32_t x = 0; x < kGaussianTestSize; ++x) {
      const auto color = bitmap.GetPixel(x, y);
      result.push_back({static_cast<uint8_t>(ColorGetR(color)),
                        static_cast<uint8_t>(ColorGetG(color)),
                        static_cast<uint8_t>(ColorGetB(color)),
                        static_cast<uint8_t>(ColorGetA(color))});
    }
  }
  return result;
}

// This is a mathematical oracle, independent of the production filter,
// radius conversion, shader source, and golden images. The source is an
// integer-aligned constant rectangle (one pixel for the impulse cases).
std::vector<double> GaussianWeights(double sigma) {
  const int support = static_cast<int>(std::ceil(3.0 * sigma));
  std::vector<double> weights(2 * support + 1);
  double total = 0.0;
  for (int offset = -support; offset <= support; ++offset) {
    const double distance = offset / sigma;
    const double weight = std::exp(-0.5 * distance * distance);
    weights[offset + support] = weight;
    total += weight;
  }
  for (auto& weight : weights) {
    weight /= total;
  }
  return weights;
}

void ExpectGaussianPixels(const GaussianPixels& pixels, int width, int height,
                          const GaussianPixel& rgba, double device_sigma) {
  ASSERT_EQ(pixels.size(), kGaussianTestSize * kGaussianTestSize);
  const auto weights = GaussianWeights(device_sigma);
  const int support = static_cast<int>(weights.size() / 2);
  for (int y = 0; y < static_cast<int>(kGaussianTestSize); ++y) {
    double vertical = 0.0;
    for (int source_y = kGaussianSourceTop;
         source_y < kGaussianSourceTop + height; ++source_y) {
      const int offset = y - source_y;
      if (std::abs(offset) <= support) {
        vertical += weights[offset + support];
      }
    }
    for (int x = 0; x < static_cast<int>(kGaussianTestSize); ++x) {
      double horizontal = 0.0;
      for (int source_x = kGaussianSourceLeft;
           source_x < kGaussianSourceLeft + width; ++source_x) {
        const int offset = x - source_x;
        if (std::abs(offset) <= support) {
          horizontal += weights[offset + support];
        }
      }
      for (size_t channel = 0; channel < rgba.size(); ++channel) {
        // Two RGBA8 convolution targets quantize between separable passes.
        // Two code values cover their rounding and final RGBA8 composition;
        // this does not use or change the golden comparator's thresholds.
        const double expected =
            std::round(std::round(rgba[channel] * horizontal) * vertical);
        ASSERT_NEAR(pixels[y * kGaussianTestSize + x][channel], expected, 2.0)
            << "pixel (" << x << ", " << y << "), channel " << channel
            << ", device sigma " << device_sigma;
      }
    }
  }
}

void ExpectUnfilteredSource(const GaussianPixels& pixels, int width, int height,
                            const GaussianPixel& rgba) {
  ASSERT_EQ(pixels.size(), kGaussianTestSize * kGaussianTestSize);
  for (int y = 0; y < static_cast<int>(kGaussianTestSize); ++y) {
    for (int x = 0; x < static_cast<int>(kGaussianTestSize); ++x) {
      const bool inside = x >= kGaussianSourceLeft &&
                          x < kGaussianSourceLeft + width &&
                          y >= kGaussianSourceTop &&
                          y < kGaussianSourceTop + height;
      const GaussianPixel expected = inside ? rgba : GaussianPixel{0, 0, 0, 0};
      ASSERT_EQ(pixels[y * kGaussianTestSize + x], expected)
          << "unfiltered pixel (" << x << ", " << y << ")";
    }
  }
}

}  // namespace

TEST(GaussianMaskHardware, AnalyticImpulseAtFractionalSigmaAndScale) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  GaussianSingleSample single_sample(env);
  const GaussianPixel white{255, 255, 255, 255};
  for (float scale : {1.f, 2.f}) {
    ExpectUnfilteredSource(RenderGaussianSource(env, GaussianSource::kRect, 1,
                                               1, scale, white, nullptr),
                           1, 1, white);
    for (float sigma : {0.5f, 1.f, 1.25f}) {
      SCOPED_TRACE(::testing::Message() << "sigma=" << sigma
                                      << ", scale=" << scale);
      auto filter = skity::MaskFilter::MakeGaussianBlur(sigma);
      ASSERT_NE(filter, nullptr);
      ExpectGaussianPixels(RenderGaussianSource(env, GaussianSource::kRect, 1,
                                               1, scale, white, filter),
                           1, 1, white, sigma * scale);
    }
  }
}

TEST(GaussianMaskHardware, AnalyticPathAndPremultipliedImage) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  GaussianSingleSample single_sample(env);
  for (auto source : {GaussianSource::kPath, GaussianSource::kImage}) {
    const GaussianPixel rgba = source == GaussianSource::kImage
                                   ? GaussianPixel{80, 40, 20, 128}
                                   : GaussianPixel{255, 255, 255, 255};
    for (float scale : {1.f, 2.f}) {
      ExpectUnfilteredSource(RenderGaussianSource(env, source, 5, 3, scale,
                                                 rgba, nullptr),
                             5, 3, rgba);
      for (float sigma : {0.5f, 1.f, 1.25f}) {
        SCOPED_TRACE(::testing::Message()
                     << "source=" << static_cast<int>(source)
                     << ", sigma=" << sigma << ", scale=" << scale);
        auto filter = skity::MaskFilter::MakeGaussianBlur(sigma);
        ASSERT_NE(filter, nullptr);
        ExpectGaussianPixels(RenderGaussianSource(env, source, 5, 3, scale,
                                                 rgba, filter),
                             5, 3, rgba, sigma * scale);
      }
    }
  }
}

TEST(GaussianMaskHardware, AlternatingLegacyAndSigmaKeepsShaderContracts) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  GaussianSingleSample single_sample(env);
  const GaussianPixel white{255, 255, 255, 255};
  const auto legacy = skity::MaskFilter::MakeBlur(skity::BlurStyle::kNormal,
                                               3.75f);
  const auto gaussian = skity::MaskFilter::MakeGaussianBlur(1.25f);
  ASSERT_NE(legacy, nullptr);
  ASSERT_NE(gaussian, nullptr);
  const auto legacy_first = RenderGaussianSource(
      env, GaussianSource::kRect, 5, 3, 1.f, white, legacy);
  const auto gaussian_first = RenderGaussianSource(
      env, GaussianSource::kRect, 5, 3, 1.f, white, gaussian);
  const auto legacy_second = RenderGaussianSource(
      env, GaussianSource::kRect, 5, 3, 1.f, white, legacy);
  const auto gaussian_second = RenderGaussianSource(
      env, GaussianSource::kRect, 5, 3, 1.f, white, gaussian);
  ASSERT_EQ(legacy_first.size(), kGaussianTestSize * kGaussianTestSize);
  ASSERT_EQ(gaussian_first.size(), legacy_first.size());
  EXPECT_EQ(legacy_first, legacy_second);
  EXPECT_EQ(gaussian_first, gaussian_second);
  EXPECT_NE(legacy_first, gaussian_first);
  ExpectGaussianPixels(gaussian_first, 5, 3, white, 1.25);
  ExpectGaussianPixels(gaussian_second, 5, 3, white, 1.25);
}
