#include "ImageAnalyzer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>

namespace pdfcompress {

// ---------------------------------------------------------------------------
// AnalysisResult helpers
// ---------------------------------------------------------------------------

std::string AnalysisResult::classificationName() const {
    switch (classification) {
        case ImageClassification::Photo:       return "Photo";
        case ImageClassification::Screenshot:  return "Screenshot/Graphics";
        case ImageClassification::LineArt:     return "Line Art";
        case ImageClassification::ScannedText: return "Scanned Text";
        case ImageClassification::Monochrome:  return "Monochrome (B&W)";
        default:                               return "Unknown";
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

AnalysisResult ImageAnalyzer::analyze(const uint8_t* pixels,
                                       int width, int height, int channels,
                                       const ImageMetadata& meta) {
    AnalysisResult result;

    if (!pixels || width <= 0 || height <= 0 || channels < 1) {
        return result;
    }

    // 1. Grayscale detection
    float grayFrac = computeGrayscaleFraction(pixels, width, height, channels);
    result.isEffectivelyGrayscale = (grayFrac > 0.95f);

    // 2. Unique color count
    result.uniqueColorsSampled = countUniqueColors(pixels, width, height, channels);

    // 3. Shannon entropy
    result.entropy = estimateEntropy(pixels, width, height, channels);

    // 4. Edge density
    result.edgeDensity = estimateEdgeDensity(pixels, width, height, channels);

    // 5. Flat region ratio
    result.flatRegionRatio = estimateFlatRegionRatio(pixels, width, height, channels);

    // 6. Compression ratio from existing metadata
    if (meta.uncompressedSize > 0) {
        result.compressionRatio = static_cast<float>(meta.compressedSize) /
                                   static_cast<float>(meta.uncompressedSize);
    }

    // 7. Final classification
    result.classification = classify(result, meta);

    return result;
}

void ImageAnalyzer::analyzeAndUpdate(const uint8_t* pixels,
                                      int width, int height, int channels,
                                      ImageMetadata& meta) {
    auto result = analyze(pixels, width, height, channels, meta);
    meta.classification = result.classification;
    meta.entropy = result.entropy;
    meta.uniqueColorsSampled = result.uniqueColorsSampled;
}

// ---------------------------------------------------------------------------
// Grayscale detection
// ---------------------------------------------------------------------------

float ImageAnalyzer::computeGrayscaleFraction(const uint8_t* pixels,
                                               int width, int height, int channels,
                                               int sampleStride) {
    if (channels < 3) return 1.0f; // Already grayscale

    const int tolerance = 3; // Allow slight channel variation from JPEG artifacts
    int sampled = 0;
    int grayCount = 0;

    for (int y = 0; y < height; y += sampleStride) {
        for (int x = 0; x < width; x += sampleStride) {
            int idx = (y * width + x) * channels;
            uint8_t r = pixels[idx];
            uint8_t g = pixels[idx + 1];
            uint8_t b = pixels[idx + 2];

            if (std::abs(r - g) <= tolerance &&
                std::abs(g - b) <= tolerance &&
                std::abs(r - b) <= tolerance) {
                ++grayCount;
            }
            ++sampled;
        }
    }

    return sampled > 0 ? static_cast<float>(grayCount) / sampled : 0.0f;
}

// ---------------------------------------------------------------------------
// Unique color counting
// ---------------------------------------------------------------------------

int ImageAnalyzer::countUniqueColors(const uint8_t* pixels,
                                        int width, int height, int channels,
                                        int sampleStride, int maxColors) {
    // Pack RGB into a 32-bit key for fast hashing
    std::unordered_set<uint32_t> colorSet;

    for (int y = 0; y < height; y += sampleStride) {
        for (int x = 0; x < width; x += sampleStride) {
            int idx = (y * width + x) * channels;

            uint32_t key;
            if (channels >= 3) {
                key = (static_cast<uint32_t>(pixels[idx]) << 16) |
                      (static_cast<uint32_t>(pixels[idx + 1]) << 8) |
                      (static_cast<uint32_t>(pixels[idx + 2]));
            } else {
                // Grayscale
                key = static_cast<uint32_t>(pixels[idx]);
            }

            colorSet.insert(key);
            if (static_cast<int>(colorSet.size()) >= maxColors) {
                return maxColors; // Early exit — very colorful
            }
        }
    }

    return static_cast<int>(colorSet.size());
}

// ---------------------------------------------------------------------------
// Shannon entropy estimation
// ---------------------------------------------------------------------------

float ImageAnalyzer::estimateEntropy(const uint8_t* pixels,
                                      int width, int height, int channels,
                                      int sampleStride) {
    // Build histogram of luminance values
    std::array<int, 256> histogram{};
    int totalSamples = 0;

    for (int y = 0; y < height; y += sampleStride) {
        for (int x = 0; x < width; x += sampleStride) {
            int idx = (y * width + x) * channels;

            uint8_t luma;
            if (channels >= 3) {
                // Fast luminance: Y = 0.299R + 0.587G + 0.114B
                luma = static_cast<uint8_t>(
                    (77 * pixels[idx] + 150 * pixels[idx + 1] + 29 * pixels[idx + 2]) >> 8);
            } else {
                luma = pixels[idx];
            }

            histogram[luma]++;
            totalSamples++;
        }
    }

    if (totalSamples == 0) return 0.0f;

    // Shannon entropy: H = -Σ p(x) * log2(p(x))
    float entropy = 0.0f;
    for (int i = 0; i < 256; ++i) {
        if (histogram[i] > 0) {
            float p = static_cast<float>(histogram[i]) / totalSamples;
            entropy -= p * std::log2(p);
        }
    }

    return entropy;
}

// ---------------------------------------------------------------------------
// Edge density estimation (simplified Sobel)
// ---------------------------------------------------------------------------

float ImageAnalyzer::estimateEdgeDensity(const uint8_t* pixels,
                                          int width, int height, int channels,
                                          int sampleStride,
                                          int gradientThreshold) {
    if (width < 3 || height < 3) return 0.0f;

    int sampled = 0;
    int edgeCount = 0;

    // Helper to get luminance at (x, y)
    auto luma = [&](int x, int y) -> int {
        int idx = (y * width + x) * channels;
        if (channels >= 3) {
            return (77 * pixels[idx] + 150 * pixels[idx + 1] + 29 * pixels[idx + 2]) >> 8;
        }
        return pixels[idx];
    };

    for (int y = 1; y < height - 1; y += sampleStride) {
        for (int x = 1; x < width - 1; x += sampleStride) {
            // Simplified Sobel: horizontal and vertical gradients
            int gx = -luma(x - 1, y - 1) + luma(x + 1, y - 1)
                     - 2 * luma(x - 1, y) + 2 * luma(x + 1, y)
                     - luma(x - 1, y + 1) + luma(x + 1, y + 1);

            int gy = -luma(x - 1, y - 1) - 2 * luma(x, y - 1) - luma(x + 1, y - 1)
                     + luma(x - 1, y + 1) + 2 * luma(x, y + 1) + luma(x + 1, y + 1);

            int magnitude = static_cast<int>(std::sqrt(gx * gx + gy * gy));

            if (magnitude > gradientThreshold) {
                ++edgeCount;
            }
            ++sampled;
        }
    }

    return sampled > 0 ? static_cast<float>(edgeCount) / sampled : 0.0f;
}

// ---------------------------------------------------------------------------
// Flat region ratio estimation
// ---------------------------------------------------------------------------

float ImageAnalyzer::estimateFlatRegionRatio(const uint8_t* pixels,
                                              int width, int height, int channels,
                                              int blockSize,
                                              int varianceThreshold) {
    if (width < blockSize || height < blockSize) return 0.0f;

    int totalBlocks = 0;
    int flatBlocks = 0;

    // Helper to get luminance at (x, y)
    auto luma = [&](int x, int y) -> int {
        int idx = (y * width + x) * channels;
        if (channels >= 3) {
            return (77 * pixels[idx] + 150 * pixels[idx + 1] + 29 * pixels[idx + 2]) >> 8;
        }
        return pixels[idx];
    };

    for (int by = 0; by + blockSize <= height; by += blockSize) {
        for (int bx = 0; bx + blockSize <= width; bx += blockSize) {
            // Compute mean and variance for this block
            int sum = 0;
            int sumSq = 0;
            int count = blockSize * blockSize;

            for (int dy = 0; dy < blockSize; ++dy) {
                for (int dx = 0; dx < blockSize; ++dx) {
                    int val = luma(bx + dx, by + dy);
                    sum += val;
                    sumSq += val * val;
                }
            }

            float mean = static_cast<float>(sum) / count;
            float variance = (static_cast<float>(sumSq) / count) - (mean * mean);

            if (variance < static_cast<float>(varianceThreshold)) {
                ++flatBlocks;
            }
            ++totalBlocks;
        }
    }

    return totalBlocks > 0 ? static_cast<float>(flatBlocks) / totalBlocks : 0.0f;
}

// ---------------------------------------------------------------------------
// Classification decision tree
// ---------------------------------------------------------------------------

ImageClassification ImageAnalyzer::classify(const AnalysisResult& r,
                                             const ImageMetadata& meta) {
    // --- Rule 1: Pure monochrome ---
    // Very few unique colors AND effectively grayscale AND low entropy
    if (r.uniqueColorsSampled <= 4 && r.isEffectivelyGrayscale) {
        return ImageClassification::Monochrome;
    }

    // --- Rule 2: Scanned text ---
    // Grayscale, moderate entropy, strong edges (text has lots of edges),
    // high DPI typical of scans (>= 200 DPI)
    if (r.isEffectivelyGrayscale &&
        r.edgeDensity > 0.15f &&
        r.entropy > 3.0f && r.entropy < 6.5f &&
        meta.dpiX >= 150.0f) {
        return ImageClassification::ScannedText;
    }

    // --- Rule 3: Screenshot / UI graphics ---
    // Low unique colors relative to resolution, many flat regions,
    // very low entropy (large areas of solid color)
    if (r.uniqueColorsSampled < 1000 &&
        r.flatRegionRatio > 0.5f &&
        r.entropy < 5.0f) {
        return ImageClassification::Screenshot;
    }

    // --- Rule 4: Line art ---
    // High edge density, lots of flat regions, limited color palette
    if (r.edgeDensity > 0.25f &&
        r.flatRegionRatio > 0.4f &&
        r.uniqueColorsSampled < 5000) {
        return ImageClassification::LineArt;
    }

    // --- Rule 5: Photo (default) ---
    // High entropy, high unique color count = natural photographic content
    if (r.entropy > 5.5f || r.uniqueColorsSampled > 10000) {
        return ImageClassification::Photo;
    }

    // --- Fallback: best guess based on entropy ---
    if (r.entropy > 4.0f) {
        return ImageClassification::Photo;
    }

    return ImageClassification::Screenshot;
}

} // namespace pdfcompress
