// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_FILTERS_HW_BLUR_FILTER_HPP
#define SRC_RENDER_HW_FILTERS_HW_BLUR_FILTER_HPP

#include <memory>
#include <skity/effect/mask_filter.hpp>

#include "src/gpu/gpu_render_pass.hpp"
#include "src/render/hw/filters/hw_filter.hpp"

namespace skity {

class HWBlurFilter : public HWFilter {
 public:
  HWBlurFilter(float radius, Vec2 direction, std::shared_ptr<HWFilter> inputs,
               float downsample_scale = 1.f,
               BlurType blur_type = BlurType::kRadius, float sigma = 0.f)
      : HWFilter({inputs}, "HWBlurFilter"),
        radius_(radius),
        direction_(direction),
        downsample_scale_(downsample_scale),
        blur_type_(blur_type),
        sigma_(sigma) {}

  // For explicit Gaussian blur, radius is support and sigma sets the weights.
  float radius_;
  Vec2 direction_;
  float downsample_scale_;
  BlurType blur_type_;
  float sigma_;

  HWFilterOutput Prepare(const HWFilterContext &context) override;

 private:
  void PrepareWGXCMD(Command *cmd, HWDrawContext *context,
                     const std::shared_ptr<GPUTexture> &texture,
                     const std::shared_ptr<GPUTexture> &output_texture,
                     const Vec2 &dir, float radius, float sigma, Vec2 uv_scale,
                     Vec2 uv_offset);
};

}  // namespace skity

#endif  // SRC_RENDER_HW_FILTERS_HW_BLUR_FILTER_HPP
