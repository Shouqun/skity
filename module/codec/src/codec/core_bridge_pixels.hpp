// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CODEC_SRC_CODEC_CORE_BRIDGE_PIXELS_HPP
#define MODULE_CODEC_SRC_CODEC_CORE_BRIDGE_PIXELS_HPP
#include <cstdint>
#include <skity/io/pixmap.hpp>
namespace skity {
namespace core_bridge {
inline size_t BytesPerPixel(ColorType color_type) {
  switch (color_type) {
    case ColorType::kRGBA:
    case ColorType::kBGRA:
      return 4;
    case ColorType::kRGB565:
      return 2;
    case ColorType::kA8:
      return 1;
    case ColorType::kUnknown:
      return 0;
  }
  return 0;
}

inline const uint8_t* PixmapAddr8(const Pixmap* pixmap, uint32_t x,
                                  uint32_t y) {
  if (pixmap == nullptr || x >= pixmap->Width() || y >= pixmap->Height()) {
    return nullptr;
  }
  return static_cast<const uint8_t*>(pixmap->Addr()) +
         static_cast<size_t>(y) * pixmap->RowBytes() +
         static_cast<size_t>(x) * BytesPerPixel(pixmap->GetColorType());
}

inline uint8_t* PixmapWritableAddr8(Pixmap* pixmap, uint32_t x, uint32_t y) {
  return const_cast<uint8_t*>(PixmapAddr8(pixmap, x, y));
}

//
// Color conversions — bit-identical reimplementations of the core
// ColorToPMColor / PMColorToColor, including their rounding.
//

// a*b/255 with the core's two-step rounding (color_priv.hpp
// Mul16ShiftRound(a, b, 8)).
inline uint32_t MulDiv255Round(uint32_t a, uint32_t b) {
  uint32_t prod = a * b + 128u;
  return (prod + (prod >> 8)) >> 8;
}

inline uint32_t ColorToPMColor(uint32_t color) {
  uint32_t a = (color >> 24) & 0xFFu;
  uint32_t r = (color >> 16) & 0xFFu;
  uint32_t g = (color >> 8) & 0xFFu;
  uint32_t b = color & 0xFFu;
  if (a != 255u) {
    r = MulDiv255Round(r, a);
    g = MulDiv255Round(g, a);
    b = MulDiv255Round(b, a);
  }
  return (a << 24) | (r << 16) | (g << 8) | b;
}

inline uint32_t PMColorToColor(uint32_t color) {
  uint32_t a = (color >> 24) & 0xFFu;
  uint32_t r = (color >> 16) & 0xFFu;
  uint32_t g = (color >> 8) & 0xFFu;
  uint32_t b = color & 0xFFu;
  if (a == 0u) {
    return 0u;
  }
  // The core un-premultiply scale is (255 << 24) / a with the product
  // rounded at bit 23 (color_priv.cc UnPreMultiply); recomputing it per
  // channel yields the same bytes as the core lookup table.
  uint32_t scale = (255u << 24) / a;
  auto apply = [scale](uint32_t c) { return (scale * c + (1u << 23)) >> 24; };
  return (a << 24) | (apply(r) << 16) | (apply(g) << 8) | apply(b);
}

}  // namespace core_bridge
}  // namespace skity
#endif
