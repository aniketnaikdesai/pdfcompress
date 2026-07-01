#pragma once

#include "CodecInterface.h"

namespace pdfcompress {

/// JPEG encoder using libjpeg-turbo (TurboJPEG API).
class JpegCodec : public ImageCodec {
public:
    std::string name() const override { return "TurboJPEG"; }

    std::vector<uint8_t> encode(const uint8_t* pixels, 
                                const CompressionParams& params) override;
};

} // namespace pdfcompress
