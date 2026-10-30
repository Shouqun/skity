// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Deliberately separate from the public SDK consumer. Only this fixture uses
// the internal IO archive and its existing noninstalled implementation headers.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <skity/recorder/picture_recorder.hpp>
#include <skity/skity.hpp>
#include <vector>

#include "src/picture.hpp"

namespace {
class Writer final : public skity::WriteStream {
 public:
  bool Write(const void* p, size_t n) override {
    const auto* bytes = static_cast<const uint8_t*>(p);
    data.insert(data.end(), bytes, bytes + n);
    return true;
  }
  bool Flush() override { return true; }
  size_t BytesWritten() const override { return data.size(); }
  std::vector<uint8_t> data;
};
class Reader final : public skity::ReadStream {
 public:
  explicit Reader(const std::vector<uint8_t>& bytes) : bytes_(bytes) {}
  size_t Peek(void* p, size_t n) override {
    n = std::min(n, bytes_.size() - offset_);
    if (p && n) std::memcpy(p, bytes_.data() + offset_, n);
    return n;
  }
  size_t Read(void* p, size_t n) override {
    n = Peek(p, n);
    offset_ += n;
    return n;
  }
  bool IsAtEnd() const override { return offset_ == bytes_.size(); }

 private:
  const std::vector<uint8_t>& bytes_;
  size_t offset_ = 0;
};
}  // namespace
TEST(PrivateCppIOProduct, LinksAndRoundTripsWithProductionBridge) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(16, 16));
  skity::Paint paint;
  paint.SetColor(skity::Color_RED);
  paint.SetAntiAlias(false);
  recorder.GetRecordingCanvas()->DrawRect(skity::Rect::MakeXYWH(2, 3, 5, 6),
                                          paint);
  auto commands = recorder.FinishRecording();
  auto picture = skity::Picture::MakeFromDisplayList(commands.get());
  ASSERT_NE(picture, nullptr);
  Writer writer;
  picture->Serialize(writer, nullptr);
  ASSERT_FALSE(writer.data.empty());
  Reader reader(writer.data);
  auto restored = skity::Picture::MakeFromStream(reader);
  ASSERT_NE(restored, nullptr);
  skity::Bitmap bitmap(16, 16);
  auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
  ASSERT_NE(canvas, nullptr);
  canvas->Clear(skity::Color_WHITE);
  auto depth = canvas->GetSaveCount();
  auto matrix = canvas->GetTotalMatrix();
  restored->PlayBack(canvas.get());
  canvas->Flush();
  EXPECT_EQ(canvas->GetSaveCount(), depth);
  EXPECT_EQ(canvas->GetTotalMatrix(), matrix);
  EXPECT_EQ(bitmap.GetPixel(2, 3), skity::Color_RED);
  EXPECT_EQ(bitmap.GetPixel(0, 0), skity::Color_WHITE);
}
