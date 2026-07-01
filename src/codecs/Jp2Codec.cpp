#include "Jp2Codec.h"
#include <openjpeg.h>
#include <stdexcept>
#include <iostream>
#include <cstring>

namespace pdfcompress {

// OpenJPEG stream callbacks for memory buffer
struct Jp2StreamData {
    std::vector<uint8_t>* buffer;
};

static OPJ_SIZE_T opj_memory_write(void* p_buffer, OPJ_SIZE_T p_nb_bytes, void* p_user_data) {
    auto* data = static_cast<Jp2StreamData*>(p_user_data);
    auto* buf = static_cast<uint8_t*>(p_buffer);
    data->buffer->insert(data->buffer->end(), buf, buf + p_nb_bytes);
    return p_nb_bytes;
}

static OPJ_OFF_T opj_memory_skip(OPJ_OFF_T p_nb_bytes, void* p_user_data) {
    auto* data = static_cast<Jp2StreamData*>(p_user_data);
    // OpenJPEG mostly only writes sequentially for encoding JP2/J2K.
    // If it asks to skip, we just write zeros (though skipping is rare in write mode).
    if (p_nb_bytes > 0) {
        data->buffer->insert(data->buffer->end(), p_nb_bytes, 0);
    }
    return p_nb_bytes;
}

static OPJ_BOOL opj_memory_seek(OPJ_OFF_T p_nb_bytes, void* p_user_data) {
    // OpenJPEG encoding doesn't usually seek backwards. If it does, we'd need a more
    // complex buffer wrapper (like a position pointer). For standard J2K it won't.
    return OPJ_FALSE;
}

std::vector<uint8_t> Jp2Codec::encode(const uint8_t* pixels, 
                                      const CompressionParams& params) {
    if (!pixels || params.width <= 0 || params.height <= 0) {
        return {};
    }

    // 1. Setup OpenJPEG parameters
    opj_cparameters_t parameters;
    opj_set_default_encoder_parameters(&parameters);

    // Profile settings
    if (params.profile == CompressionProfile::MaxQuality) {
        parameters.tcp_numlayers = 1;
        parameters.tcp_rates[0] = 0; // Lossless
        parameters.cp_disto_alloc = 1;
    } else if (params.profile == CompressionProfile::MaxCompression) {
        parameters.tcp_numlayers = 1;
        parameters.tcp_rates[0] = 50; // Very high compression (1/50th size)
        parameters.cp_disto_alloc = 1;
    } else {
        parameters.tcp_numlayers = 1;
        parameters.tcp_rates[0] = 20; // 1/20th size (typical default)
        parameters.cp_disto_alloc = 1;
    }

    // 2. Create OpenJPEG image
    std::vector<opj_image_cmptparm_t> cmptparm(params.channels);
    for (int i = 0; i < params.channels; ++i) {
        cmptparm[i].dx = 1;
        cmptparm[i].dy = 1;
        cmptparm[i].w = params.width;
        cmptparm[i].h = params.height;
        cmptparm[i].x0 = 0;
        cmptparm[i].y0 = 0;
        cmptparm[i].prec = 8;
        cmptparm[i].bpp = 8;
        cmptparm[i].sgnd = 0;
    }

    OPJ_COLOR_SPACE color_space = OPJ_CLRSPC_SRGB;
    if (params.channels == 1) {
        color_space = OPJ_CLRSPC_GRAY;
    }

    opj_image_t* image = opj_image_create(params.channels, cmptparm.data(), color_space);
    if (!image) {
        std::cerr << "Jp2Codec: opj_image_create failed\n";
        return {};
    }

    image->x0 = 0;
    image->y0 = 0;
    image->x1 = params.width;
    image->y1 = params.height;

    // 3. Copy pixels to OpenJPEG image (planar format)
    // OpenJPEG stores pixels as 32-bit integers in separate channel arrays.
    int numPixels = params.width * params.height;
    for (int i = 0; i < numPixels; ++i) {
        for (int c = 0; c < params.channels; ++c) {
            image->comps[c].data[i] = pixels[i * params.channels + c];
        }
    }

    // 4. Create encoder (PDFs usually use J2K codestream rather than JP2 box format)
    opj_codec_t* codec = opj_create_compress(OPJ_CODEC_J2K);
    
    // Ignore verbose output
    opj_set_info_handler(codec, [](const char*, void*) {}, nullptr);
    opj_set_warning_handler(codec, [](const char*, void*) {}, nullptr);
    opj_set_error_handler(codec, [](const char* msg, void*) { std::cerr << "OpenJPEG: " << msg; }, nullptr);

    opj_setup_encoder(codec, &parameters, image);

    // 5. Setup stream
    std::vector<uint8_t> result;
    Jp2StreamData streamData{&result};

    opj_stream_t* stream = opj_stream_create(1024, OPJ_FALSE);
    opj_stream_set_write_function(stream, opj_memory_write);
    opj_stream_set_skip_function(stream, opj_memory_skip);
    opj_stream_set_seek_function(stream, opj_memory_seek);
    opj_stream_set_user_data(stream, &streamData, nullptr);

    // 6. Encode
    if (!opj_start_compress(codec, image, stream)) {
        std::cerr << "Jp2Codec: opj_start_compress failed\n";
        opj_stream_destroy(stream);
        opj_destroy_codec(codec);
        opj_image_destroy(image);
        return {};
    }

    if (!opj_encode(codec, stream)) {
        std::cerr << "Jp2Codec: opj_encode failed\n";
    }

    if (!opj_end_compress(codec, stream)) {
        std::cerr << "Jp2Codec: opj_end_compress failed\n";
    }

    // 7. Cleanup
    opj_stream_destroy(stream);
    opj_destroy_codec(codec);
    opj_image_destroy(image);

    return result;
}

} // namespace pdfcompress
