// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/rect.hpp>
#include <skity/graphic/path.hpp>
#include <vector>

#include "src/render/sw/sw_raster.hpp"

namespace {

// A small closed outline with one curve, shaped like a glyph contour.
skity::Path GlyphLikePath() {
  skity::Path path;
  path.MoveTo(1.f, 1.f);
  path.LineTo(9.f, 1.f);
  path.QuadTo(11.f, 5.f, 9.f, 9.f);
  path.LineTo(1.f, 9.f);
  path.Close();
  return path;
}

// Coordinates a 16.16 fixed-point scanline raster can represent after the
// edge builder's extra two accuracy bits.
constexpr int32_t kFixedSafeLimit = 8192;

bool SpansWithin(const std::vector<skity::Span>& spans, int32_t left,
                 int32_t top, int32_t right, int32_t bottom) {
  for (const skity::Span& span : spans) {
    if (span.y < top || span.y >= bottom || span.x < left ||
        span.x + span.len > right || span.len <= 0) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST(SWRaster, FarOffscreenRotatedPathProducesNoSpansInsideClip) {
  skity::SWRaster raster;
  const skity::Matrix transform =
      skity::Matrix::Translate(8.f, -120000.f) * skity::Matrix::RotateDeg(45.f);
  raster.RastePath(GlyphLikePath(), transform,
                   skity::Rect::MakeWH(1024.f, 768.f));
  EXPECT_TRUE(raster.CurrentSpans().empty());
}

TEST(SWRaster, FarOffscreenRotatedPathWithDefaultCullStaysInFixedRange) {
  skity::SWRaster raster;
  const skity::Matrix transform =
      skity::Matrix::Translate(8.f, -120000.f) * skity::Matrix::RotateDeg(45.f);
  raster.RastePath(GlyphLikePath(), transform);
  EXPECT_TRUE(SpansWithin(raster.CurrentSpans(), -kFixedSafeLimit,
                          -kFixedSafeLimit, kFixedSafeLimit,
                          kFixedSafeLimit));
}

TEST(SWRaster, HugePartiallyVisibleTriangleIsClippedToScanBounds) {
  skity::Path path;
  path.MoveTo(10.f, 10.f);
  path.LineTo(200000.f, 150000.f);
  path.LineTo(-100000.f, 120000.f);
  path.Close();
  skity::SWRaster raster;
  raster.RastePath(path, skity::Matrix{}, skity::Rect::MakeWH(64.f, 64.f));
  EXPECT_FALSE(raster.CurrentSpans().empty());
  EXPECT_TRUE(SpansWithin(raster.CurrentSpans(), 0, 0, 64, 64));
}

TEST(SWRaster, HugeCurvedPathIsClippedToScanBounds) {
  skity::Path path;
  path.MoveTo(-50000.f, 20.f);
  path.QuadTo(30.f, -90000.f, 70000.f, 40.f);
  path.LineTo(20.f, 150000.f);
  path.Close();
  skity::SWRaster raster;
  raster.RastePath(path, skity::Matrix{}, skity::Rect::MakeWH(64.f, 64.f));
  EXPECT_TRUE(SpansWithin(raster.CurrentSpans(), 0, 0, 64, 64));
}

TEST(SWRaster, ClippedVisiblePortionMatchesUnclippedRaster) {
  // A rectangle that extends beyond the clip on every side must cover the
  // whole clip exactly as an equivalent in-range rectangle does.
  skity::Path huge;
  huge.AddRect(skity::Rect::MakeLTRB(-100000.f, -100000.f, 100000.f,
                                     100000.f));
  skity::Path fitted;
  fitted.AddRect(skity::Rect::MakeLTRB(-4.f, -4.f, 20.f, 20.f));
  skity::SWRaster huge_raster;
  huge_raster.RastePath(huge, skity::Matrix{}, skity::Rect::MakeWH(16.f, 16.f));
  skity::SWRaster fitted_raster;
  fitted_raster.RastePath(fitted, skity::Matrix{},
                          skity::Rect::MakeWH(16.f, 16.f));
  std::array<int32_t, 16 * 16> huge_cover{};
  std::array<int32_t, 16 * 16> fitted_cover{};
  const auto accumulate = [](const std::vector<skity::Span>& spans,
                             std::array<int32_t, 16 * 16>& cover) {
    for (const skity::Span& span : spans) {
      for (int32_t x = span.x; x < span.x + span.len; ++x) {
        if (span.y >= 0 && span.y < 16 && x >= 0 && x < 16) {
          cover[static_cast<size_t>(span.y * 16 + x)] += span.cover;
        }
      }
    }
  };
  accumulate(huge_raster.CurrentSpans(), huge_cover);
  accumulate(fitted_raster.CurrentSpans(), fitted_cover);
  EXPECT_EQ(huge_cover, fitted_cover);
  EXPECT_EQ(huge_cover[0], 255);
  EXPECT_EQ(huge_cover[16 * 16 - 1], 255);
}

TEST(SpanBuilder, HorizontalWritesAreClampedToTheCoverageRow) {
  skity::SpanBuilder builder(0, 8, skity::Rect::MakeWH(8.f, 1.f), nullptr);
  std::array<uint8_t, 16> antialias{};
  antialias.fill(255);
  builder.BuildSpans(-4, 0, antialias.data(), 16);
  builder.BuildSpan(-2, 0, 255);
  builder.BuildSpan(6, 0, 8, 255);
  builder.Flush();
  const std::vector<skity::Span> spans = builder.TakeSpans();
  EXPECT_FALSE(spans.empty());
  EXPECT_TRUE(SpansWithin(spans, 0, 0, 8, 1));
}

TEST(SWRaster, IntegralRectanglesPreserveExactSpansAndDelegateOrder) {
  struct Case {
    skity::Rect local;
    skity::Matrix transform;
    skity::Rect clip;
    int x, y, width, height;
  };
  const auto clip = skity::Rect::MakeWH(16, 16);
  const Case cases[] = {
      {skity::Rect::MakeXYWH(1, 2, 6, 7), {}, clip, 1, 2, 6, 7},
      {skity::Rect::MakeXYWH(1, 2, 6, 7), skity::Matrix::Translate(3, 4), clip,
       4, 6, 6, 7},
      {skity::Rect::MakeXYWH(2, 4, 8, 6), skity::Matrix::Scale(.5f, .5f), clip,
       1, 2, 4, 3},
      {skity::Rect::MakeXYWH(1, 2, 6, 7),
       skity::Matrix::Translate(12, 0) * skity::Matrix::Scale(-1, 1), clip, 5,
       2, 6, 7},
      {skity::Rect::MakeXYWH(-3, -4, 10, 12),
       {},
       skity::Rect::MakeWH(5, 6),
       0,
       0,
       5,
       6},
      {skity::Rect::MakeLTRB(-100000, -100000, 100000, 100000),
       {},
       clip,
       0,
       0,
       16,
       16},
      {skity::Rect::MakeXYWH(30, 30, 2, 2), {}, clip, 0, 0, 0, 0},
  };
  struct Delegate : skity::SpanBuilderDelegate {
    std::vector<skity::Span> spans;
    void OnBuildSpan(int x, int y, int width, uint8_t alpha) override {
      spans.push_back({x, y, width, alpha});
    }
  };
  for (const auto& c : cases) {
    for (auto direction :
         {skity::Path::Direction::kCW, skity::Path::Direction::kCCW}) {
      for (auto fill : {skity::Path::PathFillType::kWinding,
                        skity::Path::PathFillType::kEvenOdd}) {
        skity::Path path;
        path.AddRect(c.local, direction);
        path.SetFillType(fill);
        skity::SWRaster raster;
        Delegate delegate;
        raster.RastePath(path, c.transform, c.clip, &delegate);
        ASSERT_EQ(raster.CurrentSpans().size(), static_cast<size_t>(c.height));
        ASSERT_EQ(delegate.spans.size(), raster.CurrentSpans().size());
        for (int i = 0; i < c.height; ++i) {
          const auto& span = raster.CurrentSpans()[i];
          const auto& observed = delegate.spans[i];
          EXPECT_EQ(span.x, c.x);
          EXPECT_EQ(span.y, c.y + i);
          EXPECT_EQ(span.len, c.width);
          EXPECT_EQ(span.cover, 255);
          EXPECT_EQ(observed.x, span.x);
          EXPECT_EQ(observed.y, span.y);
          EXPECT_EQ(observed.len, span.len);
          EXPECT_EQ(observed.cover, span.cover);
        }
      }
    }
  }
}
