#include "ReferenceRewriter.h"

namespace pdfcompress {

namespace {

/// Repoint every direct reference to `from` inside `container` (a dict, array
/// or stream dict) to `to`. Descends only into DIRECT dictionaries and arrays:
/// an indirect child is inspected for a match but never traversed, so cycles
/// terminate and each direct container is visited once.
std::size_t repointIn(QPDFObjectHandle container,
                      const QPDFObjGen& from,
                      const QPDFObjectHandle& to) {
    std::size_t count = 0;

    if (container.isDictionary()) {
        for (const auto& key : container.getKeys()) {
            QPDFObjectHandle value = container.getKey(key);
            if (value.isIndirect()) {
                if (value.getObjGen() == from) {
                    container.replaceKey(key, to);
                    ++count;
                }
                // Never follow indirect references.
            } else if (value.isDictionary() || value.isArray()) {
                count += repointIn(value, from, to);
            }
        }
    } else if (container.isArray()) {
        const int n = container.getArrayNItems();
        for (int i = 0; i < n; ++i) {
            QPDFObjectHandle value = container.getArrayItem(i);
            if (value.isIndirect()) {
                if (value.getObjGen() == from) {
                    container.setArrayItem(i, to);
                    ++count;
                }
            } else if (value.isDictionary() || value.isArray()) {
                count += repointIn(value, from, to);
            }
        }
    }

    return count;
}

} // namespace

std::size_t ReferenceRewriter::repointAll(QPDF& pdf,
                                          const QPDFObjGen& from,
                                          const QPDFObjectHandle& to) {
    std::size_t count = 0;

    for (auto& obj : pdf.getAllObjects()) {
        if (obj.isStream()) {
            count += repointIn(obj.getDict(), from, to);
        } else if (obj.isDictionary() || obj.isArray()) {
            count += repointIn(obj, from, to);
        }
    }

    count += repointIn(pdf.getTrailer(), from, to);

    return count;
}

} // namespace pdfcompress
