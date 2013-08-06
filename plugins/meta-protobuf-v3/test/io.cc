#include <gtest/gtest.h>
#include "tilebase.h"
#include "plugins/meta-protobuf-v3/config.h"

struct FetchProtobufV3: public testing::Test 
{
  void SetUp()
  { ndioAddPluginPath(ND_ROOT_DIR"/bin/plugins");
  }
};

TEST_F(FetchProtobufV3,NonExistent)
{ tiles_t tiles;
  EXPECT_EQ((void*)NULL,tiles=TileBaseOpen("totally.not.here",NULL));
  EXPECT_EQ(0,TileBaseCount(tiles));
  TileBaseClose(tiles);
}

TEST_F(FetchProtobufV3,AutoDetect)
{ tiles_t tiles;
  EXPECT_NE((void*)NULL,tiles=TileBaseOpen(PBUFV3_TEST_DATA_PATH "/00100",""));
  EXPECT_EQ(1,TileBaseCount(tiles));
  TileBaseClose(tiles);
}

TEST_F(FetchProtobufV3,ByName)
{ tiles_t tiles;
  EXPECT_NE((void*)NULL,tiles=TileBaseOpen(PBUFV3_TEST_DATA_PATH "/00100","fetch.protobuf.v3"));
  EXPECT_EQ(1,TileBaseCount(tiles));
  TileBaseClose(tiles);
}