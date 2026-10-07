# macOS PDF Compressor — Usage Guide

## Overview

macOS PDF Compressor is a native desktop application that reduces PDF file
sizes by intelligently re-compressing embedded images and stripping
interactive elements (annotations, bookmarks, forms). It runs entirely
offline — no data leaves your machine.

The app inspects every image in a PDF, classifies it (photo, screenshot,
scanned text, etc.), and selects the optimal compression codec and
quality settings for each one. Supported codecs:

- **JPEG** (via libjpeg-turbo) — photos, natural images
- **JPEG 2000** (via OpenJPEG) — scanned documents, high-compression photos
- **PNG** (via libpng) — screenshots, line art (lossless)
- **zlib/Deflate** (via zlib) — monochrome, lossless fallback for screenshots

You can control the JPEG quality via a slider in the GUI, and override
per-classification codec choices using the compression profile.

---

## System Requirements

- **OS:** macOS 13.0 (Ventura) or later
- **Architecture:** Apple Silicon (arm64) or Intel (x86_64)
- **RAM:** 8 GB minimum (16 GB recommended for large PDFs)
- **Build-time dependencies:** Qt 6, CMake, vcpkg, Xcode Command Line Tools

---

## Building from Source

### 1. Install Build Tools

```bash
brew install cmake ninja pkg-config qt@6
```

### 2. Bootstrap vcpkg (one-time)

```bash
cd third_party/vcpkg
./bootstrap-vcpkg.sh
cd ../..
```

### 3. Configure and Build

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=third_party/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

This fetches PDFium binaries automatically, installs vcpkg dependencies
(qpdf, libpng, libjpeg-turbo, openjpeg, libdeflate, zlib), and produces:

| Target | Path | Description |
|--------|------|-------------|
| `PDFCompressor.app` | `build/PDFCompressor.app` | Desktop GUI application |
| `run_optimize` | `build/run_optimize` | CLI tool to optimize a PDF |
| `diagnose` | `build/diagnose` | CLI tool to inspect PDF structure |

---

## Running the App

```bash
open build/PDFCompressor.app
```

Or navigate to the `build/` directory in Finder and double-click
`PDFCompressor.app`.

> **Note:** On Apple Silicon macOS 15+ you may need to run `xattr -dr
> com.apple.quarantine build/PDFCompressor.app` the first time if the app
> hasn't been code-signed.

---

## Using the App

The GUI is a single window with the following controls from top to bottom:

- **Version info bar** — shows QPDF, Qt, and PDFium version
- **Inspect PDF** button — read-only scan of PDF image contents
- **Optimize PDF** button — run the full compression pipeline
- **JPEG Quality slider** — sets the JPEG quality (1–100, default 70).
  Lower = smaller file, lower quality. Higher = larger file, higher quality.
  The value is read at the moment you click "Optimize PDF".
- **Results area** — scrollable log of inspection or optimization output

The window also accepts **dragged PDF files** from Finder. When you drag
files over the window, a blue dashed overlay appears. Drop one or more
PDFs to load their paths, then click **Inspect PDF** or **Optimize PDF**
to process them as a batch.

### Drag & Drop

- Drag any number of `.pdf` files from Finder onto the app window.
- A blue overlay with "Drop PDF(s) here" appears while dragging.
- On drop, the file paths are listed in the results area.
- Non-PDF files are silently ignored.
- After dropping, click a button to process all queued files.
- Files are processed sequentially in the order they were dropped.
- Dropped paths are consumed once a button is clicked; subsequent clicks
  fall back to the file dialog.

### Inspect PDF

1. Drag PDF(s) onto the window, or click **Inspect PDF** and select a
   file via the file dialog.
2. The app scans every page using PDFium, extracts all embedded images,
   and displays:
   - Document info (PDF version, page count, file size)
   - Total images found and their combined size
   - Per-image details: dimensions, DPI, color space, bits-per-component,
     compression filter, alpha channel presence
3. This is a read-only operation — no files are modified.

### Optimize PDF

1. Adjust the **JPEG Quality** slider to your preference.
2. Drag PDF(s) onto the window, or click **Optimize PDF** and select a
   file via the file dialog.
3. Each file is processed independently. Output is written to the same
   directory with the suffix `_optimized` (e.g. `report_optimized.pdf`).
