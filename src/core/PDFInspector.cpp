#include "PDFInspector.h"

#include "fpdfview.h"
#include "fpdf_edit.h"

#include <stdexcept>
#include <filesystem>
#include <cmath>
#include <cstring>

namespace pdfcompress {

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

PDFInspector::PDFInspector() {
    FPDF_LIBRARY_CONFIG config;
    std::memset(&config, 0, sizeof(config));
    config.version = 2;
    FPDF_InitLibraryWithConfig(&config);
    m_initialized = true;
}

PDFInspector::~PDFInspector() {
    if (m_initialized) {
        FPDF_DestroyLibrary();
        m_initialized = false;
    }
}

PDFInspector::PDFInspector(PDFInspector&& other) noexcept
    : m_initialized(other.m_initialized) {
    other.m_initialized = false;
}

PDFInspector& PDFInspector::operator=(PDFInspector&& other) noexcept {
    if (this != &other) {
        if (m_initialized) {
            FPDF_DestroyLibrary();
        }
        m_initialized = other.m_initialized;
        other.m_initialized = false;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool PDFInspector::isEncrypted(const std::string& filePath) {
    FPDF_LIBRARY_CONFIG config;
    std::memset(&config, 0, sizeof(config));
    config.version = 2;
    FPDF_InitLibraryWithConfig(&config);

    FPDF_DOCUMENT doc = FPDF_LoadDocument(filePath.c_str(), nullptr);
    bool encrypted = false;
    if (!doc) {
        unsigned long err = FPDF_GetLastError();
        // FPDF_ERR_PASSWORD means the document is encrypted
        encrypted = (err == FPDF_ERR_PASSWORD);
    } else {
        FPDF_CloseDocument(doc);
    }
    FPDF_DestroyLibrary();
    return encrypted;
}

PDFDocumentInfo PDFInspector::inspect(const std::string& filePath,
                                       InspectionProgressCallback progressCb) {
    if (!m_initialized) {
        throw std::runtime_error("PDFInspector: library not initialized");
    }

    namespace fs = std::filesystem;

    if (!fs::exists(filePath)) {
        throw std::runtime_error("PDFInspector: file not found: " + filePath);
    }

    PDFDocumentInfo info;
    info.filePath = filePath;
    info.fileSizeBytes = fs::file_size(filePath);

    // Open the document (no password - we reject encrypted PDFs)
    FPDF_DOCUMENT doc = FPDF_LoadDocument(filePath.c_str(), nullptr);
    if (!doc) {
        unsigned long err = FPDF_GetLastError();
        if (err == FPDF_ERR_PASSWORD) {
            info.isEncrypted = true;
            throw std::runtime_error("PDFInspector: PDF is encrypted/password-protected");
        }
        throw std::runtime_error("PDFInspector: failed to open PDF (error code: "
                                 + std::to_string(err) + ")");
    }

    info.pageCount = FPDF_GetPageCount(doc);

    // Extract PDF version
    int version = 0;
    if (FPDF_GetFileVersion(doc, &version)) {
        info.pdfVersion = std::to_string(version / 10) + "." + std::to_string(version % 10);
    }

    // Iterate all pages and extract images
    for (int pageIdx = 0; pageIdx < info.pageCount; ++pageIdx) {
        auto pageImages = extractImagesFromPage(doc, pageIdx);
        info.images.insert(info.images.end(),
                           std::make_move_iterator(pageImages.begin()),
                           std::make_move_iterator(pageImages.end()));

        if (progressCb) {
            progressCb(pageIdx + 1, info.pageCount);
        }
    }

    FPDF_CloseDocument(doc);
    return info;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::vector<ImageMetadata> PDFInspector::extractImagesFromPage(
    FPDF_DOCUMENT doc, int pageIndex) {

    std::vector<ImageMetadata> results;

    FPDF_PAGE page = FPDF_LoadPage(doc, pageIndex);
    if (!page) return results;

    int objCount = FPDFPage_CountObjects(page);
    int imageIdx = 0;

    for (int i = 0; i < objCount; ++i) {
        FPDF_PAGEOBJECT obj = FPDFPage_GetObject(page, i);
        if (!obj) continue;

        // Only process image objects
        if (FPDFPageObj_GetType(obj) != FPDF_PAGEOBJ_IMAGE) continue;

        ImageMetadata meta;
        meta.pageIndex = pageIndex;
        meta.objectIndex = imageIdx++;

        // --- Dimensions ---
        unsigned int width = 0, height = 0;
        // FPDFImageObj_GetImagePixelSize was added in newer PDFium
        // We use the metadata approach instead
        FPDF_IMAGEOBJ_METADATA imgMeta;
        if (FPDFImageObj_GetImageMetadata(obj, page, &imgMeta)) {
            meta.widthPx = static_cast<int>(imgMeta.width);
            meta.heightPx = static_cast<int>(imgMeta.height);
            meta.bitsPerComponent = static_cast<int>(imgMeta.bits_per_pixel);

            // Color space from PDFium metadata
            meta.colorSpace = parseColorSpace(imgMeta.colorspace);
        }

        // --- DPI calculation from page placement ---
        calculateDPI(meta, page);

        // --- Filter / compression detection ---
        // We examine the image data size as an approximation.
        // Full filter detection requires QPDF in Phase 5; here we record
        // what PDFium can tell us.
        unsigned long dataLen = 0;
        if (FPDFImageObj_GetImageDataRaw(obj, nullptr, 0)) {
            dataLen = FPDFImageObj_GetImageDataRaw(obj, nullptr, 0);
        }
        meta.compressedSize = dataLen;

        // Estimate uncompressed size from dimensions
        int channels = 3; // Default RGB
        if (meta.colorSpace == ColorSpace::DeviceGray) channels = 1;
        else if (meta.colorSpace == ColorSpace::DeviceCMYK) channels = 4;
        meta.uncompressedSize = static_cast<size_t>(meta.widthPx) *
                                meta.heightPx * channels;

        // Check for alpha (PDFium marks_transparency)
        meta.hasAlpha = FPDFPageObj_HasTransparency(obj);

        // Heuristic filter detection from compression ratio
        if (meta.compressedSize > 0 && meta.uncompressedSize > 0) {
            float ratio = static_cast<float>(meta.compressedSize) /
                          static_cast<float>(meta.uncompressedSize);
            if (ratio > 0.8f) {
                meta.filter = StreamFilter::None; // Nearly uncompressed
            } else if (ratio < 0.15f) {
                meta.filter = StreamFilter::JPXDecode; // Very high compression
            } else {
                meta.filter = StreamFilter::DCTDecode; // Moderate = likely JPEG
            }
        }

        results.push_back(std::move(meta));
    }

    FPDF_ClosePage(page);
    return results;
}

StreamFilter PDFInspector::parseFilter(const std::string& filterName) {
    if (filterName == "DCTDecode")       return StreamFilter::DCTDecode;
    if (filterName == "JPXDecode")       return StreamFilter::JPXDecode;
    if (filterName == "FlateDecode")     return StreamFilter::FlateDecode;
    if (filterName == "JBIG2Decode")     return StreamFilter::JBIG2Decode;
    if (filterName == "CCITTFaxDecode")  return StreamFilter::CCITTFaxDecode;
    if (filterName == "RunLengthDecode") return StreamFilter::RunLengthDecode;
    return StreamFilter::Unknown;
}

ColorSpace PDFInspector::parseColorSpace(int pdfiumColorSpace) {
    // PDFium FPDF_COLORSPACE_* constants
    switch (pdfiumColorSpace) {
        case 1: return ColorSpace::DeviceGray;   // FPDF_COLORSPACE_DEVICEGRAY
        case 2: return ColorSpace::DeviceRGB;    // FPDF_COLORSPACE_DEVICERGB
        case 3: return ColorSpace::DeviceCMYK;   // FPDF_COLORSPACE_DEVICECMYK
        default: return ColorSpace::Unknown;
    }
}

void PDFInspector::calculateDPI(ImageMetadata& meta, FPDF_PAGE page) {
    if (meta.widthPx <= 0 || meta.heightPx <= 0) return;

    // Get page dimensions in points (1 point = 1/72 inch)
    double pageWidthPts = FPDF_GetPageWidth(page);
    double pageHeightPts = FPDF_GetPageHeight(page);

    if (pageWidthPts <= 0 || pageHeightPts <= 0) return;

    // Convert page dimensions to inches
    double pageWidthInches = pageWidthPts / 72.0;
    double pageHeightInches = pageHeightPts / 72.0;

    // DPI = pixels / inches
    // Note: This is a simplification; ideally we'd use the image's
    // transformation matrix to get its rendered size on the page.
    // For full-page images this is accurate.
    meta.dpiX = static_cast<float>(meta.widthPx / pageWidthInches);
    meta.dpiY = static_cast<float>(meta.heightPx / pageHeightInches);
}

} // namespace pdfcompress
