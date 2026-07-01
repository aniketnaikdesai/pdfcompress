#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include "../core/ImageMetadata.h"

namespace pdfcompress {

/// User-selectable compression profiles.
enum class CompressionProfile {
    MaxQuality,   ///< Prioritize visual quality over file size
    Balanced,     ///< Good balance of quality and size (default)
    MaxCompression ///< Prioritize file size over visual quality
};

/// Parameters passed to the codec for encoding.
struct CompressionParams {
    int width = 0;
    int height = 0;
    int channels = 3; ///< 1=Gray, 3=RGB, 4=RGBA
    CompressionProfile profile = CompressionProfile::Balanced;
    
    // Optional targeted quality hints (0-100), used if applicable by the codec
    int qualityHint = 75;
};

/// Abstract base class for all image encoders.
class ImageCodec {
public:
    virtual ~ImageCodec() = default;

    /// Returns the name of the codec (e.g., "TurboJPEG", "libpng", "OpenJPEG").
    virtual std::string name() const = 0;

    /// Encodes raw pixel data into a compressed byte stream.
    /// @param pixels Pointer to raw pixel data (Gray, RGB, or RGBA tightly packed).
    /// @param params Parameters including dimensions and requested quality profile.
    /// @return A vector of compressed bytes. Empty on failure.
    virtual std::vector<uint8_t> encode(const uint8_t* pixels, 
                                        const CompressionParams& params) = 0;
};

} // namespace pdfcompress
