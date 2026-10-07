#include "PDFOptimizer.h"
#include "ImageAnalyzer.h"
#include "CmykHandler.h"
#include "ReferenceRewriter.h"
#include "../codecs/JpegCodec.h"

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

                // Determine color space / channels / bit depth from the image dict.
                ColorSpace colorSpace = ColorSpace::Unknown;
                int channels = 3;
                if (dict.hasKey("/ColorSpace")) {
                    QPDFObjectHandle cs = dict.getKey("/ColorSpace");
                    std::string csName;
                    if (cs.isName()) {
                        csName = cs.getName();
                    } else if (cs.isArray() && cs.getArrayNItems() > 0 && cs.getArrayItem(0).isName()) {
                        csName = cs.getArrayItem(0).getName();
                    }

                    if (csName == "/DeviceGray") { channels = 1; colorSpace = ColorSpace::DeviceGray; }
                    else if (csName == "/DeviceRGB") { channels = 3; colorSpace = ColorSpace::DeviceRGB; }
                    else if (csName == "/DeviceCMYK") { channels = 4; colorSpace = ColorSpace::DeviceCMYK; }
                    else if (csName == "/Indexed") { colorSpace = ColorSpace::Indexed; }
                    else if (csName == "/ICCBased") {
                        colorSpace = ColorSpace::ICCBased;
                        // ICCBased is [/ICCBased <stream>]; the stream's /N is
                        // the component count. N==4 is CMYK and must be
                        // preserved, never treated as RGBA.
                        if (cs.isArray() && cs.getArrayNItems() >= 2) {
                            QPDFObjectHandle iccStream = cs.getArrayItem(1);
                            if (iccStream.isStream() &&
                                iccStream.getDict().hasKey("/N") &&
                                iccStream.getDict().getKey("/N").getIntValueAsInt() == 4) {
                                channels = 4;
                            }
                        }
                    }
                }

                int bitsPerComponent = 8;
                if (dict.hasKey("/BitsPerComponent")) {
                    bitsPerComponent = dict.getKey("/BitsPerComponent").getIntValueAsInt();
                }

                // Safety skip: bit-packed images (/BitsPerComponent != 8) store
                // their samples packed rather than one byte per sample. The
                // raw-pixel decode path below cannot interpret that layout
                // without re-packing, so keep the original stream untouched
                // instead of mis-decoding it, and record the reason.
                if (bitsPerComponent != 8) {
                    std::cerr << "PDFOptimizer: keeping image obj "
                              << imageObj.getObjGen().getObj()
                              << " original: /BitsPerComponent " << bitsPerComponent
                              << " is not 8 (bit-packed data not re-encoded)"
                              << std::endl;
                    result.imagesSkipped++;
                    continue;
                }

                // Detect alpha. An image carrying an /SMask or /Mask keeps its
                // transparency in a separate stream; it must never be flattened
                // into JPEG. The DecisionEngine routes such images to the
                // lossless PNG/FlateDecode branch, and the mask stream below is
                // left untouched.
                bool hasAlpha = dict.hasKey("/SMask") || dict.hasKey("/Mask");

                // CMYK handling. Default: DeviceCMYK / ICCBased-CMYK images
                // are kept byte-for-byte (the JpegCodec cannot re-encode CMYK
                // without treating the 4 channels as RGBA and corrupting the
                // colors), with the skip recorded and a human-readable reason.
                // Opt-in (AC-C3b): transcode to RGB and re-encode as JPEG,
                // honoring Adobe APP14 / /Decode inversion so the colors are
                // never silently flipped.
                if (channels == 4) {
                    if (!options.transcodeCmykToRgb) {
                        std::cerr << "PDFOptimizer: preserving image obj "
                                  << imageObj.getObjGen().getObj()
                                  << " original: CMYK image kept intact "
                                     "(/ColorSpace preserved, not re-encoded)"
                                  << std::endl;
                        result.cmykPreserved++;
                        result.imagesSkipped++;
                        continue;
                    }

                    // Gather tightly-packed 4-channel CMYK samples.
                    std::vector<uint8_t> cmykPixels;
                    size_t originalImageBytes = 0;
                    bool inverted = false;

                    if (filter == "/DCTDecode") {
                        auto rawStreamData = imageObj.getRawStreamData();
                        if (rawStreamData) {
                            originalImageBytes = rawStreamData->getSize();
                            // Adobe-produced CMYK JPEGs store inverted samples.
                            inverted = CmykHandler::hasAdobeApp14(
                                rawStreamData->getBuffer(), rawStreamData->getSize());
                            tjhandle tj = tjInitDecompress();
                            if (tj) {
                                int w = 0, h = 0, subsamp = 0, cs = 0;
                                if (tjDecompressHeader3(tj, rawStreamData->getBuffer(),
                                                        rawStreamData->getSize(),
                                                        &w, &h, &subsamp, &cs) == 0) {
                                    cmykPixels.resize(static_cast<size_t>(w) * h * 4);
                                    if (tjDecompress2(tj, rawStreamData->getBuffer(),
                                                      rawStreamData->getSize(),
                                                      cmykPixels.data(), w, 0, h,
                                                      TJPF_CMYK, 0) != 0) {
                                        cmykPixels.clear();
                                    }
                                }
                                tjDestroy(tj);
                            }
                        }
                    } else if (filter == "/FlateDecode") {
                        // A /Decode array of [1 0 1 0 1 0 1 0] marks inverted
                        // CMYK samples in the raw stream.
                        if (dict.hasKey("/Decode")) {
                            QPDFObjectHandle dec = dict.getKey("/Decode");
                            if (dec.isArray() && dec.getArrayNItems() >= 8 &&
                                dec.getArrayItem(0).isNumber() &&
                                dec.getArrayItem(0).getIntValueAsInt() == 1) {
                                inverted = true;
                            }
                        }
                        auto rawStreamData = imageObj.getRawStreamData();
                        if (rawStreamData) originalImageBytes = rawStreamData->getSize();
                        auto streamData = imageObj.getStreamData(qpdf_dl_all);
                        if (streamData) {
                            cmykPixels.assign(streamData->getBuffer(),
                                              streamData->getBuffer() + streamData->getSize());
                        }
                    }

                    const size_t needed = static_cast<size_t>(width) * height * 4;
                    if (cmykPixels.size() < needed) {
                        std::cerr << "PDFOptimizer: preserving image obj "
                                  << imageObj.getObjGen().getObj()
                                  << " original: CMYK transcode could not decode "
                                     "the source stream"
                                  << std::endl;
                        result.cmykPreserved++;
                        result.imagesSkipped++;
                        continue;
                    }

                    CompressionParams rgbParams;
                    rgbParams.width = width;
                    rgbParams.height = height;
                    rgbParams.channels = 3;
                    rgbParams.profile = options.profile;
                    rgbParams.qualityHint = options.qualityHint;
                    rgbParams.colorSpace = ColorSpace::DeviceRGB;

                    JpegCodec jpegCodec;
                    float maxDeltaE = 0.0f;
                    auto compressedBytes = jpegCodec.encodeCmykAsRgb(
                        cmykPixels.data(), width, height, rgbParams, inverted, &maxDeltaE);

                    if (compressedBytes.empty()) {
                        std::cerr << "PDFOptimizer: preserving image obj "
                                  << imageObj.getObjGen().getObj()
                                  << " original: CMYK transcode rejected (max delta-E "
                                  << maxDeltaE << ")"
                                  << std::endl;
                        result.cmykPreserved++;
                        result.imagesSkipped++;
                        continue;
                    }

                    auto buffer = std::make_shared<Buffer>(compressedBytes.size());
                    memcpy(buffer->getBuffer(), compressedBytes.data(), compressedBytes.size());
                    imageObj.replaceStreamData(
                        buffer,
                        QPDFObjectHandle::newName("/DCTDecode"),
                        QPDFObjectHandle::newNull()
                    );

                    // The image is now 3-channel RGB; drop CMYK-only keys.
                    dict.replaceKey("/Filter", QPDFObjectHandle::newName("/DCTDecode"));
                    dict.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceRGB"));
                    if (dict.hasKey("/Decode")) dict.removeKey("/Decode");
                    if (dict.hasKey("/DecodeParms")) dict.removeKey("/DecodeParms");

                    if (originalImageBytes > compressedBytes.size()) {
                        result.imageBytesSaved += originalImageBytes - compressedBytes.size();
                    }
                    result.transcodedCmyk++;
                    result.imagesOptimized++;

                    std::cerr << "PDFOptimizer: transcoded image obj "
                              << imageObj.getObjGen().getObj()
                              << " CMYK -> DeviceRGB (max delta-E " << maxDeltaE << ")"
                              << std::endl;
                    continue;
                }

                std::vector<uint8_t> rawPixels;
                ImageMetadata meta;
                meta.hasAlpha = hasAlpha;
                meta.colorSpace = colorSpace;
                meta.bitsPerComponent = bitsPerComponent;

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
                    auto compressedBytes = m_decisionEngine.compress(rawPixels.data(), width, height, channels, analysis, outFilter, options.qualityHint, hasAlpha, bitsPerComponent, colorSpace);

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
                // AC-C5: streams only merge when their filter pipeline matches,
                // so differing /Filter or /DecodeParms never collapse.
                if (dict.hasKey("/Filter")) sig += dict.getKey("/Filter").unparse() + ";";
                if (dict.hasKey("/DecodeParms")) sig += dict.getKey("/DecodeParms").unparse() + ";";
                // ESC-015 / AC-C2: a soft or color-key mask changes how the
                // bytes are composited, and /Type /Intent distinguish the
                // stream's role. Two byte-identical images that differ on any
                // of these must never merge - merging would repoint referrers
                // and silently change transparency. Any differing key blocks
                // the merge; a pair matching on all of them still dedups.
                if (dict.hasKey("/SMask")) sig += dict.getKey("/SMask").unparse() + ";";
                if (dict.hasKey("/Mask")) sig += dict.getKey("/Mask").unparse() + ";";
                if (dict.hasKey("/Type")) sig += dict.getKey("/Type").unparse() + ";";
                if (dict.hasKey("/Intent")) sig += dict.getKey("/Intent").unparse() + ";";

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
                        // AC-C5: PDF streams are always indirect and
                        // QPDF::replaceObject rejects indirect handles, so the
                        // duplicate cannot be aliased by replacing it. Instead
                        // repoint every reference from the duplicate to the kept
                        // byte-identical stream; the duplicate then drops under
                        // setPreserveUnreferencedObjects(false). Keep the
                        // per-pair try/catch so one bad pair never fails a file.
                        try {
                            std::size_t repointed =
                                ReferenceRewriter::repointAll(pdf, obj.getObjGen(), it->second);
                            result.streamsDeduplicated += static_cast<int>(repointed);
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
