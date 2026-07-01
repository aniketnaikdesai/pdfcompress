#include "ImageMetadata.h"
#include <sstream>
#include <iomanip>

namespace pdfcompress {

std::string ImageMetadata::summary() const {
    std::ostringstream oss;
    oss << "Image [page " << pageIndex << ", obj " << objectIndex << "]: "
        << widthPx << "x" << heightPx << " px, "
        << std::fixed << std::setprecision(0) << dpiX << "x" << dpiY << " DPI, "
        << colorSpaceToString(colorSpace) << ", "
        << bitsPerComponent << " bpc, "
        << (hasAlpha ? "alpha" : "no-alpha") << ", "
        << streamFilterToString(filter) << ", "
        << compressedSize << " bytes compressed";
    return oss.str();
}

size_t PDFDocumentInfo::totalImageBytes() const {
    size_t total = 0;
    for (const auto& img : images) {
        total += img.compressedSize;
    }
    return total;
}

} // namespace pdfcompress
