#include <gtest/gtest.h>
#include "tilebase.h"
#include "plugins/meta-protobuf-v0/config.h"

struct FetchProtobufV0: public testing::Test 
{
  void SetUp()
  { ndioAddPluginPath(ND_ROOT_DIR"/bin/plugins");
  }
};

TEST_F(FetchProtobufV0,NonExistent)
{ tiles_t tiles;
  EXPECT_EQ((void*)NULL,tiles=TileBaseOpen("totally.not.here",NULL));
  EXPECT_EQ(0,TileBaseCount(tiles));
  TileBaseClose(tiles);
}

TEST_F(FetchProtobufV0,AutoDetect)
{ tiles_t tiles;
  EXPECT_NE((void*)NULL,tiles=TileBaseOpen(PBUFV0_TEST_DATA_PATH "/00495",""));
  EXPECT_EQ(1,TileBaseCount(tiles));
  TileBaseClose(tiles);
}

TEST_F(FetchProtobufV0,ByName)
{ tiles_t tiles;
  EXPECT_NE((void*)NULL,tiles=TileBaseOpen(PBUFV0_TEST_DATA_PATH "/00495","fetch.protobuf.v0"));
  EXPECT_EQ(1,TileBaseCount(tiles));
  TileBaseClose(tiles);
}