/**
 * \file
 * Tests for AABB operations.
 * @cond TESTS
 */
#include <gtest/gtest.h>
#include "tilebase.h"

#define countof(e) (sizeof(e)/sizeof(*e))

class AABB:public ::testing::Test
{ public:
  aabb_t a,b,c;

  AABB() : a(0),b(0),c(0) {}
  void SetUp() 
  { int64_t oa[] = {10,20,30},
            sa[] = {100,200,300}, //110,220,330
            ob[] = {1,2},
            sb[] = {1000,2000},
            oc[] = {-11, 21,-13},
            sc[] = { 50, 60, 70}; // 39,81,57
    EXPECT_TRUE(a=AABBSet(NULL,countof(sa),oa,sa));
    EXPECT_TRUE(b=AABBSet(NULL,countof(sb),ob,sb));
    EXPECT_TRUE(c=AABBSet(NULL,countof(sc),oc,sc));
  }
  void TearDown()
  { AABBFree(a);
    AABBFree(b);
    AABBFree(c);
  }
};

TEST_F(AABB,Copy)
{ AABBCopy(b,a);
  EXPECT_TRUE(AABBSame(a,b));
}

TEST_F(AABB,Subdivide)
{ aabb_t quad[4]={0},expt=0;
  int64_t s[]={50,100,300},
          o[4][3]={
            {10,20 ,30},
            {60,20 ,30},
            {10,120,30},
            {60,120,30}};
  EXPECT_EQ(a,AABBBinarySubdivision(quad,4,a));
  for(int i=0;i<4;++i)
  { EXPECT_EQ(expt,expt=AABBSet(expt,3,o[i],s));
    EXPECT_TRUE(AABBSame(expt,quad[i]));
  }
  for(int i=0;i<4;++i)
    AABBFree(quad[i]);
}

TEST_F(AABB,UnionIP)
{ int64_t eo[]={-11,20 ,-13},
          es[]={121,200,343};
  aabb_t e;
  EXPECT_TRUE(e=AABBSet(NULL,countof(eo),eo,es));
  EXPECT_EQ(a,AABBUnionIP(a,c));
  EXPECT_TRUE(AABBSame(a,e));
  AABBFree(e);
}

TEST_F(AABB,Hit)
{ EXPECT_TRUE(AABBHit(a,c));
  EXPECT_TRUE(AABBHit(c,a));
  EXPECT_FALSE(AABBHit(a,b));
  { int64_t o[]={38,81,56};
    EXPECT_EQ(a,AABBSet(a,countof(o),o,0));
    EXPECT_FALSE(AABBHit(a,c));
  }
}
///@endcond