#pragma once

#include <string>
#include <vector>
#include "DecisionEngine.h"

namespace pdfcompress {

struct OptimizationResult {
    bool success = false;
    size_t originalSizeBytes = 0;
    size_t optimizedSizeBytes = 0;
    int imagesProcessed = 0;
    int annotationsRemoved = 0;
    int bookmarksRemoved = 0;
    std::string errorMessage;
};

class PDFOptimizer {
public:
    PDFOptimizer();

    /// Optimize a PDF by replacing image streams and stripping interactive elements.
    /// @param inputPath Path to original PDF.
    /// @param outputPath Path to save the optimized PDF.
    /// @param profile Compression profile to use for images.
    /// @return Results of the optimization.
    OptimizationResult optimize(const std::string& inputPath, 
                                const std::string& outputPath,
                                CompressionProfile profile = CompressionProfile::Balanced);
                                
private:
    DecisionEngine m_decisionEngine;
};

} // namespace pdfcompress
