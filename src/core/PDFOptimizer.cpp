#include "PDFOptimizer.h"
#include "ImageAnalyzer.h"

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/Pl_Flate.hh>

#include <turbojpeg.h>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <set>
#include <map>

namespace pdfcompress {

namespace {

int countOutlines(QPDFObjectHandle outlineItem) {
    int count = 0;
    std::set<QPDFObjGen> visited;
    while (!outlineItem.isNull() && outlineItem.isDictionary()) {
        if (outlineItem.isIndirect()) {
            if (visited.count(outlineItem.getObjGen()) > 0) break;
            visited.insert(outlineItem.getObjGen());
        }
        count++;
        if (outlineItem.hasKey("/First")) {
            count += countOutlines(outlineItem.getKey("/First"));
        }
        if (outlineItem.hasKey("/Next")) {
            outlineItem = outlineItem.getKey("/Next");
        } else {
            break;
        }
    }
    return count;
}

} // namespace

PDFOptimizer::PDFOptimizer() {
}

OptimizationResult PDFOptimizer::optimize(const std::string& inputPath, 
                                          const std::string& outputPath,
                                          const OptimizationOptions& options) {
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

        m_decisionEngine = DecisionEngine(options.profile);

        QPDFPageDocumentHelper pageHelper(pdf);
        QPDFObjectHandle root = pdf.getRoot();

        // 1. Strip Document-Level Elements
        if (options.stripForms && root.hasKey("/AcroForm")) {
            root.removeKey("/AcroForm");
            result.formsRemoved++;
        }

        if (options.stripBookmarks && root.hasKey("/Outlines")) {
            QPDFObjectHandle outlines = root.getKey("/Outlines");
            int count = 1;
            if (outlines.hasKey("/First")) {
                count = countOutlines(outlines.getKey("/First"));
            }
            result.bookmarksRemoved += count;
            root.removeKey("/Outlines");
        }

        if (options.stripJavaScript) {
            if (root.hasKey("/Names")) {
                QPDFObjectHandle names = root.getKey("/Names");
                if (names.isDictionary() && names.hasKey("/JavaScript")) {
                    names.removeKey("/JavaScript");
                    result.jsRemoved++;
                }
            }
            if (root.hasKey("/OpenAction")) {
                QPDFObjectHandle oa = root.getKey("/OpenAction");
                if (oa.isDictionary() && oa.hasKey("/S") && oa.getKey("/S").getName() == "/JavaScript") {
                    root.removeKey("/OpenAction");
                    result.jsRemoved++;
                }
            }
            if (root.hasKey("/AA")) {
                root.removeKey("/AA");
                result.jsRemoved++;
            }
        }

        if (options.stripNamedDestinations) {
            if (root.hasKey("/Dests")) {
                root.removeKey("/Dests");
                result.namedDestinationsRemoved++;
            }
            if (root.hasKey("/Names")) {
                QPDFObjectHandle names = root.getKey("/Names");
                if (names.isDictionary() && names.hasKey("/Dests")) {
                    names.removeKey("/Dests");
                    result.namedDestinationsRemoved++;
                }
            }
        }

        if (options.stripMetadata) {
            pdf.getTrailer().removeKey("/Info");
            if (root.hasKey("/Metadata")) root.removeKey("/Metadata");
            if (root.hasKey("/PieceInfo")) root.removeKey("/PieceInfo");
            if (root.hasKey("/LastModified")) root.removeKey("/LastModified");
            result.metadataStripped = true;
        }

        // Keep track of processed images to avoid double-processing shared XObjects
        std::set<QPDFObjGen> processedImages;

        // 2. Iterate Pages
        for (auto& page : pageHelper.getAllPages()) {
            QPDFObjectHandle pageObj = page.getObjectHandle();

            if (options.stripJavaScript && pageObj.hasKey("/AA")) {
                pageObj.removeKey("/AA");
                result.jsRemoved++;
            }

            if (options.stripMetadata) {
                if (pageObj.hasKey("/Metadata")) pageObj.removeKey("/Metadata");
                if (pageObj.hasKey("/PieceInfo")) pageObj.removeKey("/PieceInfo");
                if (pageObj.hasKey("/LastModified")) pageObj.removeKey("/LastModified");
                if (pageObj.hasKey("/Thumb")) pageObj.removeKey("/Thumb");
            }

            // Granular Annotations & Links filtering
            if (pageObj.hasKey("/Annots")) {
                QPDFObjectHandle annots = pageObj.getKey("/Annots");
                if (annots.isArray()) {
                    std::vector<QPDFObjectHandle> keptAnnots;
                    int n = annots.getArrayNItems();
                    for (int i = 0; i < n; ++i) {
                        QPDFObjectHandle annot = annots.getArrayItem(i);
                        if (!annot.isDictionary()) continue;

                        std::string subtype = annot.hasKey("/Subtype") ? annot.getKey("/Subtype").getName() : "";
                        bool isLink = (subtype == "/Link");
                        bool isWidget = (subtype == "/Widget");

                        bool hasLinkAction = false;
                        if (annot.hasKey("/A")) {
                            QPDFObjectHandle action = annot.getKey("/A");
                            if (action.isDictionary() && action.hasKey("/S")) {
                                std::string s = action.getKey("/S").getName();
                                if (s == "/URI" || s == "/GoTo" || s == "/Launch") {
                                    hasLinkAction = true;
                                }
                            }
                        }

                        if (options.stripForms && isWidget) {
                            result.formsRemoved++;
                            continue;
                        }

                        if (options.stripLinks && (isLink || hasLinkAction)) {
                            result.linksRemoved++;
                            continue;
                        }

                        if (options.stripOtherAnnotations && !isLink && !hasLinkAction && !(isWidget && !options.stripForms)) {
                            result.annotationsRemoved++;
                            continue;
                        }

                        keptAnnots.push_back(annot);
                    }

                    if (keptAnnots.empty()) {
                        pageObj.removeKey("/Annots");
                    } else if (static_cast<int>(keptAnnots.size()) < n) {
                        pageObj.replaceKey("/Annots", QPDFObjectHandle::newArray(keptAnnots));
                    }
                }
            }

            // Process Images
            std::map<std::string, QPDFObjectHandle> images = page.getImages();
            for (auto& [name, imageObj] : images) {
                if (processedImages.count(imageObj.getObjGen()) > 0) {
                    continue; // Already processed this shared image
                }
                processedImages.insert(imageObj.getObjGen());
                result.imagesProcessed++;

                QPDFObjectHandle dict = imageObj.getDict();

                if (options.stripMetadata) {
                    if (dict.hasKey("/Metadata")) dict.removeKey("/Metadata");
                    if (dict.hasKey("/PieceInfo")) dict.removeKey("/PieceInfo");
                    if (dict.hasKey("/LastModified")) dict.removeKey("/LastModified");
                }
                
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

                // Safety skip: The current JpegCodec corrupts CMYK by treating it as RGBA.
                if (channels == 4) {
                    result.imagesSkipped++;
                    continue;
                }

                std::vector<uint8_t> rawPixels;
                ImageMetadata meta;

                // Decompress the stream to raw pixels for analysis
                if (filter == "/DCTDecode") {
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
                    auto rawStreamData = imageObj.getRawStreamData();
                    if (rawStreamData) {
                        meta.compressedSize = rawStreamData->getSize();
                        auto streamData = imageObj.getStreamData(qpdf_dl_all);
                        if (streamData) {
                            rawPixels.assign(streamData->getBuffer(), streamData->getBuffer() + streamData->getSize());
                        }
                    }
                } else {
                    result.imagesSkipped++;
                    continue;
                }

                // If we successfully got raw pixels, we can re-compress
                if (!rawPixels.empty() && rawPixels.size() >= static_cast<size_t>(width * height * channels)) {
                    meta.widthPx = width;
                    meta.heightPx = height;
                    meta.uncompressedSize = rawPixels.size();
                    
                    // Analyze
                    auto analysis = ImageAnalyzer::analyze(rawPixels.data(), width, height, channels, meta);

                    // Re-compress
                    StreamFilter outFilter;
                    auto compressedBytes = m_decisionEngine.compress(rawPixels.data(), width, height, channels, analysis, outFilter, options.qualityHint);

                    if (!compressedBytes.empty() && compressedBytes.size() < meta.compressedSize) {
                        size_t saved = meta.compressedSize - compressedBytes.size();
                        result.imageBytesSaved += saved;

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
                        
                        result.imagesOptimized++;
                    } else {
                        result.imagesKeptOriginal++;
                    }
                } else {
                    result.imagesKeptOriginal++;
                }
            }
        }

        // Clean metadata on any remaining XObjects (e.g. Form XObjects)
        if (options.stripMetadata) {
            for (auto& obj : pdf.getAllObjects()) {
                if (obj.isStream()) {
                    QPDFObjectHandle dict = obj.getDict();
                    if (dict.hasKey("/Type") && dict.getKey("/Type").getName() == "/XObject") {
                        if (dict.hasKey("/Metadata")) dict.removeKey("/Metadata");
                        if (dict.hasKey("/PieceInfo")) dict.removeKey("/PieceInfo");
                        if (dict.hasKey("/LastModified")) dict.removeKey("/LastModified");
                    }
                }
            }
        }

        // 3. Deduplicate Byte-Identical Streams
        if (options.deduplicateStreams) {
            std::map<std::string, QPDFObjectHandle> seenStreams;
            for (auto& obj : pdf.getAllObjects()) {
                if (!obj.isStream() || !obj.isIndirect()) continue;

                auto rawData = obj.getRawStreamData();
                if (!rawData) continue;

                QPDFObjectHandle dict = obj.getDict();
                std::string sig;
                if (dict.hasKey("/Subtype")) sig += dict.getKey("/Subtype").unparse() + ";";
                if (dict.hasKey("/Width")) sig += dict.getKey("/Width").unparse() + ";";
                if (dict.hasKey("/Height")) sig += dict.getKey("/Height").unparse() + ";";
                if (dict.hasKey("/ColorSpace")) sig += dict.getKey("/ColorSpace").unparse() + ";";
                if (dict.hasKey("/BitsPerComponent")) sig += dict.getKey("/BitsPerComponent").unparse() + ";";

                size_t dataSize = rawData->getSize();
                const unsigned char* buf = rawData->getBuffer();
                size_t h = 2166136261u;
                for (size_t i = 0; i < dataSize; ++i) {
                    h = (h ^ buf[i]) * 16777619u;
                }
                sig += std::to_string(dataSize) + ":" + std::to_string(h);

                auto it = seenStreams.find(sig);
                if (it == seenStreams.end()) {
                    seenStreams[sig] = obj;
                } else {
                    auto prevData = it->second.getRawStreamData();
                    if (prevData && prevData->getSize() == dataSize &&
                        memcmp(prevData->getBuffer(), buf, dataSize) == 0 &&
                        obj.getObjGen() != it->second.getObjGen()) {
                        // ESC-001: QPDF::replaceObject requires a direct object as
                        // replacement, but seenStreams stores indirect handles.
                        // Passing an indirect handle throws and fails the whole file.
                        // Skip such pairs (safe: just misses one dedup opportunity)
                        // and guard with try/catch so one bad pair never fails the file.
                        try {
                            if (it->second.isIndirect()) {
                                continue;
                            }
                            pdf.replaceObject(obj.getObjGen(), it->second);
                            result.streamsDeduplicated++;
                        } catch (const std::exception& e) {
                            std::cerr << "Dedup skip obj " << obj.getObjGen().getObj()
                                      << ": " << e.what() << std::endl;
                            continue;
                        }
                    }
                }
            }
        }

        // 4. Write Optimized PDF with Structural Optimizations
        QPDFWriter writer(pdf, outputPath.c_str());
        writer.setLinearization(options.linearize);
        writer.setObjectStreamMode(qpdf_o_generate);
        writer.setCompressStreams(true);

        if (options.recompressFlate) {
            Pl_Flate::setCompressionLevel(9);
            writer.setRecompressFlate(true);
        }

        writer.setPreserveUnreferencedObjects(false);

        if (options.stripMetadata) {
            writer.setDeterministicID(true);
        }

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
