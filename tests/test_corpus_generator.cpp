#include "test_corpus_generator.h"
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <fstream>
#include <iostream>
#include <zlib.h>
#include <turbojpeg.h>
#include <stdexcept>

namespace pdfcompress::test {

TestCorpusGenerator::TestCorpusGenerator() {
    m_dir = std::filesystem::temp_directory_path() / "pdfcompress_test_corpus";
    std::filesystem::create_directories(m_dir);
}

TestCorpusGenerator::~TestCorpusGenerator() {}

std::string TestCorpusGenerator::generateTextOnly() {
    std::string path = (m_dir / "text_only.pdf").string();
    QPDF pdf;
    pdf.emptyPDF();
    QPDFObjectHandle font = QPDFObjectHandle::parse("<< /Type /Font /Subtype /Type1 /Name /F1 /BaseFont /Helvetica >>");
    QPDFObjectHandle page = QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
    page.replaceKey("/Resources", QPDFObjectHandle::parse("<< >>"));
    page.getKey("/Resources").replaceKey("/Font", QPDFObjectHandle::parse("<< >>"));
    page.getKey("/Resources").getKey("/Font").replaceKey("/F1", font);
    QPDFObjectHandle contents = QPDFObjectHandle::newStream(&pdf, "BT /F1 12 Tf 72 720 Td (Hello World) Tj ET\n");
    page.replaceKey("/Contents", contents);
    QPDFPageDocumentHelper(pdf).addPage(page, true);
    QPDFWriter writer(pdf, path.c_str());
    writer.setDeterministicID(true);
    writer.setStaticID(true);
    writer.write();
    return path;
}

std::string TestCorpusGenerator::createPdfWithImage(
    const std::string& name, const std::vector<uint8_t>& pixelData,
    int width, int height, int channels,
    const std::string& colorSpace, const std::string& filter) {
    
    std::string path = (m_dir / (name + ".pdf")).string();
    QPDF pdf;
    pdf.emptyPDF();
    
    // Create image stream
    QPDFObjectHandle imageStream = QPDFObjectHandle::newStream(&pdf);
    QPDFObjectHandle dict = imageStream.getDict();
    dict.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
    dict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
    dict.replaceKey("/Width", QPDFObjectHandle::newInteger(width));
    dict.replaceKey("/Height", QPDFObjectHandle::newInteger(height));
    dict.replaceKey("/ColorSpace", QPDFObjectHandle::newName(colorSpace));
    dict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
    
    dict.replaceKey("/Filter", QPDFObjectHandle::newName(filter));
    
    std::string streamStr;
    if (filter == "/FlateDecode") {
        uLongf compSize = compressBound(pixelData.size());
        std::vector<uint8_t> compData(compSize);
        if (compress(compData.data(), &compSize, pixelData.data(), pixelData.size()) == Z_OK) {
            streamStr.assign(reinterpret_cast<char*>(compData.data()), compSize);
        } else {
            streamStr.assign(reinterpret_cast<const char*>(pixelData.data()), pixelData.size());
        }
    } else {
        streamStr.assign(reinterpret_cast<const char*>(pixelData.data()), pixelData.size());
    }

    // Set stream data
    imageStream.replaceStreamData(
        streamStr, 
        QPDFObjectHandle::newName(filter), 
        QPDFObjectHandle::newNull()
    );

    QPDFObjectHandle page = QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
    // Provide Helvetica font resource for determinism even if not used
    QPDFObjectHandle font = QPDFObjectHandle::parse("<< /Type /Font /Subtype /Type1 /Name /F1 /BaseFont /Helvetica >>");
    page.replaceKey("/Resources", QPDFObjectHandle::parse("<< /XObject << >> /Font << >> >>"));
    page.getKey("/Resources").getKey("/XObject").replaceKey("/Im1", imageStream);
    page.getKey("/Resources").getKey("/Font").replaceKey("/F1", font);
    QPDFObjectHandle contents = QPDFObjectHandle::newStream(&pdf, "q 100 0 0 100 72 600 cm /Im1 Do Q\n");
    page.replaceKey("/Contents", contents);
    QPDFPageDocumentHelper(pdf).addPage(page, true);
    
    QPDFWriter writer(pdf, path.c_str());
    writer.setDeterministicID(true);
    writer.setStaticID(true);
    writer.write();
    return path;
}

std::string TestCorpusGenerator::generatePhotoJpeg() {
    // ESC-002 (rework): 200x150 DeviceRGB DCTDecode with valid JPEG bytes via
    // TurboJPEG so the optimizer /DCTDecode branch (PDFOptimizer.cpp:244-258)
    // is exercised. Deterministic: fixed pixels + fixed quality/subsampling.
    const int width = 200, height = 150;
    std::vector<uint8_t> pixels(static_cast<size_t>(width * height * 3));
    for (size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<uint8_t>((i * 3) % 256);

    tjhandle tj = tjInitCompress();
    if (!tj) throw std::runtime_error("generatePhotoJpeg: tjInitCompress failed");
    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;
    int rc = tjCompress2(tj, pixels.data(), width, 0, height, TJPF_RGB,
                         &jpegBuf, &jpegSize, TJSAMP_420, 75, 0);
    if (rc != 0) {
        std::string err = tjGetErrorStr2(tj);
        tjDestroy(tj);
        throw std::runtime_error("generatePhotoJpeg: tjCompress2 failed: " + err);
    }
    std::string jpegStr(reinterpret_cast<char*>(jpegBuf), jpegSize);
    tjFree(jpegBuf);
    tjDestroy(tj);

    std::string path = (m_dir / "photo_jpeg.pdf").string();
    QPDF pdf;
    pdf.emptyPDF();
    QPDFObjectHandle imageStream = QPDFObjectHandle::newStream(&pdf);
    QPDFObjectHandle dict = imageStream.getDict();
    dict.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
    dict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
    dict.replaceKey("/Width", QPDFObjectHandle::newInteger(width));
    dict.replaceKey("/Height", QPDFObjectHandle::newInteger(height));
    dict.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceRGB"));
    dict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
    dict.replaceKey("/Filter", QPDFObjectHandle::newName("/DCTDecode"));
    imageStream.replaceStreamData(
        jpegStr,
        QPDFObjectHandle::newName("/DCTDecode"),
        QPDFObjectHandle::newNull());

    QPDFObjectHandle font = QPDFObjectHandle::parse("<< /Type /Font /Subtype /Type1 /Name /F1 /BaseFont /Helvetica >>");
    QPDFObjectHandle page = QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
    page.replaceKey("/Resources", QPDFObjectHandle::parse("<< /XObject << >> /Font << >> >>"));
    page.getKey("/Resources").getKey("/XObject").replaceKey("/Im1", imageStream);
    page.getKey("/Resources").getKey("/Font").replaceKey("/F1", font);
    QPDFObjectHandle contents = QPDFObjectHandle::newStream(&pdf, "q 100 0 0 100 72 600 cm /Im1 Do Q\n");
    page.replaceKey("/Contents", contents);
    QPDFPageDocumentHelper(pdf).addPage(page, true);

    QPDFWriter writer(pdf, path.c_str());
    writer.setDeterministicID(true);
    writer.setStaticID(true);
    writer.write();
    return path;
}

std::string TestCorpusGenerator::generatePhotoHeavy() {
    std::string path = (m_dir / "photo_heavy.pdf").string();
    QPDF pdf;
    pdf.emptyPDF();
    QPDFObjectHandle font = QPDFObjectHandle::parse("<< /Type /Font /Subtype /Type1 /Name /F1 /BaseFont /Helvetica >>");
    // Create 3 distinct images with fixed dimensions and distinct pixel content
    struct Img { int w,h; std::vector<uint8_t> pixels; };
    std::vector<Img> imgs;
    imgs.push_back({300, 200, std::vector<uint8_t>(300*200*3)});
    imgs.push_back({200, 150, std::vector<uint8_t>(200*150*3)});
    imgs.push_back({250, 180, std::vector<uint8_t>(250*180*3)});
    for (size_t k=0;k<imgs.size();++k) {
        for (size_t i=0;i<imgs[k].pixels.size();++i) {
            uint8_t base = static_cast<uint8_t>((k*80) % 256);
            imgs[k].pixels[i] = static_cast<uint8_t>((base + i*7) % 256);
        }
    }
    QPDFPageDocumentHelper helper(pdf);
    for (size_t k=0;k<imgs.size();++k) {
        int w = imgs[k].w;
        int h = imgs[k].h;
        auto& pix = imgs[k].pixels;
        uLongf compSize = compressBound(pix.size());
        std::vector<uint8_t> comp(compSize);
        compress(comp.data(), &compSize, pix.data(), pix.size());
        std::string str(reinterpret_cast<char*>(comp.data()), compSize);
        QPDFObjectHandle img = QPDFObjectHandle::newStream(&pdf);
        QPDFObjectHandle d = img.getDict();
        d.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
        d.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
        d.replaceKey("/Width", QPDFObjectHandle::newInteger(w));
        d.replaceKey("/Height", QPDFObjectHandle::newInteger(h));
        d.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceRGB"));
        d.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
        d.replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));
        img.replaceStreamData(str, QPDFObjectHandle::newName("/FlateDecode"), QPDFObjectHandle::newNull());
        std::string imName = "/Im" + std::to_string(k+1);
        QPDFObjectHandle page = QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
        page.replaceKey("/Resources", QPDFObjectHandle::parse("<< /XObject << >> /Font << >> >>"));
        page.getKey("/Resources").getKey("/XObject").replaceKey(imName, img);
        page.getKey("/Resources").getKey("/Font").replaceKey("/F1", font);
        std::string content = "q 100 0 0 100 72 600 cm " + imName + " Do Q\n";
        QPDFObjectHandle contents = QPDFObjectHandle::newStream(&pdf, content);
        page.replaceKey("/Contents", contents);
        helper.addPage(page, true);
    }
    QPDFWriter writer(pdf, path.c_str());
    writer.setDeterministicID(true);
    writer.setStaticID(true);
    writer.write();
    return path;
}

