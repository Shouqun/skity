// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <skity/effect/shader.hpp>
#include <skity/text/font.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/typeface.hpp>
#include <thread>
#include <type_traits>
#include <vector>

#include "src/text/scaler_context_cache.hpp"
#include "src/text/scaler_context_desc.hpp"

namespace skity {
namespace {
struct Request {
  uint8_t x = 0, y = 0, native = 0;
};
template <typename T, typename = void>
struct HasRasterOptions : std::false_type {};
template <typename T>
struct HasRasterOptions<T, std::void_t<typename T::GlyphBitmapRasterOptions>>
    : std::true_type {};
template <typename T, typename Callback>
bool Visit(const T& font, GlyphID glyph, Request request, const Paint& paint,
           float density, const Matrix& matrix, size_t bytes,
           const Callback& callback) {
  if constexpr (HasRasterOptions<T>::value) {
    typename T::GlyphBitmapRasterOptions options;
    options.subpixel_x_phase = request.x;
    options.subpixel_y_phase = request.y;
    options.native_raster_phase = request.native;
    return font.VisitGlyphBitmapRaster(
        glyph, options, paint, density, matrix, bytes,
        [](const GlyphData& data, size_t size, const void* context) {
          return (*static_cast<const Callback*>(context))(data, size);
        },
        &callback);
  } else {
    return false;
  }
}
ScalerContextDesc OriginalDescriptor(const Font& font, const Paint& paint,
                                     float density, const Matrix& matrix,
                                     Request request) {
  Matrix22 linear{matrix.GetScaleX(), matrix.GetSkewX(), matrix.GetSkewY(),
                  matrix.GetScaleY()};
  auto desc = ScalerContextDesc::MakeTransformed(font, paint, density, linear);
  desc.native_raster_phase = request.native;
  if (request.native) {
    auto color = Color4fToColor(paint.GetFillColor());
    auto luminance = (77 * ColorGetR(color) + 150 * ColorGetG(color) +
                      29 * ColorGetB(color) + 128) >>
                     8;
    desc.foreground_color = ColorSetARGB(255, luminance, luminance, luminance);
  }
  return desc;
}
OwnedGlyphImage Original(const Font& font, GlyphID glyph, const Paint& paint,
                         float density, const Matrix& matrix, Request request) {
  auto desc = OriginalDescriptor(font, paint, density, matrix, request);
  auto container =
      ScalerContextCache::GlobalScalerContextCache()->FindOrCreateScalerContext(
          desc, font.GetTypeface());
  return container->PrepareOwnedImage(
      PackedGlyphID(glyph, request.x, request.y), paint, 8 * 1024 * 1024);
}
void Same(const GlyphData& actual, size_t bytes,
          const OwnedGlyphImage& expected) {
  ASSERT_TRUE(expected.Complete());
  ASSERT_NE(expected.Get(), nullptr);
  const auto& a = actual.Image();
  const auto& b = expected.Image();
  EXPECT_EQ(actual.Id(), expected.Get()->Id());
  EXPECT_EQ(a.width, b.width);
  EXPECT_EQ(a.height, b.height);
  EXPECT_EQ(a.RowBytes(), b.RowBytes());
  EXPECT_EQ(a.format, b.format);
  EXPECT_EQ(a.origin_x, b.origin_x);
  EXPECT_EQ(a.origin_y, b.origin_y);
  EXPECT_EQ(a.origin_x_for_raster, b.origin_x_for_raster);
  EXPECT_EQ(a.origin_y_for_raster, b.origin_y_for_raster);
  EXPECT_EQ(actual.GetWidth(), expected.Get()->GetWidth());
  EXPECT_EQ(actual.GetHeight(), expected.Get()->GetHeight());
  EXPECT_EQ(actual.AdvanceX(), expected.Get()->AdvanceX());
  EXPECT_EQ(actual.AdvanceY(), expected.Get()->AdvanceY());
  EXPECT_EQ(actual.FontSize(), expected.Get()->FontSize());
  EXPECT_EQ(actual.FixedSize(), expected.Get()->FixedSize());
  EXPECT_EQ(bytes, expected.ReadableBytes());
  EXPECT_FALSE(a.need_free);
  ASSERT_EQ(a.buffer == nullptr, bytes == 0);
  if (bytes) EXPECT_EQ(std::memcmp(a.buffer, b.buffer, bytes), 0);
}
class FontRasterVisitTest : public ::testing::Test {
 protected:
  void SetUp() override {
    face = Typeface::MakeFromFile(SKITY_FONT_DIR
                                  "fonts/resources/Roboto-Regular.ttf");
    ASSERT_NE(face, nullptr);
    a = face->UnicharToGlyph('A');
    g = face->UnicharToGlyph('g');
    ASSERT_NE(a, 0);
    ASSERT_NE(g, 0);
    paint.SetColor(ColorSetARGB(128, 32, 64, 160));
    paint.SetAntiAlias(true);
  }
  void CheckOwnership(Request request) {
    Font font(face, 24.f);
    auto desc = OriginalDescriptor(font, paint, 1.f, Matrix{}, request);
    auto* cache = ScalerContextCache::GlobalScalerContextCache();
    std::weak_ptr<Typeface> weak_face = face;
    std::weak_ptr<ScalerContextContainer> weak_container =
        cache->FindOrCreateScalerContext(desc, face);
    unsigned calls = 0;
    EXPECT_TRUE(Visit(
        font, a, request, paint, 1.f, Matrix{}, 1048576,
        [&](const GlyphData& glyph, size_t bytes) {
          ++calls;
          EXPECT_GT(bytes, 0u);
          std::vector<uint8_t> saved(glyph.Image().buffer,
                                     glyph.Image().buffer + bytes);
          EXPECT_TRUE(Visit(font, g, {3, 1, 0}, paint, 1.f, Matrix{}, 1048576,
                            [](const GlyphData&, size_t) { return true; }));
          font.SetTypeface(nullptr);
          face.reset();
          cache->PurgeByTypeface(desc.typeface_id);
          EXPECT_FALSE(weak_face.expired());
          EXPECT_FALSE(weak_container.expired());
          if (weak_face.expired() || weak_container.expired()) return false;
          EXPECT_EQ(std::memcmp(saved.data(), glyph.Image().buffer, bytes), 0);
          return true;
        }));
    EXPECT_EQ(calls, 1u);
    EXPECT_TRUE(weak_face.expired());
    EXPECT_TRUE(weak_container.expired());
  }
  std::shared_ptr<Typeface> face;
  GlyphID a = 0, g = 0;
  Paint paint;
};
TEST_F(FontRasterVisitTest, PackedPhasesMatchOriginalSource) {
  Font font(face, 24.f);
  unsigned cases = 0;
  const std::array<Matrix, 2> matrices = {
      Matrix{}, Matrix(1.25f, .125f, 0, -.25f, .95f, 0, 0, 0, 1)};
  for (auto glyph : {a, g})
    for (float density : {1.f, 2.f})
      for (const auto& matrix : matrices)
        for (uint8_t x = 0; x < 4; ++x)
          for (uint8_t y = 0; y < 4; ++y) {
            Request request{x, y, 0};
            auto expected =
                Original(font, glyph, paint, density, matrix, request);
            ASSERT_TRUE(expected.Complete());
            unsigned calls = 0;
            ASSERT_TRUE(Visit(font, glyph, request, paint, density, matrix,
                              8388608,
                              [&](const GlyphData& actual, size_t bytes) {
                                ++calls;
                                Same(actual, bytes, expected);
                                return true;
                              }));
            EXPECT_EQ(calls, 1u);
            ++cases;
          }
  EXPECT_EQ(cases, 128u);
  RecordProperty("exact_original_requests", cases);
}
#if defined(__APPLE__)
TEST_F(FontRasterVisitTest, PackedColorPhasesMatchOriginalSource) {
  auto color_face = FontManager::RefDefault()->MatchFamilyStyle(
      "Apple Color Emoji", FontStyle{});
  ASSERT_NE(color_face, nullptr);
  const auto glyph = color_face->UnicharToGlyph(0x1f600);
  ASSERT_NE(glyph, 0);
  Font font(color_face, 24.f);
  unsigned cases = 0;
  for (float density : {1.f, 2.f})
    for (uint8_t x = 0; x < 4; ++x)
      for (uint8_t y = 0; y < 4; ++y) {
        Request request{x, y, 0};
        auto expected =
            Original(font, glyph, paint, density, Matrix{}, request);
        ASSERT_TRUE(expected.Complete());
        ASSERT_GT(expected.ReadableBytes(), 0u);
        ASSERT_TRUE(Visit(font, glyph, request, paint, density, Matrix{},
                          8388608, [&](const GlyphData& actual, size_t bytes) {
                            Same(actual, bytes, expected);
                            return true;
                          }));
        ++cases;
      }
  unsigned calls = 0;
  EXPECT_FALSE(Visit(font, glyph, {1, 0, 2}, paint, 1.f, Matrix{}, 8388608,
                     [&](const GlyphData&, size_t) {
                       ++calls;
                       return true;
                     }));
  EXPECT_EQ(calls, 0u);
  EXPECT_EQ(cases, 32u);
  RecordProperty("exact_color_requests", cases);
}
TEST_F(FontRasterVisitTest, NativePhasesMatchOriginalAndLegacySources) {
  Font font(face, 24.f);
  unsigned cases = 0;
  for (auto glyph : {a, g})
    for (uint8_t phase = 0; phase < 4; ++phase)
      for (uint8_t y = 0; y < 4; ++y) {
        Request request{phase, y, uint8_t(phase + 1)};
        auto expected = Original(font, glyph, paint, 1.f, Matrix{}, request);
        ASSERT_TRUE(expected.Complete());
        const GlyphData* legacy = nullptr;
        font.LoadGlyphBitmapAtPhase(&glyph, 1, &legacy, paint, phase);
        ASSERT_NE(legacy, nullptr);
        Same(*legacy,
             legacy->Image().RowBytes() * size_t(legacy->Image().height),
             expected);
        ASSERT_TRUE(Visit(font, glyph, request, paint, 1.f, Matrix{}, 8388608,
                          [&](const GlyphData& actual, size_t bytes) {
                            Same(actual, bytes, expected);
                            return true;
                          }));
        ++cases;
      }
  EXPECT_EQ(cases, 32u);
  RecordProperty("exact_native_requests", cases);
}
#endif
TEST_F(FontRasterVisitTest, RejectsInvalidPhaseInputsWithoutVisiting) {
  ASSERT_TRUE(HasRasterOptions<Font>::value);
  Font font(face, 24.f);
  unsigned calls = 0;
  auto callback = [&](const GlyphData&, size_t) {
    ++calls;
    return true;
  };
  for (Request request : {Request{4, 0, 0}, Request{0, 4, 0}, Request{0, 0, 5},
                          Request{255, 255, 255}})
    EXPECT_FALSE(
        Visit(font, a, request, paint, 1.f, Matrix{}, 8388608, callback));
  EXPECT_FALSE(
      Visit(font, a, {1, 0, 2}, paint, 2.f, Matrix{}, 8388608, callback));
  EXPECT_FALSE(Visit(font, a, {1, 0, 2}, paint, 1.f, Matrix::Scale(2.f, 1.f),
                     8388608, callback));
  Paint stroke = paint;
  stroke.SetStyle(Paint::kStroke_Style);
  EXPECT_FALSE(
      Visit(font, a, {1, 0, 2}, stroke, 1.f, Matrix{}, 8388608, callback));
  Font skew = font;
  skew.SetSkewX(.25f);
  EXPECT_FALSE(
      Visit(skew, a, {1, 0, 2}, paint, 1.f, Matrix{}, 8388608, callback));
  EXPECT_EQ(calls, 0u);
}
TEST_F(FontRasterVisitTest, BoundsPackedSpanAndPropagatesRefusal) {
  const auto legacy_member = &Font::VisitGlyphBitmap;
  EXPECT_NE(legacy_member, nullptr);
  Font font(face, 24.f);
  size_t bytes = 0;
  unsigned calls = 0;
  ASSERT_TRUE(Visit(font, a, {1, 3, 0}, paint, 1.f, Matrix{}, 8388608,
                    [&](const GlyphData&, size_t n) {
                      bytes = n;
                      return true;
                    }));
  ASSERT_GT(bytes, 1u);
  auto callback = [&](const GlyphData&, size_t) {
    ++calls;
    return true;
  };
  EXPECT_FALSE(Visit(font, a, {1, 3, 0}, paint, 1.f, Matrix{}, 0, callback));
  EXPECT_FALSE(
      Visit(font, a, {1, 3, 0}, paint, 1.f, Matrix{}, bytes - 1, callback));
  EXPECT_EQ(calls, 0u);
  EXPECT_FALSE(Visit(font, a, {1, 3, 0}, paint, 1.f, Matrix{}, bytes,
                     [&](const GlyphData&, size_t) {
                       ++calls;
                       return false;
                     }));
  EXPECT_EQ(calls, 1u);
}
TEST_F(FontRasterVisitTest, PackedViewsRetainOwnersAcrossReentryAndPurge) {
  CheckOwnership({1, 2, 0});
}
#if defined(__APPLE__)
TEST_F(FontRasterVisitTest, NativeViewsRetainOwnersAcrossReentryAndPurge) {
  CheckOwnership({1, 2, 2});
}
#endif
TEST_F(FontRasterVisitTest, ConcurrentPackedRequestsHaveIndependentStorage) {
  ASSERT_TRUE(HasRasterOptions<Font>::value);
  const Font font(face, 24.f);
  std::array<unsigned, 4> completed{};
  std::array<std::thread, 4> workers;
  for (size_t i = 0; i < workers.size(); ++i)
    workers[i] = std::thread([&, i] {
      for (unsigned n = 0; n < 16; ++n) {
        bool ok = Visit(
            font, a, {uint8_t(i), uint8_t(n % 4), 0}, paint, 1.f, Matrix{},
            1048576, [&](const GlyphData& glyph, size_t bytes) {
              if (!bytes || !glyph.Image().buffer) return false;
              std::vector<uint8_t> saved(glyph.Image().buffer,
                                         glyph.Image().buffer + bytes);
              if (!Visit(font, g,
                         {uint8_t((i + 1) % 4), uint8_t((n + 1) % 4), 0}, paint,
                         1.f, Matrix{}, 1048576,
                         [](const GlyphData&, size_t) { return true; }))
                return false;
              if (n % 4 == 0)
                ScalerContextCache::GlobalScalerContextCache()->PurgeByTypeface(
                    face->TypefaceId());
              return std::memcmp(saved.data(), glyph.Image().buffer, bytes) ==
                     0;
            });
        if (ok) ++completed[i];
      }
    });
  for (auto& worker : workers) worker.join();
  for (auto n : completed) EXPECT_EQ(n, 16u);
  RecordProperty("concurrent_requests", 64);
}
}  // namespace
}  // namespace skity
