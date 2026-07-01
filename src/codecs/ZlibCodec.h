#pragma once

#include "CodecInterface.h"

namespace pdfcompress {

class ZlibCodec : public ImageCodec {
public:
    std::string name() const override { return "zlib"; }

    std::vector<uint8_t> encode(const uint8_t* pixels,
                                const CompressionParams& params) override;
};

} // namespace pdfcompress
