cmake_minimum_required(VERSION 3.20)
project(PDFCompressor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include PDFium fetch module
include(cmake/FetchPDFium.cmake)

# Find Qt 6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Find vcpkg dependencies
find_package(qpdf CONFIG REQUIRED)
find_package(PNG REQUIRED)
find_package(libjpeg-turbo CONFIG REQUIRED)
find_package(OpenJPEG CONFIG REQUIRED)
find_package(libdeflate CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(ZLIB REQUIRED)

# Override JPEG::JPEG to use vcpkg's static lib (not Homebrew's dynamic lib which has ABI version mismatch)
set(_VCPKG_LIB_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib")
if(TARGET JPEG::JPEG)
    set_target_properties(JPEG::JPEG PROPERTIES
        IMPORTED_LOCATION "${_VCPKG_LIB_DIR}/libjpeg.a"
        IMPORTED_LOCATION_RELEASE "${_VCPKG_LIB_DIR}/libjpeg.a"
        IMPORTED_LOCATION_DEBUG "${_VCPKG_LIB_DIR}/debug/lib/libjpeg.a"
    )
endif()

# Core sources (no Qt dependency)
set(CORE_SOURCES
    src/core/ImageMetadata.cpp
    src/core/PDFInspector.cpp
    src/core/ImageAnalyzer.cpp
    src/core/DecisionEngine.cpp
    src/core/PDFOptimizer.cpp
    src/codecs/JpegCodec.cpp
    src/codecs/PngCodec.cpp
    src/codecs/Jp2Codec.cpp
    src/codecs/ZlibCodec.cpp
)

# Static library for the core logic
add_library(pdfcompress_core STATIC ${CORE_SOURCES})
target_include_directories(pdfcompress_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
)
target_link_directories(pdfcompress_core PUBLIC
    "${_VCPKG_LIB_DIR}"
)
target_link_libraries(pdfcompress_core PUBLIC
    qpdf::libqpdf
    PNG::PNG
    JPEG::JPEG
    ZLIB::ZLIB
    "${_VCPKG_LIB_DIR}/libturbojpeg.a"
    openjp2
    $<IF:$<TARGET_EXISTS:libdeflate::libdeflate_shared>,libdeflate::libdeflate_shared,libdeflate::libdeflate_static>
    pdfium
)

# Application Executable (Qt-based GUI)
add_executable(PDFCompressor MACOSX_BUNDLE src/main.cpp)
target_link_libraries(PDFCompressor PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets
    pdfcompress_core
)
target_link_directories(PDFCompressor PRIVATE
    "${_VCPKG_LIB_DIR}"
    /opt/homebrew/lib
)

# Basic macOS bundle properties
set_target_properties(PDFCompressor PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.pdfcompressor
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

# CLI tool for running optimization from the command line
add_executable(run_optimize tools/run_optimize.cpp)
target_link_libraries(run_optimize PRIVATE pdfcompress_core)

# Diagnostic tool for inspecting PDF contents
add_executable(diagnose tools/diagnose.cpp)
target_link_libraries(diagnose PRIVATE qpdf::libqpdf JPEG::JPEG)
target_link_directories(diagnose PRIVATE ${_VCPKG_LIB_DIR})

# Copy PDFium dylib into the app bundle so the app can find it at runtime
add_custom_command(TARGET PDFCompressor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_BUNDLE_DIR:PDFCompressor>/Contents/Frameworks"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PDFIUM_DYLIB}"
        "$<TARGET_BUNDLE_DIR:PDFCompressor>/Contents/Frameworks/"
    COMMAND install_name_tool -change
        "./libpdfium.dylib"
        "@executable_path/../Frameworks/libpdfium.dylib"
        "$<TARGET_BUNDLE_DIR:PDFCompressor>/Contents/MacOS/PDFCompressor"
    COMMENT "Bundling libpdfium.dylib into app bundle"
)
