// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_SRC_IO_CORE_BRIDGE_CPP_HPP
#define MODULE_IO_SRC_IO_CORE_BRIDGE_CPP_HPP

#include <cmath>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_query.hpp>
#include <skity/codec/codec.hpp>
#include <skity/effect/color_filter.hpp>
#include <skity/effect/image_filter.hpp>
#include <skity/effect/mask_filter.hpp>
#include <skity/effect/path_effect.hpp>
#include <skity/effect/shader.hpp>
#include <skity/geometry/rrect.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>
#include <skity/io/data.hpp>
#include <skity/text/font.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/text_blob.hpp>

namespace skity {
namespace core_bridge {
inline std::shared_ptr<Data> MakeDataWithCopy(const void* p, size_t n) {
  return Data::MakeWithCopy(p, n);
}
inline std::shared_ptr<Data> MakeDataFromMalloc(void* p, size_t n) {
  return Data::MakeFromMalloc(p, n);
}
inline std::shared_ptr<Image> DecodeDataToImage(const void* bytes,
                                                size_t size) {
  if (!bytes || !size) return nullptr;
  auto data = Data::MakeWithCopy(bytes, size);
  auto codec = Codec::MakeFromData(data);
  if (!codec) return nullptr;
  codec->SetData(data);
  auto pixmap = codec->Decode(DecodeOptions{});
  return pixmap ? Image::MakeImage(pixmap) : nullptr;
}
inline std::shared_ptr<Typeface> MakeTypefaceFromBytes(const void* p,
                                                       size_t n) {
  auto data = Data::MakeWithCopy(p, n);
  if (!data) return nullptr;
  auto manager = FontManager::RefDefault();
  return manager ? manager->MakeFromData(data, 0) : nullptr;
}
inline std::shared_ptr<ColorFilter> MakeColorFilterMatrix(const float* m) {
  return m ? ColorFilters::Matrix(m) : nullptr;
}
inline std::shared_ptr<ColorFilter> MakeColorFilterBlend(uint32_t c,
                                                         BlendMode m) {
  return ColorFilters::Blend(c, m);
}
inline std::shared_ptr<ColorFilter> MakeColorFilterCompose(
    const std::shared_ptr<ColorFilter>& o,
    const std::shared_ptr<ColorFilter>& i) {
  return ColorFilters::Compose(o, i);
}
inline std::shared_ptr<ColorFilter> MakeColorFilterLinearToSRGB() {
  return ColorFilters::LinearToSRGBGamma();
}
inline std::shared_ptr<ColorFilter> MakeColorFilterSRGBToLinear() {
  return ColorFilters::SRGBToLinearGamma();
}
inline std::shared_ptr<ImageFilter> MakeImageFilterBlur(float x, float y) {
  return ImageFilters::Blur(x, y);
}
inline std::shared_ptr<ImageFilter> MakeImageFilterDilate(float x, float y) {
  return ImageFilters::Dilate(x, y);
}
inline std::shared_ptr<ImageFilter> MakeImageFilterErode(float x, float y) {
  return ImageFilters::Erode(x, y);
}
inline std::shared_ptr<ImageFilter> MakeImageFilterColorFilter(
    const std::shared_ptr<ColorFilter>& f) {
  return ImageFilters::ColorFilter(f);
}
inline std::shared_ptr<ImageFilter> MakeImageFilterCompose(
    const std::shared_ptr<ImageFilter>& o,
    const std::shared_ptr<ImageFilter>& i) {
  return ImageFilters::Compose(o, i);
}
inline std::shared_ptr<ImageFilter> MakeImageFilterMatrixTransform(
    const Matrix& m) {
  return ImageFilters::MatrixTransform(m);
}
inline std::shared_ptr<ImageFilter> MakeImageFilterLocalMatrix(
    const std::shared_ptr<ImageFilter>& f, const Matrix& m) {
  return f ? ImageFilters::LocalMatrix(f, m) : nullptr;
}
inline std::shared_ptr<ImageFilter> MakeImageFilterDropShadow(
    float dx, float dy, float sx, float sy, uint32_t c,
    const std::shared_ptr<ImageFilter>& i, const Rect* r) {
  return ImageFilters::DropShadow(dx, dy, sx, sy, c, i, r ? *r : Rect{});
}
inline std::shared_ptr<MaskFilter> MakeMaskFilterBlur(BlurStyle s, float r) {
  return MaskFilter::MakeBlur(s, r);
}
inline std::shared_ptr<MaskFilter> MakeMaskFilterGaussianBlur(float s) {
  return MaskFilter::MakeGaussianBlur(s);
}
inline std::shared_ptr<PathEffect> MakePathEffectDiscrete(float s, float d,
                                                          uint32_t seed) {
  return PathEffect::MakeDiscretePathEffect(s, d, seed);
}
inline std::shared_ptr<PathEffect> MakePathEffectDash(const float* p, int32_t n,
                                                      float phase) {
  return p && n > 0 ? PathEffect::MakeDashPathEffect(p, n, phase) : nullptr;
}
inline std::shared_ptr<Shader> MakeShaderLinear(const Vec4* p, const Vec4* c,
                                                const float* pos, int32_t n,
                                                TileMode t, int32_t f) {
  return Shader::MakeLinear(p, c, pos, n, t, f);
}
inline std::shared_ptr<Shader> MakeShaderRadial(const Vec4& p, float r,
                                                const Vec4* c, const float* pos,
                                                int32_t n, TileMode t,
                                                int32_t f) {
  return Shader::MakeRadial(p, r, c, pos, n, t, f);
}
inline std::shared_ptr<Shader> MakeShaderSweep(float x, float y, float s,
                                               float e, const Vec4* c,
                                               const float* pos, int32_t n,
                                               TileMode t, int32_t f) {
  return Shader::MakeSweep(x, y, s, e, c, pos, n, t, f);
}
inline std::shared_ptr<Shader> MakeShaderTwoPointConical(
    const Vec4& s, float sr, const Vec4& e, float er, const Vec4* c,
    const float* pos, int32_t n, TileMode t, int32_t f) {
  return Shader::MakeTwoPointConical(s, sr, e, er, c, pos, n, t, f);
}
inline std::shared_ptr<Shader> MakeShaderImage(const std::shared_ptr<Image>& i,
                                               const SamplingOptions& s,
                                               TileMode x, TileMode y,
                                               const Matrix& m) {
  return i ? Shader::MakeShader(i, s, x, y, m) : nullptr;
}
inline void SetShaderLocalMatrix(const std::shared_ptr<Shader>& s,
                                 const Matrix& m) {
  if (s) s->SetLocalMatrix(m);
}
inline std::shared_ptr<Paint> MakePaint() { return std::make_shared<Paint>(); }
inline void SetPaintColor(const std::shared_ptr<Paint>& p, uint32_t v) {
  if (p) p->SetColor(v);
}
inline void SetPaintStyle(const std::shared_ptr<Paint>& p, Paint::Style v) {
  if (p) p->SetStyle(v);
}
inline void SetPaintStrokeWidth(const std::shared_ptr<Paint>& p, float v) {
  if (p) p->SetStrokeWidth(v);
}
inline void SetPaintStrokeMiter(const std::shared_ptr<Paint>& p, float v) {
  if (p) p->SetStrokeMiter(v);
}
inline void SetPaintStrokeCap(const std::shared_ptr<Paint>& p, Paint::Cap v) {
  if (p) p->SetStrokeCap(v);
}
inline void SetPaintStrokeJoin(const std::shared_ptr<Paint>& p, Paint::Join v) {
  if (p) p->SetStrokeJoin(v);
}
inline void SetPaintAntiAlias(const std::shared_ptr<Paint>& p, bool v) {
  if (p) p->SetAntiAlias(v);
}
inline void SetPaintPathEffect(const std::shared_ptr<Paint>& p,
                               const std::shared_ptr<PathEffect>& v) {
  if (p) p->SetPathEffect(v);
}
inline void SetPaintShader(const std::shared_ptr<Paint>& p,
                           const std::shared_ptr<Shader>& v) {
  if (p) p->SetShader(v);
}
inline void SetPaintColorFilter(const std::shared_ptr<Paint>& p,
                                const std::shared_ptr<ColorFilter>& v) {
  if (p) p->SetColorFilter(v);
}
inline void SetPaintImageFilter(const std::shared_ptr<Paint>& p,
                                const std::shared_ptr<ImageFilter>& v) {
  if (p) p->SetImageFilter(v);
}
inline void SetPaintMaskFilter(const std::shared_ptr<Paint>& p,
                               const std::shared_ptr<MaskFilter>& v) {
  if (p) p->SetMaskFilter(v);
}
class PathBuilder {
 public:
  PathBuilder() : path_(std::make_shared<Path>()) {}
  PathBuilder(const PathBuilder&) = delete;
  PathBuilder& operator=(const PathBuilder&) = delete;
  void MoveTo(float x, float y) { path_->MoveTo(x, y); }
  void LineTo(float x, float y) { path_->LineTo(x, y); }
  void QuadTo(float a, float b, float c, float d) { path_->QuadTo(a, b, c, d); }
  void ConicTo(float a, float b, float c, float d, float w) {
    path_->ConicTo(a, b, c, d, w);
  }
  void CubicTo(float a, float b, float c, float d, float e, float f) {
    path_->CubicTo(a, b, c, d, e, f);
  }
  void Close() { path_->Close(); }
  void ArcTo(float rx, float ry, float rotate, Path::ArcSize large,
             Path::Direction sweep, float x, float y) {
    path_->ArcTo(rx, ry, rotate, large, sweep, x, y);
  }
  void AddRRect(const Rect& r, const Vec2 radii[4], Path::Direction d) {
    path_->AddRoundRect(r, reinterpret_cast<const float*>(radii), d);
  }
  void AddRRectStart(const Rect& r, const Vec2 radii[4], Path::Direction d,
                     int32_t start) {
    RRect rr;
    rr.SetRectRadii(r, radii);
    path_->AddRRect(rr, d, start);
  }
  void SetFillType(Path::PathFillType t) { path_->SetFillType(t); }
  std::shared_ptr<Path> Peek() const { return path_; }
  std::shared_ptr<Path> Take() { return std::move(path_); }

