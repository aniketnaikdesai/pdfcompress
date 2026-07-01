#include "ZlibCodec.h"

#include <cstring>
#include <iostream>
#include <zlib.h>

namespace pdfcompress {

std::vector<uint8_t> ZlibCodec::encode(const uint8_t* pixels,
                                       const CompressionParams& params) {
    if (!pixels || params.width <= 0 || params.height <= 0 || params.channels < 1) {
        return {};
    }

    size_t rawSize = static_cast<size_t>(params.width) *
                     static_cast<size_t>(params.height) *
                     static_cast<size_t>(params.channels);

    uLongf compressedSize = compressBound(rawSize);
    std::vector<uint8_t> result(compressedSize);

    int level = 9;

    if (compress2(result.data(), &compressedSize,
                  pixels, rawSize, level) != Z_OK) {
        std::cerr << "ZlibCodec: compression failed\n";
        return {};
    }

    result.resize(compressedSize);
    return result;
}

} // namespace pdfcompress
