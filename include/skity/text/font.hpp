// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_TEXT_FONT_HPP
#define INCLUDE_SKITY_TEXT_FONT_HPP

#include <cstddef>
#include <skity/geometry/rect.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/macros.hpp>
#include <skity/text/font_metrics.hpp>
#include <skity/text/typeface.hpp>
#include <type_traits>

namespace skity {

class Paint;

template <typename T>
static constexpr bool ToBool(const T& x) {
  return static_cast<bool>(x);
}

class SKITY_API Font {
 public:
  enum class FontHinting {
    kNone,    //!< glyph outlines unchanged
    kSlight,  //!< minimal modification to improve constrast
    kNormal,  //!< glyph outlines modified to improve constrast
    kFull,    //!< modifies glyph outlines for maximum constrast
  };

  /** Whether edge pixels draw opaque or with partial transparency.
   */
  enum class Edging {
    kAlias,              //!< no transparent pixels on glyph edges
    kAntiAlias,          //!< may have transparent pixels on glyph edges
    kSubpixelAntiAlias,  //!< glyph positioned in pixel using transparency
  };

  Font();

  Font(std::shared_ptr<Typeface> typeface, float size);

  explicit Font(std::shared_ptr<Typeface> typeface);

  Font(std::shared_ptr<Typeface> typeface, float size, float scaleX,
       float skewX);

  bool IsForceAutoHinting() const {
    return ToBool(flags_ & kForceAutoHinting_PrivFlag);
  }
  void SetForceAutoHinting(bool forceAutoHinting);

  bool IsEmbeddedBitmaps() const {
    return ToBool(flags_ & kEmbeddedBitmaps_PrivFlag);
  }
  void SetEmbeddedBitmaps(bool embeddedBitmaps);

  bool IsSubpixel() const { return ToBool(flags_ & kSubpixel_PrivFlag); }
  void SetSubpixel(bool subpixel);

  bool IsLinearMetrics() const {
    return ToBool(flags_ & kLinearMetrics_PrivFlag);
  }
  void SetLinearMetrics(bool linearMetrics);

  bool IsEmbolden() const { return ToBool(flags_ & kEmbolden_PrivFlag); }
  void SetEmbolden(bool embolden);

  bool IsBaselineSnap() const {
    return ToBool(flags_ & kBaselineSnap_PrivFlag);
  }
  void SetBaselineSnap(bool baselineSnap);

  Edging GetEdging() const { return (Edging)edging_; }
  void SetEdging(Edging edging) { edging_ = (uint8_t)edging; }

  FontHinting GetHinting() const { return (FontHinting)hinting_; }
  void SetHinting(FontHinting hintingLevel) {
    hinting_ = (uint8_t)hintingLevel;
  }

  float GetSize() const { return size_; }
  void SetSize(float textSize);

  float GetScaleX() const { return scale_x_; }
  void SetScaleX(float scaleX) { scale_x_ = scaleX; }

  float GetSkewX() const { return skew_x_; }
  void SetSkewX(float skewX) { skew_x_ = skewX; }

  Font MakeWithSize(float size) const;

  void SetTypeface(std::shared_ptr<Typeface> tf) { typeface_ = std::move(tf); }
  std::shared_ptr<Typeface> GetTypeface() const { return typeface_; }
  std::shared_ptr<Typeface> GetTypefaceOrDefault() const;

  void GetWidths(const GlyphID glyphs[], int count, float widths[],
                 Rect bounds[]) const {
    this->GetWidthsBounds(glyphs, count, widths, bounds, Paint());
  }

  void GetWidths(const GlyphID glyphs[], int count, float widths[]) const {
    this->GetWidthsBounds(glyphs, count, widths, nullptr, Paint());
  }

  void GetWidthsBounds(const GlyphID glyphs[], int count, float widths[],
                       Rect bounds[], const Paint& paint) const;

  void GetMetrics(FontMetrics* metrics) const;

  //  SkScalar getSpacing() const { return this->getSpacing(); }

  void LoadGlyphMetrics(const GlyphID glyphs[], uint32_t count,
                        const GlyphData* glyph_data[],
                        const Paint& paint = Paint()) const;
  // Cache-backed borrowed views; use VisitGlyphPaths to retain source owners
  // while consuming paths across cache eviction or purge.
  void LoadGlyphPath(const GlyphID* glyphs, uint32_t count,
                     const GlyphData* glyph_data[]) const;

  // Visits canonical glyph paths while retaining their scaler and typeface.
  // scratch must have count entries. Its pointers are borrowed only during the
  // synchronous visitor and are cleared before return, including unwinding.
  // No callback runs for count == 0. Otherwise null arguments are refused.
  // The callback runs outside cache/container locks and may purge the cache.
  // Serialize mutation/preparation of the borrowed glyphs until it returns;
  // retaining a Font or a const pointer alone does not synchronize glyph data.
  using GlyphPathVisitor = bool (*)(const GlyphData* const glyph_data[],
                                    uint32_t count, const void* context);
  bool VisitGlyphPaths(const GlyphID* glyphs, uint32_t count,
                       const GlyphData* scratch[], GlyphPathVisitor visitor,
                       const void* context) const;

