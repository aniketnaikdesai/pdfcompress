#include "PngCodec.h"
#include <zlib.h>
#include <iostream>

namespace pdfcompress {

// Lossless encoder for the alpha and MaxQuality-lossless branches.
//
// DecisionEngine labels this branch StreamFilter::FlateDecode, and
// PDFOptimizer embeds the returned bytes as a PDF image XObject with
// /Filter /FlateDecode and no /DecodeParms. The output therefore has to be a
// plain zlib stream of the raw samples, never a full PNG container (which
// carries a 0x89 'PNG' signature plus IHDR/IDAT/IEND and cannot be inflated
// under /FlateDecode). ESC-011: the previous libpng container writer produced
// PNG bytes under a FlateDecode label, so qpdf failed with "incorrect header
// check".
//
// The class keeps its name and its role: it is the lossless, alpha-capable
// codec reachable by the DecisionEngine's m_pngCodec branches (AC-C2).
std::vector<uint8_t> PngCodec::encode(const uint8_t* pixels,
                                      const CompressionParams& params) {
    if (!pixels || params.width <= 0 || params.height <= 0) {
        return {};
    }
    if (params.channels != 1 && params.channels != 3 && params.channels != 4) {
        std::cerr << "PngCodec: Unsupported channel count " << params.channels << "\n";
        return {};
    }

    const size_t rawSize = static_cast<size_t>(params.width) *
                           static_cast<size_t>(params.height) *
                           static_cast<size_t>(params.channels);

    // Compression is lossless, so "quality" really means CPU effort vs size.
    int level = 6; // Balanced
    if (params.profile == CompressionProfile::MaxCompression) {
        level = 9;
    } else if (params.profile == CompressionProfile::MaxQuality) {
        level = 1;
    }

    uLongf compressedSize = compressBound(static_cast<uLong>(rawSize));
    std::vector<uint8_t> result(compressedSize);

    if (compress2(result.data(), &compressedSize, pixels,
                  static_cast<uLong>(rawSize), level) != Z_OK) {
        std::cerr << "PngCodec: zlib compression failed\n";
        return {};
    }

    result.resize(compressedSize);
    return result;
}

} // namespace pdfcompress