4. The optimization pipeline:
   - Applies the selected profile's stripping defaults (Balanced strips
     JavaScript and metadata; Max Compression strips all interactive
     elements and metadata; Max Quality strips nothing) — see
     [Compression Profiles](#compression-profiles)
   - Iterates every embedded image across all pages
   - Decompresses each image stream (TurboJPEG for JPEG, QPDF for Flate)
   - Runs pixel-level analysis to classify the image
   - Selects the best codec and quality settings
   - Re-encodes the stream and replaces it in the PDF if the result is
     smaller
   - Writes the output (linearization is off unless the **Linearize**
     checkbox is enabled)
5. Results show per-file:
   - Original and optimized file sizes
   - Size reduction percentage
   - Number of images processed
   - Annotations and bookmarks removed

---

## CLI Tools

### run_optimize

Command-line equivalent of the GUI Optimize button:

```bash
build/run_optimize input.pdf
```

Writes `input_optimized.pdf` to the same directory and prints results to stdout.

### diagnose

Inspects a PDF's internal structure — lists every page, its embedded images,
dimensions, color space, filter type, and stream sizes:

```bash
build/diagnose input.pdf
```

---

## Compression Profiles

The Decision Engine supports three profiles. In the GUI choose one from the
**Profile** dropdown (default **Balanced**); on the CLI select one with
`--profile <max-quality|balanced|max-compression>`.

Each profile is a starting set of options produced by
`OptimizationOptions::forProfile`. The table below lists exactly those
defaults; every stripping option can then be overridden individually with
the flags documented in the next section.

| Profile            | Links | Other annots | Bookmarks | Forms | JavaScript | Named dests | Metadata | Linearize | Flate recompress | Dedup |
|--------------------|-------|--------------|-----------|-------|------------|-------------|----------|-----------|------------------|-------|
| Max Quality        | keep  | keep         | keep      | keep  | keep       | keep        | keep     | off       | on               | on    |
| Balanced (default) | keep  | keep         | keep      | keep  | **strip**  | keep        | **strip**| off       | on               | on    |
| Max Compression    | strip | strip        | strip     | strip | strip      | strip       | strip    | off       | on               | on    |

- **Max Quality** strips nothing — interactive elements and metadata are
  preserved.
- **Balanced** strips JavaScript and metadata only; links, other
  annotations, bookmarks, forms, and named destinations are preserved.
- **Max Compression** strips everything listed.
- Linearization is **off** for all three profiles; enable it explicitly
  with `--linearize`.
- Flate recompression and stream de-duplication are **on** for all three
  profiles; disable them with `--no-flate-recompress` and `--no-dedup`.

### Profile and stripping flags

| Flag | Effect |
|------|--------|
| `--profile <max-quality\|balanced\|max-compression>` | Select the profile defaults (default `balanced`) |
| `--quality <1-100>` | JPEG quality hint; `0` (the default) uses the profile default |
| `--strip-links` / `--no-strip-links` | Strip (or preserve) link annotations and their URI/GoTo/Launch actions |
| `--strip-annots` / `--no-strip-annots` | Strip (or preserve) non-link annotations |
| `--strip-bookmarks` / `--no-strip-bookmarks` | Strip (or preserve) the document outline/bookmarks |
| `--strip-forms` / `--no-strip-forms` | Strip (or preserve) AcroForm forms and widget annotations |
| `--strip-js` / `--no-strip-js` | Strip (or preserve) JavaScript actions in `/Names`, `/OpenAction`, `/AA` |
| `--strip-dests` / `--no-strip-dests` | Strip (or preserve) named destinations |
| `--strip-metadata` / `--no-strip-metadata` | Strip (or preserve) `/Info`, XMP `/Metadata`, `PieceInfo`, `LastModified`, `Thumb` |
| `--linearize` | Enable PDF linearization (Fast Web View); off by default |
| `--no-flate-recompress` | Do not recompress Flate streams at level 9 (on by default) |
| `--no-dedup` | Do not de-duplicate byte-identical streams (on by default) |

Stripping flags override the selected profile's defaults, so for example
`--profile balanced --strip-bookmarks` strips bookmarks on top of the
Balanced defaults.

---

## Image Classification Heuristics

The Decision Engine classifies each image using pixel analysis (no
external ML/AI libraries):

| Classification | Heuristics | Default Codec |
|----------------|------------|---------------|
| Photo          | High entropy, many unique colors | JPEG (or JP2 at Max Compression) |
| Screenshot     | Few colors, large flat regions, low entropy | JPEG (zlib if MaxQuality + no slider) |
| Line Art       | High edge density, flat regions, limited palette | JPEG (zlib if MaxQuality + no slider) |
| Scanned Text   | Grayscale, strong edges, moderate entropy, high DPI | JPEG 2000 |
| Monochrome     | ≤4 unique colors, grayscale | zlib/Deflate (lossless) |

When a JPEG quality hint is set (the slider moved off its default),
Screenshot and Line Art images use JPEG instead of the Max Quality zlib
fallback; Monochrome stays zlib and Scanned Text stays JPEG 2000.

---

## Understanding the Output

The results pane reports:

- **Size Reduction** — `(1 - optimized/original) * 100`. Measured on the
  14-file benchmark corpus (archived as
  `build/benchmark_results_20261007_160128.tsv`), pdfcompress results
  ranged from -4.65% (`text_only.pdf`, a slight increase) to 47.55%
  (`line_art.pdf`), with a median of about 11%; image-heavy files benefit
  most. Encrypted PDFs are rejected before optimization, so their savings
  are not meaningful. Actual savings depend entirely on the original PDF
  contents.
- **Images Processed** — count of image streams that were successfully
  re-encoded and replaced. Shared XObjects are de-duplicated.
- **Annotations Removed** — page-level `/Annots` entries stripped
  (links, widgets, comments, sticky notes).
- **Bookmarks Removed** — document-level `/Outlines` stripped.

---

## Architecture Overview

```
src/
  main.cpp                Entry point — QMainWindow with Inspect/Optimize buttons,
                          JPEG quality slider, and drag-and-drop setup
  DropHandler.h/.cpp      Installs event filters on all widgets; accepts .pdf
                          drags from Finder; emits filesDropped signal
  DropOverlay.h/.cpp      Blue dashed overlay that appears during drag operations
  core/
    PDFInspector.h/.cpp   PDFium wrapper: opens PDF, extracts image metadata
    ImageAnalyzer.h/.cpp  Pixel analysis: grayscale, entropy, edges,
                          unique colors, flat regions → classification
    DecisionEngine.h/.cpp Maps classification + profile + slider → codec + stream filter
    PDFOptimizer.h/.cpp   QPDF wrapper: strips interactivity, replaces
                          image streams, writes optimized output
    ImageMetadata.h/.cpp  Shared data types (structs, enums)
  codecs/
    CodecInterface.h      Abstract ImageCodec base class + CompressionParams
    JpegCodec.h/.cpp      libjpeg-turbo encoder
    PngCodec.h/.cpp       libpng encoder (lossless, alpha support)
    Jp2Codec.h/.cpp       OpenJPEG (JPEG 2000) encoder
    ZlibCodec.h/.cpp      zlib/Deflate encoder (lossless, for monochrome/fallback)
  gui/                    (reserved for future GUI refactoring)
  utils/                  (reserved)
tools/
  run_optimize.cpp        CLI optimizer
  diagnose.cpp            CLI PDF structure inspector
  test_tj.cpp             TurboJPEG smoke test
```

The pipeline flow:

**Inspect** → PDFium extracts images → **Analyze** → classify each image
→ **Decide** → pick codec + quality → **Encode** → replace stream →
**Write** → QPDFWriter produces the output (linearized only when
requested).

---

## Limitations & Known Issues

- **Encrypted PDFs** are rejected (no password support yet).
- **CMYK images** may have reduced accuracy during conversion (handled as
  4-channel, but color space metadata is preserved).
- **JPEG XL** is not yet integrated (listed in the spec but not wired).
- **JBIG2** is not yet integrated (monochrome uses zlib instead).
- **No before/after preview** yet.
- **Tests** — automated GoogleTest suites live in `tests/` and are built as
  the `pdfcompress_tests` target; run them with `ctest --test-dir build`.

---

## Troubleshooting

| Problem | Likely Cause | Solution |
|---------|-------------|----------|
| App crashes on launch | Missing Qt frameworks | Run `macdeployqt` on the `.app` bundle |
| "Failed to open PDF" | Corrupted or encrypted PDF | Verify the file opens in Preview |
| No size reduction | Images already well-compressed | Try a PDF with uncompressed or JPEG images |
| Build error: PDFium not found | Network issue fetching PDFium | Run `cmake --build build` again (retries fetch) |
| Linker errors for Qt | Homebrew Qt not in path | `export CMAKE_PREFIX_PATH=$(brew --prefix qt@6)` |
| `undefined symbol: _tjInitCompress` | System libjpeg vs vcpkg conflict | Delete build dir and reconfigure with vcpkg toolchain |
