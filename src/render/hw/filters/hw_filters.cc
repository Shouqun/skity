// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/filters/hw_filters.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <skity/effect/color_filter.hpp>
#include <skity/effect/image_filter.hpp>
#include <skity/effect/mask_filter.hpp>

#include "src/effect/image_filter_base.hpp"
#include "src/render/hw/filters/hw_down_sampler_filter.hpp"
#include "src/render/hw/filters/hw_matrix_filter.hpp"
#include "src/render/hw/filters/hw_merge_filter.hpp"

namespace skity {

namespace {

float radial_to_sigma(float radius) {
  return radius > 0.f ? radius * 0.57735f + 0.5f : 0.f;
}

float sigma_to_radius(float sigma) {
  return sigma > 0.5f ? (sigma - 0.5f) / 0.57735f : 0.0f;
}

int scale_blur_radius(float radius, float scalar) {
  return static_cast<int>(std::round(radius * scalar));
}

/** This value was extracted from Skia, see:
 * https://github.com/google/skia/blob/d29cc3fe182f6e8a8539004a6a4ee8251677a6fd/src/gpu/ganesh/GrBlurUtils.cpp#L2561-L2576
 * https://github.com/google/skia/blob/d29cc3fe182f6e8a8539004a6a4ee8251677a6fd/src/gpu/BlurUtils.h#L57
 *
 * We did some changes:
 * 1. Skia use kMaxBlurSigma to determin if need to use a single pass blur or
 * two pass blur(vertical and horizontal).
 * 2. We use this value to calculate the downsampler scale.
 *
 */
float calculate_blur_scale(float sigma, BlurType blur_type,
                           float support_radius) {
  static constexpr float kMaxBlurSigma = 16.f;
  if (sigma <= kMaxBlurSigma) {
    return 1.0;
  }
  float raw_result = kMaxBlurSigma / sigma;
  // Round to the nearest 1/(2^n) to get the best quality down scaling.
  float exponent = round(log2f(raw_result));
  // Don't scale down below 1/16th to preserve signal.
  exponent = std::max(-4.0f, exponent);
  float rounded = powf(2.0f, exponent);
  float result = rounded;
  // Extend the range of the 1/8th downsample based on the effective kernel size
  // for the blur.
  if (rounded < 0.125f) {
    float rounded_plus = powf(2.0f, exponent + 1);
    // This constant was picked by looking at the results to make sure no
    // shimmering was introduced at the highest sigma values that downscale to
    // 1/16th.
    static constexpr int32_t kEighthDownsampleKernalWidthMax = 41;
    bool use_rounded_plus;
    if (blur_type == BlurType::kGaussianSigma) {
      // Compare support without narrowing a potentially large finite radius.
      float kernel_radius_plus = std::ceil(support_radius * rounded_plus);
      use_rounded_plus =
          kernel_radius_plus <= (kEighthDownsampleKernalWidthMax - 1) / 2;
    } else {
      float blur_radius = sigma_to_radius(sigma);
      int kernel_size_plus =
          (scale_blur_radius(blur_radius, rounded_plus) * 2) + 1;
      use_rounded_plus =
          kernel_size_plus <= kEighthDownsampleKernalWidthMax;
    }
    result = use_rounded_plus ? rounded_plus : rounded;
  }
  return result;
}

}  // namespace

bool HWFilters::GaussianBlurIsRepresentable(float sigma,
                                            const Rect& layer_bounds,
                                            Vec2 scale,
                                            uint32_t max_texture_size) {
  const float radius = 3.f * sigma;
  if (!(sigma > 0.f) || !std::isfinite(sigma) || !std::isfinite(radius) ||
      !layer_bounds.IsFinite() || layer_bounds.IsEmpty() ||
      !layer_bounds.MakeOutset(radius, radius).IsFinite() ||
      !(scale.x > 0.f) || !(scale.y > 0.f) || !std::isfinite(scale.x) ||
      !std::isfinite(scale.y) || max_texture_size == 0) {
    return false;
  }

  const double max_dimension =
      std::min(static_cast<double>(max_texture_size),
               static_cast<double>(std::numeric_limits<uint32_t>::max()));
  const auto valid_dimension = [max_dimension](double value) {
    return std::isfinite(value) && value > 0.0 &&
           std::ceil(value) <= max_dimension;
  };

  // GenLayer rounds the source size; downsampling and filter outputs round
  // outward. Keep the floating operations in the same order as those owners.
  float width = std::round(layer_bounds.Width() * scale.x);
  float height = std::round(layer_bounds.Height() * scale.y);
  if (!valid_dimension(width) || !valid_dimension(height)) {
    return false;
  }

  const float scaled_radius_x = radius * scale.x;
  const float scaled_radius_y = radius * scale.y;
  const float scaled_sigma_x = sigma * scale.x;
  const float scaled_sigma_y = sigma * scale.y;
  if (!std::isfinite(scaled_radius_x) || !std::isfinite(scaled_radius_y) ||
      !std::isfinite(scaled_sigma_x) || !std::isfinite(scaled_sigma_y)) {
    return false;
  }
  const float downsample_scale = calculate_blur_scale(
      std::max(scaled_sigma_x, scaled_sigma_y), BlurType::kGaussianSigma,
      std::max(scaled_radius_x, scaled_radius_y));
  if (downsample_scale != 1.f) {
    width = std::ceil(width * downsample_scale);
    height = std::ceil(height * downsample_scale);
    if (!valid_dimension(width) || !valid_dimension(height)) {
      return false;
    }
  }

  const float horizontal_radius =
      std::ceil(3.f * (sigma * (width / layer_bounds.Width())));
  const float vertical_radius =
      std::ceil(3.f * (sigma * (height / layer_bounds.Height())));
  const double max_kernel =
      static_cast<double>(std::numeric_limits<int32_t>::max()) - 1.0;
  if (!std::isfinite(horizontal_radius) || !std::isfinite(vertical_radius) ||
      std::ceil(static_cast<double>(horizontal_radius)) > max_kernel ||
      std::ceil(static_cast<double>(vertical_radius)) > max_kernel) {
    return false;
  }

  width = std::ceil(width + 2.f * horizontal_radius);
  if (!valid_dimension(width)) {
    return false;
  }
  height = std::ceil(height + 2.f * vertical_radius);
  return valid_dimension(height);
}

std::shared_ptr<HWFilter> HWFilters::HandleMaskFilter(
    std::shared_ptr<HWFilter> input, std::shared_ptr<MaskFilter> mask_filter,
    Vec2 scale) {
  auto radius = mask_filter->GetBlurRadius();
  if (mask_filter->GetBlurType() == BlurType::kGaussianSigma) {
    auto sigma = mask_filter->GetBlurSigma();
    return HWFilters::Blur(radius, radius, scale, input,
                           BlurType::kGaussianSigma, Vec2{sigma});
  }
  return HWFilters::Blur(radius, radius, scale, input);
}
std::shared_ptr<HWFilter> HWFilters::HandleColorFilter(
    std::shared_ptr<HWFilter> input,
    std::shared_ptr<skity::ColorFilter> color_filter) {
  return HWFilters::ColorFilter(color_filter, input);
}

std::shared_ptr<HWFilter> HWFilters::HandleImageFilter(
    std::shared_ptr<HWFilter> input, std::shared_ptr<ImageFilter> image_filter,
    Vec2 scale) {
  auto image_filter_type =
      static_cast<ImageFilterBase*>(image_filter.get())->GetType();
  switch (image_filter_type) {
    case ImageFilterType::kBlur: {
      BlurImageFilter* blur_image_filter =
          static_cast<BlurImageFilter*>(image_filter.get());
      return HWFilters::Blur(blur_image_filter->GetRadiusX(),
                             blur_image_filter->GetRadiusY(), scale, input);
    }
    case ImageFilterType::kColorFilter: {
      auto cf = static_cast<ColorFilterImageFilter*>(image_filter.get())
                    ->GetColorFilter();
      return HWFilters::ColorFilter(cf, input);
    }
    case ImageFilterType::kMatix: {
      auto matrix =
          static_cast<MatrixImageFilter*>(image_filter.get())->GetMatrix();
      return HWFilters::Matrix(matrix, input);
    }
    case ImageFilterType::kDropShadow: {
      DropShadowImageFilter* drop_shadow_image_filter =
          static_cast<DropShadowImageFilter*>(image_filter.get());

      auto radius_x = drop_shadow_image_filter->GetRadiusX();
      auto radius_y = drop_shadow_image_filter->GetRadiusY();
      auto offset_x = drop_shadow_image_filter->GetOffsetX();
      auto offset_y = drop_shadow_image_filter->GetOffsetY();
      auto color = drop_shadow_image_filter->GetColor();
      return HWFilters::DropShadow(radius_x, radius_y, offset_x, offset_y,
                                   scale, color, input);
    }
    case ImageFilterType::kCompose: {
      ComposeImageFilter* compose_image_filter =
          static_cast<ComposeImageFilter*>(image_filter.get());
      auto outer = compose_image_filter->GetOuter();
      auto inner = compose_image_filter->GetInner();
      if (!outer && !inner) {
        return input;
      }
      if (!outer) {
        return HandleImageFilter(input, inner, scale);
      }
      if (!inner) {
        return HandleImageFilter(input, outer, scale);
      }
      return HandleImageFilter(HandleImageFilter(input, inner, scale), outer,
                               scale);
    }
    default:
      break;
  }
  return input;
}

std::shared_ptr<HWFilter> HWFilters::ConvertPaintToHWFilter(const Paint& paint,
                                                            Vec2 scale) {
  std::shared_ptr<HWFilter> result;
  if (paint.GetMaskFilter()) {
    result = HandleMaskFilter(result, paint.GetMaskFilter(), scale);
  }

  if (paint.GetImageFilter()) {
    result = HandleImageFilter(result, paint.GetImageFilter(), scale);
  }

  if (paint.GetColorFilter()) {
    result = HandleColorFilter(result, paint.GetColorFilter());
  }

  return result ? result : nullptr;
}

std::shared_ptr<HWFilter> HWFilters::Blur(float radius_x, float radius_y,
                                          Vec2 scale,
                                          std::shared_ptr<HWFilter> input,
                                          BlurType blur_type, Vec2 sigma) {
  float max_scaled_radius = std::max(radius_x * scale.x, radius_y * scale.y);

  /**
   * This downsampler optimize is not 100% accurate, but it is good enough to
   * improve the performance.
   * To 100% accurate downsampler, we need to do more calculations and do
   * upsampler.
   *
   * For now we restrict the condition to trigger this downsampler pass to make
   * sure most blur case is rendered correct.
   */
  float max_scaled_sigma =
      blur_type == BlurType::kGaussianSigma
          ? std::max(sigma.x * scale.x, sigma.y * scale.y)
          : radial_to_sigma(max_scaled_radius);
  auto downsample_scale =
      calculate_blur_scale(max_scaled_sigma, blur_type, max_scaled_radius);
  if (downsample_scale != 1.f) {
    input = std::make_shared<HWDownSamplerFilter>(input, downsample_scale);
  }

  input = std::make_shared<HWBlurFilter>(radius_x, Vec2{1, 0}, input,
                                         downsample_scale, blur_type, sigma.x);
  input = std::make_shared<HWBlurFilter>(radius_y, Vec2{0, 1}, input,
                                         downsample_scale, blur_type, sigma.y);

  return input;
}

std::shared_ptr<HWFilter> HWFilters::ColorFilter(
    std::shared_ptr<skity::ColorFilter> cf, std::shared_ptr<HWFilter> input) {
  return std::make_shared<HWColorFilter>(cf, input);
}

std::shared_ptr<HWFilter> HWFilters::Matrix(const skity::Matrix& matrix,
                                            std::shared_ptr<HWFilter> input) {
  return std::make_shared<HWMatrixFilter>(matrix, input);
}

std::shared_ptr<HWFilter> HWFilters::DropShadow(
    float radius_x, float radius_y, float offset_x, float offset_y, Vec2 scale,
    Color color, std::shared_ptr<HWFilter> input) {
  input = HWFilters::Blur(radius_x, radius_y, scale, input);
  input = HWFilters::ColorFilter(ColorFilters::Blend(color, BlendMode::kSrcIn),
                                 input);
  input = HWFilters::Matrix(Matrix::Translate(offset_x, offset_y), input);
  input = HWFilters::Merge({input, std::shared_ptr<HWFilter>()});
  return input;
}

std::shared_ptr<HWFilter> HWFilters::Merge(
    std::vector<std::shared_ptr<HWFilter>> inputs) {
  return std::make_shared<HWMergeFilter>(std::move(inputs));
}

}  // namespace skity