std::string TestCorpusGenerator::generateMultiImage() {
    return generatePhotoHeavy();
}

std::string TestCorpusGenerator::generateScreenshotFlat() {
    // Flat-color quadrants with fixed pixel buffers
    int width = 400, height = 300;
    std::vector<uint8_t> pixels(width * height * 3);
    for (int y=0;y<height;++y) {
        for (int x=0;x<width;++x) {
            int idx = (y*width + x)*3;
            if (x < 200 && y < 150) { pixels[idx]=200; pixels[idx+1]=200; pixels[idx+2]=255; }
            else if (x >=200 && y <150) { pixels[idx]=255; pixels[idx+1]=200; pixels[idx+2]=200; }
            else if (x <200 && y >=150) { pixels[idx]=200; pixels[idx+1]=255; pixels[idx+2]=200; }
            else { pixels[idx]=255; pixels[idx+1]=255; pixels[idx+2]=200; }
        }
    }
    return createPdfWithImage("screenshot_flat", pixels, width, height, 3, "/DeviceRGB", "/FlateDecode");
}

std::string TestCorpusGenerator::generateMonochromeBW() {
    std::vector<uint8_t> pixels(100 * 100 * 1, 0);
    for (size_t i=0;i<pixels.size();++i) pixels[i] = (i % 2 ==0 ? 0 : 255);
    return createPdfWithImage("monochrome_bw", pixels, 100, 100, 1, "/DeviceGray", "/FlateDecode");
}