  template <typename Visitor>
  bool VisitGlyphPaths(const GlyphID* glyphs, uint32_t count,
                       const GlyphData* scratch[],
                       const Visitor& visitor) const {
    if constexpr (std::is_pointer_v<Visitor>) {
      if (!visitor) {
        return VisitGlyphPaths(glyphs, count, scratch, nullptr, nullptr);
      }
    }
    // The address of a function reference is not an object pointer. Wrap the
    // callable reference on the stack so both function and object visitors use
    // a valid erased context without allocating or extending its lifetime.
    struct Context {
      const Visitor& callback;
    } context{visitor};
    return VisitGlyphPaths(
        glyphs, count, scratch,
        [](const GlyphData* const data[], uint32_t size, const void* context) {
          return static_cast<const Context*>(context)->callback(data, size);
        },
        &context);
  }
  // Loads one glyph into a thread-local snapshot. The returned glyph and pixels
  // remain valid until the next LoadGlyphBitmap call on the same thread. They
  // must not be freed by the caller (Image().need_free is false).
  void LoadGlyphBitmap(const GlyphID* glyphs, uint32_t count,
                       const GlyphData* glyph_data[], const Paint& paint,
                       float context_scale, const Matrix& transform) const;

  using GlyphBitmapVisitor = bool (*)(const GlyphData&,
                                      std::size_t readable_bytes,
                                      const void* context);
  // Visit one request-local owned bitmap without borrowing a cache/TLS entry.
  // The actual face, scaler container, glyph and readable pixel span remain
  // alive through the callback, including reentrant loads or cache purges.
  // No scaler lock is held during the callback. Do not retain borrowed views
  // or mutate the pixels; copy them before returning. False propagates refusal.
  // Empty bitmaps visit with zero bytes/null pixels; whitespace may have
  // backend raster padding. Invalid storage, missing pixels for nonempty
  // glyphs, zero allowance, null visitor or spans above maximum_readable_bytes
  // refuse without calling the visitor. The allowance bounds the delivered
  // snapshot, not prior native raster scratch or font/cache allocation. It is
  // not a retained-storage certificate. Callback exception behavior is
  // unchanged from VisitGlyphPaths and depends on the selected C++ build
  // profile.
  bool VisitGlyphBitmap(GlyphID glyph, const Paint& paint, float context_scale,
                        const Matrix& transform,
                        std::size_t maximum_readable_bytes,
                        GlyphBitmapVisitor visitor, const void* context) const;
  // Original renderer raster inputs. Packed phases are quarter-pixel values
  // 0..3. native_raster_phase is 0 for ordinary rasterization or 1..4 for
  // Apple's native unit-scale grayscale phases 0..3/4. These are source
  // requests, not glyph placement, alpha conversion or retained byte charges.
  struct GlyphBitmapRasterOptions {
    uint8_t subpixel_x_phase = 0;
    uint8_t subpixel_y_phase = 0;
    uint8_t native_raster_phase = 0;
  };
  // Same scoped ownership/span/exception contract as VisitGlyphBitmap.
  // Invalid phases and unsupported explicit native requests refuse instead of
  // silently selecting another raster. Native requests require unit density,
  // identity linear transform, no perspective and the existing unstyled,
  // noncolor native-gray font/paint policy. Translation is not rasterized.
  // VisitGlyphBitmap keeps its exact ordinary phase-zero behavior.
  bool VisitGlyphBitmapRaster(GlyphID glyph, GlyphBitmapRasterOptions options,
                              const Paint& paint, float context_scale,
                              const Matrix& transform,
                              std::size_t maximum_readable_bytes,
                              GlyphBitmapVisitor visitor,
                              const void* context) const;
  void LoadGlyphBitmapInfo(const GlyphID* glyphs, uint32_t count,
                           const GlyphData* glyph_data[], const Paint& paint,
                           float context_scale, const Matrix& transform) const;
#if defined(__APPLE__)
  // Native unit-scale alpha bitmap at one of four horizontal quarter-pixel
  // phases. The caller places its integer origin; color/stroke/transform and
  // invalid phase inputs retain LoadGlyphBitmap behavior. No Font layout
  // change.
  void LoadGlyphBitmapAtPhase(const GlyphID* glyphs, uint32_t count,
                              const GlyphData* glyph_data[], const Paint& paint,
                              uint32_t quarter_pixel_phase) const;
#endif
  uint16_t GetFixedSize() const;

 private:
  mutable std::shared_ptr<Typeface> typeface_;
  float size_;
  float scale_x_;
  float skew_x_;

  uint8_t flags_{};
  uint8_t edging_{};
  uint8_t hinting_ = static_cast<uint8_t>(Font::FontHinting::kNormal);

  enum PrivFlags {
    kForceAutoHinting_PrivFlag = 1 << 0,
    kEmbeddedBitmaps_PrivFlag = 1 << 1,
    kSubpixel_PrivFlag = 1 << 2,
    kLinearMetrics_PrivFlag = 1 << 3,
    kEmbolden_PrivFlag = 1 << 4,
    kBaselineSnap_PrivFlag = 1 << 5,
  };

  static constexpr unsigned kAllFlags =
      kForceAutoHinting_PrivFlag | kEmbeddedBitmaps_PrivFlag |
      kSubpixel_PrivFlag | kLinearMetrics_PrivFlag | kEmbolden_PrivFlag |
      kBaselineSnap_PrivFlag;
};
}  // namespace skity

#endif  // INCLUDE_SKITY_TEXT_FONT_HPP
