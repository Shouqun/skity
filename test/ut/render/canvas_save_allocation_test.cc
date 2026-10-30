// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <skity/geometry/matrix.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/blend_mode.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>
#include <skity/recorder/picture_recorder.hpp>
#include <skity/render/canvas.hpp>
#include <stdexcept>
#include <vector>

#if defined(SKITY_RECOVERY_TEST_IO)
#include "src/io/memory_writer.hpp"
#include "src/record/record_playback.hpp"
#endif

#include "base/allocation_failure.hpp"
#include "src/render/sw/sw_raster.hpp"
#include "src/render/sw/sw_span_brush.hpp"
#include "src/utils/arena_allocator.hpp"
using AllocationFailure = skity::testing::AllocationFailure;

#if defined(SKITY_CPU)
TEST(CanvasSaveAllocation, FailurePreservesCallerMatrixClipAndDepth) {
  unsigned failures = 0;
  bool completed = false;
  for (int allowance = 0; allowance < 64; ++allowance) {
    SCOPED_TRACE(allowance);
    skity::Bitmap bitmap(32, 32, skity::AlphaType::kPremul_AlphaType);
    auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
    ASSERT_NE(canvas, nullptr);
    canvas->Save();
    canvas->Translate(3.f, 7.f);
    canvas->ClipRect(skity::Rect::MakeXYWH(4.f, 4.f, 16.f, 16.f));
    const auto depth = canvas->GetSaveCount();
    const auto matrix = canvas->GetTotalMatrix();
    bool threw = false;
    int previous_depth = -1;
    {
      AllocationFailure fault(allowance);
      try {
        previous_depth = canvas->Save();
      } catch (const std::bad_alloc&) {
        threw = true;
      }
    }
    if (!threw) {
      EXPECT_EQ(previous_depth, depth);
      completed = true;
      canvas->RestoreToCount(depth);
      EXPECT_EQ(canvas->GetSaveCount(), depth);
      EXPECT_EQ(canvas->GetTotalMatrix(), matrix);
      break;
    }
    ++failures;
    EXPECT_EQ(canvas->GetSaveCount(), depth);
    EXPECT_EQ(canvas->GetTotalMatrix(), matrix);
    // Do not continue operations on a canvas already proven inconsistent.
    if (canvas->GetSaveCount() != depth || canvas->GetTotalMatrix() != matrix)
      continue;
    canvas->Translate(11.f, 13.f);
    canvas->Restore();
    EXPECT_EQ(canvas->GetSaveCount(), 1);
    EXPECT_EQ(canvas->GetTotalMatrix(), skity::Matrix{});
    if (canvas->GetSaveCount() != 1 ||
        canvas->GetTotalMatrix() != skity::Matrix{})
      continue;
    skity::Paint paint;
    paint.SetColor(skity::Color_BLUE);
    paint.SetAntiAlias(false);
    canvas->DrawRect(skity::Rect::MakeWH(32.f, 32.f), paint);
    EXPECT_EQ(bitmap.GetPixel(0, 0), skity::Color_BLUE);
  }
  EXPECT_TRUE(completed);
  EXPECT_GE(failures, 3u);
}
#endif

// Transferred native storage must have one owner even if either allocation
// needed for Data's object/control block throws. The release callback itself
// deliberately performs no allocation and never inspects retired storage.
TEST(DataAllocationFailure, ReleasesTransferredStorageExactlyOnce) {
  for (int allowance = 0; allowance < 3; ++allowance) {
    SCOPED_TRACE(allowance);
    const uint8_t bytes[] = {1, 2, 3, 4};
    unsigned releases = 0;
    std::shared_ptr<skity::Data> data;
    bool threw = false;
    {
      AllocationFailure failure(allowance);
      try {
        data = skity::Data::MakeWithProc(
            bytes, sizeof(bytes),
            [](const void*, void* count) { ++*static_cast<unsigned*>(count); },
            &releases);
      } catch (const std::bad_alloc&) {
        threw = true;
      }
    }
    if (!threw) {
      ASSERT_NE(data, nullptr);
      EXPECT_EQ(data->Size(), sizeof(bytes));
      EXPECT_EQ(releases, 0u);
    }
    data.reset();
    EXPECT_EQ(releases, 1u);
    EXPECT_EQ(threw, allowance < 2);
  }
}

