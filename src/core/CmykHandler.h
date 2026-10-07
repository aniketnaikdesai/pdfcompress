#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pdfcompress {

/// Focused handler for CMYK (DeviceCMYK / ICCBased-CMYK) image objects.
///
/// The default optimizer behaviour is preservation: CMYK samples are never
/// re-encoded as RGBA (which would corrupt colors). This component owns the
/// two CMYK-specific concerns:
///
///  * Adobe APP14 safety — Adobe-produced CMYK JPEGs store inverted samples,
///    so the decoder must know whether to flip them rather than silently
///    inverting the whole image.
///  * The opt-in CMYK->RGB transcode path, guarded by an internal CIE76
///    delta-E sample so a bad conversion is rejected instead of emitted. The
///    normative color gate remains the external SSIM >= 0.98 render match.
class CmykHandler {
public:
    /// Worst (max) and mean CIE76 delta-E over the sampled pixels. Values use
    /// the usual 0-100 Lab scale; a healthy device conversion samples to ~0.
    struct DeltaEStats {
        float maxDeltaE = 0.0f;
        float meanDeltaE = 0.0f;
    };

    /// Result of a CMYK->RGB transcode.
    struct TranscodeResult {
        bool ok = false;
        std::vector<uint8_t> rgb;   ///< width*height*3 tightly packed RGB
        DeltaEStats deltaE;
    };

    /// Threshold above which a transcode is considered unsafe. CIE76 delta-E
    /// of 10 is already a clearly perceptible shift; the device conversion is
    /// deterministic, so a healthy conversion samples far below this and only
    /// a channel-order / inversion / stride bug trips the gate.
    static constexpr float kMaxDeltaE = 10.0f;

    /// True when a JPEG byte stream carries an Adobe APP14 marker. Adobe
    /// CMYK JPEGs store inverted samples; callers use this to pick the right
    /// inversion instead of silently inverting the image.
    static bool hasAdobeApp14(const uint8_t* data, std::size_t size);

    /// Convert a tightly-packed 4-channel CMYK buffer to tightly-packed RGB.
    ///
    /// @param cmyk     width*height*4 source samples (one byte per channel).
    /// @param width    Image width in pixels.
    /// @param height   Image height in pixels.
    /// @param inverted True when the samples are Adobe-/Decode-inverted and
    ///                 must be flipped before conversion.
    static TranscodeResult transcodeToRgb(const uint8_t* cmyk,
                                          int width, int height,
                                          bool inverted = false);

    /// CIE76 delta-E sampled between a source CMYK buffer and a candidate RGB
    /// buffer, used to validate a conversion. The same `inverted` flag as the
    /// transcode must be passed so the reference matches the candidate.
    static DeltaEStats sampleDeltaE(const uint8_t* cmyk,
                                    const uint8_t* rgb,
                                    int width, int height,
                                    bool inverted = false);
};

} // namespace pdfcompress
