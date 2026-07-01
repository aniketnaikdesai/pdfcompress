# macOS Local PDF Compressor -- Implementation Plan

## Goal

Build a native macOS desktop application that performs high-quality
local PDF compression while preserving visual quality. The application
should run entirely offline.

## Technology Stack

  Layer                    Technology
  ------------------------ -------------------
  Language                 C++20
  Build                    CMake
  IDE                      CLion or Xcode
  GUI                      Qt 6
  PDF Parsing              PDFium
  PDF Rewriting            QPDF
  JPEG 2000                OpenJPEG
  JPEG XL                  libjxl
  PNG                      libpng
  Deflate                  libdeflate
  Monochrome Compression   jbig2enc
  Testing                  Catch2 (optional)

## High-Level Architecture

``` text
Qt 6 GUI
   |
Compression Controller
   |
+-----------------------------+
| PDFium (inspect/render)     |
| QPDF (rewrite/optimize)     |
| Image Analyzer              |
| Compression Decision Engine |
| Thread Pool                 |
+-----------------------------+
   |
Codec Layer
|- libjxl
|- OpenJPEG
|- libpng
|- libdeflate
|- jbig2enc
```

## Project Structure

``` text
PDFCompressor/
  CMakeLists.txt
  src/
    gui/
    core/
    codecs/
    utils/
  third_party/
  tests/
```

## System Operations

-   **Logging:** Error logs and application logs will be written locally to the project folder to facilitate easy debugging.
-   **Temporary Files:** The application will use the macOS designated temporary directory (`NSTemporaryDirectory()` / `/tmp`) for processing large files safely before saving the final output.

## Phase 1 -- Foundation

-   Create CMake project.
-   Manage dependencies using `vcpkg` for C++ libraries and Homebrew (`brew install`) for macOS-specific build tools.
-   Integrate Qt 6.
-   Build universal macOS binary (Apple Silicon + Intel if desired).
-   Integrate QPDF and PDFium.
-   Configure CI (optional).

## Phase 2 -- PDF Inspection

-   Open PDF.
-   Enumerate pages.
-   Extract image objects.
-   Record image metadata (size, DPI, color space, codec, alpha).

## Phase 3 -- Image Analysis

Implement lightweight heuristics (no OpenCV): - Detect grayscale vs
color. - Count unique colors (sampling). - Estimate entropy. - Detect
transparency. - Estimate photo vs screenshot vs line-art.

## Phase 4 -- Compression Decision Engine

Rules: - Existing efficient image -\> keep. - Photo -\> JPEG XL. - Scan
-\> JPEG 2000. - Monochrome -\> JBIG2. - Graphics/UI -\> PNG. - Optimize
Flate streams with libdeflate.

## Phase 5 -- PDF Optimization

-   Remove unused objects.
-   Remove metadata.
-   Strip all interactive elements (annotations, hyperlinks, bookmarks, form fields).
-   Deduplicate identical images.
-   Optimize object streams.
-   Linearize output (optional).

## Phase 6 -- GUI

-   Drag-and-drop.
-   Batch compression.
-   Compression presets:
    -   Lossless
    -   High Quality
    -   Balanced
    -   Maximum Compression
-   Progress and logs.
-   Before/after statistics.

## Phase 7 -- Performance

-   Parallel page processing.
-   Avoid unnecessary recompression.
-   Memory-efficient streaming for large PDFs.

## Phase 8 -- Testing

Test with: - Scanned books - Photo-heavy PDFs - Vector PDFs -
Mixed-content documents - Password-free PDFs - Large (\>500 MB) files

## macOS Packaging

-   Build with CMake.
-   Package as `.app`.
-   Apply ad-hoc code signing (required for local execution on Apple Silicon).
-   Configure macOS App Sandbox with necessary entitlements for secure execution.
-   Create signed/notarized DMG for distribution (optional).
-   Bundle required Qt frameworks and dynamic libraries.

## Future Enhancements

-   OCR plug-in (optional).
-   Image preview diff.
-   Folder watch mode.
-   CLI companion.
-   Apple Shortcuts integration.
-   Finder Quick Action.
