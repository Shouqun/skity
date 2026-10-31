// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "src/text/scaler_context_cache.hpp"
#include "src/text/scaler_context_container.hpp"

namespace skity {
namespace {

class ImageTestScalerContext final : public ScalerContext {
 public:
  explicit ImageTestScalerContext(const ScalerContextDesc* desc,
                                  bool allocate = false)
      : ScalerContext({}, desc), allocate_(allocate) {}

  GlyphBitmapData layout = [] {
    GlyphBitmapData image;
    image.origin_x = -2.f;
    image.origin_y = 5.f;
    image.width = 3.f;
    image.height = 2.f;
    image.row_bytes = 8;
    image.format = BitmapFormat::kGray8;
    return image;
  }();
  uint8_t* last_buffer = nullptr;
  bool fail = false;

 protected:
  void GenerateMetrics(GlyphData*) override {}
  void GenerateImage(PackedGlyphID, GlyphData* glyph,
                     const StrokeDesc&) override {
    if (fail) {
      return;
    }
    auto image = layout;
    last_buffer =
        allocate_ ? static_cast<uint8_t*>(std::malloc(16)) : scratch_.data();
    std::memset(last_buffer, ++requests_, allocate_ ? 16 : scratch_.size());
    image.buffer = last_buffer;
    image.need_free = allocate_;
    const_cast<GlyphBitmapData&>(glyph->Image()) = image;
  }
  void GenerateImageInfo(PackedGlyphID, GlyphData*,
                         const StrokeDesc&) override {}
  bool GeneratePath(GlyphData* glyph) override {
    const_cast<Path&>(glyph->GetPath()).AddRect(0.f, 0.f, 3.f, 2.f);
    return true;
  }
  void GenerateFontMetrics(FontMetrics* metrics) override { *metrics = {}; }
  uint16_t OnGetFixedSize() override { return 0; }

