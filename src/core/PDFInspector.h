#pragma once

#include "ImageMetadata.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

// Forward-declare PDFium types to avoid leaking C headers into consumers.
// These must match the opaque struct pointer typedefs in fpdfview.h.
struct fpdf_document_t__;
typedef struct fpdf_document_t__* FPDF_DOCUMENT;
struct fpdf_page_t__;
typedef struct fpdf_page_t__* FPDF_PAGE;

namespace pdfcompress {

/// Callback for progress reporting during inspection.
/// Parameters: (currentPage, totalPages)
using InspectionProgressCallback = std::function<void(int, int)>;

/// RAII wrapper around PDFium that inspects a PDF and extracts image metadata.
///
/// Usage:
///   PDFInspector inspector;
///   auto info = inspector.inspect("/path/to/file.pdf");
///   for (const auto& img : info.images) { ... }
class PDFInspector {
public:
    PDFInspector();
    ~PDFInspector();

    // Non-copyable, movable
    PDFInspector(const PDFInspector&) = delete;
    PDFInspector& operator=(const PDFInspector&) = delete;
    PDFInspector(PDFInspector&&) noexcept;
    PDFInspector& operator=(PDFInspector&&) noexcept;

    /// Inspect a PDF file and extract all image metadata.
    /// @param filePath Absolute path to the PDF file.
    /// @param progressCb Optional callback for progress reporting.
    /// @return Complete document info including all image metadata.
    /// @throws std::runtime_error on failure to open or parse the PDF.
    PDFDocumentInfo inspect(const std::string& filePath,
                            InspectionProgressCallback progressCb = nullptr);

    /// Check if a PDF is encrypted / password-protected.
    /// @param filePath Absolute path to the PDF file.
    /// @return true if the document requires a password.
    static bool isEncrypted(const std::string& filePath);

private:
    /// Extract image objects from a single page.
    std::vector<ImageMetadata> extractImagesFromPage(FPDF_DOCUMENT doc,
                                                      int pageIndex);

    /// Determine the stream filter from PDFium's filter name string.
    static StreamFilter parseFilter(const std::string& filterName);

    /// Determine the color space from PDFium's color space identifier.
    static ColorSpace parseColorSpace(int pdfiumColorSpace);

    /// Calculate DPI from image pixel dimensions and page placement.
    static void calculateDPI(ImageMetadata& meta,
                              FPDF_PAGE page);

    bool m_initialized = false;
};

} // namespace pdfcompress