std::string TestCorpusGenerator::generateGrayscaleScan() {
    std::vector<uint8_t> pixels(300 * 400 * 1);
    for (size_t i=0;i<pixels.size();++i) pixels[i] = static_cast<uint8_t>((i % 256));
    return createPdfWithImage("grayscale_scan", pixels, 300, 400, 1, "/DeviceGray", "/FlateDecode");
}

std::string TestCorpusGenerator::generateLineArt() {
    int width = 300, height = 300;
    std::vector<uint8_t> pixels(width * height * 3);
    // High edge-density quadrants with limited palette (at least 2 distinct colors)
    for (int y=0; y<height; ++y) {
        for (int x=0; x<width; ++x) {
            int idx = (y*width + x)*3;
            uint8_t r,g,b;
            if (x < 150 && y < 150) { r=0; g=0; b=0; }
            else if (x >=150 && y <150) { r=255; g=255; b=255; }
            else if (x <150 && y >=150) { r=255; g=0; b=0; }
            else { r=0; g=0; b=255; }
            // Add high edge density via grid lines every 20px
            if (x % 20 == 0 || y % 20 == 0) { r=0; g=0; b=0; }
            // Diagonal edge
            if ((x + y) % 30 == 0) { r=255; g=255; b=0; }
            pixels[idx]=r; pixels[idx+1]=g; pixels[idx+2]=b;
        }
    }
    return createPdfWithImage("line_art", pixels, width, height, 3, "/DeviceRGB", "/FlateDecode");
}

std::string TestCorpusGenerator::generateTransparency() {
    std::string path = (m_dir / "transparency.pdf").string();
    QPDF pdf;
    pdf.emptyPDF();
    QPDFObjectHandle font = QPDFObjectHandle::parse("<< /Type /Font /Subtype /Type1 /Name /F1 /BaseFont /Helvetica >>");
    // Create shared image XObject with SMask
    int w=200, h=150;
    std::vector<uint8_t> pixels(w*h*3);
    for (size_t i=0;i<pixels.size();++i) pixels[i]= static_cast<uint8_t>((i*5) % 256);
    uLongf compSize = compressBound(pixels.size());
    std::vector<uint8_t> comp(compSize);
    compress(comp.data(), &compSize, pixels.data(), pixels.size());
    std::string imgData(reinterpret_cast<char*>(comp.data()), compSize);
    QPDFObjectHandle imageStream = QPDFObjectHandle::newStream(&pdf);
    QPDFObjectHandle idict = imageStream.getDict();
    idict.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
    idict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
    idict.replaceKey("/Width", QPDFObjectHandle::newInteger(w));
    idict.replaceKey("/Height", QPDFObjectHandle::newInteger(h));
    idict.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceRGB"));
    idict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
    idict.replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));
    // SMask soft mask with alpha channel
    int smW=w, smH=h;
    std::vector<uint8_t> alpha(smW*smH);
    for (int y=0;y<smH;++y) for (int x=0;x<smW;++x) alpha[y*smW + x]= static_cast<uint8_t>((x*255)/smW);
    uLongf aCompSize = compressBound(alpha.size());
    std::vector<uint8_t> aComp(aCompSize);
    compress(aComp.data(), &aCompSize, alpha.data(), alpha.size());
    std::string aStr(reinterpret_cast<char*>(aComp.data()), aCompSize);
    QPDFObjectHandle smask = QPDFObjectHandle::newStream(&pdf);
    QPDFObjectHandle sdict = smask.getDict();
    sdict.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
    sdict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
    sdict.replaceKey("/Width", QPDFObjectHandle::newInteger(smW));
    sdict.replaceKey("/Height", QPDFObjectHandle::newInteger(smH));
    sdict.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceGray"));
    sdict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
    sdict.replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));
    smask.replaceStreamData(aStr, QPDFObjectHandle::newName("/FlateDecode"), QPDFObjectHandle::newNull());
    // SMask dict with /S /Alpha
    QPDFObjectHandle smaskRef = pdf.makeIndirectObject(smask);
    QPDFObjectHandle smaskDict = QPDFObjectHandle::parse("<< /S /Alpha /G << /S /Transparency >> >>");
    // Actually SMask is the stream itself with /SMask entry on image; set image's SMask to the stream
    // Simplify: imageStream has /SMask pointing to smask stream
    imageStream.replaceStreamData(imgData, QPDFObjectHandle::newName("/FlateDecode"), QPDFObjectHandle::newNull());
    // After setting data, add SMask key (must keep indirect)
    idict.replaceKey("/SMask", smaskRef);
    // Also add /SMask << /S /Alpha >> style: ensure SMask dict contains /S /Alpha
    // The smask stream dict should indicate Alpha; we already have indirect object, qpdf json will show SMask
    // Make shared image indirect
    QPDFObjectHandle sharedImage = pdf.makeIndirectObject(imageStream);
    QPDFPageDocumentHelper helper(pdf);
    for (int p=0;p<2;++p) {
        QPDFObjectHandle page = QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
        page.replaceKey("/Resources", QPDFObjectHandle::parse("<< /XObject << >> /Font << >> >>"));
        page.getKey("/Resources").getKey("/XObject").replaceKey("/Im1", sharedImage);
        page.getKey("/Resources").getKey("/Font").replaceKey("/F1", font);
        // Group transparency
        page.replaceKey("/Group", QPDFObjectHandle::parse("<< /S /Transparency /CS /DeviceRGB >>"));
        std::string content = "q 100 0 0 100 72 600 cm /Im1 Do Q\nBT /F1 12 Tf 72 520 Td (Transparency test) Tj ET\n";
        QPDFObjectHandle contents = QPDFObjectHandle::newStream(&pdf, content);
        page.replaceKey("/Contents", contents);
        helper.addPage(page, true);
    }
    QPDFWriter writer(pdf, path.c_str());
    writer.setDeterministicID(true);
    writer.setStaticID(true);
    writer.write();
    return path;
}

