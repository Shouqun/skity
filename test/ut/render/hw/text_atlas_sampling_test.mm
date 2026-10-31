// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import <Metal/Metal.h>
#include <gtest/gtest.h>
#include <skity/gpu/gpu_context_mtl.h>
#include <skity/skity.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <vector>

#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/gpu_device.hpp"
#include "src/gpu/mtl/gpu_context_impl_mtl.h"
#include "src/gpu/mtl/gpu_device_mtl.h"
#include "src/render/hw/draw/hw_dynamic_text_draw.hpp"
#include "src/render/hw/hw_canvas.hpp"

namespace skity {
// Test-only recording seam: exercise the actual text draw/shader without any
// font rasterizer, atlas packer or private access in a client drawing API.
class HWTextSamplingTestPeer {
 public:
  static void Record(HWCanvas& canvas, std::shared_ptr<GPUTexture> texture,
                     std::shared_ptr<GPUSampler> sampler, uint32_t x, uint32_t y,
                     TextAtlasEffect effect) {
    ArrayList<GlyphRect, 16> quads;
    quads.SetArenaAllocator(canvas.arena_allocator_);
    // Fractional quads include both overlapping and isolated coverage.
    const std::array<Vec4, 4> bounds = {{{23.61904716f, 43.80952454f, 38.47618866f, 61.7142868f},
                                         {34.66666794f, 45.33333206f, 49.52381134f, 63.23809433f},
                                         {51.80952454f, 46.85713959f, 60.57143021f, 64.76190186f},
                                         {73.90476227f, 49.52381134f, 85.7142868f, 68.19047546f}}};
    for (const auto& rect : bounds)
      quads.emplace_back(rect, Vec2{float(x), float(y)}, Vec2{float(x + 39), float(y + 47)});
    TextGeometryData::BatchedTexture textures{};
    textures[0] = std::move(texture);
    Paint paint;
    paint.SetAntiAlias(true);
    paint.SetColor(ColorSetARGB(128, 32, 64, 160));
    auto* draw = canvas.arena_allocator_->Make<HWDynamicTextDraw>(
        Matrix{}, Matrix{}, std::move(quads), paint, std::move(textures), std::move(sampler),
        effect, false);
    draw->SetSampleCount(canvas.GetCanvasSampleCount());
    canvas.SetupLayerSpaceBoundsForDraw(draw, Rect::MakeXYWH(20, 40, 70, 32), Matrix{});
    canvas.SetupBlendPlanForDraw(draw, paint, effect == TextAtlasEffect::kA8Coverage);
    canvas.CurrentLayer()->AddDraw(draw);
  }
};
namespace {
class TextAtlasSamplingTest : public ::testing::Test {
 protected:
  static constexpr uint32_t kWidth = 1008, kHeight = 420, kAtlas = 1024;
  void SetUp() override {
    device_ = MTLCreateSystemDefaultDevice();
    if (device_ == nil) GTEST_SKIP() << "Metal required";
    queue_ = [device_ newCommandQueue];
    ASSERT_NE(queue_, nil);
    context_ = MTLContextCreate(device_, queue_);
    ASSERT_NE(context_, nullptr);
  }
  std::vector<uint8_t> Render(uint32_t x, uint32_t y, bool linear, bool logical,
                              TextAtlasEffect effect, bool public_api = false) {
    auto* gpu = static_cast<GPUContextImpl*>(context_.get())->GetGPUDevice();
    GPUTextureDescriptor atlas{};
    atlas.width = atlas.height = kAtlas;
    const bool coverage = effect == TextAtlasEffect::kA8Coverage;
    const size_t channels = coverage ? 1 : 4;
    atlas.format = coverage ? GPUTextureFormat::kR8Unorm : GPUTextureFormat::kRGBA8Unorm;
    atlas.storage_mode = GPUTextureStorageMode::kHostVisible;
    atlas.usage = static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding) |
                  static_cast<GPUTextureUsageMask>(GPUTextureUsage::kCopyDst);
    auto texture = gpu->CreateTexture(atlas);
    if (!texture) {
      ADD_FAILURE() << "Atlas unavailable";
      return {};
    }
    std::vector<uint8_t> pixels(kAtlas * kAtlas * channels, 0);
    for (uint32_t row = 2; row < 45; ++row)
      for (uint32_t col = 2; col < 37; ++col) {
        auto* pixel = pixels.data() + ((y + row) * kAtlas + x + col) * channels;
        const auto alpha = uint8_t((17 * col + 29 * row) % 256);
        if (coverage) {
          pixel[0] = alpha;
          continue;
        }
        pixel[0] = alpha / 2;
        pixel[1] = alpha / 3;
        pixel[2] = alpha / 4;
        pixel[3] = alpha;
        if (effect == TextAtlasEffect::kColorSwizzleRB) std::swap(pixel[0], pixel[2]);
      }
    texture->UploadData(0, 0, kAtlas, kAtlas, pixels.data());
    GPUSamplerDescriptor sampling;
    sampling.min_filter = sampling.mag_filter =
        linear ? GPUFilterMode::kLinear : GPUFilterMode::kNearest;
    auto sampler = gpu->CreateSampler(sampling);
    if (!sampler) {
      ADD_FAILURE() << "Sampler unavailable";
      return {};
    }
    auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                   width:kWidth
                                                                  height:kHeight
                                                               mipmapped:NO];
    desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeShared;
    id<MTLTexture> output = [device_ newTextureWithDescriptor:desc];
    if (output == nil) {
      ADD_FAILURE() << "Target unavailable";
      return {};
    }
    GPUSurfaceDescriptorMTL target;
    target.backend = GPUBackendType::kMetal;
    target.surface_type = MTLSurfaceType::kTexture;
    target.texture = output;
    target.width = logical ? 384 : kWidth;
    target.height = logical ? 160 : kHeight;
    target.content_scale = 2.625f;
    target.sample_count = 1;
    auto surface = context_->CreateSurface(&target);
    if (!surface) {
      ADD_FAILURE() << "Surface unavailable";
      return {};
    }
    auto* canvas = surface->LockCanvas();
    if (!canvas) {
      ADD_FAILURE() << "Canvas unavailable";
      return {};
    }
    canvas->Clear(Color_WHITE);
    if (!public_api) {
      HWTextSamplingTestPeer::Record(static_cast<HWCanvas&>(*canvas), std::move(texture),
                                     std::move(sampler), x, y, effect);
    } else {
      const auto format = coverage ? GlyphRaster::Format::kA8
                          : effect == TextAtlasEffect::kColorSwizzleRB
                              ? GlyphRaster::Format::kBGRA8
                              : GlyphRaster::Format::kRGBA8;
      const auto offset = (size_t(y) * kAtlas + x) * channels;
      auto resource = GlyphRaster::MakeCopy(
          pixels.data() + offset, pixels.size() - offset, 39, 47, kAtlas * channels, format,
          coverage ? GlyphRaster::Alpha::kCoverage : GlyphRaster::Alpha::kPremultiplied, 16384);
      if (!resource) {
        ADD_FAILURE() << "Public raster refused";
        return {};
      }
      Paint paint;
      paint.SetAntiAlias(true);
      paint.SetColor(ColorSetARGB(128, 32, 64, 160));
      const std::array<Vec4, 4> bounds = {
          {{23.61904716f, 43.80952454f, 38.47618866f, 61.7142868f},
           {34.66666794f, 45.33333206f, 49.52381134f, 63.23809433f},
           {51.80952454f, 46.85713959f, 60.57143021f, 64.76190186f},
           {73.90476227f, 49.52381134f, 85.7142868f, 68.19047546f}}};
      for (const auto& b : bounds) {
        const auto rect = Rect::MakeLTRB(b.x, b.y, b.z, b.w);
        if (!canvas->DrawGlyphRaster(*resource, rect,
                                     linear ? FilterMode::kLinear : FilterMode::kNearest, paint,
                                     39 * 47 * 4)) {
          ADD_FAILURE() << "Public draw refused";
          return {};
        }
      }
      resource.reset();
      std::fill(pixels.begin(), pixels.end(), 0);
    }
    canvas->Flush();
    surface->Flush();
    auto completion = [queue_ commandBuffer];
    if (completion == nil) {
      ADD_FAILURE() << "Completion unavailable";
      return {};
    }
    [completion commit];
    [completion waitUntilCompleted];
    if (completion.status != MTLCommandBufferStatusCompleted || completion.error != nil) {
      ADD_FAILURE() << "GPU completion failed";
      return {};
    }
    std::vector<uint8_t> rgba(kWidth * kHeight * 4);
    [output getBytes:rgba.data()
         bytesPerRow:kWidth * 4
          fromRegion:MTLRegionMake2D(0, 0, kWidth, kHeight)
         mipmapLevel:0];
    surface.reset();
    return rgba;
  }
  void Compare(bool linear, TextAtlasEffect effect = TextAtlasEffect::kA8Coverage) {
    unsigned nonidentical = 0;
    for (bool logical : {false, true}) {
      const auto reference = Render(0, 0, linear, logical, effect);
      ASSERT_EQ(reference.size(), kWidth * kHeight * 4);
      ASSERT_TRUE(std::any_of(reference.begin(), reference.end(), [](auto n) { return n != 255; }));
      for (const auto origin :
           {std::pair<uint32_t, uint32_t>{514, 902}, {1, 1}, {997 - 39, 973 - 47}}) {
        SCOPED_TRACE(::testing::Message() << "linear=" << linear << " logical=" << logical
                                          << " origin=" << origin.first << "," << origin.second);
        const auto actual = Render(origin.first, origin.second, linear, logical, effect);
        ASSERT_EQ(actual.size(), reference.size());
        size_t differences = 0;
        for (size_t i = 0; i < actual.size(); ++i) differences += actual[i] != reference[i];
        if (differences) ++nonidentical;
        EXPECT_EQ(differences, 0u) << "Identical texels depend on their atlas origin";
      }
    }
    RecordProperty("nonidentical_origins", nonidentical);
    RecordProperty("origin_comparisons", 6);
  }
  id<MTLDevice> device_;
  id<MTLCommandQueue> queue_;
  std::unique_ptr<GPUContext> context_;
};
TEST_F(TextAtlasSamplingTest, LinearCoverageIsIndependentOfAtlasOrigin) { Compare(true); }
TEST_F(TextAtlasSamplingTest, NearestCoverageIsIndependentOfAtlasOrigin) { Compare(false); }
TEST_F(TextAtlasSamplingTest, LinearColorIsIndependentOfAtlasOrigin) {
  Compare(true, TextAtlasEffect::kColor);
}
TEST_F(TextAtlasSamplingTest, NearestColorIsIndependentOfAtlasOrigin) {
  Compare(false, TextAtlasEffect::kColor);
}
TEST_F(TextAtlasSamplingTest, LinearSwizzledColorIsIndependentOfAtlasOrigin) {
  Compare(true, TextAtlasEffect::kColorSwizzleRB);
}
TEST_F(TextAtlasSamplingTest, NearestSwizzledColorIsIndependentOfAtlasOrigin) {
  Compare(false, TextAtlasEffect::kColorSwizzleRB);
}

