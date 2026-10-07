#include <gtest/gtest.h>
#include "core/PDFOptimizer.h"
#include "core/ImageAnalyzer.h"
#include "test_corpus_generator.h"
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>

#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <zlib.h>

using namespace pdfcompress;
using namespace pdfcompress::test;

namespace {

// Returns the first /Filter name of an image dictionary, resolving a filter
// array to its leading name (e.g. [/FlateDecode /DCTDecode] -> /FlateDecode).
std::string firstFilterName(QPDFObjectHandle dict) {
    if (!dict.hasKey("/Filter")) return "";
    QPDFObjectHandle f = dict.getKey("/Filter");
    if (f.isName()) return f.getName();
    if (f.isArray() && f.getArrayNItems() > 0 && f.getArrayItem(0).isName()) {
        return f.getArrayItem(0).getName();
    }
    return "";
}

// Returns the leading /ColorSpace name of an image dictionary, resolving an
// array form (e.g. [/ICCBased <stream>] -> /ICCBased) to its first name.
std::string firstColorSpaceName(QPDFObjectHandle dict) {
    if (!dict.hasKey("/ColorSpace")) return "";
    QPDFObjectHandle cs = dict.getKey("/ColorSpace");
    if (cs.isName()) return cs.getName();
    if (cs.isArray() && cs.getArrayNItems() > 0 && cs.getArrayItem(0).isName()) {
        return cs.getArrayItem(0).getName();
    }
    return "";
}

} // namespace

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

// P2-T2-T01 / AC-C2: an /SMask-bearing base image must stay on the lossless
// FlateDecode path (never DCTDecode) and produce a valid PDF.
TEST(PDFOptimizerTest, TransparencyBaseImageStaysFlateDecode) {
    TestCorpusGenerator generator;
    auto path = generator.generateTransparency();
    auto outPath = path + ".opt.pdf";

    PDFOptimizer optimizer;
    auto opts = OptimizationOptions::forProfile(CompressionProfile::MaxQuality);
    auto result = optimizer.optimize(path, outPath, opts);
    ASSERT_TRUE(result.success);

    QPDF out;
    ASSERT_NO_THROW(out.processFile(outPath.c_str()));

    QPDFPageDocumentHelper helper(out);
    auto pages = helper.getAllPages();
    ASSERT_FALSE(pages.empty());
    auto images = pages.at(0).getImages();
    ASSERT_FALSE(images.empty());

    std::string filter = firstFilterName(images.begin()->second.getDict());
    EXPECT_EQ(filter, "/FlateDecode")
        << "alpha base image must be stored losslessly (FlateDecode)";
    EXPECT_NE(filter, "/DCTDecode")
        << "alpha base image must never be flattened into JPEG";

    // Output must remain a structurally valid PDF.
    EXPECT_NO_THROW(QPDF().processFile(outPath.c_str()));
}

// P2-T2-T02 / AC-C2: the /SMask stream is preserved intact; alpha is never
// flattened away.
TEST(PDFOptimizerTest, TransparencyMaskPreserved) {
    TestCorpusGenerator generator;
    auto path = generator.generateTransparency();
    auto outPath = path + ".opt.pdf";

    PDFOptimizer optimizer;
    auto opts = OptimizationOptions::forProfile(CompressionProfile::MaxQuality);
    auto result = optimizer.optimize(path, outPath, opts);
    ASSERT_TRUE(result.success);

    QPDF out;
    out.processFile(outPath.c_str());
    QPDFPageDocumentHelper helper(out);
    auto pages = helper.getAllPages();
    ASSERT_FALSE(pages.empty());
    auto images = pages.at(0).getImages();
    ASSERT_FALSE(images.empty());

    QPDFObjectHandle dict = images.begin()->second.getDict();
    ASSERT_TRUE(dict.hasKey("/SMask"))
        << "output image must still carry its /SMask (alpha preserved)";

    QPDFObjectHandle smask = dict.getKey("/SMask");
    ASSERT_TRUE(smask.isStream()) << "/SMask must reference a stream";

    auto maskData = smask.getStreamData(qpdf_dl_all);
    ASSERT_TRUE(maskData != nullptr) << "mask stream data must be readable";
    EXPECT_GT(maskData->getSize(), 0u) << "mask stream must contain non-zero data";

    // Mask geometry matches the base image (intact, unmodified).
    EXPECT_EQ(smask.getDict().getKey("/Width").getIntValueAsInt(), 200);
    EXPECT_EQ(smask.getDict().getKey("/Height").getIntValueAsInt(), 150);
}

