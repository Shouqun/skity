// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <skity_c/skity_mask_filter.h>

#include <cstring>
#include <limits>
#include <skity/effect/mask_filter.hpp>
#include <vector>

#include "src/io/memory_read.hpp"
#include "src/io/memory_writer.hpp"
#include "src/picture.hpp"

namespace skity {
namespace {

class VectorWriteStream final : public WriteStream {
 public:
  bool Write(const void* data, size_t size) override {
    const auto* bytes = static_cast<const uint8_t*>(data);
    bytes_.insert(bytes_.end(), bytes, bytes + size);
    return true;
  }
  bool Flush() override { return true; }
  size_t BytesWritten() const override { return bytes_.size(); }
  std::vector<uint8_t> bytes_;
};

TEST(GaussianMaskSerialization, RoundTripsExplicitSigmaAndFactory) {
  for (float sigma : {0.125f, 0.5f, 1.25f, 20.f}) {
    FactorySet factories;
    BinaryWriteBuffer writer;
    writer.SetFactorySet(&factories);
    const auto input = MaskFilter::MakeGaussianBlur(sigma);
    ASSERT_NE(input, nullptr);
    writer.WriteFlattenable(input.get());
    VectorWriteStream stream;
    writer.WriteToStream(stream);
    ASSERT_EQ(factories.GetFactoryCount(), 1u);
    EXPECT_EQ(factories.GetFactoryName(0), "SkityGaussianMaskFilter");
    ASSERT_EQ(stream.bytes_.size(), 16u);  // factory, size, version, sigma
    ReadBuffer reader(stream.bytes_.data(), stream.bytes_.size());
    reader.SetFactorySet(&factories);
    const auto output = reader.ReadMaskFilter();
    ASSERT_NE(output, nullptr);
    EXPECT_TRUE(reader.IsValid());
    EXPECT_TRUE(reader.IsEOF());
    EXPECT_EQ(output->GetBlurType(), BlurType::kGaussianSigma);
    EXPECT_EQ(output->GetBlurStyle(), kNormal);
    EXPECT_FLOAT_EQ(output->GetBlurSigma(), sigma);
    EXPECT_FLOAT_EQ(output->GetBlurRadius(), 3.f * sigma);
  }
}

TEST(GaussianMaskSerialization, RejectsUnknownVersionInvalidSigmaAndTruncation) {
  FactorySet factories;
  factories.AddFactory("SkityGaussianMaskFilter");
  for (float sigma : {0.f, -1.f, std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::max()}) {
    BinaryWriteBuffer writer;
    writer.WriteInt32(1);
    writer.WriteUint32(8);
    writer.WriteUint32(1);
    writer.WriteFloat(sigma);
    VectorWriteStream stream;
    writer.WriteToStream(stream);
    ReadBuffer reader(stream.bytes_.data(), stream.bytes_.size());
    reader.SetFactorySet(&factories);
    EXPECT_EQ(reader.ReadMaskFilter(), nullptr);
    EXPECT_FALSE(reader.IsValid());
  }
  for (uint32_t version : {0u, 2u, 0xffffffffu}) {
    BinaryWriteBuffer writer;
    writer.WriteInt32(1);
    writer.WriteUint32(8);
    writer.WriteUint32(version);
    writer.WriteFloat(1.f);
    VectorWriteStream stream;
    writer.WriteToStream(stream);
    ReadBuffer reader(stream.bytes_.data(), stream.bytes_.size());
    reader.SetFactorySet(&factories);
    EXPECT_EQ(reader.ReadMaskFilter(), nullptr);
    EXPECT_FALSE(reader.IsValid());
  }
  const uint32_t truncated[] = {1, 8, 1};
  ReadBuffer reader(truncated, sizeof(truncated));
  reader.SetFactorySet(&factories);
  EXPECT_EQ(reader.ReadMaskFilter(), nullptr);
  EXPECT_FALSE(reader.IsValid());
}

TEST(GaussianMaskSerialization, LegacyWireBytesRemainUnchanged) {
  for (BlurStyle style : {kNormal, kSolid, kOuter, kInner}) {
    FactorySet factories;
    BinaryWriteBuffer writer;
    writer.SetFactorySet(&factories);
    const auto legacy = MaskFilter::MakeBlur(style, 3.f);
    ASSERT_NE(legacy, nullptr);
    writer.WriteFlattenable(legacy.get());
    VectorWriteStream stream;
    writer.WriteToStream(stream);
    EXPECT_EQ(factories.GetFactoryName(0), "SkBlurMaskFilterImpl");
    const float legacy_sigma = 3.f * 0.57735f + 0.5f;
    uint32_t sigma_bits;
    std::memcpy(&sigma_bits, &legacy_sigma, sizeof(sigma_bits));
    const uint32_t expected[] = {1, 12, sigma_bits,
                                 static_cast<uint32_t>(style) - 1, 0};
    ASSERT_EQ(stream.bytes_.size(), sizeof(expected));
    EXPECT_EQ(std::memcmp(stream.bytes_.data(), expected, sizeof(expected)), 0);
  }
}

TEST(GaussianMaskSerialization, RejectsMismatchedPayloadLength) {
  FactorySet factories;
  factories.AddFactory("SkityGaussianMaskFilter");
  for (uint32_t size : {0u, 4u, 12u, 0xffffffffu}) {
    BinaryWriteBuffer writer;
    writer.WriteInt32(1);
    writer.WriteUint32(size);
    writer.WriteUint32(1);
    writer.WriteFloat(1.f);
    VectorWriteStream stream;
    writer.WriteToStream(stream);
    ReadBuffer reader(stream.bytes_.data(), stream.bytes_.size());
    reader.SetFactorySet(&factories);
    EXPECT_EQ(reader.ReadMaskFilter(), nullptr);
    EXPECT_FALSE(reader.IsValid());
  }
}

TEST(GaussianMaskSerialization, CFactoryRejectsInvalidAndOwnsValidFilter) {
  EXPECT_EQ(skity_mask_filter_create_gaussian_blur(0.f), nullptr);
  EXPECT_EQ(skity_mask_filter_create_gaussian_blur(-1.f), nullptr);
  EXPECT_EQ(skity_mask_filter_create_gaussian_blur(
                std::numeric_limits<float>::quiet_NaN()), nullptr);
  const auto filter = skity_mask_filter_create_gaussian_blur(0.75f);
  ASSERT_NE(filter, nullptr);
  skity_mask_filter_destroy(filter);
}

}  // namespace
}  // namespace skity
