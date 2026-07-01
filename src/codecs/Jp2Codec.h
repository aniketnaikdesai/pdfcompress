#pragma once

#include "CodecInterface.h"

namespace pdfcompress {

/// JPEG 2000 encoder using OpenJPEG (excellent for photos/scans at low bitrates).
class Jp2Codec : public ImageCodec {
public:
    std::string name() const override { return "OpenJPEG"; }

    std::vector<uint8_t> encode(const uint8_t* pixels, 
                                const CompressionParams& params) override;
};

} // namespace pdfcompress