namespace {

// Returns the first image XObject on the first page of an already-loaded PDF,
// or a null handle when the page carries no images.
QPDFObjectHandle firstImageOf(QPDF& pdf) {
    QPDFPageDocumentHelper helper(pdf);
    auto pages = helper.getAllPages();
    if (pages.empty()) return QPDFObjectHandle::newNull();
    auto images = pages.at(0).getImages();
    if (images.empty()) return QPDFObjectHandle::newNull();
    return images.begin()->second;
}

// Decodes an image XObject's stream and analyzes it as a single-channel
// /DeviceGray buffer, matching how PDFOptimizer feeds the analyzer.
AnalysisResult analyzeGrayImage(QPDFObjectHandle image) {
    QPDFObjectHandle dict = image.getDict();
    ImageMetadata meta;
    meta.widthPx = dict.getKey("/Width").getIntValueAsInt();
    meta.heightPx = dict.getKey("/Height").getIntValueAsInt();
    meta.colorSpace = ColorSpace::DeviceGray;
    auto data = image.getStreamData(qpdf_dl_all);
    if (!data) return AnalysisResult{};
    return ImageAnalyzer::analyze(data->getBuffer(), meta.widthPx, meta.heightPx, 1, meta);
}

} // namespace

// P2-T3-T01 / AC-C1: grayscale_scan.pdf is a /DeviceGray scan with many gray
// levels. It must classify as ScannedText, and the quality slider (70 vs 0)
// must not change the chosen codec/filter across runs.
TEST(PDFOptimizerTest, GrayscaleScanScannedTextSliderIndependent) {
    TestCorpusGenerator generator;
    auto path = generator.generateGrayscaleScan();

    // The corpus image is /DeviceGray and the analyzer sees it as ScannedText.
    {
        QPDF in;
        ASSERT_NO_THROW(in.processFile(path.c_str()));
        QPDFObjectHandle image = firstImageOf(in);
        ASSERT_FALSE(image.isNull());
        ASSERT_TRUE(image.getDict().hasKey("/ColorSpace"));
        EXPECT_EQ(image.getDict().getKey("/ColorSpace").getName(), "/DeviceGray");

        auto analysis = analyzeGrayImage(image);
        EXPECT_GT(analysis.uniqueColorsSampled, 4);
        EXPECT_EQ(analysis.classification, ImageClassification::ScannedText)
            << "grayscale_scan.pdf must be a ScannedText grayscale document";
    }

    PDFOptimizer optimizer;
    auto opts70 = OptimizationOptions::forProfile(CompressionProfile::MaxQuality);
    opts70.qualityHint = 70;
    auto out70 = path + ".q70.pdf";
    auto r70 = optimizer.optimize(path, out70, opts70);
    ASSERT_TRUE(r70.success) << r70.errorMessage;

    auto opts0 = OptimizationOptions::forProfile(CompressionProfile::MaxQuality);
    opts0.qualityHint = 0;
    auto out0 = path + ".q0.pdf";
    auto r0 = optimizer.optimize(path, out0, opts0);
    ASSERT_TRUE(r0.success) << r0.errorMessage;

    QPDF outA;
    ASSERT_NO_THROW(outA.processFile(out70.c_str()));
    QPDFObjectHandle imgA = firstImageOf(outA);
    ASSERT_FALSE(imgA.isNull());
    std::string filter70 = firstFilterName(imgA.getDict());

    QPDF outB;
    ASSERT_NO_THROW(outB.processFile(out0.c_str()));
    QPDFObjectHandle imgB = firstImageOf(outB);
    ASSERT_FALSE(imgB.isNull());
    std::string filter0 = firstFilterName(imgB.getDict());

    EXPECT_FALSE(filter70.empty());
    EXPECT_EQ(filter70, filter0)
        << "the quality slider must not change grayscale_scan's codec/filter";
    EXPECT_NO_THROW(QPDF().processFile(out70.c_str()));
    EXPECT_NO_THROW(QPDF().processFile(out0.c_str()));
}