 private:
  std::shared_ptr<Path> path_;
};
inline std::shared_ptr<Font> MakeFont() { return std::make_shared<Font>(); }
inline void SetFontSize(const std::shared_ptr<Font>& f, float size) {
  if (f) f->SetSize(size);
}
struct TextRunData {
  std::shared_ptr<Font> font;
  std::vector<uint16_t> glyphs;
  std::vector<float> pos_x;
  std::vector<float> pos_y;
};
inline std::shared_ptr<TextBlob> MakeTextBlobFromRuns(
    const std::vector<TextRunData>& runs) {
  if (runs.empty()) return nullptr;
  std::vector<TextRun> result;
  result.reserve(runs.size());
  for (const auto& r : runs) {
    if (!r.font || (!r.pos_y.empty() && r.pos_x.empty())) return nullptr;
    if (!r.pos_y.empty())
      result.emplace_back(*r.font, r.glyphs, r.pos_x, r.pos_y);
    else if (!r.pos_x.empty())
      result.emplace_back(*r.font, r.glyphs, r.pos_x);
    else
      result.emplace_back(*r.font, r.glyphs);
  }
  return std::make_shared<TextBlob>(std::move(result));
}
inline Color4f GetPaintColor4f(const Paint* p) {
  return p ? p->GetColor4f() : Color4f{};
}
inline Paint::Cap GetPaintStrokeCap(const Paint* p) {
  return p ? p->GetStrokeCap() : Paint::Cap::kButt_Cap;
}
inline Paint::Join GetPaintStrokeJoin(const Paint* p) {
  return p ? p->GetStrokeJoin() : Paint::Join::kMiter_Join;
}
inline float GetPaintStrokeMiter(const Paint* p) {
  return p ? p->GetStrokeMiter() : 4.f;
}
inline float GetPaintStrokeWidth(const Paint* p) {
  return p ? p->GetStrokeWidth() : 0.f;
}
inline Paint::Style GetPaintStyle(const Paint* p) {
  return p ? p->GetStyle() : Paint::Style::kFill_Style;
}
inline uint32_t GetPaintColor(const Paint* p) {
  return p ? p->GetColor() : 0xFF000000u;
}
inline BlendMode GetPaintBlendMode(const Paint* p) {
  return p ? p->GetBlendMode() : BlendMode::kSrcOver;
}
inline bool IsPaintAntiAlias(const Paint* p) { return p && p->IsAntiAlias(); }
inline Rect GetTextBlobBounds(const TextBlob* b) {
  return b ? b->GetBoundsRect() : Rect{};
}
inline int32_t GetTextBlobRunCount(const TextBlob* b) {
  return b ? b->GetTextRun().size() : 0;
}
inline Rect GetPathBounds(const Path* p) { return p ? p->GetBounds() : Rect{}; }
inline std::vector<TextRunData> ReadTextBlobRuns(const TextBlob* b) {
  std::vector<TextRunData> out;
  if (b)
    for (const auto& r : b->GetTextRun())
      out.push_back({std::make_shared<Font>(r.GetFont()), r.GetGlyphInfo(),
                     r.GetPosX(), r.GetPosY()});
  return out;
}
inline void SetPaintBlendMode(const std::shared_ptr<Paint>& p, BlendMode m) {
  if (p) p->SetBlendMode(m);
}
}  // namespace core_bridge
}  // namespace skity
#endif
