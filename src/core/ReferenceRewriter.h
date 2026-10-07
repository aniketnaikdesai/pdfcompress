#pragma once

#include <cstddef>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObjGen.hh>

namespace pdfcompress {

/// Repoints indirect references from one object onto another across a QPDF.
///
/// PDF streams are always indirect, and QPDF::replaceObject rejects indirect
/// handles (except a self-stream), so stream dedup cannot alias duplicates by
/// replacing the object. Instead, every reference to the duplicate is rewritten
/// to point at the kept byte-identical object; the duplicate then becomes
/// unreferenced and is dropped by the writer's
/// `setPreserveUnreferencedObjects(false)` setting.
class ReferenceRewriter {
public:
    /// Walk every indirect object (pdf.getAllObjects()) plus the trailer and
    /// repoint every indirect reference to `from` so it points at `to`.
    ///
    /// Traversal descends only into DIRECT dictionaries and arrays and never
    /// follows indirect references, so cycles terminate and the cost is bounded
    /// by the number of objects. Dictionary entries are rewritten with
    /// QPDFObjectHandle::replaceKey and array entries with
    /// QPDFObjectHandle::setArrayItem, both of which accept indirect handles.
    ///
    /// @return the number of references repointed.
    static std::size_t repointAll(QPDF& pdf,
                                  const QPDFObjGen& from,
                                  const QPDFObjectHandle& to);
};

} // namespace pdfcompress
