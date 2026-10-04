# Requirements — PDF Compressor (Phase 0: Verification and Baseline)

## Problem Statement
PDF Compressor is an existing macOS desktop app that reduces PDF file size offline by inspecting images with PDFium, classifying them with pixel heuristics, selecting a codec via DecisionEngine, and rewriting the PDF with QPDF. The Phase 0 brief assumed several limitations (always-on stripping, no metadata control, always-linearized, no profile picker, no tests/benchmark) inferred solely from USAGE.md. Phase 0 must **verify every claim against the actual code with file:line evidence**, establish a reproducible test baseline (GTest + CTest) and a licensed-free corpus, and provide a benchmark that compares the current build against external tools with size and render-diff metrics — with **no behavior changes** to the compression pipeline itself.

For whom: the repository owner (sole developer) who needs a trustworthy baseline before executing Phases A–I.

## Goals
- Verify each "Current behavior / Observed limitations" claim in §1 of the Phase 0 brief as RIGHT / WRONG / PARTIAL with precise file:line evidence (consumed by Plan).
- Lock Phase 0 baseline artifacts that are fully testable: test framework + CTest integration, deterministic corpus generator, benchmark script with size + SSIM/PSNR + timing.
- Keep the app fully offline; keep existing architecture and file layout (`core/`, `codecs/`, `tools/`); keep `run_optimize` and `diagnose` CLI tools working.
- Produce a verification report (appendix) that Plan will treat as ground truth.
- Ensure all existing and new tests pass; output is never larger than input without reporting; renders match within a threshold; output opens in Preview + one other viewer.

## Non-Goals
- No changes to image re-encoding, downsampling, codec selection, JBIG2, font handling, encryption support, or GUI redesign — all Phases A–I are out of scope for Phase 0.
- No new codecs beyond the existing ImageCodec interface; no architecture or tech-stack replacement.
- No network calls, telemetry, or online processing.
- No effort estimates, task decomposition, or dependency graphs (Task Manager owns those).

## Existing Repository
This is an existing repository.
What is being built/changed: Phase 0 verification and baseline only — validate USAGE.md claims against code, confirm the test harness (GTest + CTest), corpus generator, and benchmark script that already exist in the working tree, and flesh their specs to a testable, Quality-Bar level without changing compression behavior. Later phases (A–I) are context only.

Current Tech Stack
- Language(s): C++20 (RAII, no raw owning pointers, -Wall -Wextra -Wpedantic on pdfcompress_core)
- Framework(s): Qt 6 (QMainWindow, drag-drop DropHandler/DropOverlay, QThreadPool future), CMake + Ninja, vcpkg (manifest mode, third_party/vcpkg)
- PDF libs: PDFium (inspection/rendering, fetched via cmake/FetchPDFium.cmake → bblanchon/pdfium-binaries chromium/7920, libpdfium.dylib bundled into PDFCompressor.app/Contents/Frameworks), QPDF (rewriting, structure, linearization, object-stream mode)
- Codec libs: libjpeg-turbo (JPEG via TurboJPEG), OpenJPEG (JPEG 2000 codestream J2K), libpng (PNG), zlib/libdeflate (Deflate)
- Test libs: GTest (GTest::gtest, GTest::gtest_main via vcpkg, gtest_discover_tests)
- Build: `cmake -B build -DCMAKE_TOOLCHAIN_FILE=third_party/vcpkg/scripts/buildsystems/vcpkg.cmake && cmake --build build` ; outputs PDFCompressor.app (GUI with profile picker + JPEG quality slider 1–100 default 70), run_optimize CLI, diagnose CLI, generate_corpus, pdfcompress_tests
- Treat the existing tech stack above as a hard constraint — do not suggest replacing it.

## Constraints
- **Offline only:** no data leaves the machine; no network calls in tests, corpus generator, or benchmark (PDFium fetch at configure time is the sole exception).
- **Architecture/file layout:** keep `core/`, `codecs/`, `tools/`, `benchmark/`, `tests/`; new codecs implement `ImageCodec` (src/codecs/CodecInterface.h); keep diffs reviewable, new logic in focused files.
- **C++ hygiene:** RAII, no raw owning pointers, warnings clean at -Wall -Wextra -Wpedantic.
- **Resilience:** every image/page/cleanup step is wrapped so one failure never fails the whole file; fall back to original data and log reason (PDFOptimizer already does per-image try/catch + size guard).
- **No silent growth:** never write output larger than input without telling the user; keep original untouched; never overwrite input file; output is `<input>_optimized.pdf` or `-o` path.
- **Memory/GUI:** stream or process one image at a time, avoid holding whole decoded documents; GUI stays responsive (off-UI-thread work with cancel support — future phase, but Phase 0 must not regress batch Sequential behavior).
- **CLI parity:** keep `run_optimize` and `diagnose` working; give `run_optimize` flags for every new option so tests can drive them (already has --profile, --quality, --strip-*, --linearize, --no-flate-recompress, --no-dedup).
- **Docs:** update USAGE.md for every user-visible change (Phase 0 has no user-visible compression change, but must document test/benchmark usage if behavior of those tools changes).

