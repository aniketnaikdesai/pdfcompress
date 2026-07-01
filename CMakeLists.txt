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
find_package(JPEG REQUIRED)
find_package(libjpeg-turbo CONFIG REQUIRED)
find_package(OpenJPEG CONFIG REQUIRED)
find_package(libdeflate CONFIG REQUIRED)

# Homebrew Qt6 often requires Homebrew's lib directory for transitive dependencies like -ljpeg
link_directories(/opt/homebrew/lib)

# Add source files
set(SOURCES
    src/main.cpp
    src/core/ImageMetadata.cpp
    src/core/PDFInspector.cpp
    src/core/ImageAnalyzer.cpp
    src/core/DecisionEngine.cpp
    src/core/PDFOptimizer.cpp
    src/codecs/JpegCodec.cpp
    src/codecs/PngCodec.cpp
    src/codecs/Jp2Codec.cpp
)

# Application Executable
# On macOS, MACOSX_BUNDLE creates an .app bundle
add_executable(PDFCompressor MACOSX_BUNDLE ${SOURCES})

# Include directories for our source tree
target_include_directories(PDFCompressor PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
)

# Link libraries
target_link_libraries(PDFCompressor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    qpdf::libqpdf
    PNG::PNG
    JPEG::JPEG
    $<IF:$<TARGET_EXISTS:libjpeg-turbo::turbojpeg>,libjpeg-turbo::turbojpeg,libjpeg-turbo::turbojpeg-static>
    openjp2
    $<IF:$<TARGET_EXISTS:libdeflate::libdeflate_shared>,libdeflate::libdeflate_shared,libdeflate::libdeflate_static>
    pdfium
)

# Basic macOS bundle properties
set_target_properties(PDFCompressor PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.pdfcompressor
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

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