TEST_F(TextAtlasSamplingTest, PublicRasterMatchesOriginalGeometryAfterSourceRetirement) {
  unsigned cases = 0;
  for (auto effect :
       {TextAtlasEffect::kA8Coverage, TextAtlasEffect::kColor, TextAtlasEffect::kColorSwizzleRB})
    for (bool linear : {false, true})
      for (bool logical : {false, true}) {
        const auto original = Render(514, 902, linear, logical, effect);
        const auto actual = Render(514, 902, linear, logical, effect, true);
        ASSERT_EQ(original.size(), kWidth * kHeight * 4);
        ASSERT_EQ(actual.size(), original.size());
        EXPECT_EQ(actual, original)
            << "effect=" << int(effect) << " linear=" << linear << " logical=" << logical;
        ++cases;
      }
  EXPECT_EQ(cases, 12u);
  RecordProperty("public_original_comparisons", cases);
}

TEST_F(TextAtlasSamplingTest, IntrinsicColorAndSwizzleHaveIndependentRgbWitness) {
  for (bool linear : {false, true})
    for (bool logical : {false, true}) {
      const auto color = Render(514, 902, linear, logical, TextAtlasEffect::kColor);
      const auto swizzled = Render(514, 902, linear, logical, TextAtlasEffect::kColorSwizzleRB);
      ASSERT_EQ(color.size(), kWidth * kHeight * 4);
      ASSERT_EQ(swizzled.size(), color.size());
      EXPECT_EQ(color, swizzled);
      bool intrinsic_red_exceeds_blue = false;
      for (size_t i = 0; i < color.size(); i += 4) {
        EXPECT_GE(color[i], color[i + 2]) << "Intrinsic RGB was replaced by blue mask paint";
        intrinsic_red_exceeds_blue |= color[i] > color[i + 2];
      }
      EXPECT_TRUE(intrinsic_red_exceeds_blue) << "No independent intrinsic color witness";
    }
}
// Inject refusal only for the small source texture; real target/command/pipeline
// creation stays on the actual Metal implementation. No product test switch.
struct RasterRefusal {
  enum Mode { kNone, kTexture, kSampler, kUpload };
  Mode mode = kNone;
  unsigned sources = 0;
};
class RefusedUploadTexture final : public GPUTexture {
 public:
  explicit RefusedUploadTexture(const GPUTextureDescriptor& d) : GPUTexture(d) {}
  size_t GetBytes() const override { return 4; }
  void UploadData(uint32_t, uint32_t, uint32_t, uint32_t, void*) override {}
};
class RasterRefusalDevice final : public GPUDeviceMTL {
 public:
  RasterRefusalDevice(id<MTLDevice> d, id<MTLCommandQueue> q, RasterRefusal* f)
      : GPUDeviceMTL(d, q), failure_(f) {}
  std::shared_ptr<GPUTexture> CreateTexture(const GPUTextureDescriptor& d) override {
    if (d.format == GPUTextureFormat::kR8Unorm && d.width == 2 && d.height == 2 &&
        failure_->mode != RasterRefusal::kNone) {
      ++failure_->sources;
      if (failure_->mode == RasterRefusal::kTexture) return nullptr;
      if (failure_->mode == RasterRefusal::kUpload)
        return std::make_shared<RefusedUploadTexture>(d);
    }
    return GPUDeviceMTL::CreateTexture(d);
  }
  std::shared_ptr<GPUSampler> CreateSampler(const GPUSamplerDescriptor& d) override {
    if (failure_->mode == RasterRefusal::kSampler) return nullptr;
    return GPUDeviceMTL::CreateSampler(d);
  }

