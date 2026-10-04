#include <gtest/gtest.h>
#include "codecs/JpegCodec.h"
#include "codecs/Jp2Codec.h"
#include "codecs/ZlibCodec.h"
#include "codecs/PngCodec.h"

using namespace pdfcompress;

TEST(CodecTest, JpegEncode) {
    JpegCodec codec;
    CompressionParams params{10, 10, 3, CompressionProfile::Balanced, 75};
    std::vector<uint8_t> pixels(10 * 10 * 3, 128);
    auto res = codec.encode(pixels.data(), params);
    ASSERT_FALSE(res.empty());
    EXPECT_EQ(res[0], 0xFF);
    EXPECT_EQ(res[1], 0xD8); // JPEG SOI
}

TEST(CodecTest, ZlibEncode) {
    ZlibCodec codec;
    CompressionParams params{10, 10, 3, CompressionProfile::Balanced, 0};
    std::vector<uint8_t> pixels(10 * 10 * 3, 128);
    auto res = codec.encode(pixels.data(), params);
    ASSERT_FALSE(res.empty());
}
