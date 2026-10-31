// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <limits>
#include <skity/recorder/picture_recorder.hpp>
#include <skity/skity.hpp>
#include <skity/text/glyph_raster.hpp>
#include <type_traits>
#include <vector>

#include "src/gpu/gpu_texture.hpp"
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
#include "base/allocation_failure.hpp"
#endif
namespace skity {
namespace {
using Format = GlyphRaster::Format;
using Alpha = GlyphRaster::Alpha;
TEST(GlyphRasterTest, CopiesOnlyLogicalRowsAndOwnsImmutableStorage) {
  static_assert(!std::is_copy_constructible_v<GlyphRaster>);
  static_assert(!std::is_move_constructible_v<GlyphRaster>);
  for (auto format : {Format::kA8, Format::kRGBA8, Format::kBGRA8}) {
    const size_t row = format == Format::kA8 ? 2 : 8;
    const size_t stride = row + 5;
    std::vector<uint8_t> input(stride * 2, 239);
    for (size_t i = 0; i < row; ++i) {
      input[i] = uint8_t(i + 1);
      input[stride + i] = uint8_t(i + 31);
    }
    const auto alpha =
        format == Format::kA8 ? Alpha::kCoverage : Alpha::kPremultiplied;
    auto raster = GlyphRaster::MakeCopy(input.data(), input.size(), 2, 2,
                                        stride, format, alpha, 1024);
    ASSERT_NE(raster, nullptr);
    EXPECT_EQ(raster->RowBytes(), row);
    EXPECT_EQ(raster->PixelBytes(), row * 2);
    EXPECT_EQ(raster->RetainedBytes(), sizeof(GlyphRaster) + row * 2);
    std::fill(input.begin(), input.end(), 0);
    for (size_t i = 0; i < row; ++i) {
      EXPECT_EQ(raster->Pixels()[i], i + 1);
      EXPECT_EQ(raster->Pixels()[row + i], i + 31);
    }
  }
}
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
TEST(GlyphRasterTest, AllocationRefusalsPublishNoPartialResource) {
  const uint8_t input[4] = {0, 64, 128, 255};
  for (int allowance = 0; allowance <= 2; ++allowance) {
    std::unique_ptr<const GlyphRaster> raster;
    {
      testing::AllocationFailure failure(allowance);
      raster = GlyphRaster::MakeCopy(input, sizeof(input), 2, 2, 2, Format::kA8,
                                     Alpha::kCoverage, 1024);
    }
    if (allowance < 2) {
      EXPECT_EQ(raster, nullptr);
    } else {
      ASSERT_NE(raster, nullptr);
      EXPECT_EQ(std::memcmp(raster->Pixels(), input, sizeof(input)), 0);
    }
  }
}
#endif
TEST(GlyphRasterTest, BoundsFormatsAndReadableSpansBeforeCopy) {
  uint8_t input[16]{};
  const auto make = [&](const void* p, size_t size, uint32_t w, uint32_t h,
                        size_t row, Format f, Alpha a, size_t cap) {
    return GlyphRaster::MakeCopy(p, size, w, h, row, f, a, cap);
  };
  EXPECT_FALSE(make(nullptr, 16, 2, 2, 2, Format::kA8, Alpha::kCoverage, 1024));
  EXPECT_FALSE(make(input, 16, 0, 2, 2, Format::kA8, Alpha::kCoverage, 1024));
  EXPECT_FALSE(make(input, 16, 2, 0, 2, Format::kA8, Alpha::kCoverage, 1024));
  EXPECT_FALSE(make(input, 16, 2, 2, 1, Format::kA8, Alpha::kCoverage, 1024));
  EXPECT_FALSE(make(input, 3, 2, 2, 2, Format::kA8, Alpha::kCoverage, 1024));
  EXPECT_FALSE(make(input, 16, 2, 2, std::numeric_limits<size_t>::max(),
                    Format::kA8, Alpha::kCoverage, 1024));
  EXPECT_FALSE(make(input, 16, 2, 2, 2, Format(99), Alpha::kCoverage, 1024));
  EXPECT_FALSE(
      make(input, 16, 2, 2, 2, Format::kA8, Alpha::kPremultiplied, 1024));
  EXPECT_FALSE(
      make(input, 16, 2, 2, 8, Format::kRGBA8, Alpha::kCoverage, 1024));
  EXPECT_FALSE(make(input, 16, 2, 2, 8, Format::kBGRA8, Alpha(99), 1024));
  EXPECT_FALSE(make(input, 16, 2, 2, 2, Format::kA8, Alpha::kCoverage,
                    sizeof(GlyphRaster) + 3));
  EXPECT_TRUE(make(input, 16, 2, 2, 2, Format::kA8, Alpha::kCoverage,
                   sizeof(GlyphRaster) + 4));
}
TEST(GlyphRasterTest, RejectsSourceSpanBeyondPointerArithmeticRange) {
  uint8_t bytes[4]{};
  EXPECT_FALSE(
      GlyphRaster::MakeCopy(bytes, std::numeric_limits<size_t>::max(), 1, 2,
                            size_t(std::numeric_limits<std::ptrdiff_t>::max()),
                            Format::kA8, Alpha::kCoverage, 1024));
}
TEST(GlyphRasterTest, SoftwareAndRecordingPreserveExistingImagePolicy) {
  for (auto format : {Format::kA8, Format::kRGBA8, Format::kBGRA8})
    for (auto alpha : {Alpha::kPremultiplied, Alpha::kUnpremultiplied})
      for (auto filter : {FilterMode::kNearest, FilterMode::kLinear}) {
        const bool coverage = format == Format::kA8;
        if (coverage && alpha == Alpha::kUnpremultiplied) continue;
        const auto source_alpha = coverage ? Alpha::kCoverage : alpha;
        std::vector<uint8_t> bytes =
            coverage ? std::vector<uint8_t>{0, 64, 128, 255}
                     : std::vector<uint8_t>{0,  0,  0,  0,   16, 32,  48,  64,
                                            32, 64, 96, 128, 64, 128, 192, 255};
        auto resource =
            GlyphRaster::MakeCopy(bytes.data(), bytes.size(), 2, 2,
                                  coverage ? 2 : 8, format, source_alpha, 1024);
        ASSERT_NE(resource, nullptr);
        const auto type =
            format == Format::kBGRA8 ? ColorType::kBGRA : ColorType::kRGBA;
        const auto image_alpha = coverage || alpha == Alpha::kUnpremultiplied
                                     ? AlphaType::kUnpremul_AlphaType
                                     : AlphaType::kPremul_AlphaType;
        auto pixmap = std::make_shared<Pixmap>(2, 2, image_alpha, type);
        auto* raw = static_cast<uint8_t*>(pixmap->WritableAddr());
        ASSERT_NE(raw, nullptr);
        Paint paint;
        paint.SetColor(ColorSetARGB(128, 32, 64, 160));
        paint.SetAntiAlias(true);
        if (coverage) {
          for (size_t i = 0; i < 4; ++i) {
            if (!bytes[i]) continue;
            raw[4 * i] = 32;
            raw[4 * i + 1] = 64;
            raw[4 * i + 2] = 160;
            raw[4 * i + 3] = uint8_t((128u * bytes[i] + 127) / 255);
          }
        } else
          std::memcpy(raw, bytes.data(), bytes.size());
        Paint composite;
        composite.SetColor(Color_WHITE);
        if (!coverage) composite.SetAlphaF(paint.GetAlphaF());
        auto image = Image::MakeImage(pixmap);
        ASSERT_NE(image, nullptr);
        Bitmap expected(16, 16), actual(16, 16);
        auto a = Canvas::MakeSoftwareCanvas(&expected),
             b = Canvas::MakeSoftwareCanvas(&actual);
        ASSERT_NE(a, nullptr);
        ASSERT_NE(b, nullptr);
        a->Clear(Color_WHITE);
        b->Clear(Color_WHITE);
        const auto bounds = Rect::MakeXYWH(3.25, 4.5, 7.5, 6.25);
        a->DrawImage(image, bounds, SamplingOptions(filter, MipmapMode::kNone),
                     &composite);
        ASSERT_TRUE(
            b->CanDrawGlyphRaster(*resource, bounds, filter, paint, 16));
        ASSERT_TRUE(b->DrawGlyphRaster(*resource, bounds, filter, paint, 16));
        PictureRecorder recorder;
        recorder.BeginRecording(Rect::MakeWH(16, 16));
        ASSERT_TRUE(recorder.GetRecordingCanvas()->DrawGlyphRaster(
            *resource, bounds, filter, paint, 16));
        auto recording = recorder.FinishRecording();
        ASSERT_NE(recording, nullptr);
        resource.reset();
        std::fill(bytes.begin(), bytes.end(), 255);
        a->Flush();
        b->Flush();
        Bitmap recorded(16, 16);
        auto replay = Canvas::MakeSoftwareCanvas(&recorded);
        ASSERT_NE(replay, nullptr);
        replay->Clear(Color_WHITE);
        recording->Draw(replay.get());
        replay->Flush();
        EXPECT_EQ(std::memcmp(expected.GetPixelAddr(), recorded.GetPixelAddr(),
                              expected.RowBytes() * expected.Height()),
                  0);
        EXPECT_EQ(std::memcmp(expected.GetPixelAddr(), actual.GetPixelAddr(),
                              expected.RowBytes() * expected.Height()),
                  0);
      }
}
TEST(GlyphRasterTest, RefusalsLeavePixelsAndStateUntouched) {
  uint8_t bytes[] = {0, 64, 128, 255};
  auto resource = GlyphRaster::MakeCopy(bytes, 4, 2, 2, 2, Format::kA8,
                                        Alpha::kCoverage, 1024);
  ASSERT_NE(resource, nullptr);
  Bitmap bitmap(8, 8);
  auto canvas = Canvas::MakeSoftwareCanvas(&bitmap);
  ASSERT_NE(canvas, nullptr);
  canvas->Clear(Color_WHITE);
  Paint paint;
  const auto matrix = canvas->GetTotalMatrix();
  const auto count = canvas->GetSaveCount();
  const auto box = Rect::MakeXYWH(1, 1, 4, 4);
  EXPECT_FALSE(
      canvas->DrawGlyphRaster(*resource, box, FilterMode::kNearest, paint, 15));
  EXPECT_FALSE(
      canvas->DrawGlyphRaster(*resource, box, FilterMode(99), paint, 16));
  EXPECT_FALSE(canvas->DrawGlyphRaster(
      *resource,
      Rect::MakeXYWH(0, 0, std::numeric_limits<float>::infinity(), 1),
      FilterMode::kNearest, paint, 16));
  auto stroke = paint;
  stroke.SetStyle(Paint::kStroke_Style);
  EXPECT_FALSE(canvas->DrawGlyphRaster(*resource, box, FilterMode::kNearest,
                                       stroke, 16));
  auto filtered = paint;
  filtered.SetMaskFilter(MaskFilter::MakeGaussianBlur(1));
  EXPECT_FALSE(canvas->DrawGlyphRaster(*resource, box, FilterMode::kNearest,
                                       filtered, 16));
  auto font = paint;
  font.SetTypeface(Typeface::GetDefaultTypeface());
  EXPECT_FALSE(
      canvas->DrawGlyphRaster(*resource, box, FilterMode::kNearest, font, 16));
  EXPECT_TRUE(canvas->DrawGlyphRaster(*resource, Rect::MakeWH(0, 0),
                                      FilterMode::kNearest, paint, 0));
  canvas->Flush();
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x) EXPECT_EQ(bitmap.GetPixel(x, y), Color_WHITE);
  EXPECT_EQ(canvas->GetTotalMatrix(), matrix);
  EXPECT_EQ(canvas->GetSaveCount(), count);
}

class UnknownUploadTexture final : public GPUTexture {
 public:
  explicit UnknownUploadTexture(GPUTextureDescriptor descriptor)
      : GPUTexture(descriptor) {}
  size_t GetBytes() const override { return 16; }
  void UploadData(uint32_t, uint32_t, uint32_t, uint32_t, void*) override {
    ++legacy_calls;
  }
  unsigned legacy_calls = 0;
};
TEST(GlyphRasterTest, UnknownGpuUploadRefusesAndChecksEverySourceExtent) {
  GPUTextureDescriptor descriptor{};
  descriptor.width = descriptor.height = 2;
  descriptor.format = GPUTextureFormat::kR8Unorm;
  descriptor.storage_mode = GPUTextureStorageMode::kHostVisible;
  UnknownUploadTexture texture(descriptor);
  uint8_t data[4]{};
  EXPECT_TRUE(texture.ValidUpload(0, 0, 2, 2, data, 4));
  EXPECT_FALSE(texture.UploadDataChecked(0, 0, 2, 2, data, 4));
  EXPECT_EQ(texture.legacy_calls, 0u);
  EXPECT_FALSE(texture.ValidUpload(0, 0, 2, 2, data, 3));
  EXPECT_FALSE(texture.ValidUpload(0, 0, 2, 2, nullptr, 4));
  EXPECT_FALSE(texture.ValidUpload(0, 0, 0, 2, data, 4));
  EXPECT_FALSE(texture.ValidUpload(0, 0, 2, 0, data, 4));
  EXPECT_FALSE(texture.ValidUpload(1, 0, 2, 2, data, 4));
  EXPECT_FALSE(texture.ValidUpload(0, 1, 2, 2, data, 4));
  EXPECT_FALSE(texture.ValidUpload(UINT32_MAX, 0, 1, 1, data, 4));
  descriptor.sample_count = 2;
  UnknownUploadTexture multisampled(descriptor);
  EXPECT_FALSE(multisampled.ValidUpload(0, 0, 2, 2, data, 4));
  descriptor.sample_count = 1;
  descriptor.format = GPUTextureFormat(99);
  UnknownUploadTexture bad_format(descriptor);
  EXPECT_FALSE(bad_format.ValidUpload(0, 0, 2, 2, data, 4));
  descriptor.format = GPUTextureFormat::kR8Unorm;
  descriptor.storage_mode = GPUTextureStorageMode::kMemoryless;
  UnknownUploadTexture memoryless(descriptor);
  EXPECT_FALSE(memoryless.ValidUpload(0, 0, 2, 2, data, 4));
}
}  // namespace
}  // namespace skity
