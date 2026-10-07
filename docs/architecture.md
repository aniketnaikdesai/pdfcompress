# Architecture — PDF Compressor (Phase 0 Baseline + Phases 1–8)

> **Phase 0 sections below are retained verbatim from the validated Phase-0
> canonical artifact** (commits 67a54b9/9441c8c) as ground truth. Everything
> under "**Phases 1–8**" headings EXTENDS them — no Phase 0 section is
> rewritten. Verified-existing baseline: per-item stripping options WITH
> profile defaults (`OptimizationOptions::forProfile`), metadata stripping,
> linearization off-by-default, 3-profile GUI picker + CLI `--profile`,
> GTest + CTest wiring, corpus generator (14 types), benchmark script, and
> granular `OptimizationResult` ALL ALREADY EXIST. Phases 1–8
> verify-and-complete where code exists and build-only where the Phase-0
> Appendix A proves a gap (RIGHT/PARTIAL items).
>
> **2026-10-07 revision (mechanism corrected):** Phase-2 **AC-C5** (deferred
> ESC-010 stream-dedup activation) folded into the Phases 1–8 sections below —
> Stream dedup replace path component, new `ReferenceRewriter` component,
> `OptimizationResult.streamsDeduplicated` live note, staged
> `distinct_duplicate_streams.pdf` fixture (both streams actually referenced),
> NFR structural-correctness row, and the dedup risk rows. The mechanism is
> **reference-rewriting** (cycle-safe referrer walk + `replaceKey`/`setArrayItem`
> repoint, then drop-unreferenced), not the previously-drafted direct-copy
> `replaceObject`, which is infeasible against QPDF 12.3.2 (see Codebase
> Summary). Phase 0 sections untouched.

## Codebase Summary (Existing Repo — Factual Inventory)

> Inventory only — no proposal. Graphify was invoked first (`graphify-out/graph.json`: 404 nodes — Codebase Summary cross-checked; 350 nodes / 549 edges at Phase-0 validation). Manual repo survey verified graph findings against `src/`, `tests/`, `benchmark/`, `tools/`, `CMakeLists.txt`, `vcpkg.json`, `cmake/FetchPDFium.cmake`, `USAGE.md`.
> This section is retained from the validated proposal as evidence for Existing Repo Compliance.

### Existing Data Models / Entities

| Entity | Location | Key fields / notes |
|---|---|---|
| `ImageMetadata` [existing] | `src/core/ImageMetadata.h` | `widthPx`, `heightPx`, `dpiX/dpiY`, `colorSpace` (DeviceRGB/DeviceGray/DeviceCMYK/Unknown), `bitsPerComponent`, `filter` (StreamFilter), `compressedSize`, `uncompressedSize`, `hasAlpha`, `isInline` |
| `AnalysisResult` [existing] | `src/core/ImageAnalyzer.h` | `classification` (Photo/Screenshot/LineArt/ScannedText/Monochrome), `entropy`, `uniqueColorsSampled`, `edgeDensity`, `flatRegionRatio`, `isEffectivelyGrayscale`, `confidence` |
| `CompressionParams` [existing] | `src/codecs/CodecInterface.h` | `width`, `height`, `channels`, `profile` (CompressionProfile), `qualityHint` 1–100, `targetDPI` |
| `OptimizationOptions` [existing] | `src/core/PDFOptimizer.h:9` | `profile`, `qualityHint`, 7× strip booleans (`stripLinks`, `stripOtherAnnotations`, `stripBookmarks`, `stripForms`, `stripJavaScript`, `stripNamedDestinations`, `stripMetadata`), `linearize` default false, `recompressFlate`, `deduplicateStreams`; `forProfile()` maps profile→defaults (Balanced strips JS+metadata only) |
| `OptimizationResult` [existing] | `src/core/PDFOptimizer.h:67` | `success`, `originalSizeBytes`/`optimizedSizeBytes`, `imagesProcessed/Optimized/Skipped/KeptOriginal`, `imageBytesSaved`, `linksRemoved`, `annotationsRemoved`, `bookmarksRemoved`, `formsRemoved`, `jsRemoved`, `namedDestinationsRemoved`, `metadataStripped`, `streamsDeduplicated`, `errorMessage` |
| `PDFDocumentInfo` [existing] | `src/core/PDFInspector.h` | `filePath`, `fileSizeBytes`, `pdfVersion`, `pageCount`, `isEncrypted`, `images: vector<ImageMetadata>` |
| `QPDFObjectHandle` stream/trailer keys [existing] | `PDFOptimizer.cpp:206-231` | `/Width`, `/Height`, `/ColorSpace`, `/Filter`, `/BitsPerComponent`, `/Type /XObject /Subtype /Image`, trailer `/Info`, `/Metadata`, `/PieceInfo`, `/AcroForm`, `/Outlines`, `/Names/JavaScript`, `/OpenAction`, `/Dests`, `/AA`, `/Annots` |

**Relationships:** `PDFInspector::inspect` → `vector<ImageMetadata>` → `ImageAnalyzer::analyze(rawPixels)` → `AnalysisResult` → `DecisionEngine::compress(pixels, AnalysisResult, qualityHint)` → `(ImageCodec, StreamFilter)` → `PDFOptimizer` size-guarded replace (`compressedBytes.size() < meta.compressedSize`) → `QPDFWriter`. `OptimizationOptions.forProfile()` drives every stripping/dedup/linearize branch in `PDFOptimizer::optimize`.

### Existing Major Components & Responsibilities