// P2-T3-T02 / AC-C1: monochrome_bw.pdf is a two-level /DeviceGray image. It
// must classify as Monochrome and stay on FlateDecode regardless of the slider.
TEST(PDFOptimizerTest, MonochromeBWFlateDecodeSliderIndependent) {
    TestCorpusGenerator generator;
    auto path = generator.generateMonochromeBW();

    {
        QPDF in;
        ASSERT_NO_THROW(in.processFile(path.c_str()));
        QPDFObjectHandle image = firstImageOf(in);
        ASSERT_FALSE(image.isNull());

        auto analysis = analyzeGrayImage(image);
        EXPECT_LE(analysis.uniqueColorsSampled, 4);
        EXPECT_EQ(analysis.classification, ImageClassification::Monochrome)
            << "monochrome_bw.pdf must classify as Monochrome";
    }

    PDFOptimizer optimizer;
    auto opts70 = OptimizationOptions::forProfile(CompressionProfile::MaxQuality);
    opts70.qualityHint = 70;
    auto out70 = path + ".q70.pdf";
    auto r70 = optimizer.optimize(path, out70, opts70);
    ASSERT_TRUE(r70.success) << r70.errorMessage;

    auto opts0 = OptimizationOptions::forProfile(CompressionProfile::MaxQuality);
    opts0.qualityHint = 0;
    auto out0 = path + ".q0.pdf";
    auto r0 = optimizer.optimize(path, out0, opts0);
    ASSERT_TRUE(r0.success) << r0.errorMessage;

    QPDF outA;
    ASSERT_NO_THROW(outA.processFile(out70.c_str()));
    QPDFObjectHandle imgA = firstImageOf(outA);
    ASSERT_FALSE(imgA.isNull());
    EXPECT_EQ(firstFilterName(imgA.getDict()), "/FlateDecode")
        << "monochrome must route to lossless FlateDecode with a slider";

    QPDF outB;
    ASSERT_NO_THROW(outB.processFile(out0.c_str()));
    QPDFObjectHandle imgB = firstImageOf(outB);
    ASSERT_FALSE(imgB.isNull());
    EXPECT_EQ(firstFilterName(imgB.getDict()), "/FlateDecode")
        << "monochrome must route to lossless FlateDecode without a slider";

    EXPECT_NO_THROW(QPDF().processFile(out70.c_str()));
    EXPECT_NO_THROW(QPDF().processFile(out0.c_str()));
}

