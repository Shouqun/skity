// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cstring>
#include <limits>
#include <new>
#include <skity/text/glyph_raster.hpp>
namespace skity {
std::unique_ptr<const GlyphRaster> GlyphRaster::MakeCopy(
    const void* pixels, size_t readable_bytes, uint32_t width, uint32_t height,
    size_t row_bytes, Format format, Alpha alpha, size_t maximum) noexcept {
  const bool coverage = format == Format::kA8;
  if (!pixels || !width || !height ||
      (format != Format::kA8 && format != Format::kRGBA8 &&
       format != Format::kBGRA8) ||
      (coverage ? alpha != Alpha::kCoverage
                : (alpha != Alpha::kPremultiplied &&
                   alpha != Alpha::kUnpremultiplied)))
    return {};
  const size_t bpp = coverage ? 1 : 4;
  const size_t cap = std::numeric_limits<size_t>::max();
  if (width > cap / bpp) return {};
  const size_t row = size_t(width) * bpp;
  if (row_bytes < row || row_bytes > cap / height ||
      readable_bytes < row_bytes * height || row > cap / height)
    return {};
  if (row_bytes * height > size_t(std::numeric_limits<std::ptrdiff_t>::max()))
    return {};
  const size_t bytes = row * height;
  if (bytes > size_t(std::numeric_limits<std::ptrdiff_t>::max()) ||
      maximum < sizeof(GlyphRaster) || bytes > maximum - sizeof(GlyphRaster))
    return {};
  std::unique_ptr<uint8_t[]> owned(new (std::nothrow) uint8_t[bytes]);
  if (!owned) return {};
  for (uint32_t y = 0; y < height; ++y)
    std::memcpy(owned.get() + size_t(y) * row,
                static_cast<const uint8_t*>(pixels) + size_t(y) * row_bytes,
                row);
  return std::unique_ptr<const GlyphRaster>(new (std::nothrow) GlyphRaster(
      std::move(owned), width, height, row, bytes, format, alpha));
}
}  // namespace skity