 private:
  bool allocate_;
  uint8_t requests_ = 0;
  // Two 3-byte rows at stride 8, with no padding after the last row.
  std::array<uint8_t, 11> scratch_{};
};

TEST(GlyphImageTest,
     BorrowedPixelsSurviveLaterRequestsAndContainerDestruction) {
  OwnedGlyphImage first;
  {
    ScalerContextDesc desc{};
    auto scaler = std::make_unique<ImageTestScalerContext>(&desc);
    auto* backend = scaler.get();
    ScalerContextContainer container(std::move(scaler));
    first = container.PrepareOwnedImage(PackedGlyphID(7, 1, 2), Paint());
    EXPECT_TRUE(first.Complete());
    EXPECT_EQ(first.ReadableBytes(), 16u);
    EXPECT_NE(first.Image().buffer, backend->last_buffer);
    auto second = container.PrepareOwnedImage(PackedGlyphID(7, 1, 2), Paint());
    EXPECT_NE(first.Get(), second.Get());
    EXPECT_NE(first.Image().buffer, second.Image().buffer);
    EXPECT_EQ(first.Image().buffer[0], 1);
    EXPECT_EQ(second.Image().buffer[0], 2);
    second.Image().buffer[0] = 99;
    EXPECT_EQ(first.Image().buffer[0], 1);
  }
  EXPECT_EQ(first.Get()->Id(), 7);
  EXPECT_FALSE(first.Image().need_free);
  EXPECT_EQ(first.Image().RowBytes(), 8u);
  EXPECT_EQ(first.Image().buffer[8], 1);
  EXPECT_FLOAT_EQ(first.Image().origin_x, -2.f);
  EXPECT_FLOAT_EQ(first.Image().origin_y, 5.f);
  auto moved = std::move(first);
  EXPECT_EQ(first.Get(), nullptr);
  EXPECT_FALSE(first.Complete());
  EXPECT_EQ(first.ReadableBytes(), 0u);
  EXPECT_TRUE(moved.Complete());
  EXPECT_EQ(moved.ReadableBytes(), 16u);
  EXPECT_EQ(moved.Image().buffer[8], 1);
}

TEST(GlyphImageTest, AdoptsAllocatedPixelsWithoutCopying) {
  OwnedGlyphImage image;
  {
    ScalerContextDesc desc{};
    auto scaler = std::make_unique<ImageTestScalerContext>(&desc, true);
    auto* backend = scaler.get();
    ScalerContextContainer container(std::move(scaler));
    image = container.PrepareOwnedImage(PackedGlyphID(7), Paint());
    EXPECT_TRUE(image.Complete());
    EXPECT_EQ(image.ReadableBytes(), 16u);
    EXPECT_EQ(image.Image().buffer, backend->last_buffer);
    EXPECT_FALSE(image.Image().need_free);
  }
  EXPECT_EQ(image.Image().buffer[0], 1);
  EXPECT_EQ(image.Image().buffer[8], 1);
}

TEST(GlyphImageTest, CopiesTightBorrowedPixels) {
  ScalerContextDesc desc{};
  auto scaler = std::make_unique<ImageTestScalerContext>(&desc);
  scaler->layout.row_bytes = 3;
  ScalerContextContainer container(std::move(scaler));
  auto first = container.PrepareOwnedImage(PackedGlyphID(7), Paint());
  auto second = container.PrepareOwnedImage(PackedGlyphID(7), Paint());
  EXPECT_EQ(first.Image().RowBytes(), 3u);
  for (size_t i = 0; i < 6; ++i) {
    EXPECT_EQ(first.Image().buffer[i], 1);
    EXPECT_EQ(second.Image().buffer[i], 2);
  }
}

TEST(GlyphImageTest, InitializesOnlyDestinationPadding) {
  ScalerContextDesc desc{};
  ScalerContextContainer container(
      std::make_unique<ImageTestScalerContext>(&desc));
  auto image = container.PrepareOwnedImage(PackedGlyphID(7), Paint());
  ASSERT_EQ(image.Image().RowBytes(), 8u);
  for (size_t y = 0; y < 2; ++y) {
    for (size_t x = 0; x < 8; ++x) {
      EXPECT_EQ(image.Image().buffer[y * 8 + x], x < 3 ? 1 : 0);
    }
  }
}

TEST(GlyphImageTest, MovesPreservePathStorageAndPixelOwnership) {
  ScalerContextDesc desc{};
  ScalerContextContainer container(
      std::make_unique<ImageTestScalerContext>(&desc, true));
  const GlyphID id = 7;
  const GlyphData* cached = nullptr;
  container.PreparePaths(&id, 1, &cached);
  auto first = container.PrepareOwnedImage(PackedGlyphID(id), Paint());
  const auto* points = first.Get()->GetPath().Points();
  auto* pixels = first.Image().buffer;
  ASSERT_GT(first.Get()->GetPath().CountPoints(), 0u);
  EXPECT_NE(points, cached->GetPath().Points());
  auto moved = std::move(first);
  EXPECT_EQ(first.Get(), nullptr);
  EXPECT_EQ(moved.Get()->GetPath().Points(), points);
  auto destination = container.PrepareOwnedImage(PackedGlyphID(id), Paint());
  destination = std::move(moved);
  EXPECT_EQ(moved.Get(), nullptr);
  EXPECT_EQ(destination.Get()->GetPath().Points(), points);
  EXPECT_EQ(destination.Image().buffer, pixels);
  EXPECT_EQ(destination.Image().buffer[0], 1);
  destination = OwnedGlyphImage{};
  EXPECT_EQ(destination.Get(), nullptr);
}

TEST(GlyphImageTest, LegacyBitmapIsStableAcrossThreads) {
  ScalerContextDesc desc{};
  ScalerContextContainer container(
      std::make_unique<ImageTestScalerContext>(&desc));
  const PackedGlyphID id(7);
  const GlyphData* first = nullptr;
  container.PrepareImages(&id, 1, &first, Paint());
  ASSERT_NE(first, nullptr);
  ASSERT_EQ(first->Image().buffer[0], 1);
  // Force the formerly unsafe interleaving without racing the test itself:
  // another thread regenerates this same glyph before the first consumes it.
  std::thread other([&] {
    const GlyphData* second = nullptr;
    container.PrepareImages(&id, 1, &second, Paint());
    EXPECT_EQ(second->Image().buffer[0], 2);
  });
  other.join();
  EXPECT_EQ(first->Image().buffer[0], 1);
  EXPECT_EQ(first->Image().buffer[8], 1);
}

TEST(GlyphImageTest, FailedGenerationDoesNotReusePreviousImage) {
  ScalerContextDesc desc{};
  auto scaler = std::make_unique<ImageTestScalerContext>(&desc, true);
  auto* backend = scaler.get();
  ScalerContextContainer container(std::move(scaler));
  auto first = container.PrepareOwnedImage(PackedGlyphID(7), Paint());
  backend->fail = true;
  auto failed = container.PrepareOwnedImage(PackedGlyphID(7), Paint());
  EXPECT_EQ(failed.Image().buffer, nullptr);
  EXPECT_FALSE(failed.Image().need_free);
  EXPECT_EQ(first.Image().buffer[0], 1);
}

TEST(GlyphImageTest, RejectsInvalidBorrowedImageLayouts) {
  ScalerContextDesc desc{};
  auto scaler = std::make_unique<ImageTestScalerContext>(&desc);
  auto* backend = scaler.get();
  ScalerContextContainer container(std::move(scaler));
  const auto valid = backend->layout;
  for (float width : {-1.f, 1.5f, std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::max()}) {
    backend->layout = valid;
    backend->layout.width = width;
    EXPECT_EQ(
        container.PrepareOwnedImage(PackedGlyphID(7), Paint()).Image().buffer,
        nullptr);
  }
  backend->layout = valid;
  backend->layout.row_bytes = 2;
  EXPECT_EQ(
      container.PrepareOwnedImage(PackedGlyphID(7), Paint()).Image().buffer,
      nullptr);
  backend->layout.row_bytes = std::numeric_limits<size_t>::max();
  EXPECT_EQ(
      container.PrepareOwnedImage(PackedGlyphID(7), Paint()).Image().buffer,
      nullptr);
}

TEST(GlyphImageTest, EmptyImageDoesNotExposeZeroSizeAllocation) {
  ScalerContextDesc desc{};
  auto scaler = std::make_unique<ImageTestScalerContext>(&desc, true);
  scaler->layout.width = 0;
  scaler->layout.height = 0;
  ScalerContextContainer container(std::move(scaler));
  auto image = container.PrepareOwnedImage(PackedGlyphID(7), Paint());
  EXPECT_EQ(image.Image().buffer, nullptr);
  EXPECT_FALSE(image.Image().need_free);
  EXPECT_FLOAT_EQ(image.Image().origin_x, -2.f);
  EXPECT_TRUE(image.Complete());
  EXPECT_EQ(image.ReadableBytes(), 0u);
}

TEST(GlyphImageTest, SnapshotAllowanceRefusesWithoutPublishingPixelView) {
  for (bool allocate : {false, true}) {
    ScalerContextDesc desc{};
    auto scaler = std::make_unique<ImageTestScalerContext>(&desc, allocate);
    ScalerContextContainer container(std::move(scaler));
    auto refused = container.PrepareOwnedImage(PackedGlyphID(7), Paint(), 15);
    EXPECT_FALSE(refused.Complete());
    EXPECT_EQ(refused.ReadableBytes(), 0u);
    EXPECT_EQ(refused.Image().buffer, nullptr);
    auto accepted = container.PrepareOwnedImage(PackedGlyphID(7), Paint(), 16);
    EXPECT_TRUE(accepted.Complete());
    EXPECT_EQ(accepted.ReadableBytes(), 16u);
    EXPECT_NE(accepted.Image().buffer, nullptr);
    auto moved = std::move(accepted);
    EXPECT_FALSE(accepted.Complete());
    EXPECT_EQ(accepted.ReadableBytes(), 0u);
    EXPECT_TRUE(moved.Complete());
    EXPECT_EQ(moved.ReadableBytes(), 16u);
  }
}

// Keep this adapter compilable on the preceding implementation so the same
// permanent cases demonstrate the absent scoped ownership contract first.
using BitmapViewCallback = bool (*)(const GlyphData&, size_t, const void*);
template <class T, class = void>
struct HasBitmapVisit : std::false_type {};
template <class T>
struct HasBitmapVisit<
    T, std::void_t<decltype(std::declval<const T&>().VisitGlyphBitmap(
           GlyphID{}, std::declval<const Paint&>(), 1.f,
           std::declval<const Matrix&>(), size_t{1}, BitmapViewCallback{},
           static_cast<const void*>(nullptr)))>> : std::true_type {};
template <class T, class Callback>
bool VisitBitmap(const T& font, GlyphID id, size_t maximum_bytes,
                 const Callback& callback) {
  if constexpr (HasBitmapVisit<T>::value) {
    return font.VisitGlyphBitmap(
        id, Paint(), 1.f, Matrix(), maximum_bytes,
        [](const GlyphData& glyph, size_t bytes, const void* context) {
          return (*static_cast<const Callback*>(context))(glyph, bytes);
        },
        &callback);
  } else {
    return false;
  }
}
class FontBitmapVisitTest : public ::testing::Test {
 protected:
  void SetUp() override {
    face = Typeface::MakeFromFile(SKITY_FONT_DIR
                                  "fonts/resources/Roboto-Regular.ttf");
    ASSERT_NE(face, nullptr);
    id = face->UnicharToGlyph('A');
    other = face->UnicharToGlyph('g');
    ASSERT_NE(id, 0);
    ASSERT_NE(other, 0);
  }
  std::shared_ptr<Typeface> face;
  GlyphID id = 0, other = 0;
};
TEST_F(FontBitmapVisitTest, ReportsOwnedReadableSpanMatchingLegacyPixels) {
  Font font(face, 24.f);
  const GlyphData* legacy = nullptr;
  font.LoadGlyphBitmap(&id, 1, &legacy, Paint(), 1.f, Matrix());
  ASSERT_NE(legacy, nullptr);
  const auto image = legacy->Image();
  ASSERT_NE(image.buffer, nullptr);
  const auto size = image.RowBytes() * static_cast<size_t>(image.height);
  std::vector<uint8_t> expected(image.buffer, image.buffer + size);
  int calls = 0;
  EXPECT_TRUE(
      VisitBitmap(font, id, size, [&](const GlyphData& glyph, size_t bytes) {
        ++calls;
        EXPECT_EQ(glyph.Id(), id);
        EXPECT_EQ(bytes, size);
        EXPECT_FALSE(glyph.Image().need_free);
        EXPECT_EQ(glyph.Image().buffer == nullptr, bytes == 0);
        EXPECT_EQ(glyph.Image().RowBytes(), image.RowBytes());
        EXPECT_EQ(glyph.Image().format, image.format);
        EXPECT_EQ(std::vector<uint8_t>(glyph.Image().buffer,
                                       glyph.Image().buffer + bytes),
                  expected);
        return true;
      }));
  EXPECT_EQ(calls, 1);
}
TEST_F(FontBitmapVisitTest, RetainsOwnersAndPixelsAcrossReentryAndPurge) {
  Font font(face, 24.f);
  const auto desc =
      ScalerContextDesc::MakeTransformed(font, Paint(), 1.f, Matrix22{});
  auto* cache = ScalerContextCache::GlobalScalerContextCache();
  std::weak_ptr<Typeface> weak_face = face;
  std::weak_ptr<ScalerContextContainer> weak_container =
      cache->FindOrCreateScalerContext(desc, face);
  int calls = 0;
  EXPECT_TRUE(VisitBitmap(
      font, id, 1024 * 1024, [&](const GlyphData& glyph, size_t bytes) {
        ++calls;
        EXPECT_GT(bytes, 0u);
        const auto* pixels = glyph.Image().buffer;
        std::vector<uint8_t> saved(pixels, pixels + bytes);
        const GlyphData* nested = nullptr;
        font.LoadGlyphBitmap(&other, 1, &nested, Paint(), 1.f, Matrix());
        EXPECT_NE(nested, nullptr);
        font.SetTypeface(nullptr);
        face.reset();
        cache->PurgeByTypeface(desc.typeface_id);
        EXPECT_FALSE(weak_face.expired());
        EXPECT_FALSE(weak_container.expired());
        if (weak_face.expired() || weak_container.expired()) return false;
        EXPECT_EQ(std::vector<uint8_t>(pixels, pixels + bytes), saved);
        return true;
      }));
  EXPECT_EQ(calls, 1);
  EXPECT_TRUE(weak_face.expired());
  EXPECT_TRUE(weak_container.expired());
}
TEST_F(FontBitmapVisitTest, RefusesShortSpanAndPropagatesVisitorRefusal) {
  Font font(face, 24.f);
  size_t size = 0;
  int calls = 0;
  ASSERT_TRUE(
      VisitBitmap(font, id, 1024 * 1024, [&](const GlyphData&, size_t bytes) {
        size = bytes;
        return true;
      }));
  ASSERT_GT(size, 1u);
  auto accept = [&](const GlyphData&, size_t) {
    ++calls;
    return true;
  };
  EXPECT_FALSE(VisitBitmap(font, id, 0, accept));
  EXPECT_FALSE(VisitBitmap(font, id, size - 1, accept));
  EXPECT_EQ(calls, 0);
  EXPECT_FALSE(VisitBitmap(font, id, size, [&](const GlyphData&, size_t) {
    ++calls;
    return false;
  }));
  EXPECT_EQ(calls, 1);
}
TEST_F(FontBitmapVisitTest, SpaceBitmapPreservesBackendStorage) {
  Font font(face, 24.f);
  const GlyphID space = face->UnicharToGlyph(' ');
  ASSERT_NE(space, 0);
  const GlyphData* legacy = nullptr;
  font.LoadGlyphBitmap(&space, 1, &legacy, Paint(), 1.f, Matrix());
  ASSERT_NE(legacy, nullptr);
  const auto image = legacy->Image();
  const auto bytes =
      image.buffer ? image.RowBytes() * static_cast<size_t>(image.height) : 0u;
  std::vector<uint8_t> pixels;
  if (bytes) pixels.assign(image.buffer, image.buffer + bytes);
  int calls = 0;
  EXPECT_TRUE(VisitBitmap(
      font, space, bytes ? bytes : 1,
      [&](const GlyphData& glyph, size_t readable) {
        ++calls;
        EXPECT_EQ(glyph.Id(), space);
        EXPECT_EQ(readable, bytes);
        EXPECT_EQ(glyph.Image().width, image.width);
        EXPECT_EQ(glyph.Image().height, image.height);
        EXPECT_EQ(glyph.Image().RowBytes(), image.RowBytes());
        EXPECT_EQ(glyph.Image().buffer == nullptr, bytes == 0);
        if (bytes) {
          EXPECT_EQ(std::vector<uint8_t>(glyph.Image().buffer,
                                         glyph.Image().buffer + readable),
                    pixels);
        }
        return true;
      }));
  EXPECT_EQ(calls, 1);
  std::printf("Scoped space bitmap width=%g height=%g readable=%zu\n",
              image.width, image.height, bytes);
}

TEST(GlyphImageTest, EmptyRasterReportsZeroReadableBytes) {
  ScalerContextDesc desc{};
  auto scaler = std::make_unique<ImageTestScalerContext>(&desc);
  scaler->layout.width = 0;
  scaler->layout.height = 0;
  ScalerContextContainer container(std::move(scaler));
  auto image = container.PrepareOwnedImage(PackedGlyphID(7), Paint(), 1);
  ASSERT_TRUE(image.Complete());
  ASSERT_NE(image.Get(), nullptr);
  EXPECT_EQ(image.ReadableBytes(), 0u);
  EXPECT_EQ(image.Image().buffer, nullptr);
}

TEST_F(FontBitmapVisitTest, ConcurrentRequestsKeepEachViewIndependent) {
  Font font(face, 24.f);
  std::array<unsigned, 4> completed{};
  std::array<std::thread, 4> workers;
  for (size_t i = 0; i < workers.size(); ++i)
    workers[i] = std::thread([&, i] {
      for (int n = 0; n < 32; ++n) {
        const bool ok = VisitBitmap(
            font, id, 1024 * 1024, [&](const GlyphData& glyph, size_t bytes) {
              if (!bytes || !glyph.Image().buffer) return false;
              std::vector<uint8_t> saved(glyph.Image().buffer,
                                         glyph.Image().buffer + bytes);
              const GlyphData* nested = nullptr;
              font.LoadGlyphBitmap(&other, 1, &nested, Paint(), 1.f, Matrix());
              return nested && std::memcmp(saved.data(), glyph.Image().buffer,
                                           bytes) == 0;
            });
        if (ok) ++completed[i];
      }
    });
  for (auto& t : workers) t.join();
  for (auto n : completed) EXPECT_EQ(n, 32u);
}

TEST_F(FontBitmapVisitTest, ScopedViewDoesNotReplaceLegacyThreadLocalEntry) {
  Font font(face, 24.f);
  const GlyphData* legacy = nullptr;
  font.LoadGlyphBitmap(&id, 1, &legacy, Paint(), 1.f, Matrix());
  ASSERT_NE(legacy, nullptr);
  int calls = 0;
  EXPECT_TRUE(VisitBitmap(font, other, 1024 * 1024,
                          [&](const GlyphData& glyph, size_t) {
                            ++calls;
                            // Compare metadata addresses BEFORE any borrowed
                            // pixel read. A negative implementation that reuses
                            // TLS is rejected without touching old pixels.
                            EXPECT_NE(&glyph, legacy);
                            return &glyph != legacy;
                          }));
  EXPECT_EQ(calls, 1);
}

}  // namespace
}  // namespace skity