TEST(CanvasSaveAllocation, RefusedRecordingSaveAppendsNoOperation) {
  unsigned failures = 0;
  bool completed = false;
  for (int allowance = 0; allowance < 64; ++allowance) {
    SCOPED_TRACE(allowance);
    skity::PictureRecorder recorder;
    recorder.BeginRecording(skity::Rect::MakeWH(32, 32));
    auto* canvas = recorder.GetRecordingCanvas();
    canvas->Save();
    canvas->Translate(3, 7);
    const auto depth = canvas->GetSaveCount();
    const auto matrix = canvas->GetTotalMatrix();
    bool threw = false;
    {
      AllocationFailure failure(allowance);
      try {
        canvas->Save();
      } catch (const std::bad_alloc&) {
        threw = true;
      }
    }
    if (threw) {
      ++failures;
      EXPECT_EQ(canvas->GetSaveCount(), depth);
      EXPECT_EQ(canvas->GetTotalMatrix(), matrix);
    } else {
      completed = true;
      canvas->Restore();
    }
    canvas->Restore();
    auto display = recorder.FinishRecording();
    ASSERT_NE(display, nullptr);
    // Initial Save/Translate/Restore; only a successful second Save may add
    // its matching Save/Restore pair. No mutation is retried after refusal.
    EXPECT_EQ(display->OpCount(), threw ? 3u : 5u);
    if (completed) break;
  }
  EXPECT_TRUE(completed);
  EXPECT_GE(failures, 3u);
}

