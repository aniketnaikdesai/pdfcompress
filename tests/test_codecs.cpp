#include <gtest/gtest.h>
#include "codecs/JpegCodec.h"
#include "codecs/Jp2Codec.h"
#include "codecs/ZlibCodec.h"
#include "codecs/PngCodec.h"

#include <cstring>
#include <zlib.h>

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

// P2-T9-T03 / ESC-011: the lossless branch reports StreamFilter::FlateDecode, so
// PngCodec::encode must return an embeddable zlib/Flate stream of the raw
// samples - never a full PNG container (0x89 'PNG' + IHDR/IDAT/IEND).
TEST(CodecTest, PngEncodeIsFlateDecodable) {
    PngCodec codec;
    const int w = 16, h = 16, channels = 4;
    CompressionParams params{w, h, channels, CompressionProfile::Balanced, 75};

    std::vector<uint8_t> pixels(static_cast<size_t>(w) * h * channels);
    for (size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<uint8_t>((i * 3) % 256);
    }

    auto res = codec.encode(pixels.data(), params);
    ASSERT_FALSE(res.empty());

    const uint8_t pngSignature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    ASSERT_GE(res.size(), 8u);
    EXPECT_NE(0, std::memcmp(res.data(), pngSignature, 8))
        << "PngCodec output must not be a PNG container";

    std::vector<uint8_t> inflated(pixels.size());
    uLongf inflatedLen = static_cast<uLongf>(inflated.size());
    ASSERT_EQ(Z_OK, uncompress(inflated.data(), &inflatedLen,
                               res.data(), static_cast<uLong>(res.size())))
        << "PngCodec output must inflate as a valid zlib stream";
    ASSERT_EQ(pixels.size(), inflatedLen);
    EXPECT_EQ(0, std::memcmp(inflated.data(), pixels.data(), pixels.size()))
        << "inflated bytes must round-trip to the raw samples";
}
