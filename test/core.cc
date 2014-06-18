/**
 * \file
 * Tests: Tilebase core operations
 * @cond TESTS
 */

// solves a std::tuple problem in vs2012
#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1

#include <gtest/gtest.h>
#include "tilebase.h"
#include "config.h"
#include "nd.h"

struct TileBase:public testing::Test
{ tiles_t tiles;
  TileBase() : tiles(0) {}
  void SetUp()
  { ndioAddPluginPath(ND_ROOT_DIR"/bin/plugins");
    ndioPreloadPlugins();
    EXPECT_TRUE(tiles=TileBaseOpen(TILEBASE_TEST_DATA_PATH,NULL));
  }
  void TearDown(void)
  { TileBaseClose(tiles);
  }
};

TEST_F(TileBase,Count)
{ EXPECT_EQ(1,TileBaseCount(tiles));
}

TEST_F(TileBase,AABB)
{ for(size_t i=0;i<TileBaseCount(tiles);++i)
    EXPECT_TRUE(TileAABB(TileBaseArray(tiles)[i]));
}

TEST_F(TileBase,File)
{ for(size_t i=0;i<TileBaseCount(tiles);++i)
    EXPECT_TRUE(TileFile(TileBaseArray(tiles)[i]));
}

TEST_F(TileBase,Bounds)
{ aabb_t out;
  EXPECT_TRUE(out=TileBaseAABB(tiles));
  AABBFree(out);
}
///@endcond