// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import <Metal/Metal.h>
#include <gtest/gtest.h>
#include <skity/gpu/gpu_context_mtl.h>
#include <skity/skity.hpp>

#include "base/allocation_failure.hpp"
#include "src/render/hw/hw_canvas.hpp"

namespace skity {
// Internal-only observation: no production or installed test API is added.
class HWAllocationTestPeer {
 public:
  struct State {
    size_t active_layers;
    size_t queued_layers;
    int backend_depth;
    HWLayer* parent;
  };
  static State Read(const HWCanvas& canvas, HWLayer* parent = nullptr) {
    auto* root = parent ? parent : canvas.layer_stack_.back();
    State state{canvas.layer_stack_.size(), 0, root->GetState()->GetSelfDepth(), root};
    for (const auto* pass : root->draw_passes_) state.queued_layers += pass->draw_ops.size();
    return state;
  }
};

TEST(CanvasLayerAllocation, RefusedMetalLayerPublishesNoChild) {
  @autoreleasepool {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil) GTEST_SKIP() << "Metal device required";
    id<MTLCommandQueue> queue = [device newCommandQueue];
    auto context = MTLContextCreate(device, queue);
    ASSERT_NE(context, nullptr);
    for (int nesting : {0, 7, 15}) {
      SCOPED_TRACE(nesting);
      unsigned refusals = 0;
      bool completed = false;
      for (int allowance = 0; allowance < 128; ++allowance) {
        SCOPED_TRACE(allowance);
        auto descriptor =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                               width:64
                                                              height:64
                                                           mipmapped:NO];
        descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        descriptor.storageMode = MTLStorageModeShared;
        id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
        ASSERT_NE(texture, nil);
        GPUSurfaceDescriptorMTL target;
        target.backend = GPUBackendType::kMetal;
        target.width = target.height = 64;
        target.sample_count = 1;
        target.content_scale = 1.f;
        target.texture = texture;
        target.surface_type = MTLSurfaceType::kTexture;
        auto surface = context->CreateSurface(&target);
        ASSERT_NE(surface, nullptr);
        auto* canvas = surface->LockCanvas();
        ASSERT_NE(canvas, nullptr);
        auto* hardware = static_cast<HWCanvas*>(canvas);
        canvas->Save();
        canvas->Translate(3, 7);
        canvas->ClipRect(Rect::MakeXYWH(4, 4, 40, 40));
        for (int i = 0; i < nesting; ++i) canvas->SaveLayer(Rect::MakeWH(32, 32), Paint{});
        const auto depth = canvas->GetSaveCount();
        const auto matrix = canvas->GetTotalMatrix();
        const auto clip = canvas->GetLocalClipBounds();
        const auto before = HWAllocationTestPeer::Read(*hardware);
        bool refused = false;
        {
          testing::AllocationFailure fault(allowance);
          try {
            canvas->SaveLayer(Rect::MakeWH(32, 32), Paint{});
          } catch (const std::bad_alloc&) {
            refused = true;
          }
        }
        if (refused) {
          ++refusals;
          EXPECT_EQ(canvas->GetSaveCount(), depth);
          EXPECT_EQ(canvas->GetTotalMatrix(), matrix);
          EXPECT_EQ(canvas->GetLocalClipBounds(), clip);
          const auto after = HWAllocationTestPeer::Read(*hardware, before.parent);
          EXPECT_EQ(after.active_layers, before.active_layers);
          EXPECT_EQ(after.queued_layers, before.queued_layers);
          EXPECT_EQ(after.backend_depth, before.backend_depth);
        } else {
          completed = true;
          const auto after = HWAllocationTestPeer::Read(*hardware, before.parent);
          EXPECT_EQ(after.active_layers, before.active_layers + 1);
          EXPECT_EQ(after.queued_layers, before.queued_layers + 1);
          canvas->RestoreToCount(depth);
          EXPECT_EQ(canvas->GetSaveCount(), depth);
          EXPECT_EQ(canvas->GetTotalMatrix(), matrix);
          EXPECT_EQ(canvas->GetLocalClipBounds(), clip);
          break;
        }
      }
      EXPECT_GT(refusals, 0u);
      EXPECT_TRUE(completed);
    }
  }
}
}  // namespace skity