#if defined(SKITY_RECOVERY_TEST_IO)
namespace {
class RecoveryWriteStream final : public skity::WriteStream {
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
}  // namespace
TEST(CanvasSaveAllocation, RefusedIOSaveLeavesNoPendingRestore) {
  skity::RecordPlayback reference(32, 32);
  reference.EndRecording();
  RecoveryWriteStream expected;
  reference.Serialize(expected, nullptr, nullptr);
  unsigned failures = 0;
  bool completed = false;
  for (int allowance = 0; allowance < 64; ++allowance) {
    SCOPED_TRACE(allowance);
    skity::RecordPlayback playback(32, 32);
    auto* canvas = playback.GetDelegateCanvas();
    bool threw = false;
    {
      AllocationFailure failure(allowance);
      try {
        canvas->Save();
      } catch (const std::bad_alloc&) {
        threw = true;
      }
    }
    if (threw) {
      ++failures;
      EXPECT_EQ(canvas->GetSaveCount(), 1);
    } else {
      completed = true;
      canvas->Restore();
    }
    playback.EndRecording();
    RecoveryWriteStream actual;
    playback.Serialize(actual, nullptr, nullptr);
    if (threw) EXPECT_EQ(actual.data, expected.data);
    if (completed) break;
  }
  EXPECT_TRUE(completed);
  EXPECT_GE(failures, 4u);
}
#endif

TEST(AllocationTestSupport, PreservesNothrowAndAlignedOperatorContracts) {
  void* refused = nullptr;
  {
    AllocationFailure failure(0);
    refused = ::operator new(64, std::nothrow);
  }
  EXPECT_EQ(refused, nullptr);
  {
    AllocationFailure failure(0);
    refused = ::operator new(64, std::align_val_t{64}, std::nothrow);
  }
  EXPECT_EQ(refused, nullptr);
  void* aligned = ::operator new(64, std::align_val_t{64});
  ASSERT_NE(aligned, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(aligned) % 64, 0u);
  ::operator delete(aligned, std::align_val_t{64});
}

TEST(CanvasLayerAllocation, RefusedLayerPreservesCallerState) {
  unsigned failures = 0;
  bool completed = false;
  for (int allowance = 0; allowance < 64; ++allowance) {
    SCOPED_TRACE(allowance);
    skity::Bitmap bitmap(32, 32, skity::AlphaType::kPremul_AlphaType);
    auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
    ASSERT_NE(canvas, nullptr);
    canvas->Save();
    canvas->Translate(3, 7);
    canvas->ClipRect(skity::Rect::MakeXYWH(4, 4, 16, 16));
    const auto depth = canvas->GetSaveCount();
    const auto matrix = canvas->GetTotalMatrix();
    const auto clip = canvas->GetLocalClipBounds();
    skity::Paint layer;
    layer.SetAlpha(128);
    bool threw = false;
    {
      skity::testing::AllocationFailure fault(allowance);
      try {
        canvas->SaveLayer(skity::Rect::MakeWH(32, 32), layer);
      } catch (const std::bad_alloc&) {
        threw = true;
      }
    }
    if (threw) {
      ++failures;
      EXPECT_EQ(canvas->GetSaveCount(), depth);
      EXPECT_EQ(canvas->GetTotalMatrix(), matrix);
      EXPECT_EQ(canvas->GetLocalClipBounds(), clip);
      // Avoid exercising further operations after an inconsistent refusal.
      continue;
    }
    completed = true;
    canvas->RestoreToCount(depth);
    EXPECT_EQ(canvas->GetSaveCount(), depth);
    EXPECT_EQ(canvas->GetTotalMatrix(), matrix);
    EXPECT_EQ(canvas->GetLocalClipBounds(), clip);
    break;
  }
  EXPECT_GT(failures, 0u);
  EXPECT_TRUE(completed);
}

#if defined(SKITY_CPU)
TEST(SoftwareTemporaryAllocation, IntegralRectangleUsesOneSpanAllocation) {
  skity::Path path;
  path.AddRect(skity::Rect::MakeXYWH(2.f, 3.f, 12.f, 48.f));
  skity::SWRaster raster;
  bool completed = false;
  {
    AllocationFailure fault(1);
    try {
      raster.RastePath(path, skity::Matrix{}, skity::Rect::MakeWH(64.f, 64.f));
      completed = true;
    } catch (const std::bad_alloc&) {
    }
  }
  ASSERT_TRUE(completed);
  ASSERT_EQ(raster.CurrentSpans().size(), 48u);
  for (size_t i = 0; i < raster.CurrentSpans().size(); ++i) {
    const auto& span = raster.CurrentSpans()[i];
    EXPECT_EQ(span.x, 2);
    EXPECT_EQ(span.y, 3 + static_cast<int>(i));
    EXPECT_EQ(span.len, 12);
    EXPECT_EQ(span.cover, 255);
  }
}

TEST(SoftwareTemporaryAllocation, NearestNeonRowsUseFixedScratch) {
#if defined(SKITY_ARM_NEON)
  auto source = std::make_shared<skity::Pixmap>(
      8, 1, skity::kUnpremul_AlphaType, skity::ColorType::kRGBA);
  auto* pixels = static_cast<uint8_t*>(source->WritableAddr());
  ASSERT_NE(pixels, nullptr);
  for (int x = 0; x < 8; ++x) {
    pixels[x * 4] = x * 31;
    pixels[x * 4 + 1] = 255 - x * 23;
    pixels[x * 4 + 2] = x * 17;
    pixels[x * 4 + 3] = 255;
  }
  skity::Bitmap reference(source);
  skity::Bitmap output(16, 4);
  std::vector<skity::Span> spans{
      {0, 0, 16, 255}, {0, 1, 16, 255}, {0, 2, 16, 255}, {0, 3, 16, 255}};
  skity::PixmapBrush brush(spans, &output, nullptr, skity::BlendMode::kSrc, 1.f,
                           source, skity::Matrix::Scale(1.f / 8, 1.f),
                           skity::SamplingOptions{}, skity::TileMode::kRepeat,
                           skity::TileMode::kClamp);
  bool completed = false;
  {
    AllocationFailure fault(0);
    try {
      brush.Brush();
      completed = true;
    } catch (const std::bad_alloc&) {
    }
  }
  ASSERT_TRUE(completed);
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 16; ++x)
      EXPECT_EQ(output.GetPixel(x, y), reference.GetPixel(x % 8, 0));
#else
  GTEST_SKIP() << "NEON-only fixed scratch contract";
#endif
}
TEST(SWRaster, DelegateFailurePreservesPriorSpans) {
  skity::Path first;
  first.AddRect(skity::Rect::MakeXYWH(1, 2, 3, 4));
  skity::SWRaster raster;
  raster.RastePath(first, {}, skity::Rect::MakeWH(32, 32));
  const auto published = raster.CurrentSpans();
  ASSERT_FALSE(published.empty());
  struct Refusal {};
  struct Delegate : skity::SpanBuilderDelegate {
    int calls = 0;
    void OnBuildSpan(int, int, int, uint8_t) override {
      if (++calls == 2) throw Refusal{};
    }
  } delegate;
  skity::Path next;
  next.AddRect(skity::Rect::MakeXYWH(6, 7, 8, 9));
  EXPECT_THROW(
      raster.RastePath(next, {}, skity::Rect::MakeWH(32, 32), &delegate),
      Refusal);
  EXPECT_EQ(delegate.calls, 2);
  ASSERT_EQ(raster.CurrentSpans().size(), published.size());
  for (size_t i = 0; i < published.size(); ++i) {
    EXPECT_EQ(raster.CurrentSpans()[i].x, published[i].x);
    EXPECT_EQ(raster.CurrentSpans()[i].y, published[i].y);
    EXPECT_EQ(raster.CurrentSpans()[i].len, published[i].len);
    EXPECT_EQ(raster.CurrentSpans()[i].cover, published[i].cover);
  }
}