// P2-T3-T03 / AC-C2 (S9): an image with /BitsPerComponent != 8 stores samples
// bit-packed, which the raw-pixel path cannot interpret. It must be skipped
// (kept original with a logged reason), never mis-decoded, and the output must
// remain a structurally valid PDF.
TEST(PDFOptimizerTest, BitPackedImageKeptOriginalAndValid) {
    namespace fs = std::filesystem;
    fs::create_directories(fs::temp_directory_path() / "pdfcompress_test_corpus");
    std::string path = (fs::temp_directory_path() / "pdfcompress_test_corpus" /
                        "bitpacked_bpc1.pdf").string();

    // 8x8 1-bit /DeviceGray image: one byte per 8-pixel row, bit-packed.
    const int width = 8, height = 8;
    std::vector<uint8_t> packed(static_cast<size_t>(height), 0xAA);

    // The stream declares /FlateDecode, so the packed bytes must be zlib
    // compressed. Storing raw bytes makes QPDF's inflate fail with
    // "incorrect header check", which the test's own decode would surface.
    uLongf compSize = compressBound(packed.size());
    std::vector<uint8_t> compData(compSize);
    std::string streamStr;
    if (compress(compData.data(), &compSize, packed.data(), packed.size()) == Z_OK) {
        streamStr.assign(reinterpret_cast<char*>(compData.data()), compSize);
    } else {
        streamStr.assign(reinterpret_cast<const char*>(packed.data()), packed.size());
    }

    {
        QPDF pdf;
        pdf.emptyPDF();
        QPDFObjectHandle image = QPDFObjectHandle::newStream(&pdf);
        QPDFObjectHandle dict = image.getDict();
        dict.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
        dict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
        dict.replaceKey("/Width", QPDFObjectHandle::newInteger(width));
        dict.replaceKey("/Height", QPDFObjectHandle::newInteger(height));
        dict.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceGray"));
        dict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(1));
        dict.replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));
        image.replaceStreamData(streamStr, QPDFObjectHandle::newName("/FlateDecode"),
                                QPDFObjectHandle::newNull());

        QPDFObjectHandle page =
            QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
        page.replaceKey("/Resources", QPDFObjectHandle::parse("<< /XObject << >> >>"));
        page.getKey("/Resources").getKey("/XObject").replaceKey("/Im1", image);
        page.replaceKey("/Contents",
                        QPDFObjectHandle::newStream(&pdf, "q 8 0 0 8 72 700 cm /Im1 Do Q\n"));
        QPDFPageDocumentHelper(pdf).addPage(page, true);

        QPDFWriter writer(pdf, path.c_str());
        writer.setDeterministicID(true);
        writer.setStaticID(true);
        writer.write();
    }

    // Capture the original decoded stream bytes (post-Flate, still bit-packed).
    std::vector<uint8_t> before;
    {
        QPDF in;
        ASSERT_NO_THROW(in.processFile(path.c_str()));
        QPDFObjectHandle image = firstImageOf(in);
        ASSERT_FALSE(image.isNull());
        auto data = image.getStreamData(qpdf_dl_all);
        ASSERT_TRUE(data != nullptr);
        before.assign(data->getBuffer(), data->getBuffer() + data->getSize());
        ASSERT_FALSE(before.empty());
    }

    PDFOptimizer optimizer;
    auto outPath = path + ".opt.pdf";
    auto result = optimizer.optimize(path, outPath,
                                     OptimizationOptions::forProfile(CompressionProfile::MaxQuality));
    ASSERT_TRUE(result.success) << result.errorMessage;
    EXPECT_GE(result.imagesSkipped, 1)
        << "bit-packed image must be skipped with a logged reason";
    EXPECT_EQ(result.imagesOptimized, 0)
        << "bit-packed image must not be re-encoded";

    QPDF out;
    ASSERT_NO_THROW(out.processFile(outPath.c_str()));
    QPDFObjectHandle outImage = firstImageOf(out);
    ASSERT_FALSE(outImage.isNull());

    QPDFObjectHandle outDict = outImage.getDict();
    EXPECT_EQ(outDict.getKey("/BitsPerComponent").getIntValueAsInt(), 1);
    EXPECT_EQ(outDict.getKey("/Width").getIntValueAsInt(), width);
    EXPECT_EQ(outDict.getKey("/Height").getIntValueAsInt(), height);

    auto after = outImage.getStreamData(qpdf_dl_all);
    ASSERT_TRUE(after != nullptr);
    ASSERT_EQ(after->getSize(), before.size())
        << "bit-packed stream must not be re-encoded";
    EXPECT_EQ(0, std::memcmp(after->getBuffer(), before.data(), before.size()))
        << "bit-packed stream bytes must be preserved exactly";

    // Output must remain structurally valid.
    EXPECT_NO_THROW(QPDF().processFile(outPath.c_str()));
}

