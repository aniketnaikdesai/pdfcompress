#pragma once

#include <string>
#include <vector>
#include "DecisionEngine.h"

namespace pdfcompress {

struct OptimizationOptions {
    CompressionProfile profile = CompressionProfile::Balanced;
    int qualityHint = 0;

    // Granular stripping options
    bool stripLinks = false;
    bool stripOtherAnnotations = false;
    bool stripBookmarks = false;
    bool stripForms = false;
    bool stripJavaScript = false;
    bool stripNamedDestinations = false;
    bool stripMetadata = false;

    // Structural options
    bool linearize = false;          // Linearization off by default
    bool recompressFlate = true;     // Level 9 Flate recompression
    bool deduplicateStreams = true;  // Content-hash stream deduplication

    // Opt-in CMYK->RGB transcode (AC-C3b). OFF by default so DeviceCMYK /
    // ICCBased-CMYK images are preserved byte-for-byte; when enabled the
    // optimizer converts them to DeviceRGB and re-encodes as JPEG.
    bool transcodeCmykToRgb = false;

    static OptimizationOptions forProfile(CompressionProfile prof) {
        OptimizationOptions opts;
        opts.profile = prof;
        opts.qualityHint = 0;
        opts.linearize = false;
        opts.recompressFlate = true;
        opts.deduplicateStreams = true;
        switch (prof) {
            case CompressionProfile::MaxQuality:
                opts.stripLinks = false;
                opts.stripOtherAnnotations = false;
                opts.stripBookmarks = false;
                opts.stripForms = false;
                opts.stripJavaScript = false;
                opts.stripNamedDestinations = false;
                opts.stripMetadata = false;
                break;
            case CompressionProfile::Balanced:
                opts.stripLinks = false;
                opts.stripOtherAnnotations = false;
                opts.stripBookmarks = false;
                opts.stripForms = false;
                opts.stripJavaScript = true;
                opts.stripNamedDestinations = false;
                opts.stripMetadata = true;
                break;
            case CompressionProfile::MaxCompression:
                opts.stripLinks = true;
                opts.stripOtherAnnotations = true;
                opts.stripBookmarks = true;
                opts.stripForms = true;
                opts.stripJavaScript = true;
                opts.stripNamedDestinations = true;
                opts.stripMetadata = true;
                break;
        }
        return opts;
    }
};

struct OptimizationResult {
    bool success = false;
    size_t originalSizeBytes = 0;
    size_t optimizedSizeBytes = 0;

    // Image breakdown
    int imagesProcessed = 0;
    int imagesOptimized = 0;
    int imagesSkipped = 0;
    int imagesKeptOriginal = 0;
    size_t imageBytesSaved = 0;
    // CMYK (DeviceCMYK / ICCBased-CMYK) images are preserved byte-for-byte
    // rather than re-encoded; this counts how many took that path.
    int cmykPreserved = 0;
    // Number of CMYK images converted to RGB under the opt-in
    // transcodeCmykToRgb option (AC-C3b).
    int transcodedCmyk = 0;

    // Stripping breakdown
    int linksRemoved = 0;
    int annotationsRemoved = 0;
    int bookmarksRemoved = 0;
    int formsRemoved = 0;
    int jsRemoved = 0;
    int namedDestinationsRemoved = 0;
    bool metadataStripped = false;

    // Structural breakdown
    int streamsDeduplicated = 0;

    std::string errorMessage;
};

class PDFOptimizer {
public:
    PDFOptimizer();

    /// Optimize a PDF using full options.
    OptimizationResult optimize(const std::string& inputPath,
                                const std::string& outputPath,
                                const OptimizationOptions& options);

    /// Backwards-compatible overload.
    OptimizationResult optimize(const std::string& inputPath, 
                                const std::string& outputPath,
                                CompressionProfile profile = CompressionProfile::Balanced,
                                int qualityHint = 0) {
        OptimizationOptions opts = OptimizationOptions::forProfile(profile);
        opts.qualityHint = qualityHint;
        return optimize(inputPath, outputPath, opts);
    }
                                
private:
    DecisionEngine m_decisionEngine;
};

} // namespace pdfcompress