## Glossary
- **PDFium:** Google PDF rendering/inspection library; used in PDFInspector to enumerate image XObjects, read dimensions/colorSpace/DPI, detect encryption. Also used for pure-C++ render-diff (render page 1 at 150 DPI to bitmap, compare).
- **QPDF:** PDF structure library; used in PDFOptimizer to read, strip, replace image streams, deduplicate streams, recompress Flate, write with optional linearization. Used in corpus generator to create deterministic PDFs and to encrypt test fixture.
- **DecisionEngine:** Maps (ImageClassification + CompressionProfile + qualityHint) → (ImageCodec + StreamFilter). Located src/core/DecisionEngine.h/.cpp.
- **ImageAnalyzer:** Pixel heuristics (entropy, uniqueColorsSampled, edgeDensity, flatRegionRatio, isEffectivelyGrayscale) → ImageClassification (Photo/Screenshot/LineArt/ScannedText/Monochrome).
- **CompressionProfile:** MaxQuality / Balanced / MaxCompression (src/codecs/CodecInterface.h).
- **OptimizationOptions:** Struct in PDFOptimizer.h carrying profile, qualityHint, per-item strip booleans (stripLinks, stripOtherAnnotations, stripBookmarks, stripForms, stripJavaScript, stripNamedDestinations, stripMetadata), linearize, recompressFlate, deduplicateStreams.
- **StreamFilter:** PDF stream filter name (DCTDecode, JPXDecode, FlateDecode, etc.).
- **Linearization:** Fast Web View (QPDFWriter::setLinearization).
- **Corpus:** Set of 14 licensed-free PDFs generated on the fly via QPDF (no embedded third-party copyrighted content), stored ephemerally in `$TMPDIR/pdfcompress_test_corpus` (not committed).
- **Render-diff:** Pixel-level comparison of original vs optimized renders (SSIM/PSNR) implemented purely in C++ via PDFium rendering at fixed DPI; no Python dependency. If PDFium render is unavailable, record N/A.

## User Journeys

### J1 — Developer verifies the baseline (primary Phase 0 journey)
- **Preconditions:** Repo checked out; vcpkg bootstrapped; `cmake -B build -DCMAKE_TOOLCHAIN_FILE=third_party/vcpkg/scripts/buildsystems/vcpkg.cmake` succeeds; PDFium fetched.
- **Steps:**
  1. Read docs/requirements/requirements.md Appendix A (verification report) and cross-check each file:line citation.
  2. Run `cmake --build build` then `ctest --test-dir build --output-on-failure`.
  3. Run `./build/generate_corpus` and inspect `corpus_info.txt` for the 14 corpus types at `$TMPDIR/pdfcompress_test_corpus`.
  4. Run `benchmark/run_benchmark.sh [build_dir]` and open the TSV results.
  5. Open a generated `_optimized.pdf` in Preview and one other viewer (e.g., Adobe Reader or Chrome) and confirm text remains selectable.
- **Postconditions:** Verification report is trusted; 12+ tests pass; corpus contains 14 deterministic PDFs in `$TMPDIR/pdfcompress_test_corpus`; benchmark TSV contains size + pure-C++ render-diff + timing per file/tool; no compression behavior was changed.
- **Edge Cases:**
  - PDFium fetch fails on offline CI → build errors with clear message from FetchPDFium.cmake; developer retries or uses cached _deps/pdfium-subbuild.
  - Corpus dir already exists with stale PDFs → generator overwrites deterministically (same QPDF emptyPDF + fixed content).
  - `qpdf`/`gs`/`ocrmypdf` not installed → benchmark records `SKIPPED (not installed)` instead of `FAILED` and still produces pdfcompress rows.

### J2 — Developer runs the test suite in CI/headless
- **Preconditions:** No display server; build dir exists.
- **Steps:** 1. `ctest --test-dir build -R PDFOptimizerTest` 2. Check JUnit/XML or CTest output.
- **Postconditions:** Tests pass without requiring GUI or network.
- **Edge Cases:** GTest not found via vcpkg → configure fails fast with `find_package(GTest CONFIG REQUIRED)` error; message tells developer to run vcpkg install.

