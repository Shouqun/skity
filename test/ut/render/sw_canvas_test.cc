// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <skity/effect/mask_filter.hpp>
#include <skity/effect/shader.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/render/canvas.hpp>
#include <skity/text/font.hpp>
#include <skity/text/typeface.hpp>

namespace {

void DrawRotatedSaveLayerScene(skity::Bitmap* bitmap,
                               const skity::Rect& layer_bounds) {
  auto canvas = skity::Canvas::MakeSoftwareCanvas(bitmap);
  ASSERT_TRUE(canvas);

  skity::Paint paint;
  paint.SetColor(skity::Color_RED);

  canvas->Translate(16.f, 16.f);
  canvas->Rotate(30.f);
  canvas->Translate(-16.f, -16.f);

  canvas->SaveLayer(layer_bounds, skity::Paint{});
  canvas->DrawRect(skity::Rect::MakeXYWH(8.f, 8.f, 16.f, 16.f), paint);
  canvas->Restore();
}

}  // namespace

TEST(SWCanvas, SaveLayerClipsHugeBoundsToDevice) {
  skity::Bitmap bitmap(16, 16, skity::AlphaType::kPremul_AlphaType,
                       skity::ColorType::kRGBA);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  ASSERT_TRUE(canvas);

  skity::Paint paint;
  paint.SetColor(skity::Color_RED);

  canvas->SaveLayer(skity::kMaxCullRect, skity::Paint{});
  canvas->DrawRect(skity::Rect::MakeXYWH(4.f, 4.f, 4.f, 4.f), paint);
  canvas->Restore();

  EXPECT_EQ(bitmap.GetPixel(5, 5), skity::Color_RED);
  EXPECT_EQ(bitmap.GetPixel(0, 0), skity::Color_TRANSPARENT);
}

TEST(SWCanvas, SaveLayerHugeBoundsPreservesRotatedDraw) {
  skity::Bitmap bounded(32, 32, skity::AlphaType::kPremul_AlphaType,
                        skity::ColorType::kRGBA);
  skity::Bitmap huge(32, 32, skity::AlphaType::kPremul_AlphaType,
                     skity::ColorType::kRGBA);

  DrawRotatedSaveLayerScene(&bounded, skity::Rect::MakeWH(32.f, 32.f));
  DrawRotatedSaveLayerScene(&huge, skity::kMaxCullRect);

  for (uint32_t y = 0; y < bounded.Height(); ++y) {
    for (uint32_t x = 0; x < bounded.Width(); ++x) {
      EXPECT_EQ(huge.GetPixel(x, y), bounded.GetPixel(x, y))
          << "x=" << x << " y=" << y;
    }
  }
}

TEST(SWCanvas, SetAndResetMatrixInsideSaveLayerUseGlobalMatrix) {
  skity::Bitmap bitmap(64, 64, skity::AlphaType::kPremul_AlphaType,
                       skity::ColorType::kRGBA);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  ASSERT_TRUE(canvas);

  canvas->Translate(20.f, 0.f);
  canvas->SaveLayer(skity::kMaxCullRect, skity::Paint{});

  skity::Paint red;
  red.SetColor(skity::Color_RED);
  canvas->SetMatrix(skity::Matrix::Translate(5.f, 0.f));
  canvas->DrawRect(skity::Rect::MakeXYWH(0.f, 5.f, 4.f, 4.f), red);

  skity::Paint blue;
  blue.SetColor(skity::Color_BLUE);
  canvas->ResetMatrix();
  canvas->DrawRect(skity::Rect::MakeXYWH(0.f, 20.f, 4.f, 4.f), blue);
  canvas->Restore();

  EXPECT_EQ(bitmap.GetPixel(6, 6), skity::Color_RED);
  EXPECT_EQ(bitmap.GetPixel(26, 6), skity::Color_TRANSPARENT);
  EXPECT_EQ(bitmap.GetPixel(1, 21), skity::Color_BLUE);
  EXPECT_EQ(bitmap.GetPixel(21, 21), skity::Color_TRANSPARENT);
}

TEST(SWCanvas, ResetMatrixInsideSmallScaleLayerUsesGlobalMatrix) {
  skity::Bitmap bitmap(128, 128, skity::AlphaType::kPremul_AlphaType,
                       skity::ColorType::kRGBA);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  ASSERT_TRUE(canvas);

  canvas->Scale(0.01f, 0.01f);
  canvas->Skew(0.25f, 0.f);
  canvas->SaveLayer(skity::Rect::MakeWH(10000.f, 10000.f), skity::Paint{});
  canvas->ResetMatrix();

  skity::Paint red;
  red.SetColor(skity::Color_RED);
  canvas->DrawRect(skity::Rect::MakeXYWH(20.f, 20.f, 20.f, 20.f), red);
  canvas->Restore();

  EXPECT_EQ(bitmap.GetPixel(30, 30), skity::Color_RED);
}

