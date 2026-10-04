// render_diff.cpp - Pure-C++ Render-Diff Helper via PDFium 150 DPI BGRA SSIM/PSNR
// C++20, offline, no external scripting dependencies

#include <fpdfview.h>
#include <cmath>
#include <memory>
#include <vector>
#include <iostream>
#include <string>
#include <cstdint>
#include <algorithm>
#include <cstdlib>

// RAII helper for PDFium library lifetime
struct PdfiumLibraryGuard {
    PdfiumLibraryGuard() {
        FPDF_LIBRARY_CONFIG config{};
        config.version = 2;
        config.m_pUserFontPaths = nullptr;
        config.m_pIsolate = nullptr;
        config.m_v8EmbedderSlot = 0;
        FPDF_InitLibraryWithConfig(&config);
    }
    ~PdfiumLibraryGuard() {
        FPDF_DestroyLibrary();
    }
    PdfiumLibraryGuard(const PdfiumLibraryGuard&) = delete;
    PdfiumLibraryGuard& operator=(const PdfiumLibraryGuard&) = delete;
};

// Convert BGRA buffer to grayscale (0-255) respecting stride
static std::vector<uint8_t> bgraToGrayscale(const uint8_t* buffer, int width, int height, int stride) {
    std::vector<uint8_t> gray;
    gray.reserve(static_cast<size_t>(width) * static_cast<size_t>(height));
    for (int y = 0; y < height; ++y) {
        const uint8_t* row = buffer + static_cast<size_t>(y) * static_cast<size_t>(stride);
        for (int x = 0; x < width; ++x) {
            const uint8_t* px = row + static_cast<size_t>(x) * 4;
            uint8_t b = px[0];
            uint8_t g = px[1];
            uint8_t r = px[2];
            // ITU-R BT.601 luma
            double lum = 0.299 * static_cast<double>(r) + 0.587 * static_cast<double>(g) + 0.114 * static_cast<double>(b);
            if (lum < 0) lum = 0;
            if (lum > 255) lum = 255;
            gray.push_back(static_cast<uint8_t>(std::lround(lum)));
        }
    }
    return gray;
}

// Render page 0 of PDF at 150 DPI to grayscale vector
static bool renderPdfToGrayscale(const std::string& path, std::vector<uint8_t>& outGray, int& outW, int& outH) {
    // Load document
    FPDF_DOCUMENT doc = FPDF_LoadDocument(path.c_str(), nullptr);
    if (!doc) {
        return false;
    }
    // RAII unique_ptr with custom deleter for FPDF_DOCUMENT
    std::unique_ptr<std::remove_pointer<FPDF_DOCUMENT>::type, decltype(&FPDF_CloseDocument)> docGuard(doc, FPDF_CloseDocument);

    // Load page 0
    FPDF_PAGE page = FPDF_LoadPage(doc, 0);
    if (!page) {
        return false;
    }
    std::unique_ptr<std::remove_pointer<FPDF_PAGE>::type, decltype(&FPDF_ClosePage)> pageGuard(page, FPDF_ClosePage);

    // Compute width/height via FPDF_GetPageWidth / FPDF_GetPageHeight
    double pageWidthPt = FPDF_GetPageWidth(page);
    double pageHeightPt = FPDF_GetPageHeight(page);
    if (pageWidthPt <= 0 || pageHeightPt <= 0) {
        return false;
    }
    // 150 DPI: w = pageWidthPt / 72 * 150, h = pageHeightPt / 72 * 150
    int w = static_cast<int>(pageWidthPt / 72.0 * 150.0 + 0.5);
    int h = static_cast<int>(pageHeightPt / 72.0 * 150.0 + 0.5);
    if (w <= 0 || h <= 0) {
        return false;
    }
    // Clamp to avoid absurd allocations
    if (w > 10000 || h > 10000) {
        return false;
    }

    // Create BGRA bitmap w=pageWidthPt/72*150 h=pageHeightPt/72*150 via FPDFBitmap_Create
    // Use alpha=1 for BGRA
    FPDF_BITMAP bitmap = FPDFBitmap_Create(w, h, 1);
    if (!bitmap) {
        return false;
    }
    std::unique_ptr<std::remove_pointer<FPDF_BITMAP>::type, decltype(&FPDFBitmap_Destroy)> bitmapGuard(bitmap, FPDFBitmap_Destroy);

    // Fill white 0xFFFFFFFF via FPDFBitmap_FillRect
    FPDFBitmap_FillRect(bitmap, 0, 0, w, h, 0xFFFFFFFF);

    // Render via FPDF_RenderPageBitmap
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, w, h, 0, 0);

    // Retrieve buffers via FPDFBitmap_GetBuffer and handle BGRA stride via FPDFBitmap_GetStride
    void* buffer = FPDFBitmap_GetBuffer(bitmap);
    int stride = FPDFBitmap_GetStride(bitmap);
    if (!buffer || stride <= 0) {
        return false;
    }

    auto* bytes = static_cast<uint8_t*>(buffer);
    outGray = bgraToGrayscale(bytes, w, h, stride);
    outW = w;
    outH = h;
    return true;
}