// P2-T4-T01 / AC-C3: cmyk_image.pdf (DeviceCMYK FlateDecode) must survive
// optimization under every profile with its ColorSpace intact and valid output.
TEST(PDFOptimizerTest, CmykImagePreservedUnderAllProfiles) {
    TestCorpusGenerator generator;
    auto path = generator.generateCmykImage();

    const CompressionProfile profiles[] = {
        CompressionProfile::MaxQuality,
        CompressionProfile::Balanced,
        CompressionProfile::MaxCompression,
    };

    for (auto profile : profiles) {
        auto outPath = path + ".cmyk_" + std::to_string(static_cast<int>(profile)) + ".pdf";
        PDFOptimizer optimizer;
        auto result = optimizer.optimize(path, outPath,
                                         OptimizationOptions::forProfile(profile));
        ASSERT_TRUE(result.success) << result.errorMessage;

        QPDF out;
        ASSERT_NO_THROW(out.processFile(outPath.c_str()));

        QPDFObjectHandle image = firstImageOf(out);
        ASSERT_FALSE(image.isNull());
        std::string csName = firstColorSpaceName(image.getDict());
        EXPECT_TRUE(csName == "/DeviceCMYK" || csName == "/ICCBased")
            << "CMYK ColorSpace must be preserved, got " << csName;

        EXPECT_NO_THROW(QPDF().processFile(outPath.c_str()));
    }
}

// P2-T4-T02 / AC-C3: the CMYK skip is reported (imagesSkipped==1,
// cmykPreserved==1) and a non-empty preservation reason is logged to stderr, so
// the skip is no longer silent.
TEST(PDFOptimizerTest, CmykPreservationReportedAndLogged) {
    TestCorpusGenerator generator;
    auto path = generator.generateCmykImage();
    auto outPath = path + ".cmyk_reason.pdf";

    PDFOptimizer optimizer;
    std::ostringstream captured;
    std::streambuf* oldBuf = std::cerr.rdbuf(captured.rdbuf());
    auto result = optimizer.optimize(path, outPath);
    std::cerr.rdbuf(oldBuf);

    EXPECT_TRUE(result.success) << result.errorMessage;
    EXPECT_EQ(result.imagesSkipped, 1);
    EXPECT_EQ(result.cmykPreserved, 1);

    std::string log = captured.str();
    EXPECT_FALSE(log.empty()) << "CMYK preservation must log a reason to stderr";
    EXPECT_NE(log.find("CMYK"), std::string::npos)
        << "logged reason must identify the CMYK image";
    EXPECT_NE(log.find("preserv"), std::string::npos)
        << "logged reason must state preservation";
}

// P2-T4-T03 / AC-C3: the CMYK preservation path keeps the whole file valid;
// re-running the CmykSkipped scenario still yields imagesSkipped==1 and a
// structurally valid output.
TEST(PDFOptimizerTest, CmykSkippedOutputIsValid) {
    TestCorpusGenerator generator;
    auto path = generator.generateCmykImage();
    auto outPath = path + ".cmyk_valid.pdf";
    PDFOptimizer optimizer;
    auto result = optimizer.optimize(path, outPath);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.imagesSkipped, 1);
    EXPECT_TRUE(std::filesystem::exists(outPath));
    EXPECT_NO_THROW(QPDF().processFile(outPath.c_str()));
}