### J3 — Developer benchmarks against external tools
- **Preconditions:** Corpus generated in `$TMPDIR/pdfcompress_test_corpus`; `qpdf`, `gs`, `ocrmypdf` may or may not be installed.
- **Steps:** 1. Run benchmark script 2. Review columns: original size, optimized size, % saved, SSIM/PSNR (pure C++ PDFium), time.
- **Postconditions:** Table shows pdfcompress vs each available external tool per corpus file; missing tools are marked SKIPPED, not FAILED.
- **Edge Cases:** Benchmark run pollutes corpus dir with `*_optimized.pdf` → script skips `*_optimized*`, `*.qpdf*`, `*.opt*` patterns to avoid re-benchmarking outputs.

## Acceptance Criteria (feature-level) — Given/When/Then

### AC1 — Verification report completeness
- **Given** the Phase 0 brief §1 lists 11 distinct limitations
- **When** Plan reads Appendix A
- **Then** each claim has a verdict (RIGHT / WRONG / PARTIAL) **and** at least one `file:line` citation that can be opened to confirm the verdict, **and** the evidence distinguishes current code from the USAGE.md claim where they differ.

### AC2 — Test framework choice and CTest integration
- **Given** the repo already builds with GTest (CMakeLists.txt `find_package(GTest CONFIG REQUIRED)` + `gtest_discover_tests`)
- **When** a developer runs `cmake -B build -DCMAKE_TOOLCHAIN_FILE=… && cmake --build build && ctest --test-dir build --output-on-failure`
- **Then** all tests pass (currently 12), `pdfcompress_tests` is discovered via CTest, each test has a distinct name matching `TEST(Suite, Case)`, **and** adding a new `tests/test_*.cpp` only requires appending one line to `CMakeLists.txt` `add_executable(pdfcompress_tests …)`.
- **Rationale (locked):** GTest is chosen over Catch2 because it is already wired via vcpkg (`vcpkg.json` includes `gtest`), uses `GoogleTest` CMake module for auto-discovery, and matches the existing 4 test files. Switching to Catch2 would be a behavior change and is out of scope for Phase 0. `BUILD_TESTING=ON` remains the default.

