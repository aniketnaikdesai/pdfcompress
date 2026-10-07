#include "CmykHandler.h"

#include <algorithm>
#include <cmath>

namespace pdfcompress {

namespace {

struct Lab {
    float L;
    float a;
    float b;
};

/// Convert an 8-bit sRGB triple to CIE L*a*b* (D65 white point).
Lab rgbToLab(uint8_t r, uint8_t g, uint8_t b) {
    auto linearize = [](float c) {
        c /= 255.0f;
        return (c <= 0.04045f) ? (c / 12.92f)
                               : std::pow((c + 0.055f) / 1.055f, 2.4f);
    };

    const float R = linearize(r);
    const float G = linearize(g);
    const float B = linearize(b);

    const float X = R * 0.4124564f + G * 0.3575761f + B * 0.1804375f;
    const float Y = R * 0.2126729f + G * 0.7151522f + B * 0.0721750f;
    const float Z = R * 0.0193339f + G * 0.1191920f + B * 0.9503041f;

    const float Xn = 0.95047f;
    const float Yn = 1.00000f;
    const float Zn = 1.08883f;

    auto f = [](float t) {
        return (t > 0.008856f) ? std::cbrt(t) : (7.787f * t + 16.0f / 116.0f);
    };

    const float fx = f(X / Xn);
    const float fy = f(Y / Yn);
    const float fz = f(Z / Zn);

    Lab lab;
    lab.L = 116.0f * fy - 16.0f;
    lab.a = 500.0f * (fx - fy);
    lab.b = 200.0f * (fy - fz);
    return lab;
}

/// Naive device CMYK->RGB (multiplicative ink model). This is the same
/// transform the transcode emits, so a correct conversion samples to ~0
/// delta-E while a wrong channel order or inversion trips the gate.
void deviceCmykToRgb(uint8_t c, uint8_t m, uint8_t y, uint8_t k, bool inverted,
                     uint8_t& r, uint8_t& g, uint8_t& b) {
    float C = c / 255.0f;
    float M = m / 255.0f;
    float Y = y / 255.0f;
    float K = k / 255.0f;

    if (inverted) {
        C = 1.0f - C;
        M = 1.0f - M;
        Y = 1.0f - Y;
        K = 1.0f - K;
    }

    auto toByte = [](float v) {
        int i = static_cast<int>(std::lround(v * 255.0f));
        i = std::max(0, std::min(255, i));
        return static_cast<uint8_t>(i);
    };

    r = toByte((1.0f - C) * (1.0f - K));
    g = toByte((1.0f - M) * (1.0f - K));
    b = toByte((1.0f - Y) * (1.0f - K));
}

} // namespace

bool CmykHandler::hasAdobeApp14(const uint8_t* data, std::size_t size) {
    if (!data || size < 8) return false;
    // Scan JPEG markers for APP14 (0xFF 0xEE) followed by the "Adobe"
    // identifier at offset +4 (marker 2 bytes + segment length 2 bytes).
    for (std::size_t i = 0; i + 7 < size; ++i) {
        if (data[i] == 0xFF && data[i + 1] == 0xEE &&
            data[i + 4] == 'A' && data[i + 5] == 'd' &&
            data[i + 6] == 'o' && data[i + 7] == 'b') {
            return true;
        }
    }
    return false;
}

CmykHandler::TranscodeResult CmykHandler::transcodeToRgb(const uint8_t* cmyk,
                                                         int width, int height,
                                                         bool inverted) {
    TranscodeResult result;
    if (!cmyk || width <= 0 || height <= 0) return result;

    const std::size_t pixelCount =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    result.rgb.resize(pixelCount * 3);

    for (std::size_t p = 0; p < pixelCount; ++p) {
        uint8_t r = 0, g = 0, b = 0;
        deviceCmykToRgb(cmyk[p * 4], cmyk[p * 4 + 1],
                        cmyk[p * 4 + 2], cmyk[p * 4 + 3],
                        inverted, r, g, b);
        result.rgb[p * 3] = r;
        result.rgb[p * 3 + 1] = g;
        result.rgb[p * 3 + 2] = b;
    }

    result.deltaE = sampleDeltaE(cmyk, result.rgb.data(), width, height, inverted);
    result.ok = result.deltaE.maxDeltaE <= kMaxDeltaE;
    return result;
}

CmykHandler::DeltaEStats CmykHandler::sampleDeltaE(const uint8_t* cmyk,
                                                   const uint8_t* rgb,
                                                   int width, int height,
                                                   bool inverted) {
    DeltaEStats stats;
    if (!cmyk || !rgb || width <= 0 || height <= 0) return stats;

    const std::size_t pixelCount =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    // Cap the work on large images; sample at most ~4096 pixels.
    const std::size_t stride = std::max<std::size_t>(1, pixelCount / 4096);

    double sum = 0.0;
    float worst = 0.0f;
    std::size_t sampled = 0;

    for (std::size_t p = 0; p < pixelCount; p += stride) {
        uint8_t er = 0, eg = 0, eb = 0;
        deviceCmykToRgb(cmyk[p * 4], cmyk[p * 4 + 1],
                        cmyk[p * 4 + 2], cmyk[p * 4 + 3],
                        inverted, er, eg, eb);

        const Lab expected = rgbToLab(er, eg, eb);
        const Lab actual = rgbToLab(rgb[p * 3], rgb[p * 3 + 1], rgb[p * 3 + 2]);

        const float dl = expected.L - actual.L;
        const float da = expected.a - actual.a;
        const float db = expected.b - actual.b;
        const float de = std::sqrt(dl * dl + da * da + db * db);

        if (de > worst) worst = de;
        sum += de;
        ++sampled;
    }

    stats.maxDeltaE = worst;
    stats.meanDeltaE = sampled ? static_cast<float>(sum / sampled) : 0.0f;
    return stats;
}

} // namespace pdfcompress