TEST(SWCanvas, StrokeThenFillDrawsFillAfterStroke) {
  skity::Bitmap bitmap(48, 48, skity::AlphaType::kPremul_AlphaType,
                       skity::ColorType::kRGBA);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  ASSERT_TRUE(canvas);

  skity::Paint paint;
  paint.SetStyle(skity::Paint::kStrokeThenFill_Style);
  paint.SetStrokeWidth(10.f);
  paint.SetStrokeColor(skity::Color_RED);
  paint.SetFillColor(skity::Color_WHITE);
  paint.SetAntiAlias(false);

  skity::Path path;
  path.AddRect(skity::Rect::MakeXYWH(10.f, 10.f, 24.f, 24.f));
  canvas->DrawPath(path, paint);

  EXPECT_EQ(bitmap.GetPixel(12, 20), skity::Color_WHITE);
  EXPECT_EQ(bitmap.GetPixel(7, 20), skity::Color_RED);
}

namespace {

// Evaluate the mathematical two-dimensional kernel directly in double
// precision. This intentionally does not use the renderer's separable kernel,
// intermediate bitmap or filter helpers. The two-byte bound allows the two
// eight-bit intermediate/composite roundings of software rendering.
void ExpectGaussianConvolution(skity::Bitmap& source, skity::Bitmap& actual,
                               double sigma) {
  ASSERT_EQ(source.Width(), actual.Width());
  ASSERT_EQ(source.Height(), actual.Height());
  const int support = static_cast<int>(std::ceil(3.0 * sigma));
  const auto weight = [sigma](int dx, int dy) {
    return std::exp(-(static_cast<double>(dx) * dx +
                      static_cast<double>(dy) * dy) /
                    (2.0 * sigma * sigma));
  };
  double normalizer = 0.0;
  for (int dy = -support; dy <= support; ++dy) {
    for (int dx = -support; dx <= support; ++dx) {
      normalizer += weight(dx, dy);
    }
  }

  for (uint32_t y = 0; y < source.Height(); ++y) {
    for (uint32_t x = 0; x < source.Width(); ++x) {
      std::array<double, 4> expected{};
      for (int dy = -support; dy <= support; ++dy) {
        for (int dx = -support; dx <= support; ++dx) {
          const int sx = static_cast<int>(x) + dx;
          const int sy = static_cast<int>(y) + dy;
          if (sx < 0 || sy < 0 || sx >= static_cast<int>(source.Width()) ||
              sy >= static_cast<int>(source.Height())) {
            continue;
          }
          const auto color = source.GetPixel(sx, sy);
          const double factor = weight(dx, dy) / normalizer;
          expected[0] += ColorGetR(color) * factor;
          expected[1] += ColorGetG(color) * factor;
          expected[2] += ColorGetB(color) * factor;
          expected[3] += ColorGetA(color) * factor;
        }
      }
      const auto color = actual.GetPixel(x, y);
      const std::array<uint32_t, 4> channels{
          ColorGetR(color), ColorGetG(color),
          ColorGetB(color), ColorGetA(color)};
      for (size_t c = 0; c < channels.size(); ++c) {
        EXPECT_NEAR(channels[c], expected[c], 2.0)
            << "x=" << x << " y=" << y << " channel=" << c
            << " sigma=" << sigma;
      }
      EXPECT_LE(channels[0], channels[3]);
      EXPECT_LE(channels[1], channels[3]);
      EXPECT_LE(channels[2], channels[3]);
    }
  }
}

template <typename Draw>
void ExpectGaussianDraw(Draw draw, float sigma) {
  skity::Bitmap source(40, 40, skity::AlphaType::kPremul_AlphaType);
  skity::Bitmap actual(40, 40, skity::AlphaType::kPremul_AlphaType);
  auto plain_canvas = skity::Canvas::MakeSoftwareCanvas(&source);
  auto blur_canvas = skity::Canvas::MakeSoftwareCanvas(&actual);
  ASSERT_NE(plain_canvas, nullptr);
  ASSERT_NE(blur_canvas, nullptr);
  draw(plain_canvas.get(), nullptr);
  const auto filter = skity::MaskFilter::MakeGaussianBlur(sigma);
  ASSERT_NE(filter, nullptr);
  draw(blur_canvas.get(), filter);
  uint64_t source_alpha = 0;
  for (uint32_t y = 0; y < source.Height(); ++y) {
    for (uint32_t x = 0; x < source.Width(); ++x) {
      source_alpha += ColorGetA(source.GetPixel(x, y));
    }
  }
  ASSERT_GT(source_alpha, 0u);
  ExpectGaussianConvolution(source, actual, sigma);
}

}  // namespace

