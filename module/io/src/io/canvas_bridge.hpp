// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_SRC_IO_CANVAS_BRIDGE_HPP
#define MODULE_IO_SRC_IO_CANVAS_BRIDGE_HPP
#include <skity/render/canvas.hpp>
#if !defined(SKITY_CPP_ALLOCATION_RECOVERY)
#include <skity/c/skity_canvas.h>
#endif
namespace skity {
namespace replay_bridge {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
using CanvasRef = Canvas*;
using RectValue = Rect;
using MatrixValue = Matrix;
using SamplingValue = SamplingOptions;
using ClipOp = Canvas::ClipOp;
using BlendValue = BlendMode;
using PathRef = const Path*;
using PaintRef = const Paint*;
using ImageRef = std::shared_ptr<Image>;
using BlobRef = std::shared_ptr<TextBlob>;
constexpr auto kIntersect = Canvas::ClipOp::kIntersect;
constexpr auto kDifference = Canvas::ClipOp::kDifference;
constexpr auto kSrc = BlendMode::kSrc;
#else
using CanvasRef = skity_canvas;
using RectValue = skity_rect;
using MatrixValue = skity_matrix;
using SamplingValue = skity_sampling_options;
using ClipOp = skity_clip_op;
using BlendValue = skity_blend_mode;
using PathRef = skity_path;
using PaintRef = skity_paint;
using ImageRef = skity_image;
using BlobRef = skity_text_blob;
constexpr auto kIntersect = SKITY_CLIP_OP_INTERSECT;
constexpr auto kDifference = SKITY_CLIP_OP_DIFFERENCE;
constexpr auto kSrc = SKITY_BLEND_MODE_SRC;
#endif
inline int32_t Save(CanvasRef c) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  return c ? c->Save() : 0;
#else
  return skity_canvas_save(c);
#endif
}
inline void Restore(CanvasRef c) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c) c->Restore();
#else
  skity_canvas_restore(c);
#endif
}
inline void RestoreToCount(CanvasRef c, int32_t n) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c) c->RestoreToCount(n);
#else
  skity_canvas_restore_to_count(c, n);
#endif
}
inline void GetTotalMatrix(CanvasRef c, MatrixValue* out) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c && out) *out = c->GetTotalMatrix();
#else
  skity_canvas_get_total_matrix(c, out);
#endif
}
inline void GetGlobalClipBounds(CanvasRef c, RectValue* out) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c && out) *out = c->GetGlobalClipBounds();
#else
  skity_canvas_get_global_clip_bounds(c, out);
#endif
}
inline void Concat(CanvasRef c, const MatrixValue* m) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c && m) c->Concat(*m);
#else
  skity_canvas_concat(c, m);
#endif
}
inline void SetMatrix(CanvasRef c, const MatrixValue* m) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c && m) c->SetMatrix(*m);
#else
  skity_canvas_set_matrix(c, m);
#endif
}
inline void ClipPath(CanvasRef c, PathRef p, ClipOp op) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c && p) c->ClipPath(*p, op);
#else
  skity_canvas_clip_path(c, p, op);
#endif
}
inline void ClipRect(CanvasRef c, const RectValue* r, ClipOp op) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c && r) c->ClipRect(*r, op);
#else
  skity_canvas_clip_rect(c, r, op);
#endif
}
inline void DrawColor(CanvasRef c, uint32_t color, BlendValue mode) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c) c->DrawColor(color, mode);
#else
  skity_canvas_draw_color(c, color, mode);
#endif
}
inline void DrawPath(CanvasRef c, PathRef p, PaintRef paint) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c && p && paint) c->DrawPath(*p, *paint);
#else
  skity_canvas_draw_path(c, p, paint);
#endif
}
inline void DrawPaint(CanvasRef c, PaintRef paint) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c && paint) c->DrawPaint(*paint);
#else
  skity_canvas_draw_paint(c, paint);
#endif
}
inline void DrawRect(CanvasRef c, const RectValue* r, PaintRef paint) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c && r && paint) c->DrawRect(*r, *paint);
#else
  skity_canvas_draw_rect(c, r, paint);
#endif
}
inline void DrawOval(CanvasRef c, const RectValue* r, PaintRef paint) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c && r && paint) c->DrawOval(*r, *paint);
#else
  skity_canvas_draw_oval(c, r, paint);
#endif
}
inline void DrawArc(CanvasRef c, const RectValue* r, float start, float sweep,
                    uint32_t center, PaintRef paint) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c && r && paint) c->DrawArc(*r, start, sweep, center != 0, *paint);
#else
  skity_canvas_draw_arc(c, r, start, sweep, center, paint);
#endif
}
inline void DrawImageWithSampling(CanvasRef c, ImageRef image, float x, float y,
                                  const SamplingValue* s, PaintRef p) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c && image) c->DrawImage(image, x, y, s ? *s : SamplingOptions{}, p);
#else
  skity_canvas_draw_image_with_sampling(c, image, x, y, s, p);
#endif
}
inline void DrawImageRect(CanvasRef c, ImageRef image, const RectValue* src,
                          const RectValue* dst, const SamplingValue* s,
                          PaintRef p) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c && image && src && dst)
    c->DrawImageRect(image, *src, *dst, s ? *s : SamplingOptions{}, p);
#else
  skity_canvas_draw_image_rect(c, image, src, dst, s, p);
#endif
}
inline void DrawTextBlob(CanvasRef c, BlobRef blob, float x, float y,
                         PaintRef p) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c && blob && p) c->DrawTextBlob(blob, x, y, *p);
#else
  skity_canvas_draw_text_blob(c, blob, x, y, p);
#endif
}
inline int32_t SaveLayer(CanvasRef c, const RectValue* r, PaintRef p) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  return c && p ? c->SaveLayer(r ? *r : Rect{}, *p) : 0;
#else
  return skity_canvas_save_layer(c, r, p);
#endif
}
inline void Translate(CanvasRef c, float x, float y) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c) c->Translate(x, y);
#else
  skity_canvas_translate(c, x, y);
#endif
}
inline void Scale(CanvasRef c, float x, float y) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c) c->Scale(x, y);
#else
  skity_canvas_scale(c, x, y);
#endif
}
inline void Skew(CanvasRef c, float x, float y) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c) c->Skew(x, y);
#else
  skity_canvas_skew(c, x, y);
#endif
}
inline void Rotate(CanvasRef c, float d) {
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  if (c) c->Rotate(d);
#else
  skity_canvas_rotate(c, d);
#endif
}
}  // namespace replay_bridge
}  // namespace skity
#endif
