#include <gtest/gtest.h>
#include "core/PDFOptimizer.h"

using namespace pdfcompress;

TEST(ProfileDefaults, MaxQualityStripsNothing) {
    OptimizationOptions opts = OptimizationOptions::forProfile(CompressionProfile::MaxQuality);
    EXPECT_FALSE(opts.stripLinks);
    EXPECT_FALSE(opts.stripOtherAnnotations);
    EXPECT_FALSE(opts.stripBookmarks);
    EXPECT_FALSE(opts.stripForms);
    EXPECT_FALSE(opts.stripJavaScript);
    EXPECT_FALSE(opts.stripNamedDestinations);
    EXPECT_FALSE(opts.stripMetadata);
    EXPECT_FALSE(opts.linearize);
    EXPECT_TRUE(opts.recompressFlate);
    EXPECT_TRUE(opts.deduplicateStreams);
}

TEST(ProfileDefaults, BalancedStripsJsAndMetadataOnly) {
    OptimizationOptions opts = OptimizationOptions::forProfile(CompressionProfile::Balanced);
    EXPECT_FALSE(opts.stripLinks);
    EXPECT_FALSE(opts.stripOtherAnnotations);
    EXPECT_FALSE(opts.stripBookmarks);
    EXPECT_FALSE(opts.stripForms);
    EXPECT_TRUE(opts.stripJavaScript);
    EXPECT_FALSE(opts.stripNamedDestinations);
    EXPECT_TRUE(opts.stripMetadata);
    EXPECT_FALSE(opts.linearize);
    EXPECT_TRUE(opts.recompressFlate);
    EXPECT_TRUE(opts.deduplicateStreams);
}

TEST(ProfileDefaults, MaxCompressionStripsEverything) {
    OptimizationOptions opts = OptimizationOptions::forProfile(CompressionProfile::MaxCompression);
    EXPECT_TRUE(opts.stripLinks);
    EXPECT_TRUE(opts.stripOtherAnnotations);
    EXPECT_TRUE(opts.stripBookmarks);
    EXPECT_TRUE(opts.stripForms);
    EXPECT_TRUE(opts.stripJavaScript);
    EXPECT_TRUE(opts.stripNamedDestinations);
    EXPECT_TRUE(opts.stripMetadata);
    EXPECT_FALSE(opts.linearize);
    EXPECT_TRUE(opts.recompressFlate);
    EXPECT_TRUE(opts.deduplicateStreams);
}