std::string TestCorpusGenerator::generateDistinctDuplicateStreams() {
    // AC-C5 staged fixture: two DISTINCT indirect image streams carrying
    // byte-identical raw data with matching dict signatures (including /Filter
    // and /DecodeParms), both actually referenced and drawn. Separate from the
    // reference-shared transparency.pdf and never added to the canonical 14.
    std::string path = (m_dir / "distinct_duplicate_streams.pdf").string();
    QPDF pdf;
    pdf.emptyPDF();

    const int w = 64, h = 64;
    std::vector<uint8_t> pixels(static_cast<size_t>(w) * h * 3);
    for (size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<uint8_t>((i * 7) % 256);

    // Compress once and reuse the exact same raw bytes for both streams.
    uLongf compSize = compressBound(pixels.size());
    std::vector<uint8_t> comp(compSize);
    compress(comp.data(), &compSize, pixels.data(), pixels.size());
    std::string imgData(reinterpret_cast<char*>(comp.data()), compSize);

    auto makeImage = [&]() {
        QPDFObjectHandle stream = QPDFObjectHandle::newStream(&pdf);
        QPDFObjectHandle d = stream.getDict();
        d.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
        d.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
        d.replaceKey("/Width", QPDFObjectHandle::newInteger(w));
        d.replaceKey("/Height", QPDFObjectHandle::newInteger(h));
        d.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceRGB"));
        d.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
        d.replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));
        stream.replaceStreamData(
            imgData,
            QPDFObjectHandle::newName("/FlateDecode"),
            QPDFObjectHandle::newNull());
        return pdf.makeIndirectObject(stream);
    };

    QPDFObjectHandle imgA = makeImage();
    QPDFObjectHandle imgB = makeImage();

    QPDFObjectHandle font = QPDFObjectHandle::parse("<< /Type /Font /Subtype /Type1 /Name /F1 /BaseFont /Helvetica >>");
    QPDFObjectHandle page = QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
    page.replaceKey("/Resources", QPDFObjectHandle::parse("<< /XObject << >> /Font << >> >>"));
    page.getKey("/Resources").getKey("/XObject").replaceKey("/Im1", imgA);
    page.getKey("/Resources").getKey("/XObject").replaceKey("/Im2", imgB);
    page.getKey("/Resources").getKey("/Font").replaceKey("/F1", font);
    QPDFObjectHandle contents = QPDFObjectHandle::newStream(
        &pdf,
        "q 100 0 0 100 72 600 cm /Im1 Do Q\nq 100 0 0 100 200 600 cm /Im2 Do Q\n");
    page.replaceKey("/Contents", contents);
    QPDFPageDocumentHelper(pdf).addPage(page, true);

    QPDFWriter writer(pdf, path.c_str());
    writer.setDeterministicID(true);
    writer.setStaticID(true);
    writer.write();
    return path;
}