TEST(SWCanvasGaussianMask, FractionalSigmaImpulseMatchesNormalizedKernel) {
  for (float sigma : {0.4f, 0.75f, 1.f, 1.25f, 2.f}) {
    ExpectGaussianDraw(
        [](skity::Canvas* canvas, std::shared_ptr<skity::MaskFilter> filter) {
          skity::Paint paint;
          paint.SetColor(skity::Color_WHITE);
          paint.SetAntiAlias(false);
          paint.SetMaskFilter(std::move(filter));
          skity::Path path;
          path.AddRect(skity::Rect::MakeXYWH(20.f, 20.f, 1.f, 1.f));
          canvas->DrawPath(path, paint);
        },
        sigma);
  }
}

TEST(SWCanvasGaussianMask, PositiveSubnormalSigmaHasFiniteIdentityLimit) {
  ExpectGaussianDraw(
      [](skity::Canvas* canvas, std::shared_ptr<skity::MaskFilter> filter) {
        skity::Paint paint;
        paint.SetColor(skity::Color_WHITE);
        paint.SetAntiAlias(false);
        paint.SetMaskFilter(std::move(filter));
        canvas->DrawRect(skity::Rect::MakeXYWH(20.f, 20.f, 1.f, 1.f), paint);
      },
      std::numeric_limits<float>::denorm_min());
}

TEST(SWCanvasGaussianMask, PremultipliedRgbaImagePreservesColorAndAlpha) {
  skity::Bitmap pixels(5, 5, skity::AlphaType::kPremul_AlphaType);
  for (uint32_t y = 0; y < pixels.Height(); ++y) {
    for (uint32_t x = 0; x < pixels.Width(); ++x) {
      const uint32_t alpha = 48 + 8 * x + 24 * y;
      pixels.SetPixel(x, y, skity::ColorSetARGB(alpha, alpha / 2,
                                               alpha / 4, alpha));
    }
  }
  const auto image = skity::Image::MakeImage(pixels.GetPixmap());
  ASSERT_NE(image, nullptr);
  ExpectGaussianDraw(
      [&image](skity::Canvas* canvas,
               std::shared_ptr<skity::MaskFilter> filter) {
        skity::Paint paint;
        paint.SetAlpha(192);
        paint.SetMaskFilter(std::move(filter));
        canvas->DrawImage(image, skity::Rect::MakeXYWH(17.f, 17.f, 5.f, 5.f),
                           &paint);
      },
      1.f);
}

TEST(SWCanvasGaussianMask, GradientPathFiltersColoredOffscreenContent) {
  const skity::Point points[] = {{12.f, 12.f, 0.f, 1.f},
                                  {28.f, 12.f, 0.f, 1.f}};
  const skity::Vec4 colors[] = {{1.f, 0.2f, 0.f, 0.4f},
                                {0.f, 0.4f, 1.f, 0.8f}};
  const auto shader = skity::Shader::MakeLinear(points, colors, nullptr, 2);
  ASSERT_NE(shader, nullptr);
  ExpectGaussianDraw(
      [&shader](skity::Canvas* canvas,
                std::shared_ptr<skity::MaskFilter> filter) {
        skity::Paint paint;
        paint.SetShader(shader);
        paint.SetAlpha(160);
        paint.SetMaskFilter(std::move(filter));
        skity::Path path;
        path.MoveTo(12.f, 12.f);
        path.LineTo(28.f, 14.f);
        path.LineTo(23.f, 27.f);
        path.Close();
        canvas->DrawPath(path, paint);
      },
      1.25f);
}

TEST(SWCanvasGaussianMask, DirectTextPathUsesGaussianSigma) {
  const auto typeface = skity::Typeface::MakeFromFile(
      SKITY_FONT_DIR "fonts/resources/Roboto-Regular.ttf");
  ASSERT_NE(typeface, nullptr);
  const auto glyph = typeface->UnicharToGlyph('H');
  ASSERT_NE(glyph, 0u);
  skity::Font font(typeface, 15.f);
  const float x = 13.f;
  const float y = 25.f;
  ExpectGaussianDraw(
      [&](skity::Canvas* canvas,
          std::shared_ptr<skity::MaskFilter> filter) {
        skity::Paint paint;
        paint.SetColor(skity::ColorSetARGB(180, 255, 64, 128));
        paint.SetMaskFilter(std::move(filter));
        canvas->DrawGlyphs(1, &glyph, &x, &y, font, paint);
      },
      0.75f);
}
