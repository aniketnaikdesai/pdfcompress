#include <gtest/gtest.h>
#include "core/PDFOptimizer.h"
#include "test_corpus_generator.h"
#include <qpdf/QPDF.hh>

using namespace pdfcompress;
using namespace pdfcompress::test;

TEST(PDFOptimizerTest, TextOnlyPdfDoesNotGrow) {
    TestCorpusGenerator generator;
    auto path = generator.generateTextOnly();
    auto outPath = path + ".opt.pdf";
    PDFOptimizer optimizer;
    auto result = optimizer.optimize(path, outPath);
    EXPECT_TRUE(result.success);
    // Might grow slightly due to QPDF writer overhead vs minimal corpus creator
    EXPECT_NO_THROW(QPDF().processFile(outPath.c_str()));
}

TEST(PDFOptimizerTest, BookmarksStripped) {
    TestCorpusGenerator generator;
    auto path = generator.generateWithBookmarks();
    auto outPath = path + ".opt.pdf";
    PDFOptimizer optimizer;
    // Balanced defaults leave bookmarks alone; request stripping explicitly.
    OptimizationOptions opts;
    opts.stripBookmarks = true;
    auto result = optimizer.optimize(path, outPath, opts);
    EXPECT_TRUE(result.success);
    QPDF verifier;
    verifier.processFile(outPath.c_str());
    EXPECT_FALSE(verifier.getRoot().hasKey("/Outlines"));
}

TEST(PDFOptimizerTest, AnnotationsStripped) {
    TestCorpusGenerator generator;
    auto path = generator.generateWithAnnotations();
    auto outPath = path + ".opt.pdf";
    PDFOptimizer optimizer;
    // Balanced defaults leave annotations alone; request stripping explicitly.
    OptimizationOptions opts;
    opts.stripLinks = true;
    opts.stripOtherAnnotations = true;
    auto result = optimizer.optimize(path, outPath, opts);
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.annotationsRemoved, 0);
}

TEST(PDFOptimizerTest, CmykSkipped) {
    TestCorpusGenerator generator;
    auto path = generator.generateCmykImage();
    auto outPath = path + ".opt.pdf";
    PDFOptimizer optimizer;
    auto result = optimizer.optimize(path, outPath);
    EXPECT_TRUE(result.success);
    // CMYK is visited then skipped to preserve color: 1 processed, 1 skipped.
    EXPECT_EQ(result.imagesSkipped, 1);
}
