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
    int quality = params.qualityHint;
    if (params.profile == CompressionProfile::MaxQuality) {
        quality = 95;
        if (params.channels >= 3) subsamp = TJSAMP_444; // No subsampling
    } else if (params.profile == CompressionProfile::MaxCompression) {
        quality = 60;
        if (params.channels >= 3) subsamp = TJSAMP_420; // High subsampling
    } else { // Balanced
        quality = 80;
        if (params.channels >= 3) subsamp = TJSAMP_422; // Moderate subsampling
    }

    tjhandle tjInstance = tjInitCompress();
    if (!tjInstance) {
        std::cerr << "JpegCodec: tjInitCompress failed\n";
        return {};
    }

    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;

    int res = tjCompress2(tjInstance, 
                          pixels, 
                          params.width, 
                          0, // pitch (0 = width * channels)
                          params.height, 
                          pixelFormat, 
                          &jpegBuf, 
                          &jpegSize, 
                          subsamp, 
                          quality, 
                          TJFLAG_NOREALLOC); // Try allocating ourselves if we want, but letting TJ allocate is easier initially.
                          
    // Actually, TJFLAG_NOREALLOC requires jpegBuf to be pre-allocated. Let's not use it.
    // We should pass 0 to let TurboJPEG allocate, then tjFree() it.
    // Wait, tjCompress2 is deprecated in libjpeg-turbo 3+, but still widely used. Let's use the standard way.
    tjDestroy(tjInstance);
    
    // Retry with proper allocation flags
    tjInstance = tjInitCompress();
    jpegBuf = nullptr;
    jpegSize = 0;
    
    res = tjCompress2(tjInstance, 
                      pixels, 
                      params.width, 
                      0, 
                      params.height, 
                      pixelFormat, 
                      &jpegBuf, 
                      &jpegSize, 
                      subsamp, 
                      quality, 
                      0); // Let TurboJPEG allocate

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