std::string TestCorpusGenerator::generateMaskedDuplicateStreams() {
    // ESC-015 / AC-C2 regression fixture: two DISTINCT indirect image streams
    // carrying byte-identical raw data and matching on every legacy signature
    // key (/Subtype /Width /Height /ColorSpace /BitsPerComponent /Filter
    // /DecodeParms), where one additionally carries an /SMask. The mask changes
    // how the pixels are composited, so the pair must NOT be deduplicated -
    // merging would repoint referrers and silently drop transparency.
    //
    // /BitsPerComponent 1 is deliberate: PDFOptimizer leaves bit-packed images
    // untouched (it cannot re-encode them), so both streams keep their identical
    // raw bytes through the image pass and the only thing that can prevent the
    // dedup merge is the added /SMask signature key.
    std::string path = (m_dir / "masked_duplicate_streams.pdf").string();
    QPDF pdf;
    pdf.emptyPDF();

    const int w = 64, h = 64;
    // 1-bit packed samples: 64*64/8 = 512 bytes, identical for both images.
    std::vector<uint8_t> bits(static_cast<size_t>(w) * h / 8);
    for (size_t i = 0; i < bits.size(); ++i) bits[i] = static_cast<uint8_t>((i * 13 + 7) % 256);

    uLongf compSize = compressBound(bits.size());
    std::vector<uint8_t> comp(compSize);
    compress(comp.data(), &compSize, bits.data(), bits.size());
    std::string imgData(reinterpret_cast<char*>(comp.data()), compSize);

    // Soft mask stream: 8-bit DeviceGray alpha plane of the same dimensions.
    std::vector<uint8_t> alpha(static_cast<size_t>(w) * h);
    for (size_t i = 0; i < alpha.size(); ++i) alpha[i] = static_cast<uint8_t>((i * 3) % 256);
    uLongf aCompSize = compressBound(alpha.size());
    std::vector<uint8_t> aComp(aCompSize);
    compress(aComp.data(), &aCompSize, alpha.data(), alpha.size());
    std::string aStr(reinterpret_cast<char*>(aComp.data()), aCompSize);

    QPDFObjectHandle smask = QPDFObjectHandle::newStream(&pdf);
    QPDFObjectHandle sdict = smask.getDict();
    sdict.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
    sdict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
    sdict.replaceKey("/Width", QPDFObjectHandle::newInteger(w));
    sdict.replaceKey("/Height", QPDFObjectHandle::newInteger(h));
    sdict.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceGray"));
    sdict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
    sdict.replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));
    smask.replaceStreamData(aStr, QPDFObjectHandle::newName("/FlateDecode"), QPDFObjectHandle::newNull());
    QPDFObjectHandle smaskRef = pdf.makeIndirectObject(smask);

    auto makeImage = [&]() {
        QPDFObjectHandle stream = QPDFObjectHandle::newStream(&pdf);
        QPDFObjectHandle d = stream.getDict();
        d.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
        d.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
        d.replaceKey("/Width", QPDFObjectHandle::newInteger(w));
        d.replaceKey("/Height", QPDFObjectHandle::newInteger(h));
        d.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceGray"));
        d.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(1));
        d.replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));
        stream.replaceStreamData(
            imgData,
            QPDFObjectHandle::newName("/FlateDecode"),
            QPDFObjectHandle::newNull());
        return pdf.makeIndirectObject(stream);
    };

    QPDFObjectHandle imgMasked = makeImage();
    QPDFObjectHandle imgPlain = makeImage();
    // Only the masked stream carries /SMask; the raw bytes stay identical.
    imgMasked.getDict().replaceKey("/SMask", smaskRef);

    QPDFObjectHandle font = QPDFObjectHandle::parse("<< /Type /Font /Subtype /Type1 /Name /F1 /BaseFont /Helvetica >>");
    QPDFObjectHandle page = QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
    page.replaceKey("/Resources", QPDFObjectHandle::parse("<< /XObject << >> /Font << >> >>"));
    page.getKey("/Resources").getKey("/XObject").replaceKey("/Im1", imgMasked);
    page.getKey("/Resources").getKey("/XObject").replaceKey("/Im2", imgPlain);
    page.getKey("/Resources").getKey("/Font").replaceKey("/F1", font);
    QPDFObjectHandle contents = QPDFObjectHandle::newStream(
        &pdf,
        "q 100 0 0 100 72 600 cm /Im1 Do Q\nq 100 0 0 100 200 600 cm /Im2 Do Q\n");
    page.replaceKey("/Contents", contents);
    QPDFPageDocumentHelper(pdf).addPage(page, true);

    QPDFWriter writer(pdf, path.c_str());
    writer.setDeterministicID(true);
    writer.setStaticID(true);
    writer.write();
    return path;
}

std::string TestCorpusGenerator::generateEncrypted() {
    std::string path = (m_dir / "encrypted.pdf").string();
    QPDF pdf;
    pdf.emptyPDF();
    QPDFObjectHandle font = QPDFObjectHandle::parse("<< /Type /Font /Subtype /Type1 /Name /F1 /BaseFont /Helvetica >>");
    QPDFObjectHandle page = QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
    page.replaceKey("/Resources", QPDFObjectHandle::parse("<< >>"));
    page.getKey("/Resources").replaceKey("/Font", QPDFObjectHandle::parse("<< >>"));
    page.getKey("/Resources").getKey("/Font").replaceKey("/F1", font);
    QPDFObjectHandle contents = QPDFObjectHandle::newStream(&pdf, "BT /F1 12 Tf 72 720 Td (Encrypted Hello) Tj ET\n");
    page.replaceKey("/Contents", contents);
    QPDFPageDocumentHelper(pdf).addPage(page, true);
    QPDFWriter writer(pdf, path.c_str());
    writer.setDeterministicID(false);
    writer.setStaticID(true);
    writer.setStaticAesIV(true);
    // Use R6 AES-128 with user password test123, no owner password
    try {
        writer.setR6EncryptionParameters("test123", "", true, true, true, true, true, true, qpdf_r3p_full, true);
    } catch (...) {
        writer.setR3EncryptionParametersInsecure("test123", "", true, true, true, true, true, true, qpdf_r3p_full);
    }
    writer.write();
    return path;
}

