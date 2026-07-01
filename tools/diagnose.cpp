#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.pdf>" << std::endl;
        return 1;
    }

    try {
        QPDF pdf;
        pdf.processFile(argv[1]);

        std::cout << "PDF: " << argv[1] << std::endl;
        std::cout << "Pages: " << pdf.getRoot().getKey("/Pages").getKey("/Count").getIntValueAsInt() << std::endl;

        QPDFPageDocumentHelper pageHelper(pdf);
        int pageNum = 0;
        for (auto& page : pageHelper.getAllPages()) {
            pageNum++;
            auto images = page.getImages();
            std::cout << "\nPage " << pageNum << ": " << images.size() << " images" << std::endl;

            for (auto& [name, img] : images) {
                auto dict = img.getDict();
                int w = dict.getKey("/Width").getIntValueAsInt();
                int h = dict.getKey("/Height").getIntValueAsInt();
                int bpc = dict.hasKey("/BitsPerComponent") ? dict.getKey("/BitsPerComponent").getIntValueAsInt() : 8;
                std::string filter;
                if (dict.hasKey("/Filter")) {
                    auto f = dict.getKey("/Filter");
                    if (f.isName()) filter = f.getName();
                    else if (f.isArray() && f.getArrayNItems() > 0) filter = f.getArrayItem(0).getName();
                }
                std::string cs;
                if (dict.hasKey("/ColorSpace")) {
                    auto c = dict.getKey("/ColorSpace");
                    if (c.isName()) cs = c.getName();
                    else if (c.isArray() && c.getArrayNItems() > 0) cs = c.getArrayItem(0).getName();
                    else cs = "(complex)";
                }

                // Try to get raw stream data
                size_t rawSize = 0;
                try {
                    auto raw = img.getRawStreamData();
                    if (raw) rawSize = raw->getSize();
                } catch (std::exception& e) {
                    std::cout << "  getRawStreamData error: " << e.what() << std::endl;
                }

                int channels = 3;
                if (cs == "/DeviceGray") channels = 1;
                else if (cs == "/DeviceCMYK") channels = 4;
                int expectedRaw = w * h * channels;
                std::cout << "  Image: " << name 
                          << " (" << w << "x" << h
                          << " ch=" << channels << ")"
                          << " " << cs << " BPC:" << bpc
                          << " F:" << (filter.empty() ? "(none)" : filter)
                          << " raw:" << rawSize << " bytes"
                          << std::endl;

                // Check if we can decode the stream
                try {
                    auto decoded = img.getStreamData(qpdf_dl_all);
                    if (decoded) {
                        std::cout << "    Decoded OK: " << decoded->getSize() << " bytes" << std::endl;
                    }
                } catch (std::exception& e) {
                    std::cout << "    Decode error: " << e.what() << std::endl;
                }
            }
        }

    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
