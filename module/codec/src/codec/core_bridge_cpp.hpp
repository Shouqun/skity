// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CODEC_SRC_CODEC_CORE_BRIDGE_CPP_HPP
#define MODULE_CODEC_SRC_CODEC_CORE_BRIDGE_CPP_HPP
#include <memory>
#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>
#include <utility>

namespace skity {
namespace core_bridge {
inline std::shared_ptr<Data> MakeDataWithCopy(const void* data, size_t length) {
  return Data::MakeWithCopy(data, length);
}
inline std::shared_ptr<Data> MakeDataFromMalloc(void* data, size_t length) {
  return Data::MakeFromMalloc(data, length);
}
inline std::shared_ptr<Pixmap> MakePixmap(std::shared_ptr<Data> data,
                                          size_t row_bytes, uint32_t width,
                                          uint32_t height, AlphaType alpha_type,
                                          ColorType color_type) {
  if (!data || !width || !height) return nullptr;
  return std::make_shared<Pixmap>(std::move(data), row_bytes, width, height,
                                  alpha_type, color_type);
}
inline std::shared_ptr<Pixmap> MakePixmapAlloc(uint32_t width, uint32_t height,
                                               AlphaType alpha_type,
                                               ColorType color_type) {
  if (!width || !height) return nullptr;
  return std::make_shared<Pixmap>(width, height, alpha_type, color_type);
}
inline bool SetPixmapColorInfo(Pixmap* pixmap, AlphaType alpha_type,
                               ColorType color_type) {
  return pixmap && pixmap->SetColorInfo(alpha_type, color_type);
}
}  // namespace core_bridge
}  // namespace skity
#endif
