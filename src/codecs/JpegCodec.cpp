#include "JpegCodec.h"
#include <turbojpeg.h>
#include <stdexcept>
#include <iostream>

namespace pdfcompress {

std::vector<uint8_t> JpegCodec::encode(const uint8_t* pixels, 
                                       const CompressionParams& params) {
    if (!pixels || params.width <= 0 || params.height <= 0) {
        return {};
    }

    // Determine TurboJPEG pixel format
    enum TJPF pixelFormat;
    int subsamp = TJSAMP_420; // Default chroma subsampling

    if (params.channels == 1) {
        pixelFormat = TJPF_GRAY;
        subsamp = TJSAMP_GRAY;
    } else if (params.channels == 3) {
        pixelFormat = TJPF_RGB;
    } else if (params.channels == 4) {
        // JPEG doesn't support alpha, but TurboJPEG can read RGBA buffers
        // and ignore the alpha channel.
        pixelFormat = TJPF_RGBA;
    } else {
        std::cerr << "JpegCodec: Unsupported channel count " << params.channels << "\n";
        return {};
    }

    // Determine quality and subsampling based on profile
    int quality = params.qualityHint > 0 ? params.qualityHint : 75;
    if (params.profile == CompressionProfile::MaxQuality) {
        if (params.qualityHint <= 0) quality = 90;
        if (params.channels >= 3) subsamp = TJSAMP_444; // No subsampling
    } else if (params.profile == CompressionProfile::MaxCompression) {
        if (params.qualityHint <= 0) quality = 50;
        if (params.channels >= 3) subsamp = TJSAMP_420; // High subsampling
    } else { // Balanced
        if (params.qualityHint <= 0) quality = 70;
        if (params.channels >= 3) subsamp = TJSAMP_420; // Chroma subsampling for size reduction
    }

    tjhandle tjInstance = tjInitCompress();
    if (!tjInstance) {
        std::cerr << "JpegCodec: tjInitCompress failed: " << tjGetErrorStr() << "\n";
        return {};
    }

    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;

    int res = tjCompress2(tjInstance, 
                          pixels, 
                          params.width, 
                          0, 
                          params.height, 
                          pixelFormat, 
                          &jpegBuf, 
                          &jpegSize, 
                          subsamp, 
                          quality, 
                          0);

    if (res < 0) {
        std::cerr << "JpegCodec: tjCompress2 failed: " << tjGetErrorStr2(tjInstance) << "\n";
        tjDestroy(tjInstance);
        return {};
    }

    std::vector<uint8_t> result(jpegBuf, jpegBuf + jpegSize);

    tjFree(jpegBuf);
    tjDestroy(tjInstance);

    return result;
}

} // namespace pdfcompress
