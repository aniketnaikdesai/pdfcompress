#pragma once

#include "CodecInterface.h"

namespace pdfcompress {

/// Lossless zlib/Flate encoder (supports alpha).
class PngCodec : public ImageCodec {
public:
    std::string name() const override { return "flate"; }

    std::vector<uint8_t> encode(const uint8_t* pixels, 
                                const CompressionParams& params) override;
};

} // namespace pdfcompress
