#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pdfcompress {

/// Represents the color space of an image object within a PDF.
enum class ColorSpace {
    Unknown,
    DeviceGray,
    DeviceRGB,
    DeviceCMYK,
    Indexed,
    ICCBased
};

/// Represents the compression filter applied to the image stream.
enum class StreamFilter {
    Unknown,
    None,         // Raw / uncompressed
    DCTDecode,    // JPEG
    JPXDecode,    // JPEG 2000
    FlateDecode,  // zlib/Deflate (often PNG-like)
    JBIG2Decode,  // JBIG2
    CCITTFaxDecode, // Fax compression (monochrome)
    RunLengthDecode
};

/// Classification of an image for the Compression Decision Engine.
enum class ImageClassification {
    Unknown,
    Photo,        // Natural photographic content
    Screenshot,   // UI / rendered graphics
    LineArt,      // Vector-like drawings rendered to bitmap
    ScannedText,  // Scanned document page
    Monochrome    // Pure black-and-white
};

/// Complete metadata record for a single image object extracted from a PDF.
struct ImageMetadata {
    // Identity
    int pageIndex = -1;        ///< 0-based page number
    int objectIndex = -1;      ///< Object index within the page
    uint32_t pdfObjectId = 0;  ///< QPDF object ID for replacement

    // Dimensions
    int widthPx = 0;           ///< Width in pixels
    int heightPx = 0;          ///< Height in pixels
    float dpiX = 0.0f;         ///< Horizontal DPI on the page
    float dpiY = 0.0f;         ///< Vertical DPI on the page

    // Color properties
    ColorSpace colorSpace = ColorSpace::Unknown;
    int bitsPerComponent = 0;
    bool hasAlpha = false;

    // Stream properties
    StreamFilter filter = StreamFilter::Unknown;
    size_t compressedSize = 0;   ///< Size of the compressed stream in bytes
    size_t uncompressedSize = 0; ///< Estimated size of the raw pixel data

    // Analysis results (filled in Phase 3)
    ImageClassification classification = ImageClassification::Unknown;
    float entropy = 0.0f;
    int uniqueColorsSampled = 0;

    /// Human-readable summary for logging.
    std::string summary() const;
};

/// Result of inspecting an entire PDF document.
struct PDFDocumentInfo {
    std::string filePath;
    int pageCount = 0;
    size_t fileSizeBytes = 0;
    bool isEncrypted = false;
    std::string pdfVersion;
    std::vector<ImageMetadata> images;

    /// Total number of images found.
    size_t imageCount() const { return images.size(); }

    /// Total compressed image bytes.
    size_t totalImageBytes() const;
};

// --- Inline helpers ---

inline std::string colorSpaceToString(ColorSpace cs) {
    switch (cs) {
        case ColorSpace::DeviceGray: return "DeviceGray";
        case ColorSpace::DeviceRGB:  return "DeviceRGB";
        case ColorSpace::DeviceCMYK: return "DeviceCMYK";
        case ColorSpace::Indexed:    return "Indexed";
        case ColorSpace::ICCBased:   return "ICCBased";
        default:                     return "Unknown";
    }
}

inline std::string streamFilterToString(StreamFilter f) {
    switch (f) {
        case StreamFilter::None:            return "None";
        case StreamFilter::DCTDecode:       return "DCTDecode (JPEG)";
        case StreamFilter::JPXDecode:       return "JPXDecode (JPEG 2000)";
        case StreamFilter::FlateDecode:     return "FlateDecode (zlib)";
        case StreamFilter::JBIG2Decode:     return "JBIG2Decode";
        case StreamFilter::CCITTFaxDecode:  return "CCITTFaxDecode";
        case StreamFilter::RunLengthDecode: return "RunLengthDecode";
        default:                            return "Unknown";
    }
}

} // namespace pdfcompress