 private:
  RasterRefusal* failure_;
};
class RasterRefusalContext final : public GPUContextImplMTL {
 public:
  RasterRefusalContext(id<MTLDevice> d, id<MTLCommandQueue> q, RasterRefusal* f)
      : GPUContextImplMTL(d, q), failure_(f) {}

 protected:
  std::unique_ptr<GPUDevice> CreateGPUDevice() override {
    return std::make_unique<RasterRefusalDevice>(GetNativeDevice(), GetNativeQueue(), failure_);
  }

 private:
  RasterRefusal* failure_;
};
TEST_F(TextAtlasSamplingTest, BackendResourceRefusalsQueueNoPartialQuad) {
  for (auto failure_mode :
       {RasterRefusal::kTexture, RasterRefusal::kSampler, RasterRefusal::kUpload}) {
    RasterRefusal failure;
    RasterRefusalContext context(device_, queue_, &failure);
    ASSERT_TRUE(context.Init());
    auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                   width:32
                                                                  height:32
                                                               mipmapped:NO];
    desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeShared;
    id<MTLTexture> output = [device_ newTextureWithDescriptor:desc];
    ASSERT_NE(output, nil);
    GPUSurfaceDescriptorMTL target;
    target.backend = GPUBackendType::kMetal;
    target.surface_type = MTLSurfaceType::kTexture;
    target.texture = output;
    target.width = target.height = 32;
    target.content_scale = 1;
    target.sample_count = 1;
    auto surface = context.CreateSurface(&target);
    ASSERT_NE(surface, nullptr);
    auto* canvas = surface->LockCanvas();
    ASSERT_NE(canvas, nullptr);
    canvas->Clear(Color_WHITE);
    const auto matrix = canvas->GetTotalMatrix();
    const auto count = canvas->GetSaveCount();
    const uint8_t bytes[] = {0, 64, 128, 255};
    auto resource = GlyphRaster::MakeCopy(bytes, 4, 2, 2, 2, GlyphRaster::Format::kA8,
                                          GlyphRaster::Alpha::kCoverage, 1024);
    ASSERT_NE(resource, nullptr);
    Paint paint;
    const auto bounds = Rect::MakeXYWH(4, 4, 20, 20);
    ASSERT_TRUE(canvas->CanDrawGlyphRaster(*resource, bounds, FilterMode::kNearest, paint, 16));
    failure.mode = failure_mode;
    EXPECT_FALSE(canvas->DrawGlyphRaster(*resource, bounds, FilterMode::kNearest, paint, 16));
    EXPECT_EQ(failure.sources, 1u);
    failure.mode = RasterRefusal::kNone;
    EXPECT_EQ(canvas->GetTotalMatrix(), matrix);
    EXPECT_EQ(canvas->GetSaveCount(), count);
    canvas->Flush();
    surface->Flush();
    auto done = [queue_ commandBuffer];
    ASSERT_NE(done, nil);
    [done commit];
    [done waitUntilCompleted];
    ASSERT_EQ(done.status, MTLCommandBufferStatusCompleted);
    ASSERT_EQ(done.error, nil);
    std::vector<uint8_t> pixels(32 * 32 * 4);
    [output getBytes:pixels.data()
         bytesPerRow:32 * 4
          fromRegion:MTLRegionMake2D(0, 0, 32, 32)
         mipmapLevel:0];
    EXPECT_TRUE(std::all_of(pixels.begin(), pixels.end(), [](auto n) { return n == 255; }));
    surface.reset();
  }
}
TEST_F(TextAtlasSamplingTest, PublicColorAlphaMatchesExistingImageContract) {
  unsigned cases = 0;
  for (auto format : {GlyphRaster::Format::kRGBA8, GlyphRaster::Format::kBGRA8})
    for (auto alpha : {GlyphRaster::Alpha::kPremultiplied, GlyphRaster::Alpha::kUnpremultiplied})
      for (uint8_t paint_alpha : {uint8_t(128), uint8_t(255)}) {
        std::array<uint8_t, 4> source = alpha == GlyphRaster::Alpha::kUnpremultiplied
                                            ? std::array<uint8_t, 4>{200, 100, 50, 128}
                                            : std::array<uint8_t, 4>{96, 64, 32, 128};
        if (format == GlyphRaster::Format::kBGRA8) std::swap(source[0], source[2]);
        const auto render = [&](bool pub) {
          auto desc =
              [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                 width:32
                                                                height:32
                                                             mipmapped:NO];
          desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
          desc.storageMode = MTLStorageModeShared;
          id<MTLTexture> output = [device_ newTextureWithDescriptor:desc];
          if (output == nil) {
            ADD_FAILURE() << "Target unavailable";
            return std::vector<uint8_t>{};
          }
          GPUSurfaceDescriptorMTL target;
          target.backend = GPUBackendType::kMetal;
          target.surface_type = MTLSurfaceType::kTexture;
          target.texture = output;
          target.width = target.height = 32;
          target.content_scale = 1;
          target.sample_count = 1;
          auto surface = context_->CreateSurface(&target);
          if (!surface) {
            ADD_FAILURE() << "Surface unavailable";
            return std::vector<uint8_t>{};
          }
          auto* canvas = surface->LockCanvas();
          if (!canvas) {
            ADD_FAILURE() << "Canvas unavailable";
            return std::vector<uint8_t>{};
          }
          canvas->Clear(Color_WHITE);
          Paint paint;
          paint.SetColor(ColorSetARGB(paint_alpha, 32, 64, 160));
          const auto rect = Rect::MakeXYWH(8, 8, 16, 16);
          if (pub) {
            auto resource = GlyphRaster::MakeCopy(source.data(), 4, 1, 1, 4, format, alpha, 128);
            if (!resource ||
                !canvas->DrawGlyphRaster(*resource, rect, FilterMode::kNearest, paint, 4)) {
              ADD_FAILURE() << "Public alpha draw refused";
              return std::vector<uint8_t>{};
            }
            resource.reset();
          } else {
            auto pixmap = std::make_shared<Pixmap>(
                Data::MakeWithCopy(source.data(), 4), 4, 1, 1,
                alpha == GlyphRaster::Alpha::kUnpremultiplied ? AlphaType::kUnpremul_AlphaType
                                                              : AlphaType::kPremul_AlphaType,
                format == GlyphRaster::Format::kBGRA8 ? ColorType::kBGRA : ColorType::kRGBA);
            canvas->DrawImage(Image::MakeImage(pixmap), rect,
                              SamplingOptions(FilterMode::kNearest, MipmapMode::kNone), &paint);
          }
          canvas->Flush();
          surface->Flush();
          auto done = [queue_ commandBuffer];
          if (done == nil) {
            ADD_FAILURE() << "No completion";
            return std::vector<uint8_t>{};
          }
          [done commit];
          [done waitUntilCompleted];
          if (done.status != MTLCommandBufferStatusCompleted || done.error != nil) {
            ADD_FAILURE() << "Completion failed";
            return std::vector<uint8_t>{};
          }
          std::vector<uint8_t> pixels(32 * 32 * 4);
          [output getBytes:pixels.data()
               bytesPerRow:32 * 4
                fromRegion:MTLRegionMake2D(0, 0, 32, 32)
               mipmapLevel:0];
          surface.reset();
          return pixels;
        };
        const auto reference = render(false), actual = render(true);
        ASSERT_EQ(reference.size(), 32 * 32 * 4);
        ASSERT_EQ(actual.size(), reference.size());
        EXPECT_TRUE(
            std::any_of(reference.begin(), reference.end(), [](auto n) { return n != 255; }));
        EXPECT_EQ(actual, reference) << "format=" << int(format) << " alpha=" << int(alpha)
                                     << " paint_alpha=" << int(paint_alpha);
        ++cases;
      }
  EXPECT_EQ(cases, 8u);
  RecordProperty("original_image_alpha_comparisons", cases);
}
}  // namespace
}  // namespace skity
