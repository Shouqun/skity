// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_SRC_IO_CORE_BRIDGE_VALUES_HPP
#define MODULE_IO_SRC_IO_CORE_BRIDGE_VALUES_HPP

namespace skity {
namespace core_bridge {
//
// Matrix / FontStyle helpers mirroring out-of-line core functions.
//

// Bit-identical to core Matrix::RotateDeg(deg) / RotateDeg(deg, pt): a
// rotation about the z axis wrapped in the pre/post translations.
inline Matrix RotateDegMatrix(float deg, Vec2 pt = Vec2{0.f, 0.f}) {
  const float rad =
      deg * static_cast<float>(0.01745329251994329576923690768489);
  glm::mat4 d1(1.f);
  d1[3][0] = -pt.x;
  d1[3][1] = -pt.y;
  glm::mat4 d2(1.f);
  d2[3][0] = pt.x;
  d2[3][1] = pt.y;
  glm::mat4 rotated = glm::rotate(d2, rad, glm::vec3(0.f, 0.f, 1.f)) * d1;
  Matrix m{};
  std::memcpy(&m, &rotated, sizeof(m));
  return m;
}

// Bit-identical to core Matrix::IsIdentity (glm::isIdentity with the
// default epsilon over the 4x4 word).
inline bool MatrixIsIdentity(const Matrix& m) {
  return glm::isIdentity(*reinterpret_cast<const glm::mat4*>(&m),
                         glm::epsilon<float>());
}

// Bit-identical to core Matrix::HasPersp.
inline bool MatrixHasPersp(const Matrix& m) {
  return m[0][3] != 0 || m[1][3] != 0 || m[2][3] != 0 || m[3][3] != 1;
}

// Bit-identical to core Rect::CenterX / CenterY.
inline float RectCenterX(const Rect& r) {
  return 0.5f * (r.Right() + r.Left());
}

inline float RectCenterY(const Rect& r) {
  return 0.5f * (r.Top() + r.Bottom());
}

// Bit-identical to core FontStyle::FontStyle: each component is clamped to
// its enum range before packing into the single storage word. FontStyle's
// constructor lives in the core library, so the packed word is stored
// directly (the layout is pinned by the static_assert).
inline void StoreFontStyle(FontStyle& out, int weight, int width,
                           FontStyle::Slant slant) {
  static_assert(sizeof(FontStyle) == sizeof(uint32_t),
                "FontStyle must stay a single packed word");
  struct Limits {
    static int clamp(int v, int lo, int hi) {
      return v < lo ? lo : (v > hi ? hi : v);
    }
  };
  uint32_t packed = Limits::clamp(weight, FontStyle::kInvisible_Weight,
                                  FontStyle::kExtraBlack_Weight) +
                    (Limits::clamp(width, FontStyle::kUltraCondensed_Width,
                                   FontStyle::kUltraExpanded_Width)
                     << 16) +
                    (Limits::clamp(static_cast<int>(slant),
                                   static_cast<int>(FontStyle::kUpright_Slant),
                                   static_cast<int>(FontStyle::kOblique_Slant))
                     << 24);
  std::memcpy(&out, &packed, sizeof(packed));
}

namespace font_style_detail {

// Mirror of the core axis-width interpolation tables (font_style.cc).
inline float InterpAxisWidth(float x) {
  static constexpr float kKeys[9] = {50.f,   62.5f, 75.f,  87.5f, 100.f,
                                     112.5f, 125.f, 150.f, 200.f};
  static constexpr float kValues[9] = {1.f, 2.f, 3.f, 4.f, 5.f,
                                       6.f, 7.f, 8.f, 9.f};
  if (x <= kKeys[0]) {
    return kValues[0];
  }
  for (int i = 0; i < 8; i++) {
    if (x <= kKeys[i + 1]) {
      float t = (x - kKeys[i]) / (kKeys[i + 1] - kKeys[i]);
      return kValues[i] + t * (kValues[i + 1] - kValues[i]);
    }
  }
  return kValues[8];
}

}  // namespace font_style_detail

// Bit-identical to core FontStyle::WidthFromAxisWidth.
inline FontStyle::Width WidthFromAxisWidth(float axis_width) {
  float value = font_style_detail::InterpAxisWidth(axis_width);
  return static_cast<FontStyle::Width>(static_cast<int>(std::round(value)));
}

//
// Small value helpers mirroring out-of-line core functions.
//

// Mirrors core Color4fToColor bit for bit: each channel is scaled to [0,
// 255], clamped, and truncated (no rounding).
inline uint32_t Color4fToColor(const Vec4& color) {
  auto to_byte = [](float value) -> uint32_t {
    float scaled = value * 255.f;
    if (scaled < 0.f) {
      scaled = 0.f;
    }
    if (scaled > 255.f) {
      scaled = 255.f;
    }
    return static_cast<uint32_t>(scaled);
  };
  return (to_byte(color.w) << 24) | (to_byte(color.x) << 16) |
         (to_byte(color.y) << 8) | (to_byte(color.z));
}

}  // namespace core_bridge
}  // namespace skity
#endif
