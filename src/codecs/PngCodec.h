#pragma once

#include "CodecInterface.h"

namespace pdfcompress {

/// PNG encoder using libpng (lossless, supports alpha).
class PngCodec : public ImageCodec {
public:
    std::string name() const override { return "libpng"; }

    std::vector<uint8_t> encode(const uint8_t* pixels, 
                                const CompressionParams& params) override;
};

} // namespace pdfcompress