#endif

TEST(CanvasSaveAllocation, RefusedRecordingLayerAppendsNoOperation) {
  unsigned failures = 0;
  bool completed = false;
  for (int allowance = 0; allowance < 64; ++allowance) {
    SCOPED_TRACE(allowance);
    skity::PictureRecorder recorder;
    recorder.BeginRecording(skity::Rect::MakeWH(32, 32));
    auto* canvas = recorder.GetRecordingCanvas();
    canvas->Save();
    canvas->Translate(3, 7);
    const auto depth = canvas->GetSaveCount();
    const auto matrix = canvas->GetTotalMatrix();
    bool threw = false;
    {
      AllocationFailure failure(allowance);
      try {
        canvas->SaveLayer(skity::Rect::MakeWH(32, 32), skity::Paint{});
      } catch (const std::bad_alloc&) {
        threw = true;
      }
    }
    if (threw) {
      ++failures;
      EXPECT_EQ(canvas->GetSaveCount(), depth);
      EXPECT_EQ(canvas->GetTotalMatrix(), matrix);
    } else {
      completed = true;
      canvas->Restore();
    }
    canvas->Restore();
    auto display = recorder.FinishRecording();
    ASSERT_NE(display, nullptr);
    // Initial Save/Translate/Restore; only a successful second SaveLayer may
    // add its matching Save/Restore pair. No mutation is retried after refusal.
    EXPECT_EQ(display->OpCount(), threw ? 3u : 5u);
    if (completed) break;
  }
  EXPECT_TRUE(completed);
  EXPECT_GT(failures, 0u);
}

#if defined(SKITY_RECOVERY_TEST_IO)
TEST(CanvasSaveAllocation, RefusedIOLayerLeavesNoPendingRestore) {
  skity::RecordPlayback reference(32, 32);
  reference.EndRecording();
  RecoveryWriteStream expected;
  reference.Serialize(expected, nullptr, nullptr);
  unsigned failures = 0;
  bool completed = false;
  for (int allowance = 0; allowance < 64; ++allowance) {
    SCOPED_TRACE(allowance);
    skity::RecordPlayback playback(32, 32);
    auto* canvas = playback.GetDelegateCanvas();
    bool threw = false;
    {
      AllocationFailure failure(allowance);
      try {
        canvas->SaveLayer(skity::Rect::MakeWH(32, 32), skity::Paint{});
      } catch (const std::bad_alloc&) {
        threw = true;
      }
    }
    if (threw) {
      ++failures;
      EXPECT_EQ(canvas->GetSaveCount(), 1);
    } else {
      completed = true;
      canvas->Restore();
    }
    playback.EndRecording();
    RecoveryWriteStream actual;
    playback.Serialize(actual, nullptr, nullptr);
    if (threw) EXPECT_EQ(actual.data, expected.data);
    if (completed) break;
  }
  EXPECT_TRUE(completed);
  EXPECT_GT(failures, 0u);
}

#endif

#if defined(SKITY_RECOVERY_TEST_IO)
TEST(CanvasSaveAllocation, CapacityPreparationPublishesNoBytes) {
  skity::MemoryWriter32 writer;
  writer.WriteUint32(0x12345678u);
  bool refused = false;
  {
    AllocationFailure failure(0);
    try {
      writer.PrepareWrite(8192);
    } catch (const std::bad_alloc&) {
      refused = true;
    }
  }
  EXPECT_TRUE(refused);
  EXPECT_EQ(writer.BytesWritten(), 4u);
  EXPECT_EQ(writer.ReadAt<uint32_t>(0), 0x12345678u);
  EXPECT_THROW(writer.PrepareWrite(std::numeric_limits<size_t>::max()),
               std::length_error);
  EXPECT_EQ(writer.BytesWritten(), 4u);
  writer.PrepareWrite(8192);
  EXPECT_EQ(writer.BytesWritten(), 4u);
  {
    AllocationFailure failure(0);
    for (uint32_t i = 0; i < 2048; ++i) writer.WriteUint32(i);
  }
  EXPECT_EQ(writer.BytesWritten(), 8196u);
  EXPECT_EQ(writer.ReadAt<uint32_t>(0), 0x12345678u);
  EXPECT_EQ(writer.ReadAt<uint32_t>(8192), 2047u);
}
#endif

