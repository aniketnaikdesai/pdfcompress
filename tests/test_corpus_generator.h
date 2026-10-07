#pragma once
#include <string>
#include <filesystem>
#include <vector>

namespace pdfcompress::test {

class TestCorpusGenerator {
public:
    TestCorpusGenerator();
    ~TestCorpusGenerator();

    std::filesystem::path corpusDir() const { return m_dir; } // pdfcompress_test_corpus in $TMPDIR/pdfcompress_test_corpus

    std::string generateTextOnly();
    std::string generatePhotoJpeg();
    std::string generatePhotoHeavy();
    std::string generateScreenshotFlat();
    std::string generateMonochromeBW();
    std::string generateGrayscaleScan();
    std::string generateLineArt();
    std::string generateTransparency();
    std::string generateEncrypted();
    std::string generateWithForm();
    std::string generateWithMetadata();
    std::string generateWithJavaScript();
    std::string generateMultiImage();
    std::string generateSharedXObject();
    std::string generateMergedDuplicateFonts();
    std::string generateWithBookmarks();
    std::string generateWithAnnotations();
    std::string generateWithBookmarksAndLinks();
    std::string generateCmykImage();
    std::string generateLargeUncompressed();
    std::string generateDistinctDuplicateStreams();
    std::string generateMaskedDuplicateStreams();

    void generateAll();

private:
    std::filesystem::path m_dir;

    std::string createPdfWithImage(
        const std::string& name,
        const std::vector<uint8_t>& pixelData,
        int width, int height, int channels,
        const std::string& colorSpace,
        const std::string& filter = "/FlateDecode");
};

} // namespace pdfcompress::test
