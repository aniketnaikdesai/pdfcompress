cmake_minimum_required(VERSION 3.20)
project(PDFCompressor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

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

# Override JPEG::JPEG to use a static lib when possible (avoids ABI mismatches with Homebrew's dynamic lib).
# Prefer vcpkg's static lib if available; fall back to Homebrew's static lib.
set(_VCPKG_LIB_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib")
if(TARGET JPEG::JPEG)
    if(EXISTS "${_VCPKG_LIB_DIR}/libjpeg.a")
        set(JPEG_STATIC_PATH "${_VCPKG_LIB_DIR}/libjpeg.a")
    elseif(EXISTS "/opt/homebrew/lib/libjpeg.a")
        set(JPEG_STATIC_PATH "/opt/homebrew/lib/libjpeg.a")
    else()
        set(JPEG_STATIC_PATH "${_VCPKG_LIB_DIR}/libjpeg.a")
    endif()
    set_target_properties(JPEG::JPEG PROPERTIES
        IMPORTED_LOCATION "${JPEG_STATIC_PATH}"
        IMPORTED_LOCATION_RELEASE "${JPEG_STATIC_PATH}"
        IMPORTED_LOCATION_DEBUG "${JPEG_STATIC_PATH}"
    )
endif()

# Resolve turbojpeg static lib path (vcpkg or Homebrew)
if(EXISTS "${_VCPKG_LIB_DIR}/libturbojpeg.a")
    set(TURBOJPEG_LIB "${_VCPKG_LIB_DIR}/libturbojpeg.a")
elseif(EXISTS "/opt/homebrew/lib/libturbojpeg.a")
    set(TURBOJPEG_LIB "/opt/homebrew/lib/libturbojpeg.a")
else()
    find_library(TURBOJPEG_LIB NAMES turbojpeg libturbojpeg)
endif()

# Core sources (no Qt dependency)
set(CORE_SOURCES
    src/core/ImageMetadata.cpp
    src/core/PDFInspector.cpp
    src/core/ImageAnalyzer.cpp
    src/core/DecisionEngine.cpp
    src/core/CmykHandler.cpp
    src/core/PDFOptimizer.cpp
    src/core/ReferenceRewriter.cpp
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
    "${TURBOJPEG_LIB}"
    openjp2
    $<IF:$<TARGET_EXISTS:libdeflate::libdeflate_shared>,libdeflate::libdeflate_shared,libdeflate::libdeflate_static>
    pdfium
)

# Application Executable (Qt-based GUI)
add_executable(PDFCompressor MACOSX_BUNDLE
    src/main.cpp
    src/DropHandler.cpp
    src/DropOverlay.cpp
)
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

# Render-diff helper (PDFium 150 DPI BGRA SSIM/PSNR)
add_executable(render_diff tools/render_diff.cpp)
target_link_libraries(render_diff PRIVATE pdfcompress_core pdfium)
target_compile_options(render_diff PRIVATE $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wall -Wextra -Wpedantic>)
# Fix dylib path so ./build/render_diff works from repo root (uses @executable_path)
add_custom_command(TARGET render_diff POST_BUILD
    COMMAND install_name_tool -change "./libpdfium.dylib" "@executable_path/libpdfium.dylib" "$<TARGET_FILE:render_diff>" || true
    COMMENT "Fixing libpdfium rpath for render_diff"
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


# Establish a clean-warning baseline for the core logic
target_compile_options(pdfcompress_core PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wall -Wextra -Wpedantic>
)

# --- Testing & Benchmarking ---
option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)

    add_executable(pdfcompress_tests
        tests/test_image_analyzer.cpp
        tests/test_decision_engine.cpp
        tests/test_codecs.cpp
        tests/test_pdf_optimizer.cpp
        tests/test_corpus_generator.cpp
        tests/test_corpus_verifier.cpp
        tests/test_structure_writer.cpp
        tests/test_stripping_profiles.cpp
        tests/test_profile_defaults.cpp
    )
    target_link_libraries(pdfcompress_tests PRIVATE
        GTest::gtest GTest::gtest_main
        pdfcompress_core
    )
    target_include_directories(pdfcompress_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )

    include(GoogleTest)
    gtest_discover_tests(pdfcompress_tests)
    
    # Benchmark corpus generator tool
    add_executable(generate_corpus benchmark/generate_corpus.cpp tests/test_corpus_generator.cpp)
    target_link_libraries(generate_corpus PRIVATE pdfcompress_core)
    target_include_directories(generate_corpus PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
endif()

# Copy libpdfium to build dir for tests and CLI tools
file(COPY "${PDFIUM_DYLIB}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