namespace {
class TrackingArenaAllocator final : public skity::Allocator {
 public:
  ~TrackingArenaAllocator() override {
    // Keep a failing regression bounded; assertions observe the missing Free
    // before this test-only cleanup reclaims any unregistered raw block.
    for (void* pointer : live) std::free(pointer);
  }
  skity::Block Alloc(size_t bytes) override {
    auto* pointer = static_cast<uint8_t*>(std::malloc(bytes));
    if (!pointer) throw std::bad_alloc();
    for (auto& slot : live)
      if (!slot) {
        slot = pointer;
        ++allocations;
        return {pointer, bytes};
      }
    std::free(pointer);
    throw std::bad_alloc();
  }
  void Free(skity::Block& block) override {
    for (auto& slot : live)
      if (slot == block.head) {
        slot = nullptr;
        break;
      }
    std::free(block.head);
    ++releases;
  }
  unsigned allocations = 0;
  unsigned releases = 0;
  void* live[8] = {};
};
}  // namespace
TEST(ArenaAllocation, RefusedBlockRegistrationReleasesStorage) {
  for (size_t bytes : {16u, 128u}) {
    auto owner = std::make_shared<TrackingArenaAllocator>();
    skity::Arena arena(64, owner);
    bool refused = false;
    {
      AllocationFailure failure(0);
      try {
        arena.Allocate(bytes, 8);
      } catch (const std::bad_alloc&) {
        refused = true;
      }
    }
    EXPECT_TRUE(refused);
    EXPECT_EQ(owner->allocations, 1u);
    EXPECT_EQ(owner->releases, 1u);
    EXPECT_TRUE(arena.GetBlocks().empty());
  }
}
TEST(ArenaAllocation, CacheMetadataRefusalReleasesStorage) {
  auto owner = std::make_shared<TrackingArenaAllocator>();
  skity::BlockCacheAllocator cache(owner);
  auto block = cache.Alloc(skity::kDefaultBlockSize);
  bool escaped = false;
  {
    AllocationFailure failure(0);
    try {
      cache.Free(block);
    } catch (const std::bad_alloc&) {
      escaped = true;
    }
  }
  EXPECT_FALSE(escaped);
  EXPECT_EQ(owner->releases, 1u);
  EXPECT_TRUE(cache.GetBlocks().empty());
}
TEST(ArenaAllocation, RefusedFinalizerDestroysConstructedOwner) {
  struct Owner {
    explicit Owner(int* count) : count(count) { ++*count; }
    ~Owner() { --*count; }
    int* count;
    char payload[skity::kDefaultBlockSize];
  };
  int live = 0;
  auto storage = std::make_shared<TrackingArenaAllocator>();
  skity::ArenaAllocator arena(storage);
  bool refused = false;
  {
    AllocationFailure failure(1);
    try {
      arena.Make<Owner>(&live);
    } catch (const std::bad_alloc&) {
      refused = true;
    }
  }
  EXPECT_TRUE(refused);
  EXPECT_EQ(live, 0);
  EXPECT_EQ(arena.GetFinalizersCount(), 0u);
}

TEST(ArenaAllocation, NullBlockIsRefusedBeforePublication) {
  class NullAllocator final : public skity::Allocator {
   public:
    skity::Block Alloc(size_t size) override { return {nullptr, size}; }
    void Free(skity::Block&) override { ++releases; }
    unsigned releases = 0;
  };
  for (size_t bytes : {16u, 128u}) {
    auto storage = std::make_shared<NullAllocator>();
    skity::Arena arena(64, storage);
    EXPECT_THROW(arena.Allocate(bytes, 8), std::bad_alloc);
    EXPECT_TRUE(arena.GetBlocks().empty());
    EXPECT_EQ(arena.GetCursor(), nullptr);
    EXPECT_EQ(arena.GetEnd(), nullptr);
    EXPECT_EQ(storage->releases, 0u);
  }
}