| Component | Path | Responsibility |
|---|---|---|
| **GUI — MainWindow** [existing] | `src/main.cpp` | QMainWindow, QComboBox profile picker (Balanced/MaxQuality/MaxCompression), JPEG quality slider 1–100 default 70, Inspect/Optimize, QThreadPool future, drag-drop integration |
| **Drag-Drop** [existing] | `src/DropHandler.cpp/.h`, `src/DropOverlay.cpp/.h` | Event filter for `.pdf` drops, blue dashed overlay, sequential batch queue |
| **PDFInspector (PDFium wrapper)** [existing] | `src/core/PDFInspector.cpp/.h` | `FPDF_InitLibraryWithConfig`, `FPDF_LoadDocument`, `isEncrypted` via `FPDF_ERR_PASSWORD`, `inspect()` enumerates pages/images, `calculateDPI` (page-size approximation — flagged for Phase D), `extractImagesFromPage` |
| **ImageAnalyzer** [existing] | `src/core/ImageAnalyzer.cpp/.h` (331 LOC) | Pixel heuristics: entropy, uniqueColorsSampled, edgeDensity, flatRegionRatio, isEffectivelyGrayscale → 5-way classification |
| **DecisionEngine** [existing] | `src/core/DecisionEngine.cpp/.h` | Maps (classification+profile+qualityHint)→codec: Photo→JPEG/JP2, Screenshot/LineArt→zlib if MaxQuality && qualityHint<=0 else JPEG, ScannedText→JP2, Monochrome→zlib; owns `JpegCodec`, `PngCodec` (wired but never selected — dead code), `Jp2Codec`, `ZlibCodec` |
| **PDFOptimizer (QPDF wrapper)** [existing] | `src/core/PDFOptimizer.cpp` (404 LOC) | `processFile` → strip document/page/annotation levels (per-item booleans) → per-image decode (TurboJPEG `tjDecompressHeader3`/`tjDecompress2`, QPDF `getStreamData(qpdf_dl_all)` for Flate) → analyze → decide → encode → size-guarded replace → dedup byte-identical streams (FNV hash) → `QPDFWriter` with `setLinearization(linearize)`, `setObjectStreamMode(qpdf_o_generate)`, `setRecompressFlate(true)` level 9, `setDeterministicID(true)` when stripping metadata |
| **Codec layer** [existing] | `src/codecs/JpegCodec.cpp`, `PngCodec.cpp`, `Jp2Codec.cpp`, `ZlibCodec.cpp`, `CodecInterface.h` | `ImageCodec::encode(pixels, CompressionParams)→bytes`: JPEG via TurboJPEG `tjCompress2`, JP2 via OpenJPEG codestream, PNG via libpng, Zlib via `compress2` level 9 / libdeflate |
| **CLI — run_optimize** [existing] | `tools/run_optimize.cpp` (166 LOC) | Flags `--profile`, `--quality`, `--strip-*/--no-strip-*`, `--linearize`, `--no-flate-recompress`, `--no-dedup`, `-o/--output`, `--help`; prints detailed breakdown |
| **CLI — diagnose** [existing] | `tools/diagnose.cpp` (85 LOC) | Listing: pages, images, dims, colorSpace, filter, stream sizes |
| **Tests** [existing] | `tests/test_image_analyzer.cpp`, `test_decision_engine.cpp`, `test_codecs.cpp`, `test_pdf_optimizer.cpp`, `tests/test_corpus_generator.cpp/.h` | GTest `TEST(Suite,Case)` — CTests via `gtest_discover_tests`; `TestCorpusGenerator` with QPDF emptyPDF generation |
| **Benchmark & corpus** [existing] | `benchmark/generate_corpus.cpp` (9 LOC), `tests/test_corpus_generator.cpp` (153 LOC), `benchmark/run_benchmark.sh` (58 LOC) | `generate_corpus` prints `Corpus generated at: "<TMPDIR>"`; `run_benchmark.sh` loops corpus with valid tool invocations, `command -v` guards, pure-C++ SSIM |
| **Build** [existing] | `CMakeLists.txt`, `cmake/FetchPDFium.cmake`, `vcpkg.json`, `third_party/vcpkg` | `pdfcompress_core` STATIC lib, `PDFCompressor` MACOSX_BUNDLE, `run_optimize`, `diagnose`, `pdfcompress_tests`, `generate_corpus`; PDFium from `bblanchon/pdfium-binaries chromium/7920`, `libpdfium.dylib` bundled via `install_name_tool` |

### Existing External Integrations

| Integration | Usage |
|---|---|
| **PDFium** [existing] | Inspection + isEncrypted; fetched at configure via `cmake/FetchPDFium.cmake` — sole offline exception |
| **QPDF** [existing] | Rewriting, stripping, linearization, object-stream mode, `Pl_Flate`; also corpus generation via `QPDF::emptyPDF()` + `QPDFObjectHandle::newStream`/`parse` |
| **libjpeg-turbo (TurboJPEG)** [existing] | JPEG encode/decode |
| **OpenJPEG** [existing] | JPEG 2000 codestream (J2K) — `openjp2` |
| **libpng** [existing] | PNG encode (dead code until Phase 2 wires it) |
| **zlib / libdeflate** [existing] | Flate recompress level 9; ZlibCodec via `compress2` |
| **Qt6** [existing] | GUI (QMainWindow, QComboBox, QSlider, drag-drop) |
| **GTest** [existing] | `find_package(GTest CONFIG REQUIRED)`, `gtest_discover_tests`, `BUILD_TESTING=ON` |
| **vcpkg + CMake + Ninja** [existing] | Manifest mode `third_party/vcpkg`, toolchain file |

### Existing Verification Baseline (from Appendix A — not re-proposed)

Claims 4,5,6,7,10 from USAGE.md are **already FIXED on master** and are treated as verified facts, not Phase 0 work: per-item stripping (`PDFOptimizer.h:13-26`, `PDFOptimizer.cpp:68-137`, `main.cpp:88-106`), metadata stripping (`PDFOptimizer.cpp:119-144,320-332`), linearization off by default (`linearize=false`, `main.cpp:96`), profile picker in GUI+CLI (`main.cpp:63-66`, `run_optimize.cpp:46-55`), 12 GTests + benchmark/results breakdown (`CMakeLists.txt:135-163`, `PDFOptimizer.h:67-92`). Phase 0 only verifies and hardens.

### Verified Deltas for Phases 1–8 (survey 2026-10-04, re-verified by Validator)

> Labels `[existing]` / `[new]` / `[new/changed]` in the proposal sections
> below apply to the architecture, not to this inventory. Validator confirmed
> each delta against the current tree (`graphify-out/graph.json`: 404 nodes;
> targeted file reads).

