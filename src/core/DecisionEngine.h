#pragma once

#include "ImageMetadata.h"
#include "ImageAnalyzer.h"
#include "../codecs/CodecInterface.h"
#include <vector>
#include <memory>

namespace pdfcompress {

/// Determines the best codec for an image and encodes it.
class DecisionEngine {
public:
    DecisionEngine(CompressionProfile profile = CompressionProfile::Balanced);

    /// Re-compress an image buffer based on its classification.
    /// @param pixels Decoded pixel data.
    /// @param width Width in pixels.
    /// @param height Height in pixels.
    /// @param channels Number of color channels (1, 3, or 4).
    /// @param analysis The classification result from Phase 3.
    /// @param outFilter The PDF stream filter that should be used for the resulting bytes (e.g. DCTDecode).
    /// @return The compressed byte stream, or empty on failure.
    std::vector<uint8_t> compress(const uint8_t* pixels, 
                                  int width, int height, int channels,
                                  const AnalysisResult& analysis,
                                  StreamFilter& outFilter);

private:
    CompressionProfile m_profile;
    
    // Codec instances
    std::unique_ptr<ImageCodec> m_jpegCodec;
    std::unique_ptr<ImageCodec> m_pngCodec;
    std::unique_ptr<ImageCodec> m_jp2Codec;
};

} // namespace pdfcompress