std::string TestCorpusGenerator::generateWithForm() {
    std::string path = (m_dir / "with_form.pdf").string();
    QPDF pdf;
    pdf.emptyPDF();
    QPDFObjectHandle font = QPDFObjectHandle::parse("<< /Type /Font /Subtype /Type1 /Name /F1 /BaseFont /Helvetica >>");
    QPDFObjectHandle page = QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
    page.replaceKey("/Resources", QPDFObjectHandle::parse("<< >>"));
    page.getKey("/Resources").replaceKey("/Font", QPDFObjectHandle::parse("<< >>"));
    page.getKey("/Resources").getKey("/Font").replaceKey("/F1", font);
    QPDFObjectHandle contents = QPDFObjectHandle::newStream(&pdf, "BT /F1 12 Tf 72 720 Td (Form test) Tj ET\n");
    page.replaceKey("/Contents", contents);
    // Widget annotation
    QPDFObjectHandle widget = QPDFObjectHandle::parse("<< /Type /Annot /Subtype /Widget /FT /Tx /T (TextField1) /Rect [72 700 200 720] /F 4 /BS << /W 1 >> >>");
    widget = pdf.makeIndirectObject(widget);
    page.replaceKey("/Annots", QPDFObjectHandle::newArray());
    page.getKey("/Annots").appendItem(widget);
    QPDFPageDocumentHelper(pdf).addPage(page, true);
    // AcroForm with Fields
    QPDFObjectHandle acroForm = QPDFObjectHandle::parse("<< /Fields [] >>");
    acroForm.replaceKey("/Fields", QPDFObjectHandle::newArray());
    acroForm.getKey("/Fields").appendItem(widget);
    pdf.getRoot().replaceKey("/AcroForm", acroForm);
    QPDFWriter writer(pdf, path.c_str());
    writer.setDeterministicID(true);
    writer.setStaticID(true);
    writer.write();
    return path;
}

std::string TestCorpusGenerator::generateWithMetadata() {
    std::string path = (m_dir / "with_metadata.pdf").string();
    QPDF pdf;
    pdf.emptyPDF();
    QPDFObjectHandle font = QPDFObjectHandle::parse("<< /Type /Font /Subtype /Type1 /Name /F1 /BaseFont /Helvetica >>");
    QPDFObjectHandle page = QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
    page.replaceKey("/Resources", QPDFObjectHandle::parse("<< >>"));
    page.getKey("/Resources").replaceKey("/Font", QPDFObjectHandle::parse("<< >>"));
    page.getKey("/Resources").getKey("/Font").replaceKey("/F1", font);
    QPDFObjectHandle contents = QPDFObjectHandle::newStream(&pdf, "BT /F1 12 Tf 72 720 Td (Metadata test) Tj ET\n");
    page.replaceKey("/Contents", contents);
    // Thumb placeholder
    page.replaceKey("/Thumb", QPDFObjectHandle::newNull());
    QPDFPageDocumentHelper(pdf).addPage(page, true);
    // Info dict
    QPDFObjectHandle info = QPDFObjectHandle::parse("<< /Producer (TestProducer) /Title (Test Title) /Creator (pdfcompress) >>");
    info = pdf.makeIndirectObject(info);
    pdf.getTrailer().replaceKey("/Info", info);
    // Metadata XMP stream
    std::string xmp = "<?xpacket begin='\\xef\\xbb\\xbf' id='W5M0MpCehiHzreSzNTczkc9d'?><x:xmpmeta xmlns:x='adobe:ns:meta/'><rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'><rdf:Description rdf:about='' xmlns:dc='http://purl.org/dc/elements/1.1/'><dc:title><rdf:Alt><rdf:li xml:lang='x-default'>Test</rdf:li></rdf:Alt></dc:title></rdf:Description></rdf:RDF></x:xmpmeta><?xpacket end='w'?>";
    QPDFObjectHandle metadata = QPDFObjectHandle::newStream(&pdf, xmp);
    QPDFObjectHandle mdict = metadata.getDict();
    mdict.replaceKey("/Type", QPDFObjectHandle::newName("/Metadata"));
    mdict.replaceKey("/Subtype", QPDFObjectHandle::newName("/XML"));
    pdf.getRoot().replaceKey("/Metadata", pdf.makeIndirectObject(metadata));
    // PieceInfo
    pdf.getRoot().replaceKey("/PieceInfo", QPDFObjectHandle::parse("<< /TestPiece << /Private << >> >> >>"));
    QPDFWriter writer(pdf, path.c_str());
    writer.setDeterministicID(true);
    writer.setStaticID(true);
    writer.write();
    return path;
}

