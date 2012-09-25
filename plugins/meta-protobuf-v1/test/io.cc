#include <gtest/gtest.h>
#include "tilebase.h"
#include "plugins/meta-protobuf-v1/config.h"

struct FetchProtobufV1: public testing::Test 
{
  void SetUp()
  { ndioAddPluginPath(ND_ROOT_DIR"/bin/plugins");
  }
};

TEST_F(FetchProtobufV1,NonExistent)
{ tiles_t tiles;
  EXPECT_EQ((void*)NULL,tiles=TileBaseOpen("totally.not.here",NULL));
  EXPECT_EQ(0,TileBaseCount(tiles));
  TileBaseClose(tiles);
}

TEST_F(FetchProtobufV1,AutoDetect)
{ tiles_t tiles;
  EXPECT_NE((void*)NULL,tiles=TileBaseOpen(PBUFV1_TEST_DATA_PATH "/00038",""));
  EXPECT_EQ(1,TileBaseCount(tiles));
  TileBaseClose(tiles);
}

TEST_F(FetchProtobufV1,ByName)
{ tiles_t tiles;
  EXPECT_NE((void*)NULL,tiles=TileBaseOpen(PBUFV1_TEST_DATA_PATH "/00038","fetch.protobuf.v1"));
  EXPECT_EQ(1,TileBaseCount(tiles));
  TileBaseClose(tiles);
}