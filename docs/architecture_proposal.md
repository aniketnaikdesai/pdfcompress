# Architecture Proposal — PDF Compressor Phase 0: Verification and Baseline

## Codebase Summary (Existing Repo — Factual Inventory)

> Graphify was invoked first (`graphify-out/graph.json` exists: 350 nodes / 549 edges, 15 communities). Manual repo survey verified graph findings against `src/`, `tests/`, `benchmark/`, `tools/`, `CMakeLists.txt`, `vcpkg.json`, `cmake/FetchPDFium.cmake`, `USAGE.md`, `software_architecture_specification.md`. This section is inventory only — no proposal.

### Existing Data Models / Entities

| Entity | Location | Key fields / notes |
|---|---|---|
| `ImageMetadata` [existing] | `src/core/ImageMetadata.h` | `widthPx`, `heightPx`, `dpiX/dpiY`, `colorSpace` (enum DeviceRGB/DeviceGray/DeviceCMYK/Unknown), `bitsPerComponent`, `filter` (StreamFilter enum), `compressedSize`, `uncompressedSize`, `hasAlpha`, `isInline` |
| `AnalysisResult` [existing] | `src/core/ImageAnalyzer.h` | `classification` (ImageClassification: Photo/Screenshot/LineArt/ScannedText/Monochrome), `entropy`, `uniqueColorsSampled`, `edgeDensity`, `flatRegionRatio`, `isEffectivelyGrayscale`, `confidence` |
| `CompressionParams` [existing] | `src/codecs/CodecInterface.h` | `width`, `height`, `channels`, `profile` (CompressionProfile), `qualityHint` (1–100), `targetDPI` — input to `ImageCodec::encode` |
| `OptimizationOptions` [existing] | `src/core/PDFOptimizer.h:9` | `profile`, `qualityHint`, 7× strip booleans (`stripLinks`, `stripOtherAnnotations`, `stripBookmarks`, `stripForms`, `stripJavaScript`, `stripNamedDestinations`, `stripMetadata`), `linearize` (default false), `recompressFlate`, `deduplicateStreams`; `forProfile()` maps profile→defaults (Balanced strips JS+metadata only) |
| `OptimizationResult` [existing] | `src/core/PDFOptimizer.h:67` | `success`, `originalSizeBytes`/`optimizedSizeBytes`, image breakdown (`imagesProcessed/Optimized/Skipped/KeptOriginal`, `imageBytesSaved`), stripping breakdown (`linksRemoved`, `annotationsRemoved`, `bookmarksRemoved`, `formsRemoved`, `jsRemoved`, `namedDestinationsRemoved`, `metadataStripped`), `streamsDeduplicated`, `errorMessage` |
| `PDFDocumentInfo` [existing] | `src/core/PDFInspector.h` | `filePath`, `fileSizeBytes`, `pdfVersion`, `pageCount`, `isEncrypted`, `images: vector<ImageMetadata>` |
| `QPDFObjectHandle` stream dict keys [existing] | `PDFOptimizer.cpp:206-231` | `/Width`, `/Height`, `/ColorSpace`, `/Filter`, `/BitsPerComponent`, `/Type /XObject /Subtype /Image`, plus trailer keys `/Info`, `/Metadata`, `/PieceInfo`, `/AcroForm`, `/Outlines`, `/Names/JavaScript`, `/OpenAction`, `/Dests`, `/AA`, `/Annots` |

**Relationships:** `PDFInspector::inspect` → `vector<ImageMetadata>` → `ImageAnalyzer::analyze(rawPixels) → AnalysisResult` → `DecisionEngine::compress(pixels, AnalysisResult, qualityHint) → (ImageCodec, StreamFilter)` → `PDFOptimizer` size-guarded replace (`compressedBytes.size() < meta.compressedSize`) → `QPDFWriter`. `OptimizationOptions.forProfile()` drives every stripping/dedup/linearize branch in `PDFOptimizer::optimize`.

### Existing Major Components & Responsibilities

