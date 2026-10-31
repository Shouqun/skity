// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_TEXT_GLYPH_RASTER_HPP
#define INCLUDE_SKITY_TEXT_GLYPH_RASTER_HPP
#include <cstddef>
#include <cstdint>
#include <memory>
#include <skity/macros.hpp>
#include <utility>

#define SKITY_HAS_GLYPH_RASTER_API 1
namespace skity {
// Font-independent immutable texels. Construction copies only logical rows;
// no mutable pixmap, font, callback, context or caller buffer is retained.
class SKITY_API GlyphRaster final {
 public:
  enum class Format { kA8, kRGBA8, kBGRA8 };
  enum class Alpha { kCoverage, kPremultiplied, kUnpremultiplied };
  static std::unique_ptr<const GlyphRaster> MakeCopy(
      const void* pixels, size_t readable_bytes, uint32_t width,
      uint32_t height, size_t row_bytes, Format format, Alpha alpha,
      size_t maximum_retained_bytes) noexcept;
  ~GlyphRaster() = default;
  GlyphRaster(const GlyphRaster&) = delete;
  GlyphRaster& operator=(const GlyphRaster&) = delete;
  uint32_t Width() const { return width_; }
  uint32_t Height() const { return height_; }
  size_t RowBytes() const { return row_bytes_; }
  size_t PixelBytes() const { return pixel_bytes_; }
  // Exact object/pixel allocation requests. Allocator bookkeeping and later
  // canvas/backend/driver resources are separate, not certified by this value.
  size_t RetainedBytes() const { return sizeof(*this) + pixel_bytes_; }
  const uint8_t* Pixels() const { return pixels_.get(); }
  Format GetFormat() const { return format_; }
  Alpha GetAlpha() const { return alpha_; }

 private:
  GlyphRaster(std::unique_ptr<uint8_t[]> pixels, uint32_t width,
              uint32_t height, size_t row, size_t bytes, Format format,
              Alpha alpha) noexcept
      : pixels_(std::move(pixels)),
        width_(width),
        height_(height),
        row_bytes_(row),
        pixel_bytes_(bytes),
        format_(format),
        alpha_(alpha) {}
  std::unique_ptr<uint8_t[]> pixels_;
  uint32_t width_, height_;
  size_t row_bytes_, pixel_bytes_;
  Format format_;
  Alpha alpha_;
};
}  // namespace skity
#endif
