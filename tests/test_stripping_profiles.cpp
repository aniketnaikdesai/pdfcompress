// test_stripping_profiles.cpp — Stripping end-to-end verifier tests (P1-T2).
//
// Locks the `OptimizationOptions::forProfile` removal matrix (AC-B2) plus the
// CLI flag-overrides-profile round-trip semantics from tools/run_optimize.cpp:
//   T01 (P1-T2-T01): MaxQuality removes nothing on all four stripping corpus PDFs.
//   T02 (P1-T2-T02): Balanced removes only JS (+ /OpenAction JS) and metadata.
//   T03 (P1-T2-T03): MaxCompression removes >=1 item on each of the four PDFs.
//   T04 (P1-T2-T04): non-JS GoTo /OpenAction survives every profile; /AA is
//                    removed exactly when stripJavaScript=true.
//   T05 (P1-T2-T05): explicit --strip-bookmarks wins over Balanced defaults.
//
// Verification only: these tests assert strip behavior, they never change it.
// C++20, GTest only. Linked via pdfcompress_core and QPDF.

#include <gtest/gtest.h>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>

#include <filesystem>
#include <string>

#include "core/PDFOptimizer.h"
#include "test_corpus_generator.h"

namespace {

using pdfcompress::CompressionProfile;
using pdfcompress::OptimizationOptions;
using pdfcompress::OptimizationResult;
using pdfcompress::PDFOptimizer;
using pdfcompress::test::TestCorpusGenerator;

// Total items removed, counting metadataStripped as one item.
int totalRemoved(const OptimizationResult& r) {
    return r.linksRemoved + r.annotationsRemoved + r.bookmarksRemoved +
           r.formsRemoved + r.jsRemoved + r.namedDestinationsRemoved +
           (r.metadataStripped ? 1 : 0);
}

void expectZeroRemovals(const OptimizationResult& r) {
    EXPECT_EQ(r.linksRemoved, 0);
    EXPECT_EQ(r.annotationsRemoved, 0);
    EXPECT_EQ(r.bookmarksRemoved, 0);
    EXPECT_EQ(r.formsRemoved, 0);
    EXPECT_EQ(r.jsRemoved, 0);
    EXPECT_EQ(r.namedDestinationsRemoved, 0);
    EXPECT_FALSE(r.metadataStripped);
}

void expectValidPdf(const std::string& path) {
    QPDF verifier;
    EXPECT_NO_THROW(verifier.processFile(path.c_str())) << "output: " << path;
}

// Stage a $TMPDIR-only PDF (never committed) carrying a non-JS GoTo
// /OpenAction plus /AA keys at both document and page level.
std::string stageGoToOpenActionPdf() {
    const std::string path =
        (std::filesystem::temp_directory_path() / "p1t2_goto_oa.pdf").string();
    QPDF pdf;
    pdf.emptyPDF();
    QPDFObjectHandle font = QPDFObjectHandle::parse(
        "<< /Type /Font /Subtype /Type1 /Name /F1 /BaseFont /Helvetica >>");
    QPDFObjectHandle page =
        QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
    page.replaceKey("/Resources", QPDFObjectHandle::parse("<< >>"));
    page.getKey("/Resources")
        .replaceKey("/Font", QPDFObjectHandle::parse("<< >>"));
    page.getKey("/Resources").getKey("/Font").replaceKey("/F1", font);
    QPDFObjectHandle contents = QPDFObjectHandle::newStream(
        &pdf, "BT /F1 12 Tf 72 720 Td (GoTo OpenAction test) Tj ET\n");
    page.replaceKey("/Contents", contents);
    QPDFPageDocumentHelper(pdf).addPage(page, true);

    QPDFObjectHandle pageObj =
        QPDFPageDocumentHelper(pdf).getAllPages().at(0).getObjectHandle();

    // Non-JS GoTo /OpenAction: dict with /S /GoTo (the strip guard in
    // PDFOptimizer only removes /S /JavaScript, so this must survive).
    QPDFObjectHandle dest = QPDFObjectHandle::newArray();
    dest.appendItem(pageObj);
    dest.appendItem(QPDFObjectHandle::newName("/Fit"));
    QPDFObjectHandle openAction = QPDFObjectHandle::newDictionary();
    openAction.replaceKey("/S", QPDFObjectHandle::newName("/GoTo"));
    openAction.replaceKey("/D", dest);
    pdf.getRoot().replaceKey("/OpenAction", openAction);

    // /AA keys: document-level and page-level JavaScript actions.
    pdf.getRoot().replaceKey(
        "/AA", QPDFObjectHandle::parse(
                   "<< /O << /S /JavaScript /JS (app.alert('doc');) >> >>"));
    pageObj.replaceKey(
        "/AA", QPDFObjectHandle::parse(
                   "<< /O << /S /JavaScript /JS (app.alert('page');) >> >>"));

    QPDFWriter writer(pdf, path.c_str());
    writer.setDeterministicID(true);
    writer.setStaticID(true);
    writer.write();
    return path;
}

}  // namespace

