#include <gtest/gtest.h>
#include "core/DecisionEngine.h"

using namespace pdfcompress;

TEST(DecisionEngineTest, PhotoUsesJpegInBalanced) {
    DecisionEngine engine(CompressionProfile::Balanced);
    AnalysisResult analysis;
    analysis.classification = ImageClassification::Photo;
    StreamFilter outFilter;
    std::vector<uint8_t> data(100 * 100 * 3, 128);
    auto result = engine.compress(data.data(), 100, 100, 3, analysis, outFilter, 70);
    EXPECT_EQ(outFilter, StreamFilter::DCTDecode);
    EXPECT_FALSE(result.empty());
}

TEST(DecisionEngineTest, MonochromeUsesZlib) {
    DecisionEngine engine(CompressionProfile::Balanced);
    AnalysisResult analysis;
    analysis.classification = ImageClassification::Monochrome;
    StreamFilter outFilter;
    std::vector<uint8_t> data(50 * 50 * 3, 0);
    auto result = engine.compress(data.data(), 50, 50, 3, analysis, outFilter, 0);
    EXPECT_EQ(outFilter, StreamFilter::FlateDecode);
}

// P2-T1-T01 / AC-C1: the quality slider only affects the lossy classes.
TEST(DecisionEngine, SliderOnlyAffectsLossyClasses) {
    DecisionEngine engine(CompressionProfile::Balanced);

    auto filterFor = [&](ImageClassification cls, int qualityHint) {
        AnalysisResult analysis;
        analysis.classification = cls;
        StreamFilter outFilter = StreamFilter::Unknown;
        std::vector<uint8_t> data(32 * 32 * 3, 128);
        engine.compress(data.data(), 32, 32, 3, analysis, outFilter, qualityHint);
        return outFilter;
    };

    // Lossless classifications ignore the slider entirely.
    EXPECT_EQ(filterFor(ImageClassification::ScannedText, 70), StreamFilter::JPXDecode);
    EXPECT_EQ(filterFor(ImageClassification::ScannedText, 0), StreamFilter::JPXDecode);
    EXPECT_EQ(filterFor(ImageClassification::Monochrome, 70), StreamFilter::FlateDecode);
    EXPECT_EQ(filterFor(ImageClassification::Monochrome, 0), StreamFilter::FlateDecode);

    // Lossy classifications select DCTDecode while the slider is active.
    EXPECT_EQ(filterFor(ImageClassification::Photo, 70), StreamFilter::DCTDecode);
    EXPECT_EQ(filterFor(ImageClassification::Screenshot, 70), StreamFilter::DCTDecode);
    EXPECT_EQ(filterFor(ImageClassification::LineArt, 70), StreamFilter::DCTDecode);
}

// P2-T1-T02 / AC-C2: alpha-bearing images route to the PNG/FlateDecode path.
TEST(DecisionEngine, PngSelectedForAlpha) {
    DecisionEngine engine(CompressionProfile::Balanced);

    AnalysisResult analysis;
    analysis.classification = ImageClassification::Photo;

    StreamFilter outFilter = StreamFilter::Unknown;
    std::vector<uint8_t> data(16 * 16 * 4, 200);
    auto result = engine.compress(data.data(), 16, 16, 4, analysis, outFilter,
                                  /*qualityHint=*/70, /*hasAlpha=*/true);

    EXPECT_EQ(outFilter, StreamFilter::FlateDecode);
    EXPECT_FALSE(result.empty());
}

// P2-T1-T03 / AC-C2: MaxQuality without a slider uses the lossless PNG path
// for Screenshot/LineArt instead of JPEG.
TEST(DecisionEngine, MaxQualityLosslessUsesPng) {
    DecisionEngine engine(CompressionProfile::MaxQuality);

    auto filterFor = [&](ImageClassification cls, int qualityHint) {
        AnalysisResult analysis;
        analysis.classification = cls;
        StreamFilter outFilter = StreamFilter::Unknown;
        std::vector<uint8_t> data(16 * 16 * 3, 100);
        auto bytes = engine.compress(data.data(), 16, 16, 3, analysis, outFilter, qualityHint);
        EXPECT_FALSE(bytes.empty());
        return outFilter;
    };

    EXPECT_EQ(filterFor(ImageClassification::Screenshot, 0), StreamFilter::FlateDecode);
    EXPECT_EQ(filterFor(ImageClassification::LineArt, 0), StreamFilter::FlateDecode);
    EXPECT_EQ(filterFor(ImageClassification::Screenshot, 70), StreamFilter::DCTDecode);
    EXPECT_EQ(filterFor(ImageClassification::LineArt, 70), StreamFilter::DCTDecode);
}
