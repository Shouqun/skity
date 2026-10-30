// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_CANVAS_STATE_HPP
#define SRC_RENDER_CANVAS_STATE_HPP

#include <skity/geometry/matrix.hpp>
#include <skity/graphic/paint.hpp>
#include <vector>
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
#include <stdexcept>
#endif

namespace skity {

#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
// Reserve without changing logical state. Keep ordinary vector growth and
// reject an impossible append before a backend can publish a save.
template <typename T>
void ReserveRecoveryAppend(std::vector<T>& values) {
  if (values.size() < values.capacity()) return;
  if (values.size() == values.max_size())
    throw std::length_error("Canvas state capacity exhausted");
  const auto capacity = values.capacity();
  const auto grown = capacity > values.max_size() / 2
                         ? values.max_size()
                         : (capacity ? capacity * 2 : 1);
  values.reserve(grown);
}
#endif

class LayerState {
 public:
  explicit LayerState(const Matrix& world_matrix)
      : world_matrix_(world_matrix) {
    elements_.emplace_back(Matrix{}, world_matrix);
  }
  struct Element {
    Element(const Matrix& local_matrix, const Matrix& total_matrix)
        : local_matrix(local_matrix), total_matrix(total_matrix) {}
    // local_matrix maps canvas-local coordinates to the active layer, while
    // total_matrix maps them to the root canvas. When world_matrix_ is
    // invertible, total_matrix == world_matrix_ * local_matrix. For a singular
    // world_matrix_, total_matrix remains the authoritative canvas transform.
    Matrix local_matrix;
    Matrix total_matrix;
  };
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  LayerState(const LayerState&) = default;
  LayerState(LayerState&&) noexcept = default;
  void PrepareSave() { ReserveRecoveryAppend(elements_); }
#endif
  void Save();
  void Restore();

  bool CanRestore() { return elements_.size() > 1; }

  void Translate(float dx, float dy);
  void Scale(float dx, float dy);
  void Rotate(float degree);
  void Rotate(float degree, float px, float py);
  void Skew(float sx, float sy);
  void Concat(Matrix const& matrix);
  void SetMatrix(Matrix const& matrix);
  void ResetMatrix();

  const Matrix& GetWorldMatrix() const { return world_matrix_; }
  const Matrix& CurrentMatrix() const { return CurrentElement().local_matrix; }

  Matrix GetTotalMatrix() const { return CurrentElement().total_matrix; }

 private:
  Element& CurrentElement() { return elements_.back(); }
  const Element& CurrentElement() const { return elements_.back(); }

  std::vector<Element> elements_;
  Matrix world_matrix_;  // layer to world
};

class CanvasState {
 public:
  CanvasState() { PushLayer(); }

  void Save() { CurrentLayerState().Save(); }
  void SaveLayer(const Rect& bounds, const Paint& paint) {
    if (layer_fallback_) {
      layer_fallback_ = false;
      Save();
      return;
    }
    PushLayer();
  }
#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
  LayerState PrepareLayerSave() {
    const auto world = CurrentLayerState().GetTotalMatrix();
    ReserveRecoveryAppend(layer_states_);
    CurrentLayerState().PrepareSave();
    return LayerState(world);
  }
  void CommitLayerSave(LayerState prepared) noexcept {
    if (layer_fallback_) {
      layer_fallback_ = false;
      CurrentLayerState().Save();
    } else {
      layer_states_.emplace_back(std::move(prepared));
    }
  }
#endif
  void MarkLayerFallback() { layer_fallback_ = true; }
  void Restore() {
    if (CurrentLayerState().CanRestore()) {
      CurrentLayerState().Restore();
      return;
    }
    PopLayer();
  }

  const Matrix GetTotalMatrix() const {
    return CurrentLayerState().GetTotalMatrix();
  }

  const Matrix& CurrentLayerMatrix() const {
    return CurrentLayerState().CurrentMatrix();
  }

  void Translate(float dx, float dy) { CurrentLayerState().Translate(dx, dy); }
  void Scale(float dx, float dy) { CurrentLayerState().Scale(dx, dy); }
  void Rotate(float degree) { CurrentLayerState().Rotate(degree); }
  void Rotate(float degree, float px, float py) {
    CurrentLayerState().Rotate(degree, px, py);
  }
  void Skew(float sx, float sy) { CurrentLayerState().Skew(sx, sy); }
  void Concat(Matrix const& matrix) { CurrentLayerState().Concat(matrix); }
  void SetMatrix(Matrix const& matrix) {
    CurrentLayerState().SetMatrix(matrix);
  }
  void ResetMatrix() { CurrentLayerState().ResetMatrix(); }

  LayerState& CurrentLayerState() const {
    return const_cast<LayerState&>(layer_states_.back());
  }

 private:
  void PushLayer() {
    auto world_matrix = layer_states_.empty()
                            ? Matrix{}
                            : layer_states_.back().GetTotalMatrix();
    layer_states_.emplace_back(world_matrix);
  }
  void PopLayer() { layer_states_.pop_back(); }
  std::vector<LayerState> layer_states_;
  // One-shot flag set when the renderer falls back from SaveLayer to Save.
  // It is consumed by the immediately following SaveLayer().
  bool layer_fallback_ = false;
};

}  // namespace skity

#endif  // SRC_RENDER_CANVAS_STATE_HPP
