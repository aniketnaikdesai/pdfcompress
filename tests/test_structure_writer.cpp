#include <gtest/gtest.h>
#include "core/PDFOptimizer.h"
#include "test_corpus_generator.h"
#include <qpdf/QPDF.hh>
#include <filesystem>

using namespace pdfcompress;
using namespace pdfcompress::test;

// P1-T3-T01: default writer options must not lose to both-disabled options.
TEST(StructureWriter, DefaultOutputNotLargerThanNoOpt) {
    TestCorpusGenerator generator;
    std::string input = generator.generatePhotoHeavy();

    OptimizationOptions withDefaults;  // deduplicateStreams=true, recompressFlate=true
    OptimizationOptions noOpt;
    noOpt.deduplicateStreams = false;
    noOpt.recompressFlate = false;

    std::string outDefault = input + ".struct_default.pdf";
    std::string outNoOpt = input + ".struct_noopt.pdf";

    PDFOptimizer optimizer;
    auto rDefault = optimizer.optimize(input, outDefault, withDefaults);
    ASSERT_TRUE(rDefault.success) << rDefault.errorMessage;
    auto rNoOpt = optimizer.optimize(input, outNoOpt, noOpt);
    ASSERT_TRUE(rNoOpt.success) << rNoOpt.errorMessage;

    EXPECT_LE(rDefault.optimizedSizeBytes, rNoOpt.optimizedSizeBytes);
}

// P1-T3-T02 (Phase 2 rewrite): dedup counter on the distinct-duplicate fixture.
TEST(StructureWriter, DedupCountedWithDefaultsZeroWithout) {
    TestCorpusGenerator generator;
    std::string input = generator.generateDistinctDuplicateStreams();

    OptimizationOptions withDefaults;  // deduplicateStreams=true
    OptimizationOptions noDedup;
    noDedup.deduplicateStreams = false;

    std::string outDefault = input + ".dedup_default.pdf";
    std::string outNoDedup = input + ".dedup_off.pdf";

    PDFOptimizer optimizer;
    auto rDefault = optimizer.optimize(input, outDefault, withDefaults);
    ASSERT_TRUE(rDefault.success) << rDefault.errorMessage;
    auto rNoDedup = optimizer.optimize(input, outNoDedup, noDedup);
    ASSERT_TRUE(rNoDedup.success) << rNoDedup.errorMessage;

    // AC-C5: the two distinct byte-identical streams collapse with defaults;
    // --no-dedup leaves both in place and reports zero.
    EXPECT_GE(rDefault.streamsDeduplicated, 1);
    EXPECT_EQ(rNoDedup.streamsDeduplicated, 0);

    auto countImageStreams = [](const std::string& path) {
        QPDF pdf;
        pdf.processFile(path.c_str());
        int n = 0;
        for (auto& obj : pdf.getAllObjects()) {
            if (!obj.isStream()) continue;
            QPDFObjectHandle dict = obj.getDict();
            if (dict.hasKey("/Subtype") && dict.getKey("/Subtype").getName() == "/Image") {
                ++n;
            }
        }
        return n;
    };

    EXPECT_EQ(countImageStreams(outDefault), 1);  // duplicate pair shares one object
    EXPECT_EQ(countImageStreams(outNoDedup), 2);  // both distinct streams remain

    for (const auto& out : {outDefault, outNoDedup}) {
        EXPECT_GT(std::filesystem::file_size(out), 0u) << out;
        EXPECT_NO_THROW(QPDF().processFile(out.c_str())) << out;
    }
}

