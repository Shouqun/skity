// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/text/font.hpp>
#include <skity/text/typeface.hpp>
#include <vector>

#include "concurrent_runner.h"
#include "src/text/scaler_context_cache.hpp"
#include "src/text/scaler_context_desc.hpp"

namespace skity {
namespace {
class FontPathVisitTest : public ::testing::Test {
 protected:
  void SetUp() override {
    face = Typeface::MakeFromFile(SKITY_FONT_DIR
                                  "fonts/resources/Roboto-Regular.ttf");
    ASSERT_NE(face, nullptr);
    ids[0] = face->UnicharToGlyph('A');
    ids[1] = face->UnicharToGlyph('g');
    ASSERT_NE(ids[0], 0);
    ASSERT_NE(ids[1], 0);
  }
  std::shared_ptr<Typeface> face;
  GlyphID ids[2] = {};
};

TEST_F(FontPathVisitTest, EmptyAndInvalidInputsDoNotVisit) {
  Font font(face, 24.f);
  GlyphData sentinel;
  const GlyphData* scratch[] = {&sentinel, &sentinel};
  int calls = 0;
  auto visit = [&](const GlyphData* const[], uint32_t) {
    ++calls;
    return true;
  };
  EXPECT_TRUE(font.VisitGlyphPaths(nullptr, 0, nullptr, visit));
  EXPECT_FALSE(font.VisitGlyphPaths(ids, 2, nullptr, visit));
  EXPECT_FALSE(font.VisitGlyphPaths(nullptr, 2, scratch, visit));
  EXPECT_EQ(scratch[0], nullptr);
  EXPECT_EQ(scratch[1], nullptr);
  scratch[0] = scratch[1] = &sentinel;
  EXPECT_FALSE(font.VisitGlyphPaths(ids, 2, scratch, nullptr, nullptr));
  EXPECT_EQ(scratch[0], nullptr);
  EXPECT_EQ(scratch[1], nullptr);
  EXPECT_EQ(calls, 0);
}

bool FunctionPathVisitor(const GlyphData* const data[], uint32_t count) {
  return count == 2 && data[0] != nullptr && data[1] != nullptr;
}

TEST_F(FontPathVisitTest, AcceptsFunctionsAndRefusesNullFunctionPointers) {
  Font font(face, 24.f);
  const GlyphData* scratch[2] = {};
  EXPECT_TRUE(font.VisitGlyphPaths(ids, 2, scratch, FunctionPathVisitor));
  auto function = &FunctionPathVisitor;
  EXPECT_TRUE(font.VisitGlyphPaths(ids, 2, scratch, function));
  function = nullptr;
  EXPECT_FALSE(font.VisitGlyphPaths(ids, 2, scratch, function));
  EXPECT_EQ(scratch[0], nullptr);
  EXPECT_EQ(scratch[1], nullptr);
}

TEST_F(FontPathVisitTest, CopiesCanonicalPathsWithinScope) {
  Font font(face, 24.f, 1.125f, .125f);
  const GlyphData* scratch[2] = {};
  std::vector<Path> paths;
  ASSERT_TRUE(font.VisitGlyphPaths(
      ids, 2, scratch, [&](const GlyphData* const data[], uint32_t count) {
        EXPECT_EQ(count, 2u);
        for (uint32_t i = 0; i < count; ++i) {
          EXPECT_NE(data[i], nullptr);
          if (!data[i]) return false;
          EXPECT_EQ(data[i]->Id(), ids[i]);
          paths.push_back(data[i]->GetPath());
        }
        return true;
      }));
  ASSERT_EQ(paths.size(), 2u);
  EXPECT_EQ(scratch[0], nullptr);
  EXPECT_EQ(scratch[1], nullptr);
  ASSERT_TRUE(font.VisitGlyphPaths(
      ids, 2, scratch, [&](const GlyphData* const data[], uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
          EXPECT_FALSE(paths[i].IsEmpty());
          EXPECT_TRUE(paths[i] == data[i]->GetPath());
        }
        return true;
      }));
}

TEST_F(FontPathVisitTest, RetainsOwnersAcrossReentrantPurge) {
  Font font(face, 24.f);
  const auto desc = ScalerContextDesc::MakeCanonicalized(font, Paint());
  auto* cache = ScalerContextCache::GlobalScalerContextCache();
  std::weak_ptr<Typeface> weak_face = face;
  std::weak_ptr<ScalerContextContainer> weak_container =
      cache->FindOrCreateScalerContext(desc, face);
  const GlyphData* scratch[2] = {};
  std::vector<Path> copies;
  ASSERT_TRUE(font.VisitGlyphPaths(
      ids, 2, scratch, [&](const GlyphData* const data[], uint32_t count) {
        font.SetTypeface(nullptr);
        face.reset();
        cache->PurgeByTypeface(desc.typeface_id);
        // Check owners before any borrowed read, including negative controls.
        EXPECT_FALSE(weak_face.expired());
        EXPECT_FALSE(weak_container.expired());
        if (weak_face.expired() || weak_container.expired()) return false;
        for (uint32_t i = 0; i < count; ++i)
          copies.push_back(data[i]->GetPath());
        return true;
      }));
  EXPECT_TRUE(weak_face.expired());
  EXPECT_TRUE(weak_container.expired());
  EXPECT_EQ(scratch[0], nullptr);
  EXPECT_EQ(scratch[1], nullptr);
  ASSERT_EQ(copies.size(), 2u);
  EXPECT_FALSE(copies[0].IsEmpty());
  EXPECT_FALSE(copies[1].IsEmpty());
}

TEST_F(FontPathVisitTest, FalseReturnClearsScratch) {
  Font font(face, 24.f);
  const GlyphData* scratch[2] = {};
  bool called = false;
  EXPECT_FALSE(font.VisitGlyphPaths(
      ids, 2, scratch, [&](const GlyphData* const data[], uint32_t count) {
        called = true;
        EXPECT_EQ(count, 2u);
        EXPECT_NE(data[0], nullptr);
        return false;
      }));
  EXPECT_TRUE(called);
  EXPECT_EQ(scratch[0], nullptr);
  EXPECT_EQ(scratch[1], nullptr);
}

TEST_F(FontPathVisitTest, NestedVisitHasIndependentScratch) {
  Font font(face, 24.f);
  const GlyphData* outer[2] = {};
  const GlyphData* inner[2] = {};
  ASSERT_TRUE(font.VisitGlyphPaths(
      ids, 2, outer, [&](const GlyphData* const data[], uint32_t) {
        const Path copy = data[0]->GetPath();
        bool result = font.VisitGlyphPaths(
            ids, 2, inner, [&](const GlyphData* const nested[], uint32_t) {
              EXPECT_TRUE(copy == nested[0]->GetPath());
              return true;
            });
        EXPECT_EQ(inner[0], nullptr);
        EXPECT_EQ(inner[1], nullptr);
        EXPECT_NE(outer[0], nullptr);
        return result;
      }));
  EXPECT_EQ(outer[0], nullptr);
  EXPECT_EQ(outer[1], nullptr);
}

TEST_F(FontPathVisitTest, CachePurgeAcrossThreadsRetainsNonemptyPaths) {
  const Font font(face, 24.f);
  auto* cache = ScalerContextCache::GlobalScalerContextCache();
  const auto id = face->TypefaceId();
  ConcurrentRunner runner(8, 32);
  runner.Run([&](int) {
    const GlyphData* scratch[2] = {};
    EXPECT_TRUE(font.VisitGlyphPaths(
        ids, 2, scratch, [&](const GlyphData* const data[], uint32_t count) {
          cache->PurgeByTypeface(id);
          for (uint32_t i = 0; i < count; ++i) {
            EXPECT_NE(data[i], nullptr);
            EXPECT_FALSE(data[i]->GetPath().IsEmpty());
          }
          return true;
        }));
    EXPECT_EQ(scratch[0], nullptr);
    EXPECT_EQ(scratch[1], nullptr);
  });
}

#if defined(SKITY_CPP_ALLOCATION_RECOVERY)
TEST_F(FontPathVisitTest, UnwindingClearsScratchAndReleasesOwners) {
  Font font(face, 24.f);
  const auto desc = ScalerContextDesc::MakeCanonicalized(font, Paint());
  auto* cache = ScalerContextCache::GlobalScalerContextCache();
  std::weak_ptr<Typeface> weak_face = face;
  std::weak_ptr<ScalerContextContainer> weak_container =
      cache->FindOrCreateScalerContext(desc, face);
  const GlyphData* scratch[2] = {};
  EXPECT_THROW(
      font.VisitGlyphPaths(ids, 2, scratch,
                           [&](const GlyphData* const[], uint32_t) -> bool {
                             font.SetTypeface(nullptr);
                             face.reset();
                             cache->PurgeByTypeface(desc.typeface_id);
                             throw 41;
                           }),
      int);
  EXPECT_EQ(scratch[0], nullptr);
  EXPECT_EQ(scratch[1], nullptr);
  EXPECT_TRUE(weak_face.expired());
  EXPECT_TRUE(weak_container.expired());
}
#endif
}  // namespace
}  // namespace skity
