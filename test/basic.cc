#include <gtest/gtest.h>
#include "tilebase.h"
#include "config.h"

TEST(TileBase,OpenNonExistent)
{ tiles_t tiles;
  EXPECT_EQ((void*)NULL,tiles=TileBaseOpen("totally.not.here"));
  EXPECT_EQ(0,TileBaseCount(tiles));
  TileBaseClose(tiles);
}

TEST(TileBase,Open)
{ tiles_t tiles;
  EXPECT_NE((void*)NULL,tiles=TileBaseOpen(TILEBASE_TEST_DATA_PATH "/a"));
  EXPECT_EQ(3,TileBaseCount(tiles));
  TileBaseClose(tiles);
}