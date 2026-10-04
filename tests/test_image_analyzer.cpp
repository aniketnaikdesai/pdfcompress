#include <gtest/gtest.h>
#include "core/ImageAnalyzer.h"

using namespace pdfcompress;

TEST(ImageAnalyzerTest, SolidColorIsMonochrome) {
    std::vector<uint8_t> pixels(100 * 100 * 3, 0);
    ImageMetadata meta;
    meta.widthPx = 100; meta.heightPx = 100;
    auto result = ImageAnalyzer::analyze(pixels.data(), 100, 100, 3, meta);
    EXPECT_EQ(result.classification, ImageClassification::Monochrome);
    EXPECT_TRUE(result.isEffectivelyGrayscale);
    EXPECT_LE(result.uniqueColorsSampled, 4);
}

TEST(ImageAnalyzerTest, PhotoClassification) {
    std::vector<uint8_t> pixels(200 * 200 * 3);
    srand(42);
    for (auto& p : pixels) p = rand() % 256;
    ImageMetadata meta;
    meta.widthPx = 200; meta.heightPx = 200;
    auto result = ImageAnalyzer::analyze(pixels.data(), 200, 200, 3, meta);
    EXPECT_EQ(result.classification, ImageClassification::Photo);
    EXPECT_GT(result.entropy, 5.0f);
}

TEST(ImageAnalyzerTest, ScreenshotClassification) {
    std::vector<uint8_t> pixels(400 * 300 * 3);
    for (int y = 0; y < 300; ++y) {
        for (int x = 0; x < 400; ++x) {
            int idx = (y * 400 + x) * 3;
            if (y < 150 && x < 200) { pixels[idx]=255; pixels[idx+1]=0; pixels[idx+2]=0; }
            else if (y < 150) { pixels[idx]=0; pixels[idx+1]=255; pixels[idx+2]=0; }
            else if (x < 200) { pixels[idx]=0; pixels[idx+1]=0; pixels[idx+2]=255; }
            else { pixels[idx]=255; pixels[idx+1]=255; pixels[idx+2]=0; }
        }
    }
    ImageMetadata meta;
    meta.widthPx = 400; meta.heightPx = 300;
    auto result = ImageAnalyzer::analyze(pixels.data(), 400, 300, 3, meta);
    EXPECT_EQ(result.classification, ImageClassification::Screenshot);
}

TEST(ImageAnalyzerTest, NullPixelsReturnsUnknown) {
    ImageMetadata meta;
    auto result = ImageAnalyzer::analyze(nullptr, 0, 0, 3, meta);
    EXPECT_EQ(result.classification, ImageClassification::Unknown);
}
