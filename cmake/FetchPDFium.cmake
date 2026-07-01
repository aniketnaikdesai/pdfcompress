# PDFium CMake integration
# Downloads pre-built PDFium binaries from bblanchon/pdfium-binaries

include(FetchContent)

set(PDFIUM_VERSION "chromium/7920")
set(PDFIUM_TAG "chromium%2F7920") # URL-encoded

# Detect architecture
if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
    set(PDFIUM_PLATFORM "mac-x64")
elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
    set(PDFIUM_PLATFORM "mac-arm64")
else()
    # Default: detect host
    execute_process(COMMAND uname -m OUTPUT_VARIABLE HOST_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(HOST_ARCH STREQUAL "arm64")
        set(PDFIUM_PLATFORM "mac-arm64")
    else()
        set(PDFIUM_PLATFORM "mac-x64")
    endif()
endif()

set(PDFIUM_URL "https://github.com/bblanchon/pdfium-binaries/releases/download/${PDFIUM_TAG}/pdfium-${PDFIUM_PLATFORM}.tgz")

message(STATUS "Fetching PDFium from: ${PDFIUM_URL}")

FetchContent_Declare(
    pdfium
    URL ${PDFIUM_URL}
)
FetchContent_MakeAvailable(pdfium)

# Create imported target
add_library(pdfium SHARED IMPORTED GLOBAL)

set(PDFIUM_INCLUDE_DIR "${pdfium_SOURCE_DIR}/include")
set(PDFIUM_LIB_DIR "${pdfium_SOURCE_DIR}/lib")

# Find the dylib
file(GLOB PDFIUM_DYLIB "${PDFIUM_LIB_DIR}/libpdfium.dylib")
if(NOT PDFIUM_DYLIB)
    message(FATAL_ERROR "Could not find libpdfium.dylib in ${PDFIUM_LIB_DIR}")
endif()

set_target_properties(pdfium PROPERTIES
    IMPORTED_LOCATION "${PDFIUM_DYLIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${PDFIUM_INCLUDE_DIR}"
)

message(STATUS "PDFium include: ${PDFIUM_INCLUDE_DIR}")
message(STATUS "PDFium library: ${PDFIUM_DYLIB}")