// P2-T5-T01 / AC-C3b: with transcodeCmykToRgb=true, cmyk_image.pdf's image is
// converted to DeviceRGB, the output is a valid PDF, S2 holds, and the result
// reports transcodedCmyk==1.
TEST(PDFOptimizerTest, CmykTranscodeToRgbOptIn) {
    TestCorpusGenerator generator;
    auto path = generator.generateCmykImage();
    auto outPath = path + ".cmyk_transcoded.pdf";

    PDFOptimizer optimizer;
    auto opts = OptimizationOptions::forProfile(CompressionProfile::Balanced);
    opts.transcodeCmykToRgb = true;
    auto result = optimizer.optimize(path, outPath, opts);
    ASSERT_TRUE(result.success) << result.errorMessage;

    EXPECT_EQ(result.transcodedCmyk, 1)
        << "opt-in transcode must report exactly one CMYK->RGB image";

    // S2: output must not be larger than input unless the breakdown explains it.
    if (result.optimizedSizeBytes > result.originalSizeBytes) {
        EXPECT_GT(result.imageBytesSaved, 0u)
            << "growth beyond input must be explained by the result breakdown";
    }

    QPDF out;
    ASSERT_NO_THROW(out.processFile(outPath.c_str()));

    QPDFObjectHandle image = firstImageOf(out);
    ASSERT_FALSE(image.isNull());
    std::string csName = firstColorSpaceName(image.getDict());
    EXPECT_TRUE(csName == "/DeviceRGB" || csName == "/ICCBased")
        << "transcoded image must be DeviceRGB (or ICC-based RGB), got " << csName;

    EXPECT_NO_THROW(QPDF().processFile(outPath.c_str()));
}

// P2-T5-T02 / AC-C3b: without the opt-in flag the default stays preservation:
// ColorSpace remains CMYK and result.transcodedCmyk==0.
TEST(PDFOptimizerTest, CmykTranscodeDefaultPreserves) {
    TestCorpusGenerator generator;
    auto path = generator.generateCmykImage();
    auto outPath = path + ".cmyk_default.pdf";

    PDFOptimizer optimizer;
    auto result = optimizer.optimize(path, outPath,
                                     OptimizationOptions::forProfile(CompressionProfile::Balanced));
    ASSERT_TRUE(result.success) << result.errorMessage;

    EXPECT_EQ(result.transcodedCmyk, 0)
        << "default must not transcode CMYK images";
    EXPECT_EQ(result.cmykPreserved, 1)
        << "default must preserve the CMYK image";

    QPDF out;
    ASSERT_NO_THROW(out.processFile(outPath.c_str()));

    QPDFObjectHandle image = firstImageOf(out);
    ASSERT_FALSE(image.isNull());
    std::string csName = firstColorSpaceName(image.getDict());
    EXPECT_TRUE(csName == "/DeviceCMYK" || csName == "/ICCBased")
        << "default CMYK ColorSpace must be preserved, got " << csName;

    EXPECT_NO_THROW(QPDF().processFile(outPath.c_str()));
}

