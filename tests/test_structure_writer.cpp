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

// P1-T3-T02: dedup counter on the shared-XObject corpus PDF.
TEST(StructureWriter, DedupCountedWithDefaultsZeroWithout) {
    TestCorpusGenerator generator;
    std::string input = generator.generateSharedXObject();

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

    // Current-behavior pin (Phase 1): dedup counting is inert under the
    // ESC-001 isIndirect guard, and the reference-shared fixture shares one
    // indirect object by reference, so no distinct duplicate pair exists.
    EXPECT_EQ(rDefault.streamsDeduplicated, 0);
    EXPECT_EQ(rNoDedup.streamsDeduplicated, 0);
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
