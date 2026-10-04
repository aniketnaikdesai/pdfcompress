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
