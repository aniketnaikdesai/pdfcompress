#include "DecisionEngine.h"
#include "../codecs/JpegCodec.h"
#include "../codecs/PngCodec.h"
#include "../codecs/ZlibCodec.h"
#include "../codecs/Jp2Codec.h"
#include <iostream>

namespace pdfcompress {

DecisionEngine::DecisionEngine(CompressionProfile profile) 
    : m_profile(profile) {
    m_jpegCodec = std::make_unique<JpegCodec>();
    m_pngCodec = std::make_unique<PngCodec>();
    m_zlibCodec = std::make_unique<ZlibCodec>();
    m_jp2Codec = std::make_unique<Jp2Codec>();
}

std::vector<uint8_t> DecisionEngine::compress(const uint8_t* pixels, 
                                              int width, int height, int channels,
                                              const AnalysisResult& analysis,
                                              StreamFilter& outFilter,
                                              int qualityHint,
                                              bool hasAlpha,
                                              int bitsPerComponent,
                                              ColorSpace colorSpace) {
    if (!pixels || width <= 0 || height <= 0) return {};

    CompressionParams params;
    params.width = width;
    params.height = height;
    params.channels = channels;
    params.profile = m_profile;
    params.qualityHint = qualityHint;
    params.hasAlpha = hasAlpha;
    params.bitsPerComponent = bitsPerComponent;
    params.colorSpace = colorSpace;
    
    ImageCodec* selectedCodec = nullptr;

    // Phase 4 Codec Decision Logic.
    //
    // The quality slider is only meaningful for the lossy classes (Photo,
    // Screenshot, LineArt). Lossless classifications (ScannedText, Monochrome)
    // ignore it entirely, and alpha-bearing images always take the lossless
    // PNG branch so they are never flattened into a JPEG.
    if (hasAlpha) {
        // Alpha (/SMask or /Mask) cannot survive a DCTDecode flattening step;
        // route through the lossless PNG codec and report FlateDecode.
        selectedCodec = m_pngCodec.get();
        outFilter = StreamFilter::FlateDecode;
    } else {
        switch (analysis.classification) {
            case ImageClassification::Photo:
                // JPEG 2000 offers better quality at low bitrates for photos.
                // If MaxCompression profile is chosen, JP2 is the premium choice
                // for PDF size reduction; otherwise standard JPEG.
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
                // Lossy JPEG when the slider is active or the profile is not
                // MaxQuality. Under MaxQuality with no slider (qualityHint <= 0)
                // route to the lossless PNG/FlateDecode path.
                if (m_profile == CompressionProfile::MaxQuality && qualityHint <= 0) {
                    selectedCodec = m_pngCodec.get();
                    outFilter = StreamFilter::FlateDecode;
                } else {
                    selectedCodec = m_jpegCodec.get();
                    outFilter = StreamFilter::DCTDecode;
                }
                break;

            case ImageClassification::ScannedText:
                // Scanned text -> JP2 is excellent for documents and ignores the slider.
                selectedCodec = m_jp2Codec.get();
                outFilter = StreamFilter::JPXDecode;
                break;

            case ImageClassification::Monochrome:
                // Pure B&W -> zlib (lossless). JPEG would introduce artifacts.
                // The quality slider is ignored here.
                selectedCodec = m_zlibCodec.get();
                outFilter = StreamFilter::FlateDecode;
                break;

            default:
                // Fallback to JPEG
                selectedCodec = m_jpegCodec.get();
                outFilter = StreamFilter::DCTDecode;
                break;
        }
    }

    if (!selectedCodec) {
        std::cerr << "DecisionEngine: No codec selected\n";
        return {};
    }

    return selectedCodec->encode(pixels, params);
}

} // namespace pdfcompress
