#pragma once

#include "ImageMetadata.h"
#include <cstdint>
#include <vector>

namespace pdfcompress {

/// Detailed analysis results produced by ImageAnalyzer.
/// These results are written back into ImageMetadata fields.
struct AnalysisResult {
    ImageClassification classification = ImageClassification::Unknown;
    float entropy = 0.0f;           ///< Shannon entropy (0.0–8.0 for 8-bit data)
    int uniqueColorsSampled = 0;    ///< Distinct colors found during sampling
    bool isEffectivelyGrayscale = false; ///< True if RGB channels are identical
    float edgeDensity = 0.0f;       ///< Fraction of pixels near strong edges (0.0–1.0)
    float flatRegionRatio = 0.0f;   ///< Fraction of pixels in uniform regions
    float compressionRatio = 0.0f;  ///< compressedSize / uncompressedSize

    /// Human-readable description of the classification.
    std::string classificationName() const;
};

/// Lightweight image analyzer that classifies raw pixel buffers without OpenCV.
///
/// The analyzer operates on decoded RGBA/RGB pixel buffers and computes:
///   1. Grayscale detection (are R, G, B channels identical?)
///   2. Unique color count via sampling
///   3. Shannon entropy estimation
///   4. Edge density via Sobel-like gradient magnitude
///   5. Flat region ratio (large uniform blocks)
///   6. Final classification (Photo / Screenshot / LineArt / ScannedText / Monochrome)
///
/// All heuristics use strided sampling to stay fast on large images (e.g. 4K scans).
class ImageAnalyzer {
public:
    /// Analyze a decoded pixel buffer and return classification results.
    ///
    /// @param pixels     Pointer to decoded pixel data (RGBA or RGB, tightly packed).
    /// @param width      Image width in pixels.
    /// @param height     Image height in pixels.
    /// @param channels   Number of channels per pixel (3 = RGB, 4 = RGBA).
    /// @param meta       Existing metadata (used for compression ratio, color space hints).
    /// @return AnalysisResult with classification, entropy, etc.
    static AnalysisResult analyze(const uint8_t* pixels,
                                   int width, int height, int channels,
                                   const ImageMetadata& meta);

    /// Convenience: analyze and write results directly into the ImageMetadata struct.
    static void analyzeAndUpdate(const uint8_t* pixels,
                                  int width, int height, int channels,
                                  ImageMetadata& meta);

private:
    // --- Individual heuristic functions ---

    /// Check if an image is effectively grayscale by sampling pixels.
    /// Returns the fraction of sampled pixels where R == G == B (within tolerance).
    static float computeGrayscaleFraction(const uint8_t* pixels,
                                           int width, int height, int channels,
                                           int sampleStride = 7);

    /// Count unique colors by sampling a grid of pixels.
    /// Uses a hash set internally; limited to avoid excessive memory use.
    static int countUniqueColors(const uint8_t* pixels,
                                  int width, int height, int channels,
                                  int sampleStride = 5,
                                  int maxColors = 100000);

    /// Estimate Shannon entropy of the luminance channel.
    /// Returns a value between 0.0 (perfectly uniform) and 8.0 (maximum entropy for 8-bit).
    static float estimateEntropy(const uint8_t* pixels,
                                  int width, int height, int channels,
                                  int sampleStride = 3);

    /// Estimate edge density using a simple Sobel-like 3x3 gradient.
    /// Returns fraction of pixels with gradient magnitude above threshold.
    static float estimateEdgeDensity(const uint8_t* pixels,
                                      int width, int height, int channels,
                                      int sampleStride = 4,
                                      int gradientThreshold = 30);

    /// Estimate the fraction of pixels that belong to large flat (uniform) regions.
    /// Checks NxN blocks for variance below a threshold.
    static float estimateFlatRegionRatio(const uint8_t* pixels,
                                          int width, int height, int channels,
                                          int blockSize = 8,
                                          int varianceThreshold = 5);

    /// Classify the image based on all computed features.
    static ImageClassification classify(const AnalysisResult& result,
                                         const ImageMetadata& meta);
};

} // namespace pdfcompress