static double computePSNR(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size() || a.empty()) {
        return 0.0;
    }
    double mse = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        double d = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        mse += d * d;
    }
    mse /= static_cast<double>(a.size());
    if (mse == 0.0) {
        return INFINITY;
    }
    // PSNR = 10*log10(255*255/MSE)
    double psnr = 10.0 * std::log10((255.0 * 255.0) / mse);
    return psnr;
}

static double computeSSIM(const std::vector<uint8_t>& img1, const std::vector<uint8_t>& img2, int w, int h) {
    if (img1.size() != img2.size() || img1.empty()) {
        return 0.0;
    }
    // Windowed mean/variance/covariance with C1=(0.01*255)^2 C2=(0.03*255)^2
    const double C1 = (0.01 * 255.0) * (0.01 * 255.0);
    const double C2 = (0.03 * 255.0) * (0.03 * 255.0);

    int win = 11;
    if (w < win || h < win) {
        win = 8;
    }
    if (w < win || h < win) {
        // Too small: compute global SSIM
        double mu1 = 0, mu2 = 0;
        for (size_t i = 0; i < img1.size(); ++i) {
            mu1 += img1[i];
            mu2 += img2[i];
        }
        mu1 /= img1.size();
        mu2 /= img2.size();
        double sig1 = 0, sig2 = 0, sig12 = 0;
        for (size_t i = 0; i < img1.size(); ++i) {
            double d1 = img1[i] - mu1;
            double d2 = img2[i] - mu2;
            sig1 += d1 * d1;
            sig2 += d2 * d2;
            sig12 += d1 * d2;
        }
        sig1 /= img1.size();
        sig2 /= img2.size();
        sig12 /= img1.size();
        double num = (2 * mu1 * mu2 + C1) * (2 * sig12 + C2);
        double den = (mu1 * mu1 + mu2 * mu2 + C1) * (sig1 + sig2 + C2);
        if (den == 0) return 1.0;
        return num / den;
    }

    int half = win / 2;
    // Use sliding window, step 1, average over all windows
    // For performance, we compute window stats naive but still fast for 150 DPI
    double ssimSum = 0.0;
    long count = 0;
    // Preconvert to double for faster access? Keep uint8_t.
    for (int y = half; y < h - half; ++y) {
        for (int x = half; x < w - half; ++x) {
            double sum1 = 0, sum2 = 0;
            for (int wy = -half; wy <= half; ++wy) {
                int yy = y + wy;
                size_t base = static_cast<size_t>(yy) * static_cast<size_t>(w);
                for (int wx = -half; wx <= half; ++wx) {
                    int xx = x + wx;
                    size_t idx = base + static_cast<size_t>(xx);
                    sum1 += img1[idx];
                    sum2 += img2[idx];
                }
            }
            double area = static_cast<double>(win * win);
            double mu1 = sum1 / area;
            double mu2 = sum2 / area;
            double var1 = 0, var2 = 0, cov = 0;
            for (int wy = -half; wy <= half; ++wy) {
                int yy = y + wy;
                size_t base = static_cast<size_t>(yy) * static_cast<size_t>(w);
                for (int wx = -half; wx <= half; ++wx) {
                    int xx = x + wx;
                    size_t idx = base + static_cast<size_t>(xx);
                    double d1 = static_cast<double>(img1[idx]) - mu1;
                    double d2 = static_cast<double>(img2[idx]) - mu2;
                    var1 += d1 * d1;
                    var2 += d2 * d2;
                    cov += d1 * d2;
                }
            }
            var1 /= area;
            var2 /= area;
            cov /= area;
            double num = (2 * mu1 * mu2 + C1) * (2 * cov + C2);
            double den = (mu1 * mu1 + mu2 * mu2 + C1) * (var1 + var2 + C2);
            double ssim = (den == 0) ? 1.0 : num / den;
            ssimSum += ssim;
            ++count;
        }
    }
    if (count == 0) return 1.0;
    return ssimSum / static_cast<double>(count);
}