std::string TestCorpusGenerator::generateWithJavaScript() {
    std::string path = (m_dir / "with_javascript.pdf").string();
    QPDF pdf;
    pdf.emptyPDF();
    QPDFObjectHandle font = QPDFObjectHandle::parse("<< /Type /Font /Subtype /Type1 /Name /F1 /BaseFont /Helvetica >>");
    QPDFObjectHandle page = QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
    page.replaceKey("/Resources", QPDFObjectHandle::parse("<< >>"));
    page.getKey("/Resources").replaceKey("/Font", QPDFObjectHandle::parse("<< >>"));
    page.getKey("/Resources").getKey("/Font").replaceKey("/F1", font);
    QPDFObjectHandle contents = QPDFObjectHandle::newStream(&pdf, "BT /F1 12 Tf 72 720 Td (JavaScript test) Tj ET\n");
    page.replaceKey("/Contents", contents);
    QPDFPageDocumentHelper(pdf).addPage(page, true);
    // JavaScript via Names and OpenAction
    QPDFObjectHandle jsAction = QPDFObjectHandle::parse("<< /S /JavaScript /JS (app.alert('Hello from PDF');) >>");
    pdf.getRoot().replaceKey("/OpenAction", jsAction);
    QPDFObjectHandle jsName = QPDFObjectHandle::parse("<< /S /JavaScript /JS (app.alert('Embedded JS');) >>");
    QPDFObjectHandle names = QPDFObjectHandle::parse("<< /JavaScript << /Names [] >> >>");
    QPDFObjectHandle jsArray = QPDFObjectHandle::newArray();
    jsArray.appendItem(QPDFObjectHandle::newString("EmbeddedJS"));
    jsArray.appendItem(jsName);
    names.getKey("/JavaScript").replaceKey("/Names", jsArray);
    pdf.getRoot().replaceKey("/Names", names);
    QPDFWriter writer(pdf, path.c_str());
    writer.setDeterministicID(true);
    writer.setStaticID(true);
    writer.write();
    return path;
}

std::string TestCorpusGenerator::generateSharedXObject() {
    return generateTransparency();
}

std::string TestCorpusGenerator::generateMergedDuplicateFonts() {
    return generateSharedXObject();
}

std::string TestCorpusGenerator::generateWithBookmarks() {
    return generateWithBookmarksAndLinks();
}

std::string TestCorpusGenerator::generateWithAnnotations() {
    // Keep for backwards compat but delegate to bookmarks/links for completeness
    return generateWithBookmarksAndLinks();
}

std::string TestCorpusGenerator::generateWithBookmarksAndLinks() {
    std::string path = (m_dir / "with_bookmarks_and_links.pdf").string();
    QPDF pdf;
    pdf.emptyPDF();
    QPDFObjectHandle font = QPDFObjectHandle::parse("<< /Type /Font /Subtype /Type1 /Name /F1 /BaseFont /Helvetica >>");
    QPDFObjectHandle page = QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
    page.replaceKey("/Resources", QPDFObjectHandle::parse("<< >>"));
    page.getKey("/Resources").replaceKey("/Font", QPDFObjectHandle::parse("<< >>"));
    page.getKey("/Resources").getKey("/Font").replaceKey("/F1", font);
    QPDFObjectHandle contents = QPDFObjectHandle::newStream(&pdf, "BT /F1 12 Tf 72 720 Td (Bookmarks and Links) Tj ET\n");
    page.replaceKey("/Contents", contents);
    // Annots
    QPDFObjectHandle annot = QPDFObjectHandle::parse("<< /Type /Annot /Subtype /Link /Rect [72 700 200 720] /Border [0 0 0] /A << /S /GoTo /D (Chapter1) >> >>");
    page.replaceKey("/Annots", QPDFObjectHandle::newArray());
    page.getKey("/Annots").appendItem(annot);
    // Second annot Text
    QPDFObjectHandle textAnnot = QPDFObjectHandle::parse("<< /Type /Annot /Subtype /Text /Rect [72 680 92 700] /Contents (Note) /Name /Comment >>");
    page.getKey("/Annots").appendItem(textAnnot);
    QPDFPageDocumentHelper(pdf).addPage(page, true);
    // Outlines
    QPDFObjectHandle outlines = QPDFObjectHandle::parse("<< /Type /Outlines /Count 1 >>");
    QPDFObjectHandle first = QPDFObjectHandle::parse("<< /Title (Chapter 1) /Dest [0 /Fit] >>");
    outlines.replaceKey("/First", first);
    outlines.replaceKey("/Last", first);
    pdf.getRoot().replaceKey("/Outlines", outlines);
    // Dests
    pdf.getRoot().replaceKey("/Dests", QPDFObjectHandle::parse("<< /Chapter1 [0 /Fit] >>"));
    QPDFWriter writer(pdf, path.c_str());
    writer.setDeterministicID(true);
    writer.setStaticID(true);
    writer.write();
    return path;
}