| Fact | Evidence | Phase it drives |
|---|---|---|
| Slider scoping gap confirmed: Screenshot/LineArt force JPEG when `qualityHint>0`; ScannedText always JP2; Monochrome always zlib; `m_pngCodec` constructed but never selected | `src/core/DecisionEngine.cpp:13,22-71` (switch never assigns `m_pngCodec`) | Phase 2 |
| CMYK skipped (`imagesSkipped++`, channels==4 path) to avoid RGBA corruption | `src/core/PDFOptimizer.cpp:236,269` | Phase 2 |
| Stream dedup is **inert**: the ESC-001 `if (it->second.isIndirect()) continue;` guard at `src/core/PDFOptimizer.cpp:373` is always taken (all `seenStreams` handles come from `pdf.getAllObjects()` and the line-338 `!obj.isIndirect()` filter admits only indirect objects), so `streamsDeduplicated++` at line 377 is dead code; and `generateSharedXObject()` returns `generateTransparency()` (one indirect image object shared by reference across pages), so no distinct duplicate pair exists even with a working replace path | `src/core/PDFOptimizer.cpp:336-386`, `tests/test_corpus_generator.cpp:450-451`, `tests/test_structure_writer.cpp:53-54` | Phase 2 (AC-C5) |
| **AC-C5 mechanism discovery (verified against the project's in-tree QPDF 12.3.2, build-time):** `QPDF::replaceObject(og, oh)` rejects ANY indirect handle except a self-stream (`if (!oh \|\| (oh.isIndirect() && !(oh.isStream() && oh.getObjGen() == og))) throw ...`), and streams are always indirect in QPDF (`QPDF::newStream()` → `makeIndirectObject`; `getAllObjects()` handles report `isIndirect()`), so a "direct copy of the replacement stream" cannot be constructed and the previously-drafted direct-copy `replaceObject` dedup path is **infeasible**. Feasible alternative verified: `replaceKey` and array-item replacement accept indirect references (`QPDFObjectHandle.hh:917,944`; used with indirect values at `QPDFObjectHandle.cc:1231,1250`), so **reference-rewriting** (repoint every reference to the duplicate to the kept object, then drop the now-unreferenced duplicate) is the working mechanism | `third_party/vcpkg/buildtrees/qpdf/src/v12.3.2-e7ec2acdbc.clean/libqpdf/QPDF_objects.cc:1968`, `libqpdf/QPDF.cc` (`QPDF::newStream`), `include/qpdf/QPDFObjectHandle.hh:917,944`, `libqpdf/QPDFObjectHandle.cc:1231,1250` | Phase 2 (AC-C5) |
| DPI calc is page-size approximation (`calculateDPI(meta, page)`), not CTM; `CompressionParams.targetDPI` carried but unused for resampling | `src/core/PDFInspector.cpp:164,226`, `ImageMetadata.h` | Phase 3 |
| No JBIG2 encoder in tree; only `StreamFilter::JBIG2Decode` enum + string mapping exist (parse support, no encode) | `grep jbig2` → only `PDFInspector.cpp:210`, `ImageMetadata.h:26,108`; nothing in `vcpkg.json`/`CMakeLists.txt` | Phase 5 |
| No HarfBuzz/hb-subset in tree; font handling is inspect-only (emptyPDF Helvetica) | `grep harfbuzz/HarfBuzz` → zero hits | Phase 6 |
| GUI has profile picker + quality slider, but **no** QThreadPool/off-thread work, no progress/cancel, no password dialog | `grep QThreadPool|Cancel` in `src/main.cpp` → zero hits (picker `main.cpp:63-66`, slider `:75-78` confirmed) | Phases 4, 7 |
| No `--jobs`, `--dpi`, `--transcode-cmyk-to-rgb`, `--jbig2-lossy`, `--subset-fonts`, `--password`, `--re-encrypt` flags exist yet | `tools/run_optimize.cpp` flag list (AC6 set only) | Phases 2,3,5,6,7,8 |

---

## Summary

Phase 0 hardens the verification baseline without touching the compression pipeline: corpus stubs are replaced with real QPDF implementations covering 14 deterministic types in `$TMPDIR/pdfcompress_test_corpus`, `benchmark/run_benchmark.sh` is fixed to valid QPDF/Ghostscript/ocrmypdf invocations with `command -v` guards, and a focused pure-C++ PDFium render-diff helper (`tools/render_diff.cpp`) renders page 1 at 150 DPI and computes SSIM/PSNR in-process — no Python dependency. All changes stay in `tests/`, `benchmark/`, `tools/` and `CMakeLists.txt` test wiring.

Phases 1–8 build strictly on the locked Phase-0 baseline: Phase 1 verifies-and-locks existing structure/profile behavior with tests only; Phases 2–3 fix codec routing (slider scoping, PNG wiring, CMYK preservation + opt-in transcode), activate stream dedup (AC-C5: inert ESC-001 guard → reference-rewriting via a cycle-safe referrer walk that repoints references to the kept duplicate and lets the duplicate drop — `replaceObject` cannot alias duplicate streams), and add CTM-based DPI downsampling; Phase 4 moves batch work off the UI thread with progress/cancel/stats; Phase 5 adds optional JBIG2 for true 1-bit; Phase 6 adds font dedup + HarfBuzz subsetting; Phase 7 adds password UX with unencrypted-by-default output; Phase 8 hardens streaming memory, deterministic parallelism, and macOS packaging/CI. Two new third-party deps (jbig2enc optional, HarfBuzz required) arrive via vcpkg/vendored source; everything else is additive inside `core/` + `codecs/` + `tools/` with per-phase benchmark gates.

## Tech Stack Decisions

### Phase 0 (retained — hard constraints)

| # | Decision | Choice | Rationale | Alternatives considered |
|---|---|---|---|---|
| 1 | Language | **C++20** [existing — hard constraint] | Already RAII, no raw owning pointers, `-Wall -Wextra -Wpedantic` clean on `pdfcompress_core` | — |
| 2 | GUI framework | **Qt 6** [existing — hard constraint] | QMainWindow + drag-drop already wired | — |
| 3 | Build system | **CMake + Ninja + vcpkg manifest** (`third_party/vcpkg`) [existing] | Required for PDFium fetch, Qt, QPDF | — |
| 4 | PDF inspection/rendering | **PDFium via cmake/FetchPDFium.cmake → bblanchon/pdfium-binaries chromium/7920** [existing] | Sole inspection lib; also required for pure-C++ render-diff per AC4 | — |
| 5 | PDF rewriting | **QPDF** [existing] | Structure, stripping, linearization, object streams, deterministic ID; also corpus generation only-dependency | — |
| 6 | Image codecs | **libjpeg-turbo (TurboJPEG), OpenJPEG (J2K), libpng, zlib/libdeflate** [existing] | Already linked in `pdfcompress_core` | libjxl/JBIG2 deferred — not Phase 0 |
| 7 | Test framework | **GTest via vcpkg** (`GTest::gtest`/`gtest_main`, `gtest_discover_tests`, `BUILD_TESTING=ON`) [existing — locked] | Tests already pass; auto-discovery; switching to Catch2 would rewrite `vcpkg.json`+CMake for zero benefit in no-behavior-change phase. Locked 2026-10-03 | **Catch2** — rejected per Assumptions Log |
| 8 | Corpus generation lib | **QPDF only** (no PDFium render, no external binaries) [existing] | `QPDF::emptyPDF()` + `QPDFObjectHandle::newStream/parse` + `QPDFWriter` already used; keeps licensed-free invariant; `setDeterministicID(true)` for reproducibility | Using PDFium to render corpus — rejected |
| 9 | Corpus storage | **`$TMPDIR/pdfcompress_test_corpus` ephemeral, not committed** [existing — locked] | Avoids binary blobs in git; confirmed 2026-10-03; printed as `Corpus generated at: "<path>"` tee'd to `corpus_info.txt` | `tests/corpus/` fixtures — rejected per AC3 |
| 10 | Encrypted fixture encryption | **QPDF 128-bit AES, user password `test123`, no owner password, R=6 AES-128 (fallback R3)** [new] | Makes `PDFInspector::isEncrypted` true (FPDF_ERR_PASSWORD), optimizer returns `errorMessage` without crash (AC7); production password prompt is Phase H | — |
| 11 | Benchmark shell | **POSIX bash `set -euo pipefail`, `command -v` guards, `stat -f%z`/`stat -c%s` portability** [existing/new] | `command -v` ensures missing tools → `SKIPPED (not installed)` not `FAILED`; portable macOS/Linux | Hard-failing on missing tool — rejected per AC4 |
| 12 | Benchmark QPDF invocation | **Fix to `qpdf --recompress --compression-level=9 --object-streams=generate`** [new] | `qpdf --optimize` is invalid (cause of FAILED rows); correct per QPDF docs and AC4 normative | Keep `--optimize` — rejected (always fails) |
| 13 | Benchmark comparators | **Add Ghostscript `/ebook` and `/screen` (`-sDEVICE=pdfwrite -dPDFSETTINGS=`) + `ocrmypdf --optimize 3 --skip-text`**, each `command -v` guarded [new] | Completes 5-tool matrix per AC4; `--skip-text` avoids OCR cost | `ocrmypdf` without `--skip-text` — rejected |
| 14 | Render-diff implementation | **Pure C++ via PDFium (`FPDF_RenderPageBitmap` at 150 DPI page 1) + in-process SSIM/PSNR** in focused helper `tools/render_diff.cpp` linked against `pdfcompress_core` + `pdfium` [new] | Locked per 2026-10-03: no Python `scikit-image`, no `pip`; offline; `N/A` with header note `render-diff unavailable (PDFium not available)` if render fails | **Python scikit-image** — rejected (offline + pure-C++); **ImageMagick `compare`** — rejected; **No render-diff** — rejected (AC4 requires column) |
| 15 | Metrics & timing | **`date +%s%3N` (GNU)/`gdate`/fallback `date +%s`×1000, `reduction% = (1-out/orig)*100` 2 decimals, SSIM 0–1 3 decimals, PSNR dB 1 decimal** [new] | AC4 normative; ms precision | `/usr/bin/time` only — rejected |
| 16 | Output format & idempotency | **TSV header `File | Original Size | Tool | Output Size | Reduction % | Time (ms) | SSIM | PSNR | Status`**, `benchmark_results_YYYYMMDD_HHMMSS.tsv` in cwd and `build/`, `column -t` print, skip `*_optimized*` `*.qpdf*` `*.opt*` `*.gs*` `*.ocrmypdf*` [new] | Prevents re-benchmarking outputs; AC4/AC5 | — |
| 17 | Slider→codec note | **No DecisionEngine change in Phase 0 — document that slider only affects lossy classes** [existing doc, not code] | Appendix A claim 2 PARTIAL/WRONG: slider forces JPEG only for Screenshot/LineArt+Photo, never Monochrome/ScannedText; Phase C owns fix — Phase 0 must not redesign DecisionEngine | Wiring PNG in Phase 0 — rejected (Phase C scope) |

> PNG wire-up, effective-DPI downsampling (Phase D), JBIG2 (Phase F), font dedup/subsetting (Phase G), password GUI dialog (Phase H) are **explicitly deferred** per AC3/AC4 and design_gaps — noted but not designed here.

### Phases 1–8 (additions — existing/constrained choices first, then new)

| # | Decision | Choice | Rationale | Alternatives considered |
|---|---|---|---|---|
| 18 | Language | **C++20, RAII, `-Wall -Wextra` clean** [existing — hard constraint] | Locked stack; all new code follows existing hygiene | — |
| 19 | GUI / build / PDF libs | **Qt6 / CMake+Ninja+vcpkg / PDFium + QPDF** [existing — hard constraint] | Locked; QThreadPool (already a Qt6 module) is the threading vehicle, not a new dep | std::thread pool — rejected (Qt signal/slot + cancel integrate with QThreadPool) |
| 20 | Image codecs (existing) | **libjpeg-turbo, OpenJPEG J2K, libpng, zlib/libdeflate** [existing] | Unchanged; PNG goes from dead code to selected codec with no new dep | — |
| 21 | Test framework | **GTest + CTest** [existing — locked] | Locked per Phase 0; new routing/DPI/JBIG2/font/password tests are `TEST(Suite,Case)` | — |
| 22 | JBIG2 encoder | **jbig2enc (Apache-2.0) via vcpkg or vendored source, OPTIONAL dep** [new] | Locked 2026-10-04 (OQ F-1); lossless-default; configure warns + falls back to zlib when unavailable so build never breaks | GPL JBIG2 libs — rejected (license); Leptonica wrappers — rejected (extra dep for no gain) |
| 23 | Font subsetter | **HarfBuzz `hb-subset` via vcpkg** [new] | Locked 2026-10-04 (OQ G-4); pure-C++, offline, keeps `/ToUnicode`; QPDF has no subsetter | fontTools (Python) — rejected (offline/pure-C++ constraint); QPDF-only — rejected (no subset API); vendored micro-subsetter — rejected (CJK/ToUnicode risk) |
| 24 | Resampling | **Lanczos/bicubic in pixel domain before encode (header-only or small vendored resampler, Qt-free)** [new] | Phase 3 needs quality resample with no new runtime dep; implementation choice bounded by SSIM gates, offline, deterministic, no network | OpenCV — rejected (heavy dep); PDFium scale-on-render — rejected (changes print size semantics, not source pixels) |
| 25 | Render-diff / benchmark | **Pure-C++ PDFium render-diff + bash TSV matrix** [existing] | Locked; extended with `PeakRSS` column (Phase 8) only | Python — rejected (locked) |
| 26 | Password handling libs | **QPDF + PDFium password pass-through, no new lib** [existing] | Both already support passwords; only plumbing + GUI dialog is new | Keychain storage — rejected (out of scope; per-file prompt + env fallback locked) |
| 27 | Platform | **macOS-only, Phases 1–8** [locked constraint] | OQ I-6; packaging = signed DMG; CI = macOS leg | Portable `gdate`/Linux fallbacks kept in scripts but no CI legs required |
| 28 | Stream-dedup mechanism (Ph.2, AC-C5) | **Reference-rewriting: cycle-safe referrer walk + `replaceKey`/`setArrayItem` repoint, then drop-unreferenced** [new] | The direct-copy `replaceObject` path is infeasible against QPDF 12.3.2 (`replaceObject` rejects indirect handles; no direct stream can exist — verified at build time). `replaceKey`/array-item replacement accept indirect references, so repointing is the only working alias mechanism; keeps the existing signature/`objGen !=`/try-catch detection and is a QPDF-API-only change | Direct-copy `replaceObject` — rejected (infeasible: throws on indirect handle); `copyStream()` + replace-with-copy — rejected (duplicates bytes, does not alias, no size win); new "alias object" API — none exists in QPDF |

## System Components

### Phase 0 (retained)

| Component | Status | Responsibility & Relations |
|---|---|---|
| **GUI (MainWindow, DropHandler, DropOverlay)** | [existing] | Profile picker, slider 1–100 default 70, drag-drop batch, off-UI-thread dispatch. Relies on `OptimizationOptions::forProfile`. No change in Phase 0. |
| **PDFInspector (PDFium)** | [existing] | Enumerates images, reads metadata, `isEncrypted`. Also provides `FPDF_RenderPageBitmap` for render-diff helper. Existing DPI calc remains page-size approx (flagged for Phase D). |
| **ImageAnalyzer** | [existing] | 5-way heuristics. No change. |
| **DecisionEngine** | [existing] | Maps classification+profile+qualityHint→codec. Existing JPEG/JP2/zlib paths stay; `m_pngCodec` remains dead code in Phase 0 (fixed in Phase C). No behavior change. |
| **PDFOptimizer (QPDF)** | [existing] | Stripping (7 booleans), per-image decode/analyze/decide/encode/size-guard, dedup, writer options. No logic change; `linearize=false`, `recompressFlate` level 9, `deduplicateStreams`, `setDeterministicID` remain. |
| **Codec Layer (JpegCodec, Jp2Codec, PngCodec, ZlibCodec)** | [existing] | Thin RAII wrappers. PngCodec stays unselected in Phase 0. No new codecs. |
| **CLI tools: `run_optimize`, `diagnose`** | [existing] | `run_optimize` flags already cover `--profile/--quality/--strip-*/--linearize/--no-flate-recompress/--no-dedup`; Phase 0 verifies, does not extend. |
| **Test Harness (GTest + CTest)** | [existing] | `BUILD_TESTING=ON`, `gtest_discover_tests(pdfcompress_tests)`. No change except wiring. |
| **TestCorpusGenerator** | [new/changed] | `tests/test_corpus_generator.cpp/.h` — only component changed in Phase 0. Stubs replaced with real QPDF dict construction for 14 deterministic types in `$TMPDIR/pdfcompress_test_corpus` via `QPDFWriter::setDeterministicID(true)`. |
| **Corpus binary `generate_corpus`** | [existing] | `benchmark/generate_corpus.cpp` — invokes `TestCorpusGenerator::generateAll()`, prints `Corpus generated at:`; Phase 0 keeps behavior, benefits from corrected generator. |
| **Benchmark script `run_benchmark.sh`** | [new/changed] | 5-tool matrix (pdfcompress, qpdf `--recompress`, gs ebook/screen, ocrmypdf) with `command -v` guards, SKIPPED vs FAILED, ms timing, idempotency filters, TSV header including SSIM/PSNR, calls render-diff helper, writes `benchmark_results_*.tsv` to cwd+build. |
| **Render-diff helper `render_diff`** | [new] | `tools/render_diff.cpp` — takes two PDF paths, renders page 1 at 150 DPI via PDFium, computes SSIM/PSNR in C++ (no Python). Returns `N/A N/A` exit 0 if PDFium render unavailable; used by `run_benchmark.sh` per row. Keeps `core/` untouched. |

### Phases 1–8 (additions)

| Component | Status | Responsibility & relations |
|---|---|---|
| GUI MainWindow + DropHandler/DropOverlay | [existing] | Picker/slider/checkboxes unchanged; gains batch view + cancel + summary (Ph.4) and password dialog + re-encrypt checkbox (Ph.7), all wired to off-thread worker |
| Batch worker (QThreadPool + signals) | [new] | Owns per-file `optimize()` off UI thread, progress signals, cooperative cancel, partial-output rollback; later phases reuse it (password prompt bridging, `--jobs` pool sizing) |
| `OptimizationOptions::forProfile` + strip/writer paths | [existing] | Normative defaults locked by tests in Ph.1; extended with new fields only (`targetDPI`, transcode/JBIG2/subset/password flags) — no semantic change to strip logic |
| DecisionEngine routing | [new/changed] | Slider scoped to lossy classes; PNG branch for alpha + MaxQuality lossless; 1-bit→JBIG2 branch; CMYK preserve/transcode branch; each branch pinned by `TEST(DecisionEngine,…)` |
| Stream dedup replace path | [new/changed] | Replaces the inert ESC-001 `isIndirect()` skip at `PDFOptimizer.cpp:373` and the `replaceObject` call at line 376 with a **reference-rewriting** path: for each duplicate stream A byte-identical to kept B, repoint every reference to A to B, then let A drop under the already-set `setPreserveUnreferencedObjects(false)` (or `removeObject`). Retains the byte-identical signature detection (FNV hash + five dict keys), the `objGen !=` guard and per-pair try/catch; `streamsDeduplicated` now increments for distinct byte-identical indirect streams (AC-C5, Phase 2). `replaceObject` is no longer called — it cannot alias duplicate streams (see Codebase Summary) |
| ReferenceRewriter (`core/ReferenceRewriter.{h,cpp}`) | [new] | Cycle-safe referrer walk over every object (`pdf.getAllObjects()` + trailer) and its direct nested dicts/arrays; for a target `objGen` it repoints matching indirect references via `replaceKey` / `setArrayItem`. Never follows indirect references during traversal (each object is visited once from `getAllObjects()`), so cycles terminate and cost is bounded by object count. Only invoked when the byte-identical signature + `objGen !=` guard already matched, and only for `/Filter`/`/DecodeParms`-compatible streams, so repointing cannot change decoding semantics (AC-C5, Phase 2) |
| DPICalculator + Downsampler (focused new files in `core/`) | [new] | CTM→effective-DPI per image (fallback: page-size approx + log); Lanczos/bicubic resample honoring 300/150/96 targets, 32px floor, never-upscale; updates `/Width /Height` + CTM consistently |
| `diagnose` effective-DPI column | [new/changed] | Reports per-image effective DPI (±5% gate source for AC-D1) |
| Jbig2Codec (`ImageCodec` impl) | [new] | Wraps jbig2enc lossless default / `--jbig2-lossy` opt-in; size-guard + zlib fallback; disabled-with-warning when dep absent |
| CmykHandler (preserve path + opt-in transcode path) | [new] | ICC/DeviceCMYK round-trip preservation; `--transcode-cmyk-to-rgb` conversion with ΔE sampling internally, SSIM≥0.98 as normative gate |
| FontDedup + FontSubsetter (hb-subset wrapper) | [new] | Hash-join dedup reusing the Phase-2 corrected reference-rewriting path (`ReferenceRewriter`) (AC-C5 — the ESC-001 indirect-handle guard it replaces no longer exists after Ph.2; requirements Phase 6 wording staleness flagged as FI-4); subset ON Balanced/MaxCompression, OFF MaxQuality; AcroForm fonts exempt (dedup only); `/ToUnicode` preserved |
| Password flow (GUI dialog + CLI `--password`/`PDFOPTIMIZE_PASSWORD` + re-encrypt) | [new] | Per-file prompt, no caching, never logged; decrypt→optimize→unencrypted default; `--re-encrypt` preserves AES-128 params |
| Parallel page/image scheduler + RSS accounting | [new/changed] | `--jobs` (default core count), thread-local buffers, join-before-write determinism; `PeakRSS` sampling into TSV; `$TMPDIR` spill for OOM-risk files |
| `run_optimize` new flags | [new/changed] | `--transcode-cmyk-to-rgb`, `--dpi/--no-downsample`, `--no-jbig2/--jbig2-lossy`, `--subset-fonts/--no-subset-fonts`, `--password/--re-encrypt`, `--jobs` |
| Corpus (14 files) + staged fixtures | [existing] | Canonical 14 unchanged (requirements change needed for more); 1-bit scan, oversampled, tiny-icon, large-mixed, and the Phase-2 `distinct_duplicate_streams.pdf` (two distinct byte-identical indirect streams, NOT the reference-shared `transparency.pdf`) fixtures staged in `$TMPDIR`, never committed |
| Benchmark script + `render_diff` | [new/changed] | Gains `PeakRSS` column (Ph.8); per-phase before/after TSV comparison is the gate mechanism |

## Data Model

### Phase 0 (retained)

*All entities keep existing fields; Phase 0 only ensures corpus exercises keys that drive `OptimizationResult` counts.*

| Entity | Status | Key fields / constraints |
|---|---|---|
| `OptimizationOptions` | [existing] | `profile`, `qualityHint`, 7 strip booleans, `linearize=false`, `recompressFlate=true`, `deduplicateStreams=true`. No new fields. |
| `OptimizationResult` | [existing] | `success`, size bytes, image breakdown, stripping breakdown, `streamsDeduplicated`, `errorMessage`. Benchmark TSV maps these fields. |
| `ImageMetadata` | [existing] | `widthPx`, `heightPx`, `dpiX/dpiY`, `colorSpace`, `filter`, `compressedSize`, `hasAlpha`. Corpus must populate `DeviceCMYK` (channels=4 skip), `DeviceGray` 1-bit concept, alpha/SMask, realistic dims. |
| `AnalysisResult` | [existing] | `classification`, `entropy`, `uniqueColorsSampled`, `edgeDensity`, `flatRegionRatio`. Corpus image content tuned to hit each of 5 classifications. |
| `PDFDocumentInfo` | [existing] | `isEncrypted` must be true for `encrypted.pdf` (password `test123`); corpus PDFs must pass `QPDF::processFile` without warnings. |
| `CorpusFile` *(logical)* | [new/changed] | 14 files in `$TMPDIR/pdfcompress_test_corpus`, AC3 1–14 authoritative: `text_only`, `photo_jpeg` (200×150 DCT), `photo_heavy` (multi-page 3 images), `screenshot_flat` (400×300 Flate flat quadrants), `line_art` (300×300 edge-heavy limited palette) **[new]**, `grayscale_scan` (300×400 Gray Flate), `monochrome_bw` (100×100 Gray), `with_form` (AcroForm+Widget) **[new/changed]**, `with_bookmarks_and_links` (Outlines+Annots+Dests) **[existing corrected]**, `with_javascript` (Names/JavaScript+OpenAction) **[new]**, `with_metadata` (Info+Metadata XMP+PieceInfo/Thumb) **[new/changed]**, `encrypted` (128-bit AES test123) **[new]**, `cmyk_image` (DeviceCMYK via Flate or valid JPEG) **[new/changed]**, `transparency`/`merged_duplicate_fonts` (SMask+shared XObject via same indirect object) **[new]** — see plan_notes RG-001 for 14-file interpretation. Deterministic via `setDeterministicID(true)`. |
| `BenchmarkRow` *(TSV logical)* | [new] | `File | Original Size | Tool | Output Size | Reduction % | Time (ms) | SSIM | PSNR | Status` — one row per corpus PDF × per available tool (5 tools max). SSIM/PSNR = 3/1 decimals or `N/A`. |

### Phases 1–8 (additions)

| Entity | Status | Key fields / constraints |
|---|---|---|
| `OptimizationOptions` | [new/changed] | Existing fields unchanged; adds `targetDPI` (default per profile 300/150/96, `--dpi` override, `--no-downsample` disables), `transcodeCmykToRgb=false`, `useJbig2=true/lossy=false`, `subsetFonts` (per-profile matrix), `password` (never logged), `reEncrypt=false`, `jobs=coreCount` |
| `OptimizationResult` | [new/changed] | Adds `transcodedCmyk`, `jbig2Images`, `fontsDeduplicated`, `fontsSubsetted`, `peakRssBytes`; `streamsDeduplicated` becomes live in Phase 2 (AC-C5: was inert `0`, now increments for distinct byte-identical streams); other existing counters unchanged |
| `ImageMetadata` | [new/changed] | Adds `effectiveDpiX/Y` + `dpiSource` (CTM vs fallback) alongside existing page-approx `dpiX/dpiY`; `hasAlpha`, `bitsPerComponent`, `colorSpace` now drive PNG/JBIG2/CMYK branches |
| `AnalysisResult`, `PDFDocumentInfo`, `CompressionParams` | [existing] | Unchanged (params `targetDPI` now actually consumed by downsampler) |
| `BenchmarkRow` TSV | [new/changed] | Existing 9 columns + `PeakRSS` (Ph.8) |

## Integration Points

### Phase 0 (retained)

| Integration | Status | Purpose |
|---|---|---|
| **PDFium (bblanchon/pdfium-binaries, libpdfium.dylib)** | [existing] | `PDFInspector` enumeration + render-diff `FPDF_RenderPageBitmap` at 150 DPI page 1. Fetch at `cmake` configure only; offline otherwise. N/A fallback if unavailable. |
| **QPDF** | [existing] | `PDFOptimizer` rewriting; `TestCorpusGenerator` QPDF-only generation; `QPDFWriter` deterministic ID. |
| **Ghostscript `gs` (optional)** | [new] | Benchmark comparator: `-sDEVICE=pdfwrite -dPDFSETTINGS=/ebook` and `/screen`. Guarded by `command -v gs`; missing → `SKIPPED (not installed)`. |
| **ocrmypdf (optional)** | [new] | Benchmark comparator: `ocrmypdf --optimize 3 --skip-text`. Guarded by `command -v ocrmypdf`. |
| **`qpdf` CLI (optional)** | [new/changed] | Benchmark comparator fixed to `qpdf --recompress --compression-level=9 --object-streams=generate`. Guarded. |
| **libjpeg-turbo / OpenJPEG / libpng / zlib / libdeflate** | [existing] | Codec impl. No change. |
| **GTest via vcpkg** | [existing] | `find_package(GTest CONFIG REQUIRED)`, `gtest_discover_tests`. |

### Phases 1–8 (additions)

| Integration | Status | Purpose |
|---|---|---|
| PDFium (inspect + render + password) | [existing] | Adds CTM/bounds queries for effective DPI + `FPDF_LoadDocument(password)` path |
| QPDF (rewrite + password + deterministic write) | [existing] | Adds password pass-through, re-encrypt write, font-object walk for dedup/subset, and **reference-rewriting** (`replaceKey` / `setArrayItem` via `ReferenceRewriter`) for live stream dedup (Ph.2, AC-C5) |
| libjpeg-turbo / OpenJPEG / libpng / zlib | [existing] | PNG becomes live-selected; no version changes |
| jbig2enc (Apache-2.0, optional) | [new] | 1-bit lossless/lossy encode; absent → warn + zlib fallback |
| HarfBuzz hb-subset (required from Ph.6) | [new] | Embedded TTF/OTF subsetting with `/ToUnicode` intact |
| `gs` / `ocrmypdf` / `qpdf` CLI comparators | [existing] | Unchanged benchmark role |
| macOS packaging (signed DMG, entitlements, bundled `libpdfium.dylib`) | [new/changed] | Documented path; `.app` launches without `DYLD_*` |

## Non-Functional Approach

### Phase 0 (retained)

| Requirement | Approach |
|---|---|
| **Offline only** (Constraints) | No network in tests/corpus/benchmark; PDFium fetch is configure-time only. Corpus uses QPDF only; benchmark never calls `pip`/`python`; render-diff is in-process C++. |
| **Performance** (J1) | Corpus generation <2s for 14 PDFs; `run_optimize` per PDF <500ms for synthetic corpus; benchmark ms wall clock via `date +%s%3N`. No whole-document decode hold — streaming via QPDF per image. |
| **Determinism** (AC3) | Fixed dims, fixed pixel content, `QPDFWriter::setDeterministicID(true)`, no random IDs. `ls $TMPDIR/*.pdf | wc -l == 14`. |
| **Correctness / No silent growth** | Existing size guard `compressedBytes.size() < meta.compressedSize` unchanged; benchmark reports reduction %; tiny PDFs (<2KB) may show negative reduction due to writer overhead — allowed but logged via `OptimizationResult`. |
| **Security / Encryption** (AC7) | `encrypted.pdf` uses test password `test123` 128-bit AES only for fixture; production prompt is Phase H. `PDFInspector::isEncrypted` and `PDFOptimizer::processFile` without password must throw/return `errorMessage` without crash; benchmark wraps `run_optimize` with `|| true`. |
| **C++ hygiene** | RAII, no raw owning pointers, `pdfcompress_core` warnings clean `-Wall -Wextra -Wpedantic`. New `render_diff` helper follows same flags. New files in `tools/` + `tests/` + `benchmark/` only. |
| **Observability** | `OptimizationResult` breakdown already in `main.cpp:252-278` and `run_optimize.cpp:144-159`; benchmark TSV adds per-tool Status column and `column -t` print. |

### Phases 1–8 (additions)

| Requirement | Approach |
|---|---|
| **Offline** (all phases) | No network in app/tests/benchmark; jbig2enc + HarfBuzz via vcpkg/vendored at configure time (same exception class as PDFium fetch) |
| **Performance** | One-image-at-a-time kept; Ph.3 resample bounded by target DPI; Ph.8 `--jobs` parallelism + RSS cap (measure-first, lock-second) + `$TMPDIR` spill |
| **Determinism** | `setDeterministicID` retained; Ph.8 AC-I2 (1-vs-4-jobs pixel-identical) gates parallelism; no run is larger silently (S2) |
| **Structural correctness (Ph.2 dedup)** | Live dedup only rewrites references for byte-identical *distinct* streams (matching signature + `memcmp` + `objGen !=` guard); a cycle-safe referrer walk repoints every reference to the duplicate to the kept object, then the duplicate drops as unreferenced; per-pair try/catch leaves a bad pair untouched so one bad pair never fails the file; `qpdf --check` clean under defaults and `--no-dedup` (AC-C5) |
| **Security** | Passwords never logged/TSV'd; env fallback; 3-attempt GUI limit; re-encrypt preserves input AES-128 params; lossy-JBIG2 and CMYK-transcode both opt-in (safe defaults) |
| **Accessibility/correctness** | S3 render gates + S4 text-identity (extract-before/after) enforced per phase; form fonts never subset; tiny-icon floor prevents legibility loss |
| **Hygiene/UX** | RAII, `-Wall -Wextra` clean, reviewable diffs, `core/codecs/tools` layout, `USAGE.md` per change, GUI never blocked >100ms (QThreadPool + cancel) |

## Architecture Risks

### Phase 0 (retained)

| Risk | Severity | Mitigation |
|---|---|---|
| Corpus stubs remain vacuously passing if keys are wrong (e.g., `/AcroForm` missing, JS key typo) | H | Add verifier assertions per corpus type: after generation, `QPDF::processFile` + assert `hasKey("/AcroForm")`, `hasKey("/Outlines")`, `hasKey("/Names")` with `/JavaScript`, etc., before optimization; tests fail if stub not replaced. |
| QPDF encrypt API mismatch (`setR3Encryption` vs `setR6Encryption` AES-128) breaks `encrypted.pdf` | M | Try R6 then fallback R3; verify `isEncrypted==true` without password; CI logs encryption method. |
| PDFium render unavailable on CI (no dylib, headless) → SSIM/PSNR always N/A | M | Helper exits 0 with `N/A`, script notes `render-diff unavailable (PDFium not available)` in TSV header; benchmark still succeeds. Do not block. |
| Ghostscript `gs` vs `gswin32c`/missing `bc` on macOS breaks TSV math | M | Use `command -v gs` and `command -v bc || awk` fallback; reduction via `awk '{printf "%.2f",(1-out/orig)*100}'`. |
| Tiny synthetic PDFs (667 bytes) show -93% reduction due to QPDF writer overhead | M | Use ≥50KB realistic content for benchmark PDFs where feasible; acceptance allows overhead but requires explicit `imageBytesSaved` log. |
| `$TMPDIR` race (benchmark parses `Corpus generated at:`) | L | Script `grep -F 'Corpus generated at:' corpus_info.txt | awk -F'"' '{print $2}'` with fallback `CORPUS_DIR=${CORPUS_DIR:-$(./generate_corpus 2>&1 | ...)}`; create dir via `TestCorpusGenerator`. |
| Over-scope creep into Phase C (PNG wiring, DecisionEngine slider fix) | L | Plan explicitly docs slider affects only lossy classes but does NOT implement fix; DecisionEngine change blocked to Phase C; validator enforces `git diff --stat` stays in `tests/`, `benchmark/`, `tools/render_diff*`, `CMakeLists.txt`. |

### Phases 1–8 (additions)

| Risk | Severity | Mitigation |
|---|---|---|
| CTM inaccurate for rotated/cropped images (H/M) | H | AC-D1 fixtures incl. rotation; fallback to page-approx + log; SSIM gate catches over-shrink |
| Font subset breaks CJK/ToUnicode or form editing (H/M) | H | Never subset form fonts; CJK manual fixtures; byte-identical text-extraction gate; OFF for MaxQuality |
| Password leaks into logs/dumps (H/L) | H | Never-log rule + code-review checklist; env path; `TEST` asserts no secret in stdout/TSV |
| jbig2enc unavailable/fails to build (H/L) | H | Optional dep: configure warning + zlib fallback; accepted Apache-2.0 license |
| Double-degrade: downsample + JPEG recompress (M/H) | M | Resample in pixel domain pre-encode; SSIM gates per phase |
| QPDF handle races in parallel mode (M/M) | M | Thread-local buffers, join-before-write, AC-I2 determinism gate |
| Referrer walk misses or mis-repoints a reference (cycle, trailer, nested array, reference inside a stream dict), leaving the duplicate referenced or altering semantics (M/M) | M | Walk every `getAllObjects()` entry + trailer; recurse only into direct containers (cycle-safe, bounded by object count); repoint via `replaceKey`/`setArrayItem`; per-pair try/catch leaves a bad pair untouched; `qpdf --check` + `qpdf --json` single-stream gate under defaults and `--no-dedup` (AC-C5) |
| Repoint changes semantics because the signature omits `/Filter`/`/DecodeParms` (two byte-identical raw streams could decode differently) (M/L) | M | Rewrite only when the byte-identical signature matches; additionally require `/Filter` and `/DecodeParms` to match (treated as part of the dict-key signature) so no decode-affecting difference is repointed; fixture's two streams share the full signature; `qpdf --check` + S3 render gate catch any regression |
| Staged fixture has an unreferenced duplicate, so nothing can be deduped (M/M) | M | `distinct_duplicate_streams.pdf` must place **both** distinct streams behind real references (e.g. `/Resources /XObject << /Im1 A /Im2 B >>`, both drawn on the page); a duplicate with no referrer cannot be deduped and is not a valid fixture |
| RSS cap unachievable without spill redesign (M/M) | M | Measure-first-then-lock; `$TMPDIR` spill allowed; cap is a plan step, not a guess |
| Scope creep: target-size mode, JXL, Windows/Linux (M/L) | M | Locked OUT (OQ E-2/I-6); Validator rejects out-of-scope additions |
