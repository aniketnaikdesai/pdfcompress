#include "core/PDFOptimizer.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " <input.pdf> [options]\n\n"
              << "Options:\n"
              << "  -o, --output <path>         Output PDF path (default: <input>_optimized.pdf)\n"
              << "  --profile <profile>         Compression profile: max-quality, balanced, max-compression (default: balanced)\n"
              << "  --quality <1-100>           Quality slider hint (default: profile default)\n"
              << "  --strip-links               Strip link annotations (/Subtype /Link and URI/GoTo/Launch actions)\n"
              << "  --strip-annots              Strip non-link annotations\n"
              << "  --strip-bookmarks           Strip document outline / bookmarks\n"
              << "  --strip-forms               Strip AcroForm forms and widget annotations\n"
              << "  --strip-js                  Strip JavaScript actions from /Names, /OpenAction, /AA\n"
              << "  --strip-dests               Strip named destinations\n"
              << "  --strip-metadata            Strip /Info, XMP /Metadata, PieceInfo, LastModified, Thumb\n"
              << "  --no-strip-metadata         Preserve metadata\n"
              << "  --no-strip-js               Preserve JavaScript\n"
              << "  --linearize                 Enable PDF linearization (Fast Web View)\n"
              << "  --no-flate-recompress       Do not recompress Flate streams at level 9\n"
              << "  --no-dedup                  Do not deduplicate byte-identical streams\n"
              << "  --transcode-cmyk-to-rgb     Opt-in: transcode DeviceCMYK images to DeviceRGB (default: preserve CMYK)\n"
              << "  -h, --help                  Show this help message\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string input;
    std::string output;
    pdfcompress::CompressionProfile profile = pdfcompress::CompressionProfile::Balanced;
    int qualityHint = 0;

    bool profileSpecified = false;
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    // First scan for profile
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--profile" && i + 1 < args.size()) {
            std::string p = args[++i];
            if (p == "max-quality") profile = pdfcompress::CompressionProfile::MaxQuality;
            else if (p == "balanced") profile = pdfcompress::CompressionProfile::Balanced;
            else if (p == "max-compression") profile = pdfcompress::CompressionProfile::MaxCompression;
            else {
                std::cerr << "Unknown profile: " << p << "\n";
                return 1;
            }
            profileSpecified = true;
        }
    }

    auto opts = pdfcompress::OptimizationOptions::forProfile(profile);

    // Parse remaining options
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if ((arg == "-o" || arg == "--output") && i + 1 < args.size()) {
            output = args[++i];
        } else if (arg == "--profile" && i + 1 < args.size()) {
            i++; // Already handled
        } else if (arg == "--quality" && i + 1 < args.size()) {
            opts.qualityHint = std::stoi(args[++i]);
        } else if (arg == "--strip-links") {
            opts.stripLinks = true;
        } else if (arg == "--no-strip-links") {
            opts.stripLinks = false;
        } else if (arg == "--strip-annots") {
            opts.stripOtherAnnotations = true;
        } else if (arg == "--no-strip-annots") {
            opts.stripOtherAnnotations = false;
        } else if (arg == "--strip-bookmarks") {
            opts.stripBookmarks = true;
        } else if (arg == "--no-strip-bookmarks") {
            opts.stripBookmarks = false;
        } else if (arg == "--strip-forms") {
            opts.stripForms = true;
        } else if (arg == "--no-strip-forms") {
            opts.stripForms = false;
        } else if (arg == "--strip-js") {
            opts.stripJavaScript = true;
        } else if (arg == "--no-strip-js") {
            opts.stripJavaScript = false;
        } else if (arg == "--strip-dests") {
            opts.stripNamedDestinations = true;
        } else if (arg == "--no-strip-dests") {
            opts.stripNamedDestinations = false;
        } else if (arg == "--strip-metadata") {
            opts.stripMetadata = true;
        } else if (arg == "--no-strip-metadata") {
            opts.stripMetadata = false;
        } else if (arg == "--linearize") {
            opts.linearize = true;
        } else if (arg == "--no-flate-recompress") {
            opts.recompressFlate = false;
        } else if (arg == "--no-dedup") {
            opts.deduplicateStreams = false;
        } else if (arg == "--transcode-cmyk-to-rgb") {
            opts.transcodeCmykToRgb = true;
        } else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        } else {
            if (input.empty()) {
                input = arg;
            } else {
                std::cerr << "Unexpected argument: " << arg << "\n";
                return 1;
            }
        }
    }

    if (input.empty()) {
        std::cerr << "Error: No input file specified.\n";
        return 1;
    }

    if (output.empty()) {
        output = input;
        auto dot = output.find_last_of('.');
        if (dot != std::string::npos) output.insert(dot, "_optimized");
        else output += "_optimized.pdf";
    }

    pdfcompress::PDFOptimizer optimizer;
    auto result = optimizer.optimize(input, output, opts);

    if (result.success) {
        std::cout << "=== Optimization Successful ===\n";
        std::cout << "  Input:               " << input << "\n";
        std::cout << "  Output:              " << output << "\n";
        std::cout << "  Original Size:       " << result.originalSizeBytes << " bytes\n";
        std::cout << "  Optimized Size:      " << result.optimizedSizeBytes << " bytes\n";
        double savings = 100.0 * (1.0 - static_cast<double>(result.optimizedSizeBytes) / result.originalSizeBytes);
        std::cout << "  Size Reduction:      " << std::fixed << std::setprecision(2) << savings << "%\n";
        std::cout << "  Images Breakdown:    " 
                  << result.imagesOptimized << " optimized, "
                  << result.imagesKeptOriginal << " kept-original, "
                  << result.imagesSkipped << " skipped (total: " << result.imagesProcessed << ")\n";
        if (result.imageBytesSaved > 0) {
            std::cout << "  Image Bytes Saved:   " << result.imageBytesSaved << " bytes\n";
        }
        std::cout << "  Streams Deduplicated: " << result.streamsDeduplicated << "\n";
        if (result.cmykPreserved > 0) {
            std::cout << "  CMYK Preserved:      " << result.cmykPreserved << "\n";
        }
        if (result.transcodedCmyk > 0) {
            std::cout << "  CMYK Transcoded:     " << result.transcodedCmyk << "\n";
        }
        std::cout << "  Stripped Items:\n";
        std::cout << "    - Links:           " << result.linksRemoved << "\n";
        std::cout << "    - Other Annots:    " << result.annotationsRemoved << "\n";
        std::cout << "    - Bookmarks:       " << result.bookmarksRemoved << "\n";
        std::cout << "    - Forms:           " << result.formsRemoved << "\n";
        std::cout << "    - JavaScript:      " << result.jsRemoved << "\n";
        std::cout << "    - Named Dests:     " << result.namedDestinationsRemoved << "\n";
        std::cout << "    - Metadata:        " << (result.metadataStripped ? "Yes" : "No") << "\n";
    } else {
        std::cerr << "Optimization failed: " << result.errorMessage << "\n";
        return 1;
    }

    return 0;
}
