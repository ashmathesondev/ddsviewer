#include <gtest/gtest.h>
#include "TextureView.h"

TEST(TextureView, Defaults) {
    TextureView v;
    EXPECT_EQ(v.mip,   0);
    EXPECT_EQ(v.slice, 0);
    EXPECT_EQ(v.face,  0);
    EXPECT_TRUE(v.r);
    EXPECT_TRUE(v.g);
    EXPECT_TRUE(v.b);
    EXPECT_FALSE(v.a);
    EXPECT_FLOAT_EQ(v.zoom, 1.0f);
    EXPECT_FLOAT_EQ(v.panX, 0.0f);
    EXPECT_FLOAT_EQ(v.panY, 0.0f);
    EXPECT_FLOAT_EQ(v.exposure, 0.0f);
}

TEST(TextureView, AnyChannelEnabled_AllOff) {
    TextureView v;
    v.r = v.g = v.b = v.a = false;
    EXPECT_FALSE(AnyChannelEnabled(v));
}

TEST(TextureView, AnyChannelEnabled_OneOn) {
    TextureView v;
    v.r = v.g = v.b = v.a = false;
    v.b = true;
    EXPECT_TRUE(AnyChannelEnabled(v));
}

TEST(TextureView, AnyChannelEnabled_DefaultHasRGBOn) {
    TextureView v;
    EXPECT_TRUE(AnyChannelEnabled(v));
}

TEST(TextureView, ClampView_MipBelowZero) {
    TextureView v;
    v.mip = -1;
    ClampView(v, 4, 1, 1);
    EXPECT_EQ(v.mip, 0);
}

TEST(TextureView, ClampView_MipAboveMax) {
    TextureView v;
    v.mip = 99;
    ClampView(v, 4, 1, 1);
    EXPECT_EQ(v.mip, 3);
}

TEST(TextureView, ClampView_ZoomTooSmall) {
    TextureView v;
    v.zoom = 0.0f;
    ClampView(v, 1, 1, 1);
    EXPECT_FLOAT_EQ(v.zoom, 0.05f);
}

TEST(TextureView, ClampView_ZoomTooLarge) {
    TextureView v;
    v.zoom = 999.0f;
    ClampView(v, 1, 1, 1);
    EXPECT_FLOAT_EQ(v.zoom, 32.0f);
}

TEST(TextureView, ClampView_SliceAboveMax) {
    TextureView v;
    v.slice = 5;
    ClampView(v, 1, 3, 1);
    EXPECT_EQ(v.slice, 2);
}

TEST(TextureView, ClampView_FaceAboveMax) {
    TextureView v;
    v.face = 10;
    ClampView(v, 1, 1, 6);
    EXPECT_EQ(v.face, 5);
}

TEST(TextureView, ClampView_ValidValuesUnchanged) {
    TextureView v;
    v.mip = 2; v.slice = 1; v.face = 3; v.zoom = 2.0f;
    ClampView(v, 5, 4, 6);
    EXPECT_EQ(v.mip,   2);
    EXPECT_EQ(v.slice, 1);
    EXPECT_EQ(v.face,  3);
    EXPECT_FLOAT_EQ(v.zoom, 2.0f);
}
