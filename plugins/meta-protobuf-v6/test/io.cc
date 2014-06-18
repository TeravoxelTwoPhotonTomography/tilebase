// solves a std::tuple problem in vs2012
#define GTEST_HAS_TR1_TUPLE     0
#define GTEST_USE_OWN_TR1_TUPLE 1
#include <gtest/gtest.h>
#include "tilebase.h"
#include "plugins/meta-protobuf-v6/config.h"


struct FetchProtobufV5: public testing::Test
{
  void SetUp()
  { ndioAddPluginPath(ND_ROOT_DIR"/bin/plugins");
  }
};

TEST_F(FetchProtobufV5,NonExistent)
{ tiles_t tiles;
  EXPECT_EQ((void*)NULL,tiles=TileBaseOpen("totally.not.here",NULL));
  EXPECT_EQ(0,TileBaseCount(tiles));
  TileBaseClose(tiles);
}

TEST_F(FetchProtobufV5,AutoDetect)
{ tiles_t tiles;
  EXPECT_NE((void*)NULL,tiles=TileBaseOpen(PBUFV6_TEST_DATA_PATH "/06087",""));
  EXPECT_EQ(1,TileBaseCount(tiles));
  TileBaseClose(tiles);
}

TEST_F(FetchProtobufV5,ByName)
{ tiles_t tiles;
  EXPECT_NE((void*)NULL,tiles=TileBaseOpen(PBUFV6_TEST_DATA_PATH "/06087","fetch.protobuf.v6"));
  EXPECT_EQ(1,TileBaseCount(tiles));
  TileBaseClose(tiles);
}