// P1-T2-T01: MaxQuality strips nothing on any of the four corpus PDFs.
TEST(StrippingProfiles, MaxQualityRemovesNothing) {
    TestCorpusGenerator gen;
    const std::string inputs[4] = {
        gen.generateWithForm(),
        gen.generateWithBookmarksAndLinks(),
        gen.generateWithJavaScript(),
        gen.generateWithMetadata(),
    };
    const OptimizationOptions opts =
        OptimizationOptions::forProfile(CompressionProfile::MaxQuality);
    PDFOptimizer optimizer;
    for (const auto& in : inputs) {
        const std::string out = in + ".strip_profiles.mq.opt.pdf";
        auto result = optimizer.optimize(in, out, opts);
        ASSERT_TRUE(result.success) << in << ": " << result.errorMessage;
        expectZeroRemovals(result);
        expectValidPdf(out);
    }
}

// P1-T2-T02: Balanced strips exactly JS + metadata, nothing else.
TEST(StrippingProfiles, BalancedRemovesOnlyJsAndMetadata) {
    TestCorpusGenerator gen;
    const std::string form = gen.generateWithForm();
    const std::string bookmarks = gen.generateWithBookmarksAndLinks();
    const std::string javascript = gen.generateWithJavaScript();
    const std::string metadata = gen.generateWithMetadata();
    const OptimizationOptions opts =
        OptimizationOptions::forProfile(CompressionProfile::Balanced);
    PDFOptimizer optimizer;

    auto run = [&](const std::string& in) {
        const std::string out = in + ".strip_profiles.bal.opt.pdf";
        auto result = optimizer.optimize(in, out, opts);
        EXPECT_TRUE(result.success) << in << ": " << result.errorMessage;
        if (result.success) {
            expectValidPdf(out);
        }
        return result;
    };

    // with_javascript.pdf: JS removed (both /Names/JavaScript and the
    // /JavaScript /OpenAction). metadataStripped is NOT asserted here:
    // Balanced applies stripMetadata=true which reports true on all inputs
    // per the ESC-008 intent ruling (intent, NOT actual-removal).
    {
        auto r = run(javascript);
        ASSERT_TRUE(r.success);
        EXPECT_GE(r.jsRemoved, 1) << "Balanced must strip JS OpenAction/Names";
    }
    // with_metadata.pdf: metadata stripped, no JS present to remove.
    {
        auto r = run(metadata);
        ASSERT_TRUE(r.success);
        EXPECT_TRUE(r.metadataStripped);
        EXPECT_EQ(r.jsRemoved, 0);
    }
    // Everywhere else: links/annots/bookmarks/forms/dests counts are 0.
    for (const auto& in : {form, bookmarks, javascript, metadata}) {
        const std::string out = in + ".strip_profiles.bal.opt.pdf";
        // Re-read result via a fresh run is wasteful; re-optimize is cheap
        // and keeps each assertion on a recorded result. (Outputs above are
        // reused; counters re-derived here for the zero-removal gate.)
        auto r = optimizer.optimize(in, out, opts);
        ASSERT_TRUE(r.success) << in;
        EXPECT_EQ(r.linksRemoved, 0) << in;
        EXPECT_EQ(r.annotationsRemoved, 0) << in;
        EXPECT_EQ(r.bookmarksRemoved, 0) << in;
        EXPECT_EQ(r.formsRemoved, 0) << in;
        EXPECT_EQ(r.namedDestinationsRemoved, 0) << in;
    }
    // Cross-checks: no JS stripping outside with_javascript.
    // metadataStripped is NOT asserted on metadata-free PDFs: Balanced
    // applies stripMetadata=true which reports true on all inputs per the
    // ESC-008 intent ruling locked 2026-10-07 (left unasserted by design).
    {
        auto rForm = optimizer.optimize(
            form, form + ".strip_profiles.bal.opt.pdf", opts);
        auto rBm = optimizer.optimize(
            bookmarks, bookmarks + ".strip_profiles.bal.opt.pdf", opts);
        EXPECT_EQ(rForm.jsRemoved, 0);
        EXPECT_EQ(rBm.jsRemoved, 0);
    }
}

