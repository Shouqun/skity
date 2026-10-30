// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/io/core_bridge.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <skity/codec/codec.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/recorder/picture_recorder.hpp>

#include "src/io/memory_read.hpp"
#include "src/io/memory_writer.hpp"
#include "src/picture.hpp"
#include "src/utils/parse_path.hpp"

namespace skity {
namespace {
class BytesWriter final : public WriteStream {
 public:
  bool Write(const void* p, size_t n) override {
    const auto* b = static_cast<const uint8_t*>(p);
    bytes.insert(bytes.end(), b, b + n);
    return true;
  }
  bool Flush() override { return true; }
  size_t BytesWritten() const override { return bytes.size(); }
  std::vector<uint8_t> bytes;
};

class BytesReader final : public ReadStream {
 public:
  explicit BytesReader(const std::vector<uint8_t>& bytes) : bytes_(bytes) {}
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

std::string Hex(const std::vector<uint8_t>& bytes) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string out;
  for (auto b : bytes) {
    out.push_back(digits[b >> 4]);
    out.push_back(digits[b & 15]);
  }
  return out;
}

TEST(IOCoreBridge, CopiesBytesAndPreservesCodecPixels) {
  uint8_t source[] = {3, 7, 19, 255, 31, 23, 11, 255};
  auto data = core_bridge::MakeDataWithCopy(source, sizeof(source));
  ASSERT_NE(data, nullptr);
  ASSERT_EQ(data->Size(), sizeof(source));
  source[0] = 99;
  EXPECT_EQ(data->Bytes()[0], 3);
  auto pm = std::make_shared<Pixmap>(
      data, 8, 2, 1, AlphaType::kPremul_AlphaType, ColorType::kRGBA);
  auto codec = Codec::MakePngCodec();
  ASSERT_NE(codec, nullptr);
  auto encoded = codec->Encode(pm.get());
  ASSERT_NE(encoded, nullptr);
  auto image =
      core_bridge::DecodeDataToImage(encoded->RawData(), encoded->Size());
  ASSERT_NE(image, nullptr);
  auto decoded = image->GetPixmap();
  ASSERT_NE(decoded, nullptr);
  ASSERT_NE(*decoded, nullptr);
  ASSERT_EQ((*decoded)->Width(), 2u);
  ASSERT_EQ((*decoded)->Height(), 1u);
  EXPECT_EQ(std::memcmp((*decoded)->Addr(), data->RawData(), 8), 0);
  const uint8_t invalid[] = {0, 1, 2, 3};
  EXPECT_EQ(core_bridge::DecodeDataToImage(invalid, sizeof(invalid)), nullptr);
}

TEST(IOCoreBridge, PreservesPathPaintWireAndIndependentReadback) {
  core_bridge::PathBuilder builder;
  builder.MoveTo(2, 3);
  builder.LineTo(17, 5);
  builder.QuadTo(19, 7, 15, 13);
  builder.ConicTo(9, 21, 5, 17, 0.5f);
  builder.CubicTo(2, 15, 1, 9, 2, 3);
  builder.Close();
  builder.SetFillType(Path::PathFillType::kEvenOdd);
  auto path = builder.Take();
  ASSERT_NE(path, nullptr);
  auto paint = core_bridge::MakePaint();
  core_bridge::SetPaintColor(paint, 0x7f123456u);
  core_bridge::SetPaintAntiAlias(paint, true);
  core_bridge::SetPaintStyle(paint, Paint::Style::kStroke_Style);
  core_bridge::SetPaintStrokeWidth(paint, 2.5f);
  core_bridge::SetPaintStrokeCap(paint, Paint::Cap::kRound_Cap);
  core_bridge::SetPaintStrokeJoin(paint, Paint::Join::kBevel_Join);
  core_bridge::SetPaintBlendMode(paint, BlendMode::kSrc);
  FactorySet factories;
  BinaryWriteBuffer writer;
  writer.SetFactorySet(&factories);
  writer.WritePath(*path);
  writer.WritePaint(*paint);
  BytesWriter bytes;
  writer.WriteToStream(bytes);
  RecordProperty("wire_hex", Hex(bytes.bytes));
  ReadBuffer reader(bytes.bytes.data(), bytes.bytes.size());
  reader.SetFactorySet(&factories);
  auto decoded = reader.ReadPath();
  auto decoded_paint = reader.ReadPaint();
  ASSERT_TRUE(decoded.has_value());
  ASSERT_NE(decoded_paint, nullptr);
  EXPECT_TRUE(reader.IsValid());
  EXPECT_TRUE(reader.IsEOF());
  EXPECT_EQ(decoded->GetFillType(), Path::PathFillType::kEvenOdd);
  ASSERT_EQ(decoded->CountVerbs(), path->CountVerbs());
  ASSERT_EQ(decoded->CountPoints(), path->CountPoints());
  for (size_t i = 0; i < path->CountVerbs(); ++i)
    EXPECT_EQ(decoded->GetVerb(i), path->GetVerb(i));
  for (size_t i = 0; i < path->CountPoints(); ++i)
    EXPECT_EQ(decoded->GetPoint(i), path->GetPoint(i));
  EXPECT_EQ(decoded_paint->GetColor(), 0x7f123456u);
  EXPECT_EQ(decoded_paint->GetStrokeWidth(), 2.5f);
  EXPECT_EQ(decoded_paint->GetStrokeCap(), Paint::Cap::kRound_Cap);
  EXPECT_EQ(decoded_paint->GetStrokeJoin(), Paint::Join::kBevel_Join);
  EXPECT_EQ(decoded_paint->GetBlendMode(), BlendMode::kSrc);
}

TEST(IOCoreBridge, PreservesOwnedFontAndAllRunPositionings) {
  auto data = Data::MakeFromFileName(SKITY_FONT_DIR
                                     "fonts/resources/Roboto-Regular.ttf");
  ASSERT_NE(data, nullptr);
  ASSERT_FALSE(data->IsEmpty());
  auto face = core_bridge::MakeTypefaceFromBytes(data->RawData(), data->Size());
  ASSERT_NE(face, nullptr);
  auto font = core_bridge::MakeFont();
  ASSERT_NE(font, nullptr);
  font->SetTypeface(face);
  core_bridge::SetFontSize(font, 20.f);
  std::vector<uint16_t> glyphs = {face->UnicharToGlyph('A'),
                                  face->UnicharToGlyph('B')};
  std::vector<core_bridge::TextRunData> runs = {
      {font, glyphs, {}, {}},
      {font, glyphs, {1.f, 13.f}, {}},
      {font, glyphs, {2.f, 14.f}, {3.f, 5.f}}};
  auto blob = core_bridge::MakeTextBlobFromRuns(runs);
  ASSERT_NE(blob, nullptr);
  auto copies = core_bridge::ReadTextBlobRuns(blob.get());
  ASSERT_EQ(copies.size(), 3u);
  for (size_t i = 0; i < runs.size(); ++i) {
    ASSERT_NE(copies[i].font, nullptr);
    EXPECT_NE(copies[i].font.get(), font.get());
    EXPECT_EQ(copies[i].font->GetTypeface(), face);
    EXPECT_EQ(copies[i].font->GetSize(), 20.f);
    EXPECT_EQ(copies[i].glyphs, runs[i].glyphs);
    EXPECT_EQ(copies[i].pos_x, runs[i].pos_x);
    EXPECT_EQ(copies[i].pos_y, runs[i].pos_y);
  }
  TypefaceSet faces;
  BinaryWriteBuffer writer;
  writer.SetTypefaceSet(&faces);
  writer.WriteTextBlob(*blob);
  BytesWriter wire;
  writer.WriteToStream(wire);
  RecordProperty("text_wire_hex", Hex(wire.bytes));
  ReadBuffer reader(wire.bytes.data(), wire.bytes.size());
  reader.SetTypefaceSet(&faces);
  auto decoded = ReadFromMemory<std::shared_ptr<TextBlob>>(reader);
  ASSERT_NE(decoded, nullptr);
  EXPECT_TRUE(reader.IsValid());
  EXPECT_TRUE(reader.IsEOF());
  const auto restored = core_bridge::ReadTextBlobRuns(decoded.get());
  ASSERT_EQ(restored.size(), 3u);
  for (size_t i = 0; i < runs.size(); ++i) {
    EXPECT_EQ(restored[i].glyphs, runs[i].glyphs);
    EXPECT_EQ(restored[i].pos_x, runs[i].pos_x);
    EXPECT_EQ(restored[i].pos_y, runs[i].pos_y);
  }
  font->SetSize(31.f);
  EXPECT_EQ(copies[0].font->GetSize(), 20.f);
}

TEST(IOCoreBridge, PictureReplayPreservesPixelsAndCallerState) {
  PictureRecorder recorder;
  recorder.BeginRecording(Rect::MakeWH(32, 32));
  auto* c = recorder.GetRecordingCanvas();
  Paint p;
  p.SetColor(Color_RED);
  p.SetAntiAlias(false);
  c->Save();
  c->Translate(3, 4);
  c->ClipRect(Rect::MakeXYWH(0, 0, 20, 19));
  c->DrawRect(Rect::MakeXYWH(2, 3, 13, 11), p);
  p.SetColor(Color_BLUE);
  c->DrawCircle(17, 17, 4, p);
  c->Restore();
  auto dl = recorder.FinishRecording();
  auto picture = Picture::MakeFromDisplayList(dl.get());
  ASSERT_NE(picture, nullptr);
  BytesWriter wire;
  picture->Serialize(wire, nullptr);
  ASSERT_FALSE(wire.bytes.empty());
  RecordProperty("picture_wire_hex", Hex(wire.bytes));
  BytesReader input(wire.bytes);
  auto decoded = Picture::MakeFromStream(input);
  ASSERT_NE(decoded, nullptr);
  Bitmap expected(40, 40), actual(40, 40);
  auto a = Canvas::MakeSoftwareCanvas(&expected),
       b = Canvas::MakeSoftwareCanvas(&actual);
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  for (auto* canvas : {a.get(), b.get()}) {
    canvas->Clear(Color_WHITE);
    canvas->Save();
    canvas->Translate(1, 2);
    canvas->ClipRect(Rect::MakeXYWH(0, 0, 34, 33));
  }
  const auto depth = b->GetSaveCount();
  const auto matrix = b->GetTotalMatrix();
  const auto clip = b->GetGlobalClipBounds();
  dl->Draw(a.get());
  decoded->PlayBack(b.get());
  EXPECT_EQ(b->GetSaveCount(), depth);
  EXPECT_EQ(b->GetTotalMatrix(), matrix);
  EXPECT_EQ(b->GetGlobalClipBounds(), clip);
  EXPECT_EQ(
      std::memcmp(expected.GetPixelAddr(), actual.GetPixelAddr(), 40 * 40 * 4),
      0);
  b->Restore();
  EXPECT_EQ(b->GetSaveCount(), 1);
  EXPECT_EQ(b->GetTotalMatrix(), Matrix{});
}
}  // namespace
}  // namespace skity
