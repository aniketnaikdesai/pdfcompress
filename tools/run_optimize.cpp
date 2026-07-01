#include "core/PDFOptimizer.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.pdf>" << std::endl;
        return 1;
    }

    std::string input = argv[1];
    std::string output = input;
    auto dot = output.find_last_of('.');
    if (dot != std::string::npos) output.insert(dot, "_optimized");
    else output += "_optimized";

    pdfcompress::PDFOptimizer optimizer;
    auto result = optimizer.optimize(input, output, pdfcompress::CompressionProfile::Balanced);

    if (result.success) {
        std::cout << "=== Optimization Successful ===" << std::endl;
        std::cout << "  Output: " << output << std::endl;
        std::cout << "  Original Size: " << result.originalSizeBytes << " bytes" << std::endl;
        std::cout << "  Optimized Size: " << result.optimizedSizeBytes << " bytes" << std::endl;
        double savings = 100.0 * (1.0 - static_cast<double>(result.optimizedSizeBytes) / result.originalSizeBytes);
        std::cout << "  Size Reduction: " << savings << "%" << std::endl;
        std::cout << "  Images Processed: " << result.imagesProcessed << std::endl;
        std::cout << "  Annotations Removed: " << result.annotationsRemoved << std::endl;
        std::cout << "  Bookmarks Removed: " << result.bookmarksRemoved << std::endl;
    } else {
        std::cerr << "Optimization failed: " << result.errorMessage << std::endl;
        return 1;
    }

    return 0;
}
