#include "PDFOptimizer.h"
#include "ImageAnalyzer.h"

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFObjectHandle.hh>

#include <turbojpeg.h>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <set>

namespace pdfcompress {

PDFOptimizer::PDFOptimizer() {
}

OptimizationResult PDFOptimizer::optimize(const std::string& inputPath, 
                                          const std::string& outputPath,
                                          CompressionProfile profile,
                                          int qualityHint) {
    OptimizationResult result;
    namespace fs = std::filesystem;

    if (!fs::exists(inputPath)) {
        result.errorMessage = "Input file not found";
        return result;
    }
    result.originalSizeBytes = fs::file_size(inputPath);

    try {
        QPDF pdf;
        pdf.processFile(inputPath.c_str());

        m_decisionEngine = DecisionEngine(profile);

        QPDFPageDocumentHelper pageHelper(pdf);
        
        // 1. Strip Document-Level Interactive Elements
        QPDFObjectHandle root = pdf.getRoot();
        if (root.hasKey("/AcroForm")) {
            root.removeKey("/AcroForm");
        }
        if (root.hasKey("/Outlines")) {
            root.removeKey("/Outlines");
            result.bookmarksRemoved++;
        }
        if (root.hasKey("/Names")) {
            // Can contain JavaScript and other interactive mappings
            root.removeKey("/Names");
        }

        // Keep track of processed images to avoid double-processing shared XObjects
        std::set<int> processedImages;

        // 2. Iterate Pages
        for (auto& page : pageHelper.getAllPages()) {
            // Strip Page-Level Annotations (Links, Widgets, Comments)
            if (page.getObjectHandle().hasKey("/Annots")) {
                page.getObjectHandle().removeKey("/Annots");
                result.annotationsRemoved++;
            }

            // Process Images
            std::map<std::string, QPDFObjectHandle> images = page.getImages();
            for (auto& [name, imageObj] : images) {
                if (processedImages.count(imageObj.getObjGen().getObj()) > 0) {
                    continue; // Already processed this shared image
                }
                processedImages.insert(imageObj.getObjGen().getObj());

                QPDFObjectHandle dict = imageObj.getDict();
                
                int width = dict.getKey("/Width").getIntValueAsInt();
                int height = dict.getKey("/Height").getIntValueAsInt();
                
                std::string filter;
                if (dict.hasKey("/Filter")) {
                    QPDFObjectHandle f = dict.getKey("/Filter");
                    if (f.isName()) filter = f.getName();
                    else if (f.isArray() && f.getArrayNItems() > 0) {
                        filter = f.getArrayItem(0).getName();
                    }
                }

                // Determine channels from ColorSpace
                int channels = 3;
                if (dict.hasKey("/ColorSpace")) {
                    QPDFObjectHandle cs = dict.getKey("/ColorSpace");
                    if (cs.isName() && cs.getName() == "/DeviceGray") channels = 1;
                    else if (cs.isName() && cs.getName() == "/DeviceCMYK") channels = 4;
                }

                std::vector<uint8_t> rawPixels;
                ImageMetadata meta;

                // Decompress the stream to raw pixels for analysis
                if (filter == "/DCTDecode") {
                    // It's a JPEG. Use TurboJPEG to decompress.
                    auto rawStreamData = imageObj.getRawStreamData();
                    if (rawStreamData) {
                        meta.compressedSize = rawStreamData->getSize();
                        tjhandle tj = tjInitDecompress();
                        int w, h, subsamp, colorspace;
                        if (tjDecompressHeader3(tj, rawStreamData->getBuffer(), rawStreamData->getSize(), &w, &h, &subsamp, &colorspace) == 0) {
                            rawPixels.resize(w * h * channels);
                            int pixelFormat = (channels == 1) ? TJPF_GRAY : TJPF_RGB;
                            if (tjDecompress2(tj, rawStreamData->getBuffer(), rawStreamData->getSize(), rawPixels.data(), w, 0, h, pixelFormat, 0) != 0) {
                                rawPixels.clear();
                            }
                        }
                        tjDestroy(tj);
                    }
                } else if (filter == "/FlateDecode") {
                    // PNG / Zlib. QPDF can uncompress this natively.
                    auto rawStreamData = imageObj.getRawStreamData();
                    if (rawStreamData) {
                        meta.compressedSize = rawStreamData->getSize();
                        auto streamData = imageObj.getStreamData(qpdf_dl_all);
                        if (streamData) {
                            rawPixels.assign(streamData->getBuffer(), streamData->getBuffer() + streamData->getSize());
                        }
                    }
                }

                // If we successfully got raw pixels, we can re-compress
                if (!rawPixels.empty() && rawPixels.size() >= width * height * channels) {
                    meta.widthPx = width;
                    meta.heightPx = height;
                    meta.uncompressedSize = rawPixels.size();
                    
                    // Analyze
                    auto analysis = ImageAnalyzer::analyze(rawPixels.data(), width, height, channels, meta);

                    // Re-compress
                    StreamFilter outFilter;
                    auto compressedBytes = m_decisionEngine.compress(rawPixels.data(), width, height, channels, analysis, outFilter, qualityHint);

                    if (!compressedBytes.empty() && compressedBytes.size() < meta.compressedSize) {
                        // Replace stream data
                        auto buffer = std::make_shared<Buffer>(compressedBytes.size());
                        memcpy(buffer->getBuffer(), compressedBytes.data(), compressedBytes.size());
                        QPDFObjectHandle filterName;
                        if (outFilter == StreamFilter::DCTDecode) {
                            filterName = QPDFObjectHandle::newName("/DCTDecode");
                        } else if (outFilter == StreamFilter::FlateDecode) {
                            filterName = QPDFObjectHandle::newName("/FlateDecode");
                        } else if (outFilter == StreamFilter::JPXDecode) {
                            filterName = QPDFObjectHandle::newName("/JPXDecode");
                        }
                        imageObj.replaceStreamData(
                            buffer,
                            filterName,
                            QPDFObjectHandle::newNull()
                        );

                        // Update filter in dictionary for PDF readers
                        dict.replaceKey("/Filter", filterName);
                        
                        result.imagesProcessed++;
                    }
                }
            }
        }

        // 3. Write Optimized PDF
        QPDFWriter writer(pdf, outputPath.c_str());
        writer.setLinearization(true); // Optimize for fast web view
        writer.setObjectStreamMode(qpdf_o_generate); // Compress PDF structure
        writer.setStreamDataMode(qpdf_s_preserve);
        writer.write();

        result.optimizedSizeBytes = fs::file_size(outputPath);
        result.success = true;

    } catch (const std::exception& e) {
        result.errorMessage = e.what();
        result.success = false;
    }

    return result;
}

} // namespace pdfcompress
