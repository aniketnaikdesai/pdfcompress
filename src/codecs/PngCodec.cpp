#include "PngCodec.h"
#include <png.h>
#include <stdexcept>
#include <iostream>

namespace pdfcompress {

// libpng requires a write callback for custom I/O
static void custom_png_write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
    auto* out = reinterpret_cast<std::vector<uint8_t>*>(png_get_io_ptr(png_ptr));
    if (out) {
        out->insert(out->end(), data, data + length);
    }
}

static void custom_png_flush(png_structp png_ptr) {
    // No-op for memory buffers
}

std::vector<uint8_t> PngCodec::encode(const uint8_t* pixels, 
                                      const CompressionParams& params) {
    if (!pixels || params.width <= 0 || params.height <= 0) {
        return {};
    }

    std::vector<uint8_t> result;

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        std::cerr << "PngCodec: png_create_write_struct failed\n";
        return {};
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        std::cerr << "PngCodec: png_create_info_struct failed\n";
        png_destroy_write_struct(&png_ptr, nullptr);
        return {};
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        std::cerr << "PngCodec: libpng error during encoding\n";
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return {};
    }

    // Set custom writer
    png_set_write_fn(png_ptr, &result, custom_png_write_data, custom_png_flush);

    // Determine color type
    int color_type = PNG_COLOR_TYPE_RGB;
    if (params.channels == 1) color_type = PNG_COLOR_TYPE_GRAY;
    else if (params.channels == 3) color_type = PNG_COLOR_TYPE_RGB;
    else if (params.channels == 4) color_type = PNG_COLOR_TYPE_RGBA;
    else {
        std::cerr << "PngCodec: Unsupported channel count " << params.channels << "\n";
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return {};
    }

    png_set_IHDR(png_ptr, info_ptr, 
                 params.width, params.height, 
                 8, // bits per channel
                 color_type, 
                 PNG_INTERLACE_NONE, 
                 PNG_COMPRESSION_TYPE_DEFAULT, 
                 PNG_FILTER_TYPE_DEFAULT);

    // Adjust zlib compression level based on profile
    // PNG is lossless, so "quality" really means CPU effort vs file size
    if (params.profile == CompressionProfile::MaxCompression) {
        png_set_compression_level(png_ptr, 9); // Max zlib compression (slowest)
        // Also enable all filters
        png_set_filter(png_ptr, 0, PNG_ALL_FILTERS);
    } else if (params.profile == CompressionProfile::MaxQuality) {
        png_set_compression_level(png_ptr, 1); // Fast compression
    } else { // Balanced
        png_set_compression_level(png_ptr, 6); // Default zlib level
    }

    png_write_info(png_ptr, info_ptr);

    // Set up row pointers
    std::vector<png_bytep> row_pointers(params.height);
    const size_t rowStride = params.width * params.channels;
    for (int y = 0; y < params.height; ++y) {
        // Warning: const_cast is safe here because libpng doesn't modify the input buffer when writing
        row_pointers[y] = const_cast<png_bytep>(pixels + y * rowStride);
    }

    png_write_image(png_ptr, row_pointers.data());
    png_write_end(png_ptr, nullptr);

    png_destroy_write_struct(&png_ptr, &info_ptr);

    return result;
}

} // namespace pdfcompress
