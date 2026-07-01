#include "DecisionEngine.h"
#include "../codecs/JpegCodec.h"
#include "../codecs/PngCodec.h"
#include "../codecs/Jp2Codec.h"
#include <iostream>

namespace pdfcompress {

DecisionEngine::DecisionEngine(CompressionProfile profile) 
    : m_profile(profile) {
    m_jpegCodec = std::make_unique<JpegCodec>();
    m_pngCodec = std::make_unique<PngCodec>();
    m_jp2Codec = std::make_unique<Jp2Codec>();
}

std::vector<uint8_t> DecisionEngine::compress(const uint8_t* pixels, 
                                              int width, int height, int channels,
                                              const AnalysisResult& analysis,
                                              StreamFilter& outFilter) {
    if (!pixels || width <= 0 || height <= 0) return {};

    CompressionParams params;
    params.width = width;
    params.height = height;
    params.channels = channels;
    params.profile = m_profile;
    
    ImageCodec* selectedCodec = nullptr;

    // Phase 4 Codec Decision Logic
    switch (analysis.classification) {
        case ImageClassification::Photo:
            // JPEG 2000 offers better quality at low bitrates for photos.
            // If MaxQuality profile is chosen, standard JPEG is also very fast and good,
            // but JP2 is the premium choice for PDF size reduction.
            if (m_profile == CompressionProfile::MaxCompression) {
                selectedCodec = m_jp2Codec.get();
                outFilter = StreamFilter::JPXDecode;
            } else {
                selectedCodec = m_jpegCodec.get();
                outFilter = StreamFilter::DCTDecode;
            }
            break;

        case ImageClassification::Screenshot:
        case ImageClassification::LineArt:
            // Crisp edges and flat colors -> PNG (FlateDecode)
            selectedCodec = m_pngCodec.get();
            outFilter = StreamFilter::FlateDecode;
            break;

        case ImageClassification::ScannedText:
            // Scanned text -> JP2 is excellent for documents
            selectedCodec = m_jp2Codec.get();
            outFilter = StreamFilter::JPXDecode;
            break;

        case ImageClassification::Monochrome:
            // Pure B&W -> PNG (until JBIG2 is implemented)
            selectedCodec = m_pngCodec.get();
            outFilter = StreamFilter::FlateDecode;
            break;

        default:
            // Fallback to JPEG
            selectedCodec = m_jpegCodec.get();
            outFilter = StreamFilter::DCTDecode;
            break;
    }

    if (!selectedCodec) {
        std::cerr << "DecisionEngine: No codec selected\n";
        return {};
    }

    // std::cout << "DecisionEngine: Selected " << selectedCodec->name() 
    //           << " for " << analysis.classificationName() << "\n";

    return selectedCodec->encode(pixels, params);
}

} // namespace pdfcompress