// P2-T10-T01 (ESC-015): two byte-identical image streams that differ only by
// carrying an /SMask must never merge - merging would repoint referrers and
// silently change transparency (AC-C2).
TEST(StructureWriter, MaskedStreamsNotDeduplicated) {
    TestCorpusGenerator generator;
    std::string input = generator.generateMaskedDuplicateStreams();

    OptimizationOptions withDefaults;  // deduplicateStreams=true
    std::string outDefault = input + ".masked_default.pdf";

    PDFOptimizer optimizer;
    auto rDefault = optimizer.optimize(input, outDefault, withDefaults);
    ASSERT_TRUE(rDefault.success) << rDefault.errorMessage;

    // The pair differs only by /SMask, so the added signature key must keep
    // them distinct: nothing to repoint, no dedup counted.
    EXPECT_EQ(rDefault.streamsDeduplicated, 0);

    QPDF out;
    out.processFile(outDefault.c_str());

    std::vector<std::pair<std::string, bool>> images;  // raw bytes, has /SMask
    for (auto& obj : out.getAllObjects()) {
        if (!obj.isStream()) continue;
        QPDFObjectHandle dict = obj.getDict();
        if (!(dict.hasKey("/Subtype") && dict.getKey("/Subtype").getName() == "/Image")) {
            continue;
        }
        auto raw = obj.getRawStreamData();
        std::string bytes = raw
            ? std::string(reinterpret_cast<const char*>(raw->getBuffer()), raw->getSize())
            : std::string();
        images.emplace_back(bytes, dict.hasKey("/SMask") || dict.hasKey("/Mask"));
    }

    // masked image + plain image + soft-mask stream all remain distinct.
    ASSERT_EQ(images.size(), 3u);

    // Exactly one byte-identical pair survives, and its two members differ on
    // /SMask - proving the signature key (not image re-encoding) kept them
    // apart, i.e. this would have merged without the fix.
    int identicalPairs = 0;
    for (size_t i = 0; i < images.size(); ++i) {
        for (size_t j = i + 1; j < images.size(); ++j) {
            if (!images[i].first.empty() && images[i].first == images[j].first) {
                ++identicalPairs;
                EXPECT_NE(images[i].second, images[j].second);
            }
        }
    }
    EXPECT_EQ(identicalPairs, 1);

    EXPECT_GT(std::filesystem::file_size(outDefault), 0u);
    EXPECT_NO_THROW(QPDF().processFile(outDefault.c_str()));
}

// P1-T3-T03: qpdf --check semantics (processFile without warnings/exceptions).
TEST(StructureWriter, AllOutputsPassQpdfCheck) {
    TestCorpusGenerator generator;
    PDFOptimizer optimizer;

    std::string photo = generator.generatePhotoHeavy();
    std::string shared = generator.generateSharedXObject();

    OptimizationOptions withDefaults;
    OptimizationOptions noOpt;
    noOpt.deduplicateStreams = false;
    noOpt.recompressFlate = false;
    OptimizationOptions noDedup;
    noDedup.deduplicateStreams = false;

    std::string photoDefault = photo + ".check_default.pdf";
    std::string photoNoOpt = photo + ".check_noopt.pdf";
    std::string sharedDefault = shared + ".check_default.pdf";
    std::string sharedNoDedup = shared + ".check_nodedup.pdf";

    auto r1 = optimizer.optimize(photo, photoDefault, withDefaults);
    ASSERT_TRUE(r1.success) << r1.errorMessage;
    auto r2 = optimizer.optimize(photo, photoNoOpt, noOpt);
    ASSERT_TRUE(r2.success) << r2.errorMessage;
    auto r3 = optimizer.optimize(shared, sharedDefault, withDefaults);
    ASSERT_TRUE(r3.success) << r3.errorMessage;
    auto r4 = optimizer.optimize(shared, sharedNoDedup, noDedup);
    ASSERT_TRUE(r4.success) << r4.errorMessage;

    for (const auto& out : {photoDefault, photoNoOpt, sharedDefault, sharedNoDedup}) {
        EXPECT_GT(std::filesystem::file_size(out), 0u) << out;
        EXPECT_NO_THROW(QPDF().processFile(out.c_str())) << out;
    }
}
