#include <gtest/gtest.h>
#include "tilebase.h"
#include "plugins/meta-protobuf-v5/config.h"

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
  EXPECT_NE((void*)NULL,tiles=TileBaseOpen(PBUFV5_TEST_DATA_PATH "/02020",""));
  EXPECT_EQ(1,TileBaseCount(tiles));
  TileBaseClose(tiles);
}

TEST_F(FetchProtobufV5,ByName)
{ tiles_t tiles;
  EXPECT_NE((void*)NULL,tiles=TileBaseOpen(PBUFV5_TEST_DATA_PATH "/02020","fetch.protobuf.v5"));
  EXPECT_EQ(1,TileBaseCount(tiles));
  TileBaseClose(tiles);
}