| Component | Path | Responsibility |
|---|---|---|
| **GUI — MainWindow** [existing] | `src/main.cpp` | QMainWindow, QComboBox profile picker (Balanced/MaxQuality/MaxCompression), JPEG quality slider 1–100 default 70, Inspect/Optimize buttons, QThreadPool future, drag-drop integration |
| **Drag-Drop** [existing] | `src/DropHandler.cpp/.h`, `src/DropOverlay.cpp/.h` | Event filter for `.pdf` drops, blue dashed overlay, sequential batch queue |
| **PDFInspector (PDFium wrapper)** [existing] | `src/core/PDFInspector.cpp/.h` | `FPDF_InitLibraryWithConfig`, `FPDF_LoadDocument`, `isEncrypted` via `FPDF_ERR_PASSWORD`, `inspect()` enumerates pages/images, `calculateDPI` (page-size approximation — known inaccurate per design_gaps #3), `extractImagesFromPage` |
| **ImageAnalyzer** [existing] | `src/core/ImageAnalyzer.cpp/.h` (331 LOC) | Pixel heuristics: entropy, uniqueColorsSampled, edgeDensity, flatRegionRatio, isEffectivelyGrayscale → 5-way classification |
| **DecisionEngine** [existing] | `src/core/DecisionEngine.cpp/.h` | Maps (classification + profile + qualityHint) → codec: Photo→JPEG/JP2, Screenshot/LineArt→zlib if MaxQuality && qualityHint<=0 else JPEG, ScannedText→JP2, Monochrome→zlib; owns `JpegCodec`, `PngCodec` (wired but never selected — dead code), `Jp2Codec`, `ZlibCodec` |
| **PDFOptimizer (QPDF wrapper)** [existing] | `src/core/PDFOptimizer.cpp` (404 LOC) | Full pipeline: `processFile` → strip document/page/annotation levels (per-item booleans) → per-image decode (TurboJPEG `tjDecompressHeader3`/`tjDecompress2`, QPDF `getStreamData(qpdf_dl_all)` for Flate) → analyze → decide → encode → size-guarded replace → dedup byte-identical streams (FNV hash) → `QPDFWriter` with `setLinearization(linearize)`, `setObjectStreamMode(qpdf_o_generate)`, `setRecompressFlate(true)` level 9, `setDeterministicID(true)` when stripping metadata |
| **Codec layer** [existing] | `src/codecs/JpegCodec.cpp`, `PngCodec.cpp`, `Jp2Codec.cpp`, `ZlibCodec.cpp`, `CodecInterface.h` | `ImageCodec::encode(pixels, CompressionParams)→bytes`: JPEG via TurboJPEG `tjCompress2`, JP2 via OpenJPEG codestream, PNG via libpng `png_write`, Zlib via `compress2` level 9 / libdeflate |
| **CLI — run_optimize** [existing] | `tools/run_optimize.cpp` (166 LOC) | Flags `--profile`, `--quality`, `--strip-*/--no-strip-*`, `--linearize`, `--no-flate-recompress`, `--no-dedup`, `-o/--output`, `--help`; prints detailed breakdown |
| **CLI — diagnose** [existing] | `tools/diagnose.cpp` (85 LOC) | `qpdf --show-all-data` style listing: pages, images, dims, colorSpace, filter, stream sizes |
| **Tests** [existing] | `tests/test_image_analyzer.cpp`, `test_decision_engine.cpp`, `test_codecs.cpp`, `test_pdf_optimizer.cpp`, `tests/test_corpus_generator.cpp/.h` | GTest `TEST(Suite,Case)` — 12 CTests via `gtest_discover_tests`; `TestCorpusGenerator` with QPDF emptyPDF generation (several stubs) |
| **Benchmark & corpus** [existing] | `benchmark/generate_corpus.cpp` (9 LOC), `tests/test_corpus_generator.cpp` (153 LOC), `benchmark/run_benchmark.sh` (58 LOC) | `generate_corpus` binary prints `Corpus generated at: "<TMPDIR>"`; `run_benchmark.sh` loops corpus but uses invalid `qpdf --optimize`, lacks GS/ocrmypdf, lacks SSIM, time in seconds not ms |
| **Build** [existing] | `CMakeLists.txt`, `cmake/FetchPDFium.cmake`, `vcpkg.json`, `third_party/vcpkg` | `pdfcompress_core` STATIC lib, `PDFCompressor` MACOSX_BUNDLE, `run_optimize`, `diagnose`, `pdfcompress_tests`, `generate_corpus`; PDFium fetched from `bblanchon/pdfium-binaries chromium/7920`, `libpdfium.dylib` bundled via `install_name_tool` |

### Existing External Integrations

| Integration | Usage |
|---|---|
| **PDFium** [existing] | Inspection + isEncrypted; fetched at configure time via `cmake/FetchPDFium.cmake` — sole offline exception |
| **QPDF** [existing] | Rewriting, stripping, linearization, object-stream mode, `Pl_Flate`; also corpus generation via `QPDF::emptyPDF()` + `QPDFObjectHandle::newStream`/`parse` |
| **libjpeg-turbo (TurboJPEG)** [existing] | JPEG encode/decode |
| **OpenJPEG** [existing] | JPEG 2000 codestream (J2K) — `openjp2` |
| **libpng** [existing] | PNG encode (implemented but unused — dead code) |
| **zlib / libdeflate** [existing] | Flate recompress level 9; ZlibCodec via `compress2` |
| **Qt6** [existing] | GUI (QMainWindow, QComboBox, QSlider, drag-drop) |
| **GTest** [existing] | `find_package(GTest CONFIG REQUIRED)`, `gtest_discover_tests`, `BUILD_TESTING=ON` |
| **vcpkg + CMake + Ninja** [existing] | Manifest mode `third_party/vcpkg`, toolchain file |

### Existing Verification Baseline (from Appendix A — not re-proposed)

Claims 4,5,6,7,10 from USAGE.md are **already FIXED on master** and are treated as verified facts, not Phase-0 work: per-item stripping (`PDFOptimizer.h:13-26`, `PDFOptimizer.cpp:68-137`, `main.cpp:88-106`), metadata stripping (`PDFOptimizer.cpp:119-144,320-332`), linearization off by default (`linearize=false`, `main.cpp:96`), profile picker in GUI+CLI (`main.cpp:63-66`, `run_optimize.cpp:46-55`), 12 GTests + benchmark/results breakdown (`CMakeLists.txt:135-163`, `PDFOptimizer.h:67-92`). Phase 0 must only verify and harden, not re-implement.

---

## Summary

Phase 0 hardens the verification baseline without touching the compression pipeline: replace the six corpus stubs with real QPDF implementations covering 14 deterministic types in `$TMPDIR`, fix `benchmark/run_benchmark.sh` to valid QPDF/Ghostscript/ocrmypdf invocations with `command -v` guards, and add a focused pure-C++ PDFium render-diff helper (`tools/render_diff` or `benchmark/render_diff.cpp`) that renders page 1 at 150 DPI and computes SSIM/PSNR in-process — no Python dependency. All changes stay in `tests/`, `benchmark/`, `tools/` and `CMakeLists.txt` test wiring.

## Tech Stack Decisions

| # | Decision | Choice | Rationale | Alternatives considered |
|---|---|---|---|---|
| 1 | Language | **C++20** [existing — hard constraint] | Already RAII, no raw owning pointers, `-Wall -Wextra -Wpedantic` clean on `pdfcompress_core` | — |
| 2 | GUI framework | **Qt 6** [existing — hard constraint] | QMainWindow + drag-drop already wired | — |
| 3 | Build system | **CMake + Ninja + vcpkg manifest** (`third_party/vcpkg`) [existing] | Required for PDFium fetch, Qt, QPDF | — |
| 4 | PDF inspection/rendering | **PDFium via cmake/FetchPDFium.cmake → bblanchon/pdfium-binaries chromium/7920** [existing] | Sole inspection lib; also required for pure-C++ render-diff per AC4 | — |
| 5 | PDF rewriting | **QPDF** [existing] | Structure, stripping, linearization, object streams, deterministic ID; also corpus generation only-dependency | — |
| 6 | Image codecs | **libjpeg-turbo (TurboJPEG), OpenJPEG (J2K), libpng, zlib/libdeflate** [existing] | Already linked in `pdfcompress_core` | libjxl/JBIG2 deferred to Phases F/?? — not Phase 0 |
| 7 | Test framework | **GTest via vcpkg** (`GTest::gtest`/`gtest_main`, `gtest_discover_tests`, `BUILD_TESTING=ON`) [existing — locked] | 12 tests already pass; `GoogleTest` CTest auto-discovery; switching to Catch2 would rewrite `vcpkg.json`+CMake for zero benefit in a no-behavior-change phase. Confirmed 2026-10-03 | **Catch2** — rejected per Assumptions Log (locked) |
| 8 | Corpus generation lib | **QPDF only** (no PDFium render, no external binaries) [existing] | `QPDF::emptyPDF()` + `QPDFObjectHandle::newStream/parse` + `QPDFWriter` already used; keeps licensed-free invariant; `setDeterministicID(true)` for reproducibility | Using PDFium to render corpus — rejected (adds dependency, not needed) |
| 9 | Corpus storage | **`$TMPDIR/pdfcompress_test_corpus` ephemeral, not committed** [existing — locked] | Avoids binary blobs in git; confirmed 2026-10-03; printed as `Corpus generated at: "<path>"` tee'd to `corpus_info.txt` | `tests/corpus/` fixtures — rejected per AC3/open_questions #4 |
| 10 | Encrypted fixture encryption | **QPDF 128-bit AES, user password `test123`, no owner password, R=3 or R=6 AES-128** [new] | Makes `PDFInspector::isEncrypted` true (FPDF_ERR_PASSWORD), optimizer returns `errorMessage` without crash (AC7); production password prompt is Phase H, not Phase 0 | — |
| 11 | Benchmark shell | **POSIX bash `set -euo pipefail`, `command -v` guards, `stat -f%z`/`stat -c%s` portability** [existing/new] | `command -v` ensures missing tools → `SKIPPED (not installed)` not `FAILED`; portable between macOS `stat -f` and Linux `stat -c` | Hard-failing on missing tool — rejected per AC4 |
| 12 | Benchmark QPDF invocation | **Fix to `qpdf --recompress --compression-level=9 --object-streams=generate`** [new] | `qpdf --optimize` is invalid (cause of FAILED rows); correct per QPDF docs and AC4 normative spec | Keep `--optimize` — rejected (always fails) |
| 13 | Benchmark comparators | **Add Ghostscript `/ebook` and `/screen` (`-sDEVICE=pdfwrite -dPDFSETTINGS=`) + `ocrmypdf --optimize 3 --skip-text`**, each `command -v` guarded [new] | Completes 5-tool matrix per AC4; `--skip-text` avoids OCR cost | `ocrmypdf` without `--skip-text` — rejected (would OCR and mutate) |
| 14 | Render-diff implementation | **Pure C++ via PDFium (`FPDF_RenderPageBitmap` at 150 DPI page 1) + in-process SSIM/PSNR** in a focused helper binary `tools/render_diff` (or `benchmark/render_diff.cpp`) linked against `pdfcompress_core` + `pdfium` [new] | Locked per 2026-10-03 decision: no Python `scikit-image`, no `pip`; offline; `N/A` with header note `render-diff unavailable (PDFium not available)` if render fails | **Python scikit-image** — rejected (violates offline + pure-C++ constraint); **ImageMagick `compare`** — rejected (external dep, not PDFium); **No render-diff** — rejected (AC4 requires column) |
| 15 | Metrics & timing | **`date +%s%3N` (GNU) / `gdate` / fallback `date +%s` ×1000, `reduction% = (1-out/orig)*100` with 2 decimals, SSIM 0–1 3 decimals, PSNR dB 1 decimal** [new] | AC4 normative; ms precision for comparison | ` /usr/bin/time` only — rejected (not portable, needs parsing) |
| 16 | Output format & idempotency | **TSV with header `File | Original Size | Tool | Output Size | Reduction % | Time (ms) | SSIM | PSNR | Status`**, files `benchmark_results_YYYYMMDD_HHMMSS.tsv` in cwd and `build/`, `column -t` print, skip `*_optimized*` `*.qpdf*` `*.opt*` `*.gs*` `*.ocrmypdf*` [new] | Prevents re-benchmarking outputs; AC4/AC5 | — |
| 17 | Slider→codec note | **No DecisionEngine change in Phase 0 — document that slider only affects lossy classes** [existing doc, not code] | Appendix A claim 2 is PARTIAL/WRONG: slider forces JPEG only for Screenshot/LineArt+Photo, never Monochrome/ScannedText; Phase C owns the fix — Phase 0 must not redesign DecisionEngine | Wiring PNG in Phase 0 — rejected (Phase C scope) |

> PNG wire-up, effective-DPI downsampling (Phase D), JBIG2 (Phase F), font dedup/subsetting (Phase G), password GUI dialog (Phase H) are **explicitly deferred** per AC3/AC4 and design_gaps — noted but not designed here.

## System Components

| Component | Status | Responsibility & Relations |
|---|---|---|
| **GUI (MainWindow, DropHandler, DropOverlay)** | [existing] | Profile picker, slider (1–100 default 70), drag-drop batch, off-UI-thread dispatch. Relies on `OptimizationOptions::forProfile`. No change in Phase 0. |
| **PDFInspector (PDFium)** | [existing] | Enumerates images, reads metadata, `isEncrypted`. Also provides `FPDF_RenderPageBitmap` for render-diff helper. Existing DPI calc remains page-size approx (flagged for Phase D). |
| **ImageAnalyzer** | [existing] | 5-way heuristics. No change in Phase 0. |
| **DecisionEngine** | [existing] | Maps classification+profile+qualityHint→codec. Existing JPEG/JP2/zlib paths stay; `m_pngCodec` remains dead code in Phase 0 (flagged, fixed in Phase C). **No behavior change.** |
| **PDFOptimizer (QPDF)** | [existing] | Stripping (7 booleans), per-image decode/analyze/decide/encode/size-guard, dedup, writer options. No logic change in Phase 0; existing `linearize=false`, `recompressFlate` level 9, `deduplicateStreams`, `setDeterministicID` remain. |
| **Codec Layer (JpegCodec, Jp2Codec, PngCodec, ZlibCodec)** | [existing] | Thin RAII wrappers. PngCodec stays unselected in Phase 0. No new codecs. |
| **CLI tools: `run_optimize`, `diagnose`** | [existing] | `run_optimize` flags already cover `--profile/--quality/--strip-*/--linearize/--no-flate-recompress/--no-dedup`; Phase 0 verifies, does not extend. |
| **Test Harness (GTest + CTest)** | [existing] | `BUILD_TESTING=ON`, `gtest_discover_tests(pdfcompress_tests)`. No change except wiring. |
| **TestCorpusGenerator** | [new/changed] | `tests/test_corpus_generator.cpp/.h` — **only component changed in Phase 0**. Stubs replaced with real QPDF dict construction for 14 deterministic types in `$TMPDIR/pdfcompress_test_corpus` via `QPDFWriter::setDeterministicID(true)`. Provides `generateWithForm`, `generateWithMetadata`, `generateWithJavaScript`, `generatePhotoHeavy`/`generateMultiImage`, `generateSharedXObject`/`generateMergedDuplicateFonts`, `generateLineArt`, `generateTransparency`, `generateEncrypted(test123)`, fixes `generateCmykImage` to use `/FlateDecode` or real JPEG bytes instead of raw DCTDecode. |
| **Corpus binary `generate_corpus`** | [existing] | `benchmark/generate_corpus.cpp` — invokes `TestCorpusGenerator::generateAll()`, prints `Corpus generated at:`; Phase 0 keeps behavior, only benefits from corrected generator. |
| **Benchmark script `run_benchmark.sh`** | [new/changed] | Becomes 5-tool matrix (pdfcompress, qpdf `--recompress`, gs ebook/screen, ocrmypdf) with `command -v` guards, SKIPPED vs FAILED, ms timing, idempotency filters, TSV header including SSIM/PSNR, calls render-diff helper, writes `benchmark_results_*.tsv` to cwd+build. |
| **Render-diff helper `render_diff`** | [new] | Focused binary `tools/render_diff.cpp` (or `benchmark/render_diff.cpp`) — takes two PDF paths, renders page 1 at 150 DPI via PDFium `FPDF_LoadDocument`→`FPDF_LoadPage`→`FPDFBitmap_Create`→`FPDF_RenderPageBitmap`→BGRA buffers, computes SSIM/PSNR purely in C++ (no Python). Returns `N/A` with exit 0 if PDFium render unavailable; used by `run_benchmark.sh` per row. Keeps `core/` untouched — new file only. |

## Data Model

*All entities keep existing fields; Phase 0 only ensures corpus exercises the keys that drive `OptimizationResult` counts.*

| Entity | Status | Key fields / constraints |
|---|---|---|
| `OptimizationOptions` | [existing] | `profile`, `qualityHint`, 7 strip booleans, `linearize=false`, `recompressFlate=true`, `deduplicateStreams=true`. No new fields in Phase 0. |
| `OptimizationResult` | [existing] | `success`, size bytes, image breakdown, stripping breakdown, `streamsDeduplicated`, `errorMessage`. Benchmark TSV maps these fields. |
| `ImageMetadata` | [existing] | `widthPx`, `heightPx`, `dpiX/dpiY`, `colorSpace`, `filter`, `compressedSize`, `hasAlpha`. Phase 0 corpus must populate `DeviceCMYK` (channels=4 path that is skipped), `DeviceGray` 1-bit, alpha/SMask, and realistic dims to avoid 667-byte writer-overhead (-93%) in TSV. |
| `AnalysisResult` | [existing] | `classification`, `entropy`, `uniqueColorsSampled`, `edgeDensity`, `flatRegionRatio`. Corpus image content is tuned to hit each of the 5 classifications (photo_heavy, screenshot_flat, line_art flat/edge-heavy, grayscale_scan, monochrome). |
| `PDFDocumentInfo` | [existing] | `isEncrypted` must be true for `encrypted.pdf` (password `test123`); corpus PDFs must pass `QPDF::processFile` without warnings. |
| `CorpusFile` *(logical, not a struct)* | [new/changed] | 14 files in `$TMPDIR/pdfcompress_test_corpus`, listed AC3 1–14 authoritative: `text_only`, `photo_jpeg` (200×150 DCT), `photo_heavy` (multi-page 3 images), `screenshot_flat` (400×300 Flate flat quadrants), `line_art` (300×300 edge-heavy limited palette) **[new]**, `grayscale_scan` (300×400 Gray Flate), `monochrome_bw` (100×100 Gray), `with_form` (AcroForm+Widget) **[new/changed from stub]**, `with_bookmarks_and_links` (Outlines+Annots+Dests) **[existing corrected]**, `with_javascript` (Names/JavaScript+OpenAction) **[new]**, `with_metadata` (Info+Metadata XMP+PieceInfo/Thumb) **[new/changed]**, `encrypted` (128-bit AES test123) **[new]**, `cmyk_image` (DeviceCMYK via Flate or valid JPEG) **[new/changed]**, `transparency`/`merged_duplicate_fonts` (SMask+shared XObject via same indirect object) **[new]**. Deterministic via `setDeterministicID(true)`. |
| `BenchmarkRow` *(TSV logical)* | [new] | `File | Original Size | Tool | Output Size | Reduction % | Time (ms) | SSIM | PSNR | Status` — one row per corpus PDF × per available tool (5 tools max). SSIM/PSNR = 3/1 decimals or `N/A`. |

## Integration Points

| Integration | Status | Purpose |
|---|---|---|
| **PDFium (bblanchon/pdfium-binaries, libpdfium.dylib)** | [existing] | `PDFInspector` enumeration + render-diff `FPDF_RenderPageBitmap` at 150 DPI page 1. Fetch at `cmake` configure time only; offline otherwise. N/A fallback if dylib unavailable. |
| **QPDF** | [existing] | `PDFOptimizer` rewriting; `TestCorpusGenerator` QPDF-only generation; `QPDFWriter` deterministic ID. No new dependency. |
| **Ghostscript `gs` (optional)** | [new] | Benchmark comparator: `-sDEVICE=pdfwrite -dPDFSETTINGS=/ebook` and `/screen`. Guarded by `command -v gs`; missing → `SKIPPED (not installed)`, never FAILED. Alternative fallback `gs -sDEVICE=png16m -r150` only if pure-C++ PDFium path not yet wired — per AC4 the C++ path is preferred and Python is forbidden. |
| **ocrmypdf (optional)** | [new] | Benchmark comparator: `ocrmypdf --optimize 3 --skip-text`. Guarded by `command -v ocrmypdf`. |
| **`qpdf` CLI (optional)** | [new/changed] | Benchmark comparator fixed to `qpdf --recompress --compression-level=9 --object-streams=generate`. Guarded. |
| **libjpeg-turbo / OpenJPEG / libpng / zlib / libdeflate** | [existing] | Codec impl. No change. |
| **GTest via vcpkg** | [existing] | `find_package(GTest CONFIG REQUIRED)`, `gtest_discover_tests`. No change. |

## Non-Functional Approach

| Requirement | Approach |
|---|---|
| **Offline only** (Constraints) | No network in tests/corpus/benchmark; PDFium fetch is configure-time only. Corpus generation uses QPDF only; benchmark never calls `pip`/`python`; render-diff is in-process C++. |
| **Performance** (J1 — ctest + corpus + benchmark) | Corpus generation <2s for 14 PDFs; `run_optimize` per PDF stays <500ms for synthetic corpus; benchmark times with `date +%s%3N` ms wall clock. No whole-document decode hold — existing streaming via QPDF `getRawStreamData`/`getStreamData` per image. |
| **Determinism** (AC3 — 14 types) | Fixed dims, fixed pixel content, `QPDFWriter::setDeterministicID(true)`, no random IDs. `ls $TMPDIR/*.pdf | wc -l == 14`. Re-run overwrites deterministically. |
| **Correctness / No silent growth** (Constraints) | Existing size guard `compressedBytes.size() < meta.compressedSize` (PDFOptimizer.cpp:286) unchanged; benchmark reports reduction %; tiny PDFs (<2KB) may show negative reduction due to writer overhead — allowed but logged via `OptimizationResult.streamsDeduplicated`/`metadataStripped`. |
| **Security / Encryption** (AC7) | `encrypted.pdf` uses test password `test123` 128-bit AES only for fixture; production prompt is out of scope for Phase 0. `PDFInspector::isEncrypted` and `PDFOptimizer::processFile` without password must throw/return `errorMessage` without crash; benchmark wraps `run_optimize` with `\|\| true`. |
| **C++ hygiene** | RAII, no raw owning pointers, `pdfcompress_core` warnings clean `-Wall -Wextra -Wpedantic`. New `render_diff` helper follows same flags. New files in `tools/` + `tests/` + `benchmark/` only. |
| **Observability** | `OptimizationResult` breakdown already displayed in `main.cpp:252-278` and `run_optimize.cpp:144-159`; benchmark TSV adds per-tool Status column (OK/SKIPPED/FAILED) and `column -t` print. |

## Architecture Risks

| Risk | Severity | Mitigation |
|---|---|---|
| Corpus stubs remain vacuously passing if keys are wrong (e.g., `/AcroForm` missing, JS key typo) | H | Add verifier assertions per corpus type: after generation, `QPDF::processFile` + assert `hasKey("/AcroForm")`, `hasKey("/Outlines")`, `hasKey("/Names")` with `/JavaScript`, etc., before optimization; tests fail if stub not replaced. |
| QPDF encrypt API mismatch (`setR3Encryption` vs `setR6Encryption` AES-128) breaks `encrypted.pdf` generation | M | Test both APIs at CMake configure or try R6 then fallback R3; CI log must show encryption method; verify `isEncrypted==true` without password. |
| PDFium render unavailable on CI (no dylib, headless) → SSIM/PSNR always N/A | M | Helper exits 0 with `N/A`, script notes `render-diff unavailable (PDFium not available)` in TSV header per AC4; benchmark still succeeds. Do not block on render-diff. |
| Ghostscript `gs` vs `gswin32c`/missing `bc` on macOS breaks TSV math | M | Use `command -v gs` and `command -v bc || awk` fallback; reduction calc via `awk '{printf "%.2f",(1-out/orig)*100}'`. |
| Tiny synthetic PDFs (667 bytes) show -93% reduction due to QPDF writer overhead, masking real savings | M | Use ≥50KB realistic content for corpus benchmark PDFs where feasible (larger images / text); acceptance allows overhead but requires explicit `imageBytesSaved` log; note in verification report. |
| `FileNotFoundError` on `$TMPDIR` race (benchmark parses `Corpus generated at:`) | L | Script `grep -F 'Corpus generated at:' corpus_info.txt | awk -F'"' '{print $2}'` with `CORPUS_DIR=${CORPUS_DIR:-$(./generate_corpus 2>&1 | ...)}` fallback; create dir via `TestCorpusGenerator` constructor. |
| Over-scope creep into Phase C (PNG wiring, DecisionEngine slider fix) | L | Plan explicitly docs that slider affects only lossy classes but does NOT implement the fix; DecisionEngine change is blocked to Phase C; validator enforces `git diff --stat` stays in `tests/`, `benchmark/`, `tools/render_diff*`, `CMakeLists.txt`. |