// P1-T2-T03: MaxCompression removes >=1 item on each of the four PDFs.
TEST(StrippingProfiles, MaxCompressionRemovesSomethingEverywhere) {
    TestCorpusGenerator gen;
    const std::string inputs[4] = {
        gen.generateWithForm(),
        gen.generateWithBookmarksAndLinks(),
        gen.generateWithJavaScript(),
        gen.generateWithMetadata(),
    };
    const OptimizationOptions opts =
        OptimizationOptions::forProfile(CompressionProfile::MaxCompression);
    PDFOptimizer optimizer;
    for (const auto& in : inputs) {
        const std::string out = in + ".strip_profiles.mc.opt.pdf";
        auto result = optimizer.optimize(in, out, opts);
        ASSERT_TRUE(result.success) << in << ": " << result.errorMessage;
        EXPECT_GE(totalRemoved(result), 1)
            << in << " must lose >=1 item under MaxCompression";
        expectValidPdf(out);
    }
}

// P1-T2-T04: GoTo /OpenAction survives all profiles; /AA removed exactly
// when stripJavaScript=true.
TEST(StrippingProfiles, GoToOpenActionSurvivesAllProfiles) {
    const std::string in = stageGoToOpenActionPdf();
    const CompressionProfile profiles[3] = {
        CompressionProfile::MaxQuality,
        CompressionProfile::Balanced,
        CompressionProfile::MaxCompression,
    };
    PDFOptimizer optimizer;
    for (auto profile : profiles) {
        const OptimizationOptions opts =
            OptimizationOptions::forProfile(profile);
        const std::string out =
            in + ".strip_profiles.goto." + std::to_string((int)profile) +
            ".opt.pdf";
        auto result = optimizer.optimize(in, out, opts);
        ASSERT_TRUE(result.success) << "profile " << (int)profile << ": "
                                    << result.errorMessage;
        QPDF verifier;
        ASSERT_NO_THROW(verifier.processFile(out.c_str())) << out;
        QPDFObjectHandle root = verifier.getRoot();
        ASSERT_TRUE(root.hasKey("/OpenAction"))
            << "GoTo /OpenAction must survive profile " << (int)profile;
        QPDFObjectHandle oa = root.getKey("/OpenAction");
        ASSERT_TRUE(oa.isDictionary());
        EXPECT_EQ(oa.getKey("/S").getName(), "/GoTo");
        if (opts.stripJavaScript) {
            EXPECT_FALSE(root.hasKey("/AA"))
                << "root /AA must be removed when stripJavaScript=true";
            QPDFObjectHandle pageObj = QPDFPageDocumentHelper(verifier)
                                           .getAllPages()
                                           .at(0)
                                           .getObjectHandle();
            EXPECT_FALSE(pageObj.hasKey("/AA"))
                << "page /AA must be removed when stripJavaScript=true";
            EXPECT_GE(result.jsRemoved, 1);
        } else {
            EXPECT_TRUE(root.hasKey("/AA"))
                << "/AA must survive when stripJavaScript=false";
            EXPECT_EQ(result.jsRemoved, 0);
        }
    }
}

// P1-T2-T05: explicit --strip-bookmarks wins over Balanced profile default.
// Mirrors run_optimize CLI parsing: forProfile(Balanced) first, then the
// explicit flag is applied on top (tools/run_optimize.cpp: forProfile at the
// flag-scan setup, --strip-bookmarks sets stripBookmarks=true after).
TEST(StrippingProfiles, ExplicitFlagWinsOverProfileDefault) {
    TestCorpusGenerator gen;
    const std::string in = gen.generateWithBookmarksAndLinks();

    // Round-trip through CLI parsing semantics: profile defaults first...
    auto opts = OptimizationOptions::forProfile(CompressionProfile::Balanced);
    EXPECT_FALSE(opts.stripBookmarks)
        << "Balanced default must leave bookmarks alone";
    // ...then the explicit --strip-bookmarks flag wins.
    opts.stripBookmarks = true;

    const std::string out = in + ".strip_profiles.explicit.opt.pdf";
    PDFOptimizer optimizer;
    auto result = optimizer.optimize(in, out, opts);
    ASSERT_TRUE(result.success) << result.errorMessage;
    // Removal is logged in the result breakdown...
    EXPECT_GE(result.bookmarksRemoved, 1);
    // ...and reflected in the output (/Outlines gone).
    QPDF verifier;
    ASSERT_NO_THROW(verifier.processFile(out.c_str()));
    EXPECT_FALSE(verifier.getRoot().hasKey("/Outlines"));
}
