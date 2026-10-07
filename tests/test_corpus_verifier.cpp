// test_corpus_verifier.cpp — Corpus Verifier Tests.
//
// Verifies the 14-file synthetic corpus produced by TestCorpusGenerator:
// file count, per-file existence, QPDF validity, per-type key assertions
// (AcroForm, Outlines/Annots/Dests, Names+JavaScript/OpenAction,
// Info/Metadata, DeviceCMYK+FlateDecode, SMask/Transparency+shared-object,
// encrypted), CMYK skip path, encrypted handling, stripping regression with
// the <2KB exemption (RG-002), and benchmark TSV SSIM thresholds.
//
// C++20, GTest only. Linked via pdfcompress_core and QPDF.
// (CMake wiring is owned by the Build Lead in a separate task.)

#include <gtest/gtest.h>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "core/PDFOptimizer.h"
#include "test_corpus_generator.h"

namespace {

using pdfcompress::CompressionProfile;
using pdfcompress::PDFOptimizer;
using pdfcompress::test::TestCorpusGenerator;

// The 14 canonical corpus files produced by TestCorpusGenerator::generateAll().
// NOTE: the corpus directory is shared with benchmark outputs
// (*_optimized.pdf, *.gs_*.pdf, *.opt.pdf, ...), so the count below is over
// this canonical set only — not a raw "*.pdf" glob of the directory.
const std::vector<std::string>& kCanonicalFiles() {
    static const std::vector<std::string> kFiles = {
        "text_only.pdf",
        "photo_jpeg.pdf",
        "photo_heavy.pdf",
        "screenshot_flat.pdf",
        "line_art.pdf",
        "grayscale_scan.pdf",
        "monochrome_bw.pdf",
        "with_form.pdf",
        "with_bookmarks_and_links.pdf",
        "with_javascript.pdf",
        "with_metadata.pdf",
        "encrypted.pdf",
        "cmyk_image.pdf",
        "transparency.pdf",
    };
    return kFiles;
}

std::string corpusFile(TestCorpusGenerator& gen, const std::string& base) {
    return (gen.corpusDir() / base).string();
}

// Open a PDF with QPDF and require at least one page; returns the object.
// QPDF is non-copyable, so ownership is transferred via unique_ptr.
std::unique_ptr<QPDF> openValidPdf(const std::string& path,
                                   const char* password = nullptr) {
    auto pdf = std::make_unique<QPDF>();
    EXPECT_NO_THROW(pdf->processFile(path.c_str(), password))
        << "QPDF().processFile failed for " << path;
    return pdf;
}

bool hasImageWithColorSpace(QPDF& pdf, const std::string& colorSpace,
                            const std::string& filter = "") {
    QPDFPageDocumentHelper helper(pdf);
    for (auto& page : helper.getAllPages()) {
        auto images = page.getImages();
        for (auto& [name, img] : images) {
            QPDFObjectHandle dict = img.getDict();
            if (!dict.hasKey("/ColorSpace")) {
                continue;
            }
            QPDFObjectHandle cs = dict.getKey("/ColorSpace");
            if (cs.isName() && cs.getName() == colorSpace) {
                if (filter.empty()) {
                    return true;
                }
                if (dict.hasKey("/Filter")) {
                    QPDFObjectHandle f = dict.getKey("/Filter");
                    std::string fname;
                    if (f.isName()) {
                        fname = f.getName();
                    } else if (f.isArray() && f.getArrayNItems() > 0) {
                        fname = f.getArrayItem(0).getName();
                    }
                    if (fname == filter) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

}  // namespace

// ---------------------------------------------------------------------------
// File count: exactly 14 canonical corpus files.
// ---------------------------------------------------------------------------

TEST(CorpusGenerator, FileCountIs14) {
    TestCorpusGenerator gen;
    gen.generateAll();
    int count = 0;
    for (const auto& base : kCanonicalFiles()) {
        if (std::filesystem::exists(corpusFile(gen, base))) {
            ++count;
        } else {
            ADD_FAILURE() << "Missing canonical corpus file: " << base;
        }
    }
    EXPECT_EQ(count, 14) << "corpusDir file count over canonical set must be 14";
}

// ---------------------------------------------------------------------------
// Per-type existence + QPDF validity + key assertions (14 types).
// ---------------------------------------------------------------------------

TEST(CorpusGenerator, TextOnlyHasContent) {
    TestCorpusGenerator gen;
    std::string path = gen.generateTextOnly();
    EXPECT_TRUE(std::filesystem::exists(path));
    auto pdfPtr = openValidPdf(path);
    QPDF& pdf = *pdfPtr;
    EXPECT_EQ(QPDFPageDocumentHelper(pdf).getAllPages().size(), 1u);
}

TEST(CorpusGenerator, PhotoJpegIsValid) {
    TestCorpusGenerator gen;
    std::string path = gen.generatePhotoJpeg();
    EXPECT_TRUE(std::filesystem::exists(path));
    auto pdfPtr = openValidPdf(path);
    QPDF& pdf = *pdfPtr;
    EXPECT_TRUE(hasImageWithColorSpace(pdf, "/DeviceRGB"));
}

TEST(CorpusGenerator, PhotoHeavyHasThreeImages) {
    TestCorpusGenerator gen;
    std::string path = gen.generatePhotoHeavy();
    EXPECT_TRUE(std::filesystem::exists(path));
    auto pdfPtr = openValidPdf(path);
    QPDF& pdf = *pdfPtr;
    EXPECT_EQ(QPDFPageDocumentHelper(pdf).getAllPages().size(), 3u);
    EXPECT_TRUE(hasImageWithColorSpace(pdf, "/DeviceRGB"));
}

TEST(CorpusGenerator, ScreenshotFlatIsValid) {
    TestCorpusGenerator gen;
    std::string path = gen.generateScreenshotFlat();
    EXPECT_TRUE(std::filesystem::exists(path));
    auto pdfPtr = openValidPdf(path);
    QPDF& pdf = *pdfPtr;
    EXPECT_TRUE(hasImageWithColorSpace(pdf, "/DeviceRGB"));
}

TEST(CorpusGenerator, LineArtIsValid) {
    TestCorpusGenerator gen;
    std::string path = gen.generateLineArt();
    EXPECT_TRUE(std::filesystem::exists(path));
    auto pdfPtr = openValidPdf(path);
    QPDF& pdf = *pdfPtr;
    EXPECT_TRUE(hasImageWithColorSpace(pdf, "/DeviceRGB"));
}

TEST(CorpusGenerator, GrayscaleScanUsesDeviceGray) {
    TestCorpusGenerator gen;
    std::string path = gen.generateGrayscaleScan();
    EXPECT_TRUE(std::filesystem::exists(path));
    auto pdfPtr = openValidPdf(path);
    QPDF& pdf = *pdfPtr;
    EXPECT_TRUE(hasImageWithColorSpace(pdf, "/DeviceGray"));
}

TEST(CorpusGenerator, MonochromeBwUsesDeviceGray) {
    TestCorpusGenerator gen;
    std::string path = gen.generateMonochromeBW();
    EXPECT_TRUE(std::filesystem::exists(path));
    auto pdfPtr = openValidPdf(path);
    QPDF& pdf = *pdfPtr;
    EXPECT_TRUE(hasImageWithColorSpace(pdf, "/DeviceGray"));
}

TEST(CorpusGenerator, WithFormHasAcroForm) {
    TestCorpusGenerator gen;
    std::string path = gen.generateWithForm();
    EXPECT_TRUE(std::filesystem::exists(path));
    auto pdfPtr = openValidPdf(path);
    QPDF& pdf = *pdfPtr;
    EXPECT_TRUE(pdf.getRoot().hasKey("/AcroForm"))
        << "with_form.pdf must carry /AcroForm";
    QPDFPageDocumentHelper helper(pdf);
    ASSERT_EQ(helper.getAllPages().size(), 1u);
    EXPECT_TRUE(helper.getAllPages().at(0).getObjectHandle().hasKey("/Annots"));
}

TEST(CorpusGenerator, WithBookmarksAndLinksHasOutlinesAnnotsDests) {
    TestCorpusGenerator gen;
    std::string path = gen.generateWithBookmarksAndLinks();
    EXPECT_TRUE(std::filesystem::exists(path));
    auto pdfPtr = openValidPdf(path);
    QPDF& pdf = *pdfPtr;
    EXPECT_TRUE(pdf.getRoot().hasKey("/Outlines")) << "missing /Outlines";
    EXPECT_TRUE(pdf.getRoot().hasKey("/Dests")) << "missing /Dests";
    QPDFPageDocumentHelper helper(pdf);
    ASSERT_EQ(helper.getAllPages().size(), 1u);
    EXPECT_TRUE(helper.getAllPages().at(0).getObjectHandle().hasKey("/Annots"))
        << "missing /Annots";
}

TEST(CorpusGenerator, WithJavaScriptHasNamesAndOpenAction) {
    TestCorpusGenerator gen;
    std::string path = gen.generateWithJavaScript();
    EXPECT_TRUE(std::filesystem::exists(path));
    auto pdfPtr = openValidPdf(path);
    QPDF& pdf = *pdfPtr;
    ASSERT_TRUE(pdf.getRoot().hasKey("/Names")) << "missing /Names";
    EXPECT_TRUE(pdf.getRoot().getKey("/Names").unparse().find("JavaScript") !=
                std::string::npos)
        << "/Names must embed JavaScript entry";
    EXPECT_TRUE(pdf.getRoot().hasKey("/OpenAction")) << "missing /OpenAction";
}

TEST(CorpusGenerator, WithMetadataHasInfoAndMetadata) {
    TestCorpusGenerator gen;
    std::string path = gen.generateWithMetadata();
    EXPECT_TRUE(std::filesystem::exists(path));
    auto pdfPtr = openValidPdf(path);
    QPDF& pdf = *pdfPtr;
    EXPECT_TRUE(pdf.getTrailer().hasKey("/Info")) << "missing /Info";
    EXPECT_TRUE(pdf.getRoot().hasKey("/Metadata")) << "missing /Metadata";
}

TEST(CorpusGenerator, EncryptedIsPasswordProtected) {
    TestCorpusGenerator gen;
    std::string path = gen.generateEncrypted();
    EXPECT_TRUE(std::filesystem::exists(path));
    // The file carries an empty owner password, so QPDF opens it without a
    // supplied password — but it must still report isEncrypted.
    auto noPwPtr = openValidPdf(path);
    EXPECT_TRUE(noPwPtr->isEncrypted())
        << "encrypted.pdf must be flagged isEncrypted";
    // With password 'test123' it opens and reports encrypted.
    QPDF pdf;
    EXPECT_NO_THROW(pdf.processFile(path.c_str(), "test123"));
    EXPECT_TRUE(pdf.isEncrypted());
}

TEST(CorpusGenerator, CmykImageHasDeviceCmykFlateDecode) {
    TestCorpusGenerator gen;
    std::string path = gen.generateCmykImage();
    EXPECT_TRUE(std::filesystem::exists(path));
    auto pdfPtr = openValidPdf(path);
    QPDF& pdf = *pdfPtr;
    EXPECT_TRUE(hasImageWithColorSpace(pdf, "/DeviceCMYK", "/FlateDecode"))
        << "cmyk_image.pdf must carry DeviceCMYK + FlateDecode";
}

TEST(CorpusGenerator, TransparencyHasSmaskAndSharedObject) {
    TestCorpusGenerator gen;
    std::string path = gen.generateTransparency();
    EXPECT_TRUE(std::filesystem::exists(path));
    auto pdfPtr = openValidPdf(path);
    QPDF& pdf = *pdfPtr;
    QPDFPageDocumentHelper helper(pdf);
    auto pages = helper.getAllPages();
    ASSERT_EQ(pages.size(), 2u);
    auto imgs0 = pages.at(0).getImages();
    auto imgs1 = pages.at(1).getImages();
    ASSERT_FALSE(imgs0.empty());
    ASSERT_FALSE(imgs1.empty());
    // Shared XObject: both pages reference the same indirect image.
    EXPECT_EQ(imgs0.begin()->second.getObjGen(), imgs1.begin()->second.getObjGen())
        << "transparency.pdf must share one image XObject across pages";
    // SMask soft-mask on the shared image.
    EXPECT_TRUE(imgs0.begin()->second.getDict().hasKey("/SMask"))
        << "transparent image must carry /SMask";
    // Page-level transparency group (accessors resolve the indirect page).
    QPDFObjectHandle pageObj = pages.at(0).getObjectHandle();
    ASSERT_TRUE(pageObj.hasKey("/Group")) << "page must carry /Group";
    EXPECT_EQ(pageObj.getKey("/Group").getKey("/S").getName(), "/Transparency")
        << "transparency.pdf page must reference Transparency group";
}

// ---------------------------------------------------------------------------
// CMYK skip path: imagesSkipped == 1 and success true (no transcoding).
// ---------------------------------------------------------------------------

TEST(CorpusGenerator, CmykImageSkippedWithoutTranscoding) {
    TestCorpusGenerator gen;
    std::string path = gen.generateCmykImage();
    std::string out = path + ".cmyk_verify.opt.pdf";
    PDFOptimizer optimizer;
    auto result = optimizer.optimize(path, out);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.imagesSkipped, 1)
        << "cmyk_image.pdf exercises the CMYK skip path exactly once";
    EXPECT_TRUE(std::filesystem::exists(out));
}

// ---------------------------------------------------------------------------
// Encrypted handling: no-password optimize fails safely; password opens and
// re-optimizes to unencrypted output.
// ---------------------------------------------------------------------------

TEST(CorpusGenerator, EncryptedWithoutPasswordFailsSafely) {
    TestCorpusGenerator gen;
    std::string path = gen.generateEncrypted();
    std::string out = path + ".enc_verify.opt.pdf";
    PDFOptimizer optimizer;
    // Must not crash; must report failure via success==false or errorMessage.
    auto result = optimizer.optimize(path, out);
    EXPECT_TRUE(!result.success || !result.errorMessage.empty())
        << "encrypted optimize without password must fail safely";
    EXPECT_FALSE(result.success);
}

TEST(CorpusGenerator, EncryptedWithPasswordOptimizesToUnencrypted) {
    TestCorpusGenerator gen;
    std::string path = gen.generateEncrypted();
    // QPDF with password 'test123' succeeds.
    QPDF pdf;
    ASSERT_NO_THROW(pdf.processFile(path.c_str(), "test123"));
    ASSERT_TRUE(pdf.isEncrypted());
    // Decrypt to a temp copy, then optimize it.
    std::string decrypted = path + ".decrypted_verify.pdf";
    {
        QPDFWriter writer(pdf, decrypted.c_str());
        writer.setPreserveEncryption(false);
        writer.write();
    }
    QPDF check;
    ASSERT_NO_THROW(check.processFile(decrypted.c_str()));
    EXPECT_FALSE(check.isEncrypted());
    std::string out = decrypted + ".opt.pdf";
    PDFOptimizer optimizer;
    auto result = optimizer.optimize(decrypted, out);
    EXPECT_TRUE(result.success) << result.errorMessage;
    auto outPdfPtr = openValidPdf(out);
    EXPECT_FALSE(outPdfPtr->isEncrypted());
}

// ---------------------------------------------------------------------------
// Stripping regression (Balanced profile):
//  - text_only must not grow beyond QPDF writer overhead;
//  - files < 2KB are exempt from the +/-1% size gate per RG-002;
//  - only files >= 50KB (photo_heavy / line_art / large_uncompressed class)
//    are held to the +/-1% gate (or a reduction).
// ---------------------------------------------------------------------------

namespace {

// RG-002 size gates.
constexpr std::uintmax_t kSmallFileExemptBytes = 2048;     // 2KB exemption
constexpr std::uintmax_t kStrictGateMinBytes = 50 * 1024;  // 50KB strict gate
constexpr double kSizeGateTolerance = 0.01;                // +/-1%

// Balanced-profile optimize + QPDF re-open check shared by size-gate tests.
pdfcompress::OptimizationResult optimizeBalancedChecked(
    const std::string& in, const std::string& out) {
    PDFOptimizer optimizer;
    auto result = optimizer.optimize(
        in, out, CompressionProfile::Balanced);
    EXPECT_TRUE(result.success) << result.errorMessage;
    if (result.success) {
        QPDF pdf;
        EXPECT_NO_THROW(pdf.processFile(out.c_str())) << "output: " << out;
    }
    return result;
}

}  // namespace

TEST(CorpusGenerator, StrippingRegressionTextOnlyNoGrowth) {
    TestCorpusGenerator gen;
    std::string path = gen.generateTextOnly();
    std::string out = path + ".strip_verify.opt.pdf";
    auto result = optimizeBalancedChecked(path, out);
    ASSERT_TRUE(result.success);
    // text_only has no image/stripping payload: output must not grow beyond
    // ordinary QPDF writer overhead (generous 4KB ceiling for tiny files).
    const auto origSize = std::filesystem::file_size(path);
    const auto optSize = std::filesystem::file_size(out);
    EXPECT_LE(optSize, origSize + 4096u)
        << "text_only regressed: stripping must not grow tiny files";
}

TEST(CorpusGenerator, StrippingRegressionSizeGateWithExemption) {
    TestCorpusGenerator gen;
    gen.generateAll();
    // large_uncompressed.pdf is the >=50KB representative; generate on demand
    // since generateAll() keeps the canonical 14-file set.
    const std::string large = gen.generateLargeUncompressed();
    std::vector<std::string> inputs(kCanonicalFiles().begin(),
                                    kCanonicalFiles().end());
    inputs.push_back(std::filesystem::path(large).filename().string());

    for (const auto& base : inputs) {
        const std::string in = corpusFile(gen, base);
        if (base == "encrypted.pdf") {
            continue;  // encrypted needs a password; covered separately.
        }
        const std::string out = in + ".sizegate_verify.opt.pdf";
        auto result = optimizeBalancedChecked(in, out);
        if (!result.success) {
            continue;
        }
        const auto origSize = std::filesystem::file_size(in);
        const auto optSize = std::filesystem::file_size(out);
        if (origSize < kSmallFileExemptBytes) {
            // 2KB exemption per RG-002: writer overhead dominates; only
            // success + QPDF validity are required (asserted above).
            SUCCEED() << base << " exempt (<2KB)";
            continue;
        }
        if (origSize >= kStrictGateMinBytes) {
            // 50KB+ files: output must shrink or stay within +/-1%.
            const double ratio =
                static_cast<double>(optSize) / static_cast<double>(origSize);
            EXPECT_LE(ratio, 1.0 + kSizeGateTolerance)
                << base << " exceeded +/-1% size gate";
        }
        // Mid-size files (2KB..50KB): success + validity suffice.
    }
}

// ---------------------------------------------------------------------------
// Benchmark TSV integration: header carries SSIM/PSNR columns; numeric SSIM
// values meet the >=0.98 threshold for Balanced/MaxQuality synthetic corpus.
// N/A is allowed when PDFium/render_diff output is unavailable.
// ---------------------------------------------------------------------------

TEST(CorpusGenerator, BenchmarkTsvSsimThresholds) {
    // Locate the newest benchmark_results_*.tsv under build/ or repo root.
    std::vector<std::filesystem::path> searchDirs = {
        std::filesystem::path("build"),
        std::filesystem::path("."),
        std::filesystem::path("benchmark"),
    };
    std::filesystem::path newest;
    std::filesystem::file_time_type newestTime =
        std::filesystem::file_time_type::min();
    for (const auto& dir : searchDirs) {
        std::error_code ec;
        if (!std::filesystem::exists(dir, ec)) {
            continue;
        }
        for (auto it = std::filesystem::directory_iterator(dir, ec);
             !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
            const auto& p = it->path();
            const std::string name = p.filename().string();
            if (name.rfind("benchmark_results_", 0) == 0 &&
                p.extension() == ".tsv") {
                std::error_code tec;
                auto t = std::filesystem::last_write_time(p, tec);
                if (!tec && t > newestTime) {
                    newestTime = t;
                    newest = p;
                }
            }
        }
    }
    if (newest.empty()) {
        GTEST_SKIP() << "No benchmark TSV found; SSIM N/A (PDFium unavailable)";
    }

    std::ifstream tsv(newest);
    ASSERT_TRUE(tsv.is_open()) << "Cannot open " << newest.string();
    std::string header;
    ASSERT_TRUE(static_cast<bool>(std::getline(tsv, header)));
    EXPECT_TRUE(header.find("SSIM") != std::string::npos)
        << "TSV header must contain SSIM column";
    EXPECT_TRUE(header.find("PSNR") != std::string::npos)
        << "TSV header must contain PSNR column";

    // Column indexes for SSIM and Tool.
    std::vector<std::string> cols;
    {
        std::stringstream ss(header);
        std::string cell;
        while (std::getline(ss, cell, '\t')) {
            cols.push_back(cell);
        }
    }
    const auto ssimIdx =
        static_cast<int>(std::distance(cols.begin(), std::find(cols.begin(), cols.end(), "SSIM")));
    ASSERT_LT(ssimIdx, static_cast<int>(cols.size())) << "no SSIM column";

    // The TSV is a multi-tool matrix (pdfcompress, qpdf, gs_ebook, gs_screen,
    // ocrmypdf). The >=0.98 gate is a product-quality contract for our own
    // tool only; third-party rows are informational and must not fail the
    // product verifier.
    const auto toolIdx =
        static_cast<int>(std::distance(cols.begin(), std::find(cols.begin(), cols.end(), "Tool")));
    ASSERT_LT(toolIdx, static_cast<int>(cols.size())) << "no Tool column";

    // SSIM quality threshold for Balanced/MaxQuality on synthetic corpus.
    constexpr double kMinSsim = 0.98;
    int numericRows = 0;
    int pdfcompressRows = 0;
    std::string line;
    while (std::getline(tsv, line)) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> cells;
        std::stringstream ss(line);
        std::string cell;
        while (std::getline(ss, cell, '\t')) {
            cells.push_back(cell);
        }
        if (ssimIdx >= static_cast<int>(cells.size()) ||
            toolIdx >= static_cast<int>(cells.size())) {
            continue;
        }
        const std::string& ssimCell = cells[static_cast<size_t>(ssimIdx)];
        const std::string& toolCell = cells[static_cast<size_t>(toolIdx)];
        if (ssimCell == "N/A" || ssimCell == "n/a" || ssimCell.empty()) {
            continue;  // N/A allowed when PDFium/render_diff output missing.
        }
        try {
            const double ssim = std::stod(ssimCell);
            ++numericRows;
            // Gate only our own tool; third-party sub-0.98 rows are allowed.
            if (toolCell == "pdfcompress") {
                ++pdfcompressRows;
                EXPECT_GE(ssim, kMinSsim)
                    << "SSIM below 0.98 threshold for pdfcompress in "
                    << newest.string() << ": " << line;
            }
        } catch (...) {
            // Non-numeric placeholder (e.g. SKIPPED/FAILED) — not a score.
        }
    }
    if (numericRows == 0) {
        SUCCEED() << "All SSIM values N/A (PDFium unavailable); header check passed";
    } else {
        SUCCEED() << "Scored " << numericRows << " numeric SSIM row(s); gated "
                  << pdfcompressRows << " pdfcompress row(s) at >= " << kMinSsim;
    }
}