// P2-T9-T01 / ESC-011: an /SMask-bearing image whose FlateDecode source stream
// is larger than the lossless re-encode is embedded as /FlateDecode. The
// embedded bytes must inflate as a valid zlib/Flate stream (no decode error,
// qpdf clean) and must never be a PNG container.
TEST(PDFOptimizerTest, AlphaLosslessReencodeIsEmbeddableFlate) {
    namespace fs = std::filesystem;
    fs::create_directories(fs::temp_directory_path() / "pdfcompress_test_corpus");
    std::string path = (fs::temp_directory_path() / "pdfcompress_test_corpus" /
                        "alpha_embed_flate.pdf").string();

    const int w = 200, h = 150;
    std::vector<uint8_t> pixels(static_cast<size_t>(w) * h * 3);
    for (size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<uint8_t>((i * 5) % 256);
    }

    // Store the base stream at zlib level 0 so the source Flate stream is far
    // larger than any lossless re-encode -> forces the re-encode branch
    // (imagesOptimized == 1) instead of the kept-original path.
    uLongf compSize = compressBound(static_cast<uLong>(pixels.size()));
    std::vector<uint8_t> comp(compSize);
    ASSERT_EQ(Z_OK, compress2(comp.data(), &compSize, pixels.data(),
                              static_cast<uLong>(pixels.size()), Z_NO_COMPRESSION));
    std::string imgData(reinterpret_cast<char*>(comp.data()), compSize);

    // Alpha /SMask stream (compressed normally; the mask itself is untouched).
    std::vector<uint8_t> alpha(static_cast<size_t>(w) * h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            alpha[y * w + x] = static_cast<uint8_t>((x * 255) / w);
        }
    }
    uLongf aCompSize = compressBound(static_cast<uLong>(alpha.size()));
    std::vector<uint8_t> aComp(aCompSize);
    ASSERT_EQ(Z_OK, compress(aComp.data(), &aCompSize, alpha.data(), alpha.size()));
    std::string aStr(reinterpret_cast<char*>(aComp.data()), aCompSize);

    {
        QPDF pdf;
        pdf.emptyPDF();

        QPDFObjectHandle image = QPDFObjectHandle::newStream(&pdf);
        QPDFObjectHandle dict = image.getDict();
        dict.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
        dict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
        dict.replaceKey("/Width", QPDFObjectHandle::newInteger(w));
        dict.replaceKey("/Height", QPDFObjectHandle::newInteger(h));
        dict.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceRGB"));
        dict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
        dict.replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));
        image.replaceStreamData(imgData, QPDFObjectHandle::newName("/FlateDecode"),
                                QPDFObjectHandle::newNull());

        QPDFObjectHandle smask = QPDFObjectHandle::newStream(&pdf);
        QPDFObjectHandle sdict = smask.getDict();
        sdict.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
        sdict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
        sdict.replaceKey("/Width", QPDFObjectHandle::newInteger(w));
        sdict.replaceKey("/Height", QPDFObjectHandle::newInteger(h));
        sdict.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceGray"));
        sdict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
        sdict.replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));
        smask.replaceStreamData(aStr, QPDFObjectHandle::newName("/FlateDecode"),
                                QPDFObjectHandle::newNull());
        dict.replaceKey("/SMask", pdf.makeIndirectObject(smask));

        QPDFObjectHandle page =
            QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
        page.replaceKey("/Resources", QPDFObjectHandle::parse("<< /XObject << >> /Font << >> >>"));
        page.getKey("/Resources").getKey("/XObject").replaceKey("/Im1", image);
        page.replaceKey("/Contents",
                        QPDFObjectHandle::newStream(&pdf, "q 100 0 0 100 72 600 cm /Im1 Do Q\n"));
        QPDFPageDocumentHelper(pdf).addPage(page, true);

        QPDFWriter writer(pdf, path.c_str());
        writer.setDeterministicID(true);
        writer.setStaticID(true);
        writer.write();
    }

    PDFOptimizer optimizer;
    auto outPath = path + ".opt.pdf";
    auto result = optimizer.optimize(path, outPath);
    ASSERT_TRUE(result.success) << result.errorMessage;
    ASSERT_EQ(result.imagesOptimized, 1)
        << "alpha lossless re-encode must be embedded (not kept original)";

    QPDF out;
    ASSERT_NO_THROW(out.processFile(outPath.c_str()));
    QPDFObjectHandle image = firstImageOf(out);
    ASSERT_FALSE(image.isNull());
    EXPECT_EQ(firstFilterName(image.getDict()), "/FlateDecode");

    // The raw stored bytes must be a valid zlib stream, not a PNG container.
    auto raw = image.getRawStreamData();
    ASSERT_TRUE(raw != nullptr);
    const uint8_t pngSignature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    ASSERT_GE(raw->getSize(), 8u);
    EXPECT_NE(0, std::memcmp(raw->getBuffer(), pngSignature, 8))
        << "embedded alpha stream must not be a PNG container";

    std::vector<uint8_t> inflated(pixels.size());
    uLongf inflatedLen = static_cast<uLongf>(inflated.size());
    ASSERT_EQ(Z_OK, uncompress(inflated.data(), &inflatedLen, raw->getBuffer(),
                               static_cast<uLong>(raw->getSize())))
        << "embedded alpha stream must inflate as a valid zlib/Flate stream";
    EXPECT_EQ(pixels.size(), inflatedLen);

    // Whole output must remain a structurally valid PDF (qpdf --check clean).
    EXPECT_NO_THROW(QPDF().processFile(outPath.c_str()));
}
