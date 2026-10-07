#pragma once

#include "CodecInterface.h"

namespace pdfcompress {

/// JPEG encoder using libjpeg-turbo (TurboJPEG API).
class JpegCodec : public ImageCodec {
public:
    std::string name() const override { return "TurboJPEG"; }

    std::vector<uint8_t> encode(const uint8_t* pixels, 
                                const CompressionParams& params) override;

    /// Opt-in CMYK->RGB transcode encoder (AC-C3b).
    ///
    /// Converts a tightly-packed 4-channel CMYK buffer to RGB via CmykHandler
    /// (honoring `inverted` for Adobe/Decode-inverted samples) and encodes the
    /// result as JPEG. Returns an empty vector when the transcode is rejected
    /// by CmykHandler's internal CIE76 delta-E gate; the caller then preserves
    /// the original CMYK stream. On success `*maxDeltaE` (when non-null)
    /// receives the worst sampled delta-E.
    std::vector<uint8_t> encodeCmykAsRgb(const uint8_t* cmyk,
                                         int width, int height,
                                         const CompressionParams& params,
                                         bool inverted = false,
                                         float* maxDeltaE = nullptr);
};

} // namespace pdfcompress