### AC3 — Corpus generator — 14 deterministic types (QPDF-only, no copyrighted content, TMPDIR storage)
- **Given** `benchmark/generate_corpus.cpp` invokes `TestCorpusGenerator::generateAll()` and `tests/test_corpus_generator.h/.cpp` implements QPDF-based generation
- **When** `./build/generate_corpus` is run
- **Then** it creates exactly these 14 files in deterministic ephemeral directory `$TMPDIR/pdfcompress_test_corpus` (default; printed as `Corpus generated at: "<path>"` and tee'd to `corpus_info.txt`; directory is NOT committed to git):
  1. `text_only.pdf` — empty PDF + Helvetica "Hello World" (QPDF emptyPDF + Type1 font) — verifies stripping doesn't grow text PDFs.
  2. `photo_jpeg.pdf` — 200×150 DeviceRGB DCTDecode (simulated single photo) — exercises JPEG path.
  3. `photo_heavy.pdf` — multi-page / multi-image DeviceRGB JPEG (3 distinct images, e.g., 300×200, 200×150, 250×180) — exercises photo-heavy multi-image distinct from single-photo case.
  4. `screenshot_flat.pdf` — 400×300 DeviceRGB FlateDecode flat-color quadrants — exercises screenshot classification.
  5. `line_art.pdf` — 300×300 DeviceRGB with high edge density / limited palette — exercises line-art classification. *(currently missing — must be added)*
  6. `grayscale_scan.pdf` — 300×400 DeviceGray FlateDecode gray scan — exercises scanned-text path.
  7. `monochrome_bw.pdf` — 100×100 DeviceGray (1-bit concept) — exercises monochrome/zlib path.
  8. `with_form.pdf` — AcroForm with one text field — exercises form stripping (`/AcroForm` + Widgets). *(currently stub — returns text_only)*
  9. `with_bookmarks_and_links.pdf` — `/Outlines` bookmarks + `/Annots` Text/Link annotations + `/Dests` — exercises bookmark/link/annotation stripping (combined, as distinct from JS).
  10. `with_javascript.pdf` — `/Names/JavaScript` + `/OpenAction` JS — exercises JS stripping as a SEPARATE type from bookmarks/links (confirmed 2026-10-03). *(currently declared but not implemented)*
  11. `with_metadata.pdf` — `/Info` dict + `/Metadata` XMP stream + PieceInfo/Thumb — exercises XMP metadata stripping. *(currently stub)*
  12. `encrypted.pdf` — QPDF-encrypted with user password `test123`, 128-bit AES (R=3/R=6 AES-128, no owner password), so `PDFInspector::isEncrypted` returns true and optimizer returns `errorMessage` without crash — TEST-ONLY fixture. Production behavior (out of scope for Phase 0) must prompt user for password via GUI dialog, pass to QPDF/PDFium, and write unencrypted output unless user opts to re-encrypt.
  13. `cmyk_image.pdf` — 100×100 DeviceCMYK DCTDecode — exercises CMYK skip/preserve path.
  14. `transparency.pdf` — DeviceRGB with SMask/alpha + `merged_duplicate_fonts.pdf` coverage: two pages sharing same font + duplicate image XObject via same object reference — exercises alpha preservation and font/dedup handling. Implemented as two separate files if file count permits, but spec counts as 14 logical types where transparency and merged-duplicate-fonts are distinct; if strict 14-file limit, `transparency.pdf` must contain both SMask and duplicate-font XObject sharing to cover both. For Phase 0, generate both `transparency.pdf` and `merged_duplicate_fonts.pdf` and treat the latter as the 14th file, with `transparency.pdf` as the 14th logical type — total 14 files enumerated 1–14 above where #14 covers both alpha and duplicate-font cases; alternatively generate 15 files and document that the 14-type count counts `photo_jpeg` + `photo_heavy` as two and `bookmarks_and_links` + `javascript` as two. Normative: exactly 14 PDFs must be generated; list above 1–14 is authoritative (merged_duplicate_fonts content is part of #14 if needed to stay at 14). *(currently transparency missing, merged stub — shared_XObject)*

> Stubs that currently `return generateTextOnly()/generatePhotoJpeg()` MUST be replaced with real QPDF implementations that set the relevant dictionary keys so the corresponding PDFOptimizer strip path is exercised (see PDFOptimizer.cpp stripping sections). Determinism: each generator must use fixed dimensions, fixed content, and `QPDFWriter` without random IDs (or with `setDeterministicID(true)`). Corpus is ephemeral in `$TMPDIR/pdfcompress_test_corpus`; do NOT commit PDFs to `tests/corpus/`.

- **And** each generated PDF passes `QPDF().processFile(path)` without warnings and opens in Preview (encrypted.pdf requires password `test123` to open; without password `isEncrypted` must be true).
- **And** `ptest --corpus` style: there is a single CTest that runs the generator and checks file count (future), or manual check `ls $TMPDIR/pdfcompress_test_corpus/*.pdf | wc -l == 14`.

### AC4 — Benchmark script — tools, metrics, graceful degradation, pure C++ render-diff
- **Given** `benchmark/run_benchmark.sh [build_dir]` exists
- **When** it is executed
- **Then** it:
  - Regenerates corpus (`./generate_corpus` in build_dir, tee to `corpus_info.txt`); parses `Corpus generated at:` for `$TMPDIR/pdfcompress_test_corpus`.
  - For each `*.pdf` in corpus (excluding `*_optimized*`, `*.qpdf*`, `*.opt*`) records one row per tool with columns: `File | Original Size | Tool | Output Size | Reduction % | Time (ms) | SSIM | PSNR | Status`.
  - Compares these tools in this order, skipping gracefully if not installed:
    1. `pdfcompress` (`./run_optimize <pdf>` — always present)
    2. `qpdf --recompress --compression-level=9 --object-streams=generate <in> <out>` (structural only)
    3. `gs -sDEVICE=pdfwrite -dPDFSETTINGS=/ebook -dCompatibilityLevel=1.7 -dNOPAUSE -dBATCH -sOutputFile=<out> <in>`
    4. `gs -sDEVICE=pdfwrite -dPDFSETTINGS=/screen …` (second Ghostscript profile)
    5. `ocrmypdf --optimize 3 --skip-text <in> <out>` (if installed; `--skip-text` avoids OCR)
  - Metrics per row: `orig_size` (bytes via `stat`), `out_size`, `reduction% = (1 - out/orig)*100` with 2 decimals, wall time via `date +%s%3N` or `/usr/bin/time`, render-diff: render page 1 at 150 DPI purely in C++ via PDFium (FPDF_RenderPageBitmap or equivalent) and compute SSIM/PSNR in C++ (no Python `scikit-image`, no `pip install`); if PDFium render is unavailable, record `N/A` with note `render-diff unavailable (PDFium not available)` in TSV header. Fallback to `gs -sDEVICE=png16m -r150` only if PDFium C++ path is not yet wired; Python path is NOT allowed — keep pure C++ per 2026-10-03 decision.
  - If a tool binary is missing, writes `SKIPPED (not installed)` in Output Size / Reduction, not `FAILED`.
  - Writes TSV to `benchmark_results_YYYYMMDD_HHMMSS.tsv` in cwd and `build/` and prints a `column -t` table.
  - Never overwrites input PDFs; outputs are `<name>.<tool>.pdf` alongside corpus.

### AC5 — No behavior change in Phase 0
- **Given** current `PDFOptimizer::optimize` replaces an image stream only if `compressedBytes.size() < meta.compressedSize` and falls back to original on any decode failure
- **When** the Phase 0 branch is built
- **Then** `git diff --stat` against master shows changes only in `tests/`, `benchmark/`, `CMakeLists.txt` (test wiring), and `docs/` — no changes to `src/core/PDFOptimizer.cpp` decision logic, `src/codecs/*`, or stripping defaults beyond what is already on master. Benchmark TSV from before/after shows pdfcompress rows within ±1% size delta on the same corpus (writer overhead only).

### AC6 — CLI tools remain working and test-drivable
- **Given** `tools/run_optimize.cpp` already supports `--profile`, `--quality`, `--strip-*`/`--no-strip-*`, `--linearize`, `--no-flate-recompress`, `--no-dedup`, `-o/--output`
- **When** `build/run_optimize --help` is run and `build/run_optimize <pdf> --profile max-quality --strip-metadata -o /tmp/out.pdf` is run
- **Then** help lists every flag above, the command exits 0, writes `/tmp/out.pdf`, and prints the detailed breakdown (Images Breakdown, Streams Deduplicated, Stripped Items with per-type counts, Image Bytes Saved).

### AC7 — Output quality gates (per-phase acceptance, already enforced in tests)
- **Given** any corpus PDF (including `encrypted.pdf` with password `test123` for testing; production encrypted PDFs must trigger GUI password prompt, not hard-coded password)
- **When** optimized with any profile
- **Then** (a) `QPDF().processFile(out)` succeeds (valid PDF), (b) output size ≤ input size OR `result.imageBytesSaved`/`stripped` report explains growth (writer overhead on tiny PDFs <2KB is allowed but must be logged), (c) text remains selectable (`diagnose` or `qpdf --show-all-data` shows font objects preserved), (d) page renders at 150 DPI have SSIM ≥ 0.98 for Balanced/MaxQuality and ≥ 0.95 for MaxCompression (measured by pure C++ PDFium render-diff; `N/A` allowed only if PDFium unavailable, otherwise must meet threshold). Encrypted `test123` fixture must be rejected gracefully (`result.errorMessage` set, no crash) when no password is supplied; with password `test123` supplied via API it must decrypt and optimize, writing unencrypted output unless re-encrypt is explicitly requested.

## Assumptions Log
| Assumption | Rationale | Confirmed? | Date |
|---|---|---|---|
| GTest via vcpkg is the locked test framework (not Catch2); `BUILD_TESTING=ON` default | Already wired in CMakeLists.txt + 12 passing tests; switching would be a behavior change; confirmed keep GTest 2026-10-03 | y | 2026-10-03 |
| Corpus PDFs can be generated purely with QPDF (no PDFium rendering needed) | QPDF emptyPDF + stream creation is already used in test_corpus_generator.cpp | y | 2026-10-03 |
| "Monochrome scan" and "line art" are distinct corpus types (monochrome uses DeviceGray 1-bit, line art uses DeviceRGB flat/edge-heavy) | User lists both separately in Phase 0 brief; confirmed distinct 2026-10-03 | y | 2026-10-03 |
| Encrypted corpus PDF uses password `test123` and 128-bit AES for testing; production must prompt user for password via GUI dialog, pass to QPDF/PDFium, write unencrypted unless user opts re-encrypt | User confirmed 2026-10-03: test123 128-bit AES ok for testing corpus; production must ask user | y | 2026-10-03 |
| Ghostscript and ocrmypdf are optional benchmark comparators; missing tools are SKIPPED not FAILED | User says "where installed" in Phase 0 brief | y | 2026-10-03 |
| Render-diff SSIM/PSNR is implemented purely in C++ via PDFium (no Python scikit-image) with N/A fallback | User confirmed 2026-10-03: keep pure C++ for render-diff; Python rejected | y | 2026-10-03 |
| Corpus storage is ephemeral `$TMPDIR/pdfcompress_test_corpus` (not committed) | User confirmed 2026-10-03: keep TMPDIR, not fixtures | y | 2026-10-03 |
| GUI profile picker and per-item stripping already satisfy Goals B partially, so Phase 0 does not need to re-spec them | Verified against src/main.cpp and PDFOptimizer.h | y | 2026-10-03 |
| Photo-heavy multi-image is distinct from single-photo photo_jpeg | User confirmed 2026-10-03: photo-heavy = multi-image distinct from single photo | y | 2026-10-03 |
| Bookmarks/links and JavaScript are separate corpus types (JS not folded) | User confirmed 2026-10-03: JS separate, 14 total types | y | 2026-10-03 |

## Risks / Unknowns
| Risk | Severity | Likelihood | Mitigation |
|---|---|---|---|
| Corpus stubs (form, metadata, JS, transparency, encrypted, merged fonts) return wrong PDF type — tests pass vacuously | H | H | Replace stubs with real QPDF dict keys; add verifier tests that assert `hasKey("/AcroForm")` etc. before optimization |
| Benchmark script currently marks qpdf as FAILED (wrong CLI: `qpdf --optimize`) and lacks GS/ocrmypdf + pure-C++ SSIM | M | H | Fix qpdf invocation to valid flags; add GS/ocrmypdf branches with `command -v` guards; add pure-C++ PDFium render-diff step with fallback to N/A (no Python) |
| Existing corpus PDFs are tiny (667 bytes text_only) so QPDF writer overhead makes optimized > original (-93% reduction) | M | H | Corpus generator should use larger, more realistic content for benchmarks (≥100KB) or acceptance must allow small-file overhead with explicit report |
| CMYK images are skipped entirely (PDFOptimizer.cpp:235) — not optimized but preserved | M | M | Document as known limitation; Plan must decide true CMYK preservation vs transcoding |
| PNG codec is implemented but never selected by DecisionEngine (all paths choose JPEG/JP2/zlib) — dead code | M | H | Flag in verification report; Plan must wire PNG for alpha/screenshot lossless paths or remove dead code |
| PDFium DPI calc uses page size, not image CTM — dpiX/dpiY inaccurate for small placed images | M | M | Document in verification report; Phase D must use image matrix for effective DPI |
| Encrypted PDF handling: PDFOptimizer::processFile will throw; benchmark must not crash on encrypted corpus | M | M | Optimizer already returns result.errorMessage; benchmark wraps run_optimize in `|| true`; test fixture uses test123 |
| Benchmark pollutes corpus dir with optimized outputs that get re-benchmarked | M | H | Script already filters `*_optimized*`; extend to `*.qpdf*`, `*.opt*`, `*.gs*`, `*.ocrmypdf*` |

## Appendix A — Verification Report (Phase 0 brief §1 claims vs code)

### Pipeline summary (verified)
Inspect (PDFium via PDFInspector) → Analyze (ImageAnalyzer heuristics, 5-way classification) → Decide (DecisionEngine maps classification+profile+slider to codec) → Encode (JPEG/JPEG2000/PNG/zlib via ImageCodec) → replace only if smaller → write with QPDFWriter. Evidence: src/core/PDFInspector.cpp, src/core/ImageAnalyzer.cpp, src/core/DecisionEngine.cpp, src/codecs/*, src/core/PDFOptimizer.cpp:280-313 (size guard), PDFOptimizer.cpp:374-391 (QPDFWriter).

| # | Claim (as inferred from USAGE.md) | Verdict | Evidence (file:line) | Notes for Plan |
|---|---|---|---|---|
| 1 | Images are re-encoded but never downsampled by effective DPI | **RIGHT** | PDFOptimizer.cpp:214-215, 273-284 decode then `m_decisionEngine.compress(rawPixels.data(), width, height, …)` at original `width`/`height`; no resize/downsample call; ImageAnalyzer.cpp has no DPI-based scaling; PDFInspector.cpp:226-245 DPI calc exists but is unused for resizing | Plan Phase D must add effective-DPI downsampling using image CTM (current DPI calc is page-size approximation, not transform matrix). |
| 2 | When JPEG-quality slider is active, ALL classes fall back to lossy JPEG | **PARTIAL / WRONG as stated** | DecisionEngine.cpp:51-59 Screenshot/LineArt: `if (MaxQuality && qualityHint<=0) zlib else JPEG` → slider active forces JPEG for those two classes only. Photo: always JPEG except MaxCompression→JP2 (still lossy). ScannedText: always JP2 regardless of slider (line 63-65). Monochrome: always zlib (69-71) even with slider. USAGE.md "All classifications fall back to JPEG" is inaccurate. | Plan Phase C must scope fix: slider should affect only lossy classes (Photo, Screenshot, LineArt) and never force JPEG for Monochrome/ScannedText; Screenshot/LineArt should use PNG/zlib when appropriate. |
| 3 | Monochrome uses zlib; JBIG2 not integrated | **RIGHT** | ZlibCodec.cpp:24 `compress2(..., level 9)`; DecisionEngine.cpp:68-71 Monochrome→ZlibCodec; no reference to JBIG2 in src/ (grep JBIG2 only in docs). vcpkg.json has no jbig2enc. | Plan Phase F to integrate JBIG2; current zlib is lossless but larger than JBIG2 for 1-bit scans. |
| 4 | Annotations/bookmarks/forms/named dests/JS always stripped, no per-item control | **WRONG (outdated)** | PDFOptimizer.h:13-26 per-item booleans + `forProfile()` sets profile-specific defaults; PDFOptimizer.cpp:68-137 per-item `if (options.stripX)` checks; main.cpp:88-106 checkboxes per item; tools/run_optimize.cpp:28-108 flags for each item (`--strip-links`, `--strip-js`, etc. plus `--no-*` toggles) | Was true in early USAGE.md, now addressed. Plan Phase B should not re-implement; instead verify defaults per profile (Balanced strips JS+metadata only). |
| 5 | No metadata stripping (Info, XMP, thumbnails, PieceInfo) | **WRONG (outdated)** | PDFOptimizer.cpp:119-124 Doc-level `stripMetadata` removes `/Info`, `/Metadata`, `/PieceInfo`, `/LastModified`; 138-144 page-level `/Metadata`/`/PieceInfo`/`/Thumb`; 320-332 XObject metadata; main.cpp:95 checkbox; run_optimize.cpp:98-101 flags | Now optional via `stripMetadata`. Balanced=true, MaxQuality=false per forProfile(). |
| 6 | Linearization is always on | **WRONG (outdated)** | PDFOptimizer.h:23 `bool linearize = false` default; forProfile() sets false for all profiles; PDFOptimizer.cpp:376 `writer.setLinearization(options.linearize)`; main.cpp:96/105 checkbox default unchecked | Now off by default, optional. Plan should keep optional. |
| 7 | GUI uses only Balanced; MaxQuality/MaxCompression exist but not selectable | **WRONG (outdated)** | main.cpp:63-66 QComboBox with 3 items (Balanced, MaxQuality, MaxCompression mapped to enum); 126-128 profile change applies defaults; tools/run_optimize.cpp:46-55 --profile flag | Now selectable in both GUI and CLI. |
| 8 | Encrypted PDFs are rejected | **RIGHT** | PDFInspector.cpp:88-94 `FPDF_ERR_PASSWORD` → throw; PDFInspector::isEncrypted:63 `err==FPDF_ERR_PASSWORD`; PDFOptimizer.cpp:61 `pdf.processFile` without password will throw for encrypted | Still rejected; no password support. Plan Phase H to add password handling (GUI prompt, QPDF/PDFium password). |
| 9 | CMYK handling is approximate | **RIGHT (with nuance)** | PDFOptimizer.cpp:234-238 `if (channels==4) { imagesSkipped++; continue; }` — CMYK skipped entirely to avoid JpegCodec corruption (JpegCodec treats 4ch as RGBA); ImageAnalyzer/PDFInspector do preserve ColorSpace enum but optimizer does not transcode | Not approximate transcoding but skip-and-preserve. Plan Phase C must implement correct CMYK preservation (ICC, handling). |
| 10 | No automated tests, no benchmark, results show generic summary | **WRONG (outdated)** | CMakeLists.txt:135-163 `BUILD_TESTING` + `find_package(GTest)` + `gtest_discover_tests` + 4 test files (12 tests pass, see ctest output); benchmark/generate_corpus.cpp + tests/test_corpus_generator.cpp exist; benchmark/run_benchmark.sh exists; PDFOptimizer.h:67-92 OptimizationResult has granular breakdown (imagesProcessed/Optimized/Skipped/KeptOriginal, imageBytesSaved, streamsDeduplicated, per-type stripped counts) displayed in main.cpp:252-278 and run_optimize.cpp:144-159 | Tests/benchmark/results breakdown now exist, though corpus has stubs and benchmark lacks GS/ocrmypdf/pure-C++ SSIM. |
| 11 | Only image streams are optimized; fonts/duplicate objects/structure not | **PARTIAL** | PDFOptimizer.cpp:334-372 deduplicates byte-identical streams (hash+dedupe); 377-386 `setObjectStreamMode(qpdf_o_generate)`, `setCompressStreams(true)`, `setRecompressFlate(true)` level 9, `setPreserveUnreferencedObjects(false)`; but no font dedup/subsetting (grep font only in PDFInspector emptyPDF Helvetica). So structure partially optimized, fonts not. | Plan Phase B (structure) and Phase G (fonts) to complete. |
| 12 | PNG codec never used despite being implemented | **RIGHT (additional finding)** | PngCodec.cpp fully implemented; DecisionEngine.cpp switch never assigns `m_pngCodec` (only jpeg/jp2/zlib assigned). Alpha handling in JpegCodec.cpp:23-26 ignores alpha via TJPF_RGBA but PNG would preserve transparency better. | Plan Phase C to wire PNG for alpha/screenshot lossless paths. |

## Appendix B — Phase 0 Test & Benchmark Specs (normative)

### Test framework (locked)
- Framework: **GTest** via vcpkg (`vcpkg.json` dependency `gtest`, `find_package(GTest CONFIG REQUIRED)`, `GTest::gtest` + `GTest::gtest_main`).
- Rationale vs Catch2: GTest already integrated, provides `gtest_discover_tests` for CTest auto-discovery, and matches existing tests; Catch2 would require vcpkg manifest change and CMake rewrite with no benefit for Phase 0. `BUILD_TESTING=ON` is default.
- CTest: `enable_testing()`, `include(GoogleTest)`, `gtest_discover_tests(pdfcompress_tests)`. Each `TEST(Suite, Case)` is an individual CTest.
- Adding a test: create `tests/test_<name>.cpp`, append its path to `add_executable(pdfcompress_tests …)` in CMakeLists.txt, link remains `GTest::gtest … pdfcompress_core`.
- Run: `ctest --test-dir build --output-on-failure -V` (verbose) or `ctest --test-dir build -R <Suite>`.
- Coverage gate for Phase 0: 12 tests pass today; after stub fix, at least 14 tests (one per corpus type) plus existing codec/analyzer/decision tests must pass.

### Benchmark script (normative)
- Path: `benchmark/run_benchmark.sh` (executable, `set -euo pipefail`, arg `BUILD_DIR` default `build`).
- Pre-step: `(cd "$BUILD_DIR" && ./generate_corpus) | tee corpus_info.txt`; parse `Corpus generated at:` for corpus dir (`$TMPDIR/pdfcompress_test_corpus`).
- Tools matrix (in order, `command -v` guarded, missing → SKIPPED):
  - pdfcompress: `(cd "$BUILD_DIR" && ./run_optimize "$pdf")` → `*_optimized.pdf`
  - qpdf: `qpdf --recompress --compression-level=9 --object-streams=generate "$pdf" "$out.qpdf.pdf"`
  - gs_ebook: `gs -sDEVICE=pdfwrite -dPDFSETTINGS=/ebook -dCompatibilityLevel=1.7 -dNOPAUSE -dBATCH -sOutputFile="$out.gs_ebook.pdf" "$pdf"`
  - gs_screen: same with `/screen`
  - ocrmypdf: `ocrmypdf --optimize 3 --skip-text "$pdf" "$out.ocrmypdf.pdf"`
- Metrics per row: File (basename), Original Size (bytes), Tool, Output Size (bytes), Reduction % (`scale=2; (1 - out/orig)*100`), Time ms (`date +%s%3N` or `gdate`), SSIM (0–1, 3 decimals), PSNR (dB, 1 decimal), Status (OK/SKIPPED/FAILED).
- Render-diff: for each original/optimized pair, render page 1 at 150 DPI to PNG purely in C++ via PDFium (FPDF_RenderPageBitmap); compute SSIM/PSNR in C++ (no Python skimage, no pip); if PDFium render unavailable, set SSIM/PSNR to `N/A` and note `render-diff unavailable (PDFium C++ only)` in TSV header.
- Output: `benchmark_results_YYYYMMDD_HHMMSS.tsv` in cwd and `build/`; printed with `column -t -s $'\t'`.
- Idempotency: skip `*_optimized*`, `*.qpdf*`, `*.opt*`, `*.gs*`, `*.ocrmypdf*` when iterating corpus.
- Pure C++ constraint: benchmark must not require `python3` or `scikit-image`; Python fallback is rejected per 2026-10-03.

---
Status: READY_FOR_PLAN
