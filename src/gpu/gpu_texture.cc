// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gpu_texture.hpp"

#include <limits>

namespace skity {

bool GPUTexture::ValidUpload(uint32_t x, uint32_t y, uint32_t width,
                             uint32_t height, const void* data,
                             size_t readable) const {
  if (!data || !width || !height || desc_.sample_count != 1 ||
      desc_.storage_mode == GPUTextureStorageMode::kMemoryless ||
      x > desc_.width || y > desc_.height || width > desc_.width - x ||
      height > desc_.height - y)
    return false;
  if (static_cast<unsigned>(desc_.format) >=
      static_cast<unsigned>(GPUTextureFormat::kInvalid))
    return false;
  const size_t bpp = GetTextureFormatBytesPerPixel(desc_.format);
  return bpp && width <= std::numeric_limits<size_t>::max() / bpp / height &&
         size_t(width) * height * bpp <= readable;
}

GPUTexture::GPUTexture(const GPUTextureDescriptor& desc) : desc_(desc) {}

void GPUTexture::SetRelease(ReleaseCallback release_callback,
                            ReleaseUserData release_user_data) {
  release_callback_ = release_callback;
  release_user_data_ = release_user_data;
}

GPUTexture::~GPUTexture() {
  if (release_callback_) {
    release_callback_(release_user_data_);
  }
}

const GPUTextureDescriptor& GPUTexture::GetDescriptor() const { return desc_; }

}  // namespace skity