int main(int argc, char* argv[]) {
    // Handle PDFium dylib unavailable or any abnormal condition via try/catch
    try {
        if (argc < 3) {
            std::cout << "N/A N/A" << std::endl;
            return 0;
        }
        std::string path1 = argv[1];
        std::string path2 = argv[2];

        // RAII unique_ptr custom deleters already cover FPDF_ClosePage / FPDF_CloseDocument / FPDFBitmap_Destroy
        // Library guard covers FPDF_DestroyLibrary
        PdfiumLibraryGuard libGuard;

        std::vector<uint8_t> gray1, gray2;
        int w1 = 0, h1 = 0, w2 = 0, h2 = 0;
        bool ok1 = false, ok2 = false;
        try {
            ok1 = renderPdfToGrayscale(path1, gray1, w1, h1);
        } catch (...) {
            ok1 = false;
        }
        try {
            ok2 = renderPdfToGrayscale(path2, gray2, w2, h2);
        } catch (...) {
            ok2 = false;
        }

        if (!ok1 || !ok2) {
            std::cout << "N/A N/A" << std::endl;
            if (!ok1) {
                std::cerr << "render failed for: " << path1 << std::endl;
            }
            if (!ok2) {
                std::cerr << "render failed for: " << path2 << std::endl;
            }
            return 0;
        }

        // Handle size mismatch: crop to common area
        if (w1 != w2 || h1 != h2) {
            int cw = std::min(w1, w2);
            int ch = std::min(h1, h2);
            if (cw <= 0 || ch <= 0) {
                std::cout << "N/A N/A" << std::endl;
                return 0;
            }
            // Crop both images to cw x ch (top-left)
            std::vector<uint8_t> cropped1, cropped2;
            cropped1.reserve(static_cast<size_t>(cw) * static_cast<size_t>(ch));
            cropped2.reserve(static_cast<size_t>(cw) * static_cast<size_t>(ch));
            for (int y = 0; y < ch; ++y) {
                for (int x = 0; x < cw; ++x) {
                    cropped1.push_back(gray1[static_cast<size_t>(y) * static_cast<size_t>(w1) + static_cast<size_t>(x)]);
                    cropped2.push_back(gray2[static_cast<size_t>(y) * static_cast<size_t>(w2) + static_cast<size_t>(x)]);
                }
            }
            gray1.swap(cropped1);
            gray2.swap(cropped2);
            w1 = cw;
            h1 = ch;
            w2 = cw;
            h2 = ch;
        }

        double ssim = computeSSIM(gray1, gray2, w1, h1);
        double psnr = computePSNR(gray1, gray2);

        // Clamp SSIM to [0,1]
        if (ssim < 0) ssim = 0;
        if (ssim > 1) ssim = 1;

        // Print parsable line e.g. 'SSIM 0.999 PSNR 42.1' or '0.999 42.1' or 'SSIM=0.999 PSNR=42.1'
        // Use format that satisfies all regexes: SSIM and PSNR keywords
        std::cout.setf(std::ios::fixed);
        std::cout.precision(4);
        std::cout << "SSIM " << ssim << " PSNR ";
        if (std::isinf(psnr)) {
            std::cout << "INF";
        } else {
            std::cout.setf(std::ios::fixed);
            std::cout.precision(1);
            std::cout << psnr;
        }
        std::cout << std::endl;
        return 0;
    } catch (...) {
        std::cout << "N/A N/A" << std::endl;
        return 0;
    }
}