std::string TestCorpusGenerator::generateCmykImage() {
    int w=100, h=100;
    std::vector<uint8_t> pixels(w * h * 4);
    for (int y=0;y<h;++y) {
        for (int x=0;x<w;++x) {
            int idx=(y*w+x)*4;
            pixels[idx]= static_cast<uint8_t>((x*255)/w);
            pixels[idx+1]= static_cast<uint8_t>((y*255)/h);
            pixels[idx+2]= 100;
            pixels[idx+3]= 50;
        }
    }
    // Use FlateDecode with valid compressed bytes via compress()
    uLongf compSize = compressBound(pixels.size());
    std::vector<uint8_t> comp(compSize);
    compress(comp.data(), &compSize, pixels.data(), pixels.size());
    std::string streamStr(reinterpret_cast<char*>(comp.data()), compSize);
    std::string path = (m_dir / "cmyk_image.pdf").string();
    QPDF pdf;
    pdf.emptyPDF();
    QPDFObjectHandle imageStream = QPDFObjectHandle::newStream(&pdf);
    QPDFObjectHandle dict = imageStream.getDict();
    dict.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
    dict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
    dict.replaceKey("/Width", QPDFObjectHandle::newInteger(w));
    dict.replaceKey("/Height", QPDFObjectHandle::newInteger(h));
    dict.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceCMYK"));
    dict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
    dict.replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));
    imageStream.replaceStreamData(streamStr, QPDFObjectHandle::newName("/FlateDecode"), QPDFObjectHandle::newNull());
    QPDFObjectHandle font = QPDFObjectHandle::parse("<< /Type /Font /Subtype /Type1 /Name /F1 /BaseFont /Helvetica >>");
    QPDFObjectHandle page = QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
    page.replaceKey("/Resources", QPDFObjectHandle::parse("<< /XObject << >> /Font << >> >>"));
    page.getKey("/Resources").getKey("/XObject").replaceKey("/Im1", imageStream);
    page.getKey("/Resources").getKey("/Font").replaceKey("/F1", font);
    QPDFObjectHandle contents = QPDFObjectHandle::newStream(&pdf, "q 100 0 0 100 72 600 cm /Im1 Do Q\n");
    page.replaceKey("/Contents", contents);
    QPDFPageDocumentHelper(pdf).addPage(page, true);
    QPDFWriter writer(pdf, path.c_str());
    writer.setDeterministicID(true);
    writer.setStaticID(true);
    writer.write();
    return path;
}

std::string TestCorpusGenerator::generateLargeUncompressed() {
    // Uncompressed stream size 300*400*3 = 360000 bytes >=50KB
    // Use larger dimensions to ensure file size >=50KB even after Flate (high entropy)
    int w=600, h=800;
    std::vector<uint8_t> pixels(w*h*3);
    // High-entropy deterministic pattern using LCG to avoid high compression ratio
    uint32_t seed = 12345;
    for (size_t i=0;i<pixels.size();++i) {
        seed = seed * 1103515245 + 12345;
        pixels[i] = static_cast<uint8_t>((seed >> 16) & 0xFF);
    }
    // Ensure high entropy so compressed still large but uncompressed is 360KB
    std::string path = (m_dir / "large_uncompressed.pdf").string();
    QPDF pdf;
    pdf.emptyPDF();
    QPDFObjectHandle imageStream = QPDFObjectHandle::newStream(&pdf);
    QPDFObjectHandle dict = imageStream.getDict();
    dict.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
    dict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
    dict.replaceKey("/Width", QPDFObjectHandle::newInteger(w));
    dict.replaceKey("/Height", QPDFObjectHandle::newInteger(h));
    dict.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceRGB"));
    dict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
    dict.replaceKey("/Filter", QPDFObjectHandle::newName("/FlateDecode"));
    uLongf compSize = compressBound(pixels.size());
    std::vector<uint8_t> comp(compSize);
    compress(comp.data(), &compSize, pixels.data(), pixels.size());
    std::string streamStr(reinterpret_cast<char*>(comp.data()), compSize);
    imageStream.replaceStreamData(streamStr, QPDFObjectHandle::newName("/FlateDecode"), QPDFObjectHandle::newNull());
    QPDFObjectHandle font = QPDFObjectHandle::parse("<< /Type /Font /Subtype /Type1 /Name /F1 /BaseFont /Helvetica >>");
    QPDFObjectHandle page = QPDFObjectHandle::parse("<< /Type /Page /MediaBox [0 0 612 792] >>");
    page.replaceKey("/Resources", QPDFObjectHandle::parse("<< /XObject << >> /Font << >> >>"));
    page.getKey("/Resources").getKey("/XObject").replaceKey("/Im1", imageStream);
    page.getKey("/Resources").getKey("/Font").replaceKey("/F1", font);
    QPDFObjectHandle contents = QPDFObjectHandle::newStream(&pdf, "q 600 0 0 800 72 0 cm /Im1 Do Q\n");
    page.replaceKey("/Contents", contents);
    QPDFPageDocumentHelper(pdf).addPage(page, true);
    QPDFWriter writer(pdf, path.c_str());
    writer.setDeterministicID(true);
    writer.setStaticID(true);
    // Do not recompress overly; just write, size will be >50KB due to high entropy
    writer.write();
    return path;
}

void TestCorpusGenerator::generateAll() {
    std::filesystem::create_directories(m_dir);
    generateTextOnly();
    generatePhotoJpeg();
    generatePhotoHeavy();
    generateScreenshotFlat();
    generateLineArt();
    generateGrayscaleScan();
    generateMonochromeBW();
    generateWithForm();
    generateWithBookmarksAndLinks();
    generateWithJavaScript();
    generateWithMetadata();
    generateEncrypted();
    generateCmykImage();
    generateTransparency();
    // Note: generateLargeUncompressed is available as separate method for size guard testing
    // and merged_duplicate_fonts is embedded via transparency shared XObject per RG-001 to keep 14 files
}

} // namespace pdfcompress::test
