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
- **metadataStripped (`OptimizationResult`):** reports *intent*, not actual removal — `true` iff `options.stripMetadata` was applied (i.e. `OptimizationOptions.stripMetadata==true`). This pins current code behavior (`PDFOptimizer.cpp` sets `result.metadataStripped=true` unconditionally whenever `stripMetadata` is true) with no behavior change per the Phase 1 no-strip-semantics-change lock. All six removal counters (`linksRemoved`, `annotationsRemoved`, `bookmarksRemoved`, `formsRemoved`, `jsRemoved`, `namedDestinationsRemoved`) report actual removals; this bool is the sole exception. Ruled 2026-10-07 per ESC-008; see Phase 1 AC-B2 normative note.
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

---

# PART II — Full Scope: Phases 1–8 (Goals B–I)

> Phase 0 (Part I above, Status: READY_FOR_PLAN baseline with Appendix A + B) is
> COMPLETE and locked. Everything below EXTENDS it — no Phase 0 section above is
> rewritten. Verified-existing baseline (commits 67a54b9/9441c8c) is ground truth:
> per-item stripping options WITH profile defaults (`OptimizationOptions::forProfile`),
> metadata stripping, linearization off-by-default, 3-profile GUI picker + CLI
> `--profile`, GTest + CTest wiring, corpus generator (14 types), benchmark script,
> and granular `OptimizationResult` ALL ALREADY EXIST. Phases 1–8 verify-and-complete
> where code exists and build-only where Appendix A proves a gap (RIGHT/PARTIAL items).
> Genuinely-open items from §1 re-verification: no effective-DPI downsampling (RIGHT),
> slider forces JPEG for Screenshot/LineArt only not all classes (PARTIAL,
> DecisionEngine.cpp:51-71), monochrome=zlib no JBIG2 (RIGHT), encrypted rejected
> (RIGHT), CMYK skipped-not-transcoded (RIGHT nuance), structure partial / fonts not
> (PARTIAL), PNG codec dead code never selected (RIGHT).

## Standing Acceptance Criteria (apply to EVERY Phase 1–8)

These gates are evaluated per-phase in addition to each phase's own AC. A phase is
not done unless all standing gates pass on the 14-file canonical corpus
(`$TMPDIR/pdfcompress_test_corpus`, password `test123` for encrypted.pdf):

- **S1 — Tests pass:** `cmake --build build && ctest --test-dir build --output-on-failure`
  is 100% green (allowed skip: render-diff `N/A` only when PDFium unavailable).
- **S2 — Size guard:** no output is larger than its input unless `OptimizationResult`
  (`imageBytesSaved` / stripped counts / `errorMessage`) explains the growth in both
  `run_optimize` stdout and the benchmark TSV `Status` column. Tiny PDFs (<2KB) are
  exempt from the reduction% gate but must still log the reason.
- **S3 — Render match:** page-1 150-DPI pure-C++ PDFium render-diff SSIM ≥ 0.98 for
  MaxQuality/Balanced and ≥ 0.95 for MaxCompression; `N/A` allowed only with TSV
  header note `render-diff unavailable (PDFium C++ only)`. No Python in the path.
- **S4 — Selectable text:** `diagnose` (or `qpdf --show-all-data`) shows font objects
  preserved and text extractable after optimize, except where the active profile
  explicitly strips the containing element (logged in result breakdown).
- **S5 — Viewer check:** each phase's manual-test PDFs open in Preview + one other
  viewer (Adobe Reader or Chrome) without repair dialogs.
- **S6 — Per-phase benchmark before/after gate:** `benchmark/run_benchmark.sh` is run
  on the same corpus before and after the phase's change; the phase's AC states the
  expected direction (e.g. Phase 1: pdfcompress rows within ±2% unless stripping
  newly enabled; Phase 3: photo/scan PDFs shrink vs Phase-2 baseline with SSIM still
  ≥ gate). TSVs are kept (`benchmark_results_*.tsv`) and the delta is reported.
- **S7 — CLI parity:** `run_optimize` keeps working and gains a flag for every new
  user-visible option introduced by the phase; `diagnose` keeps working.
- **S8 — Docs:** `USAGE.md` is updated for every user-visible change introduced by
  the phase.
- **S9 — Hygiene:** never overwrite input (`<input>_optimized.pdf` or `-o` path only);
  per-image/page/cleanup steps wrapped with fallback to original + log (existing
  `PDFOptimizer` try/catch + size-guard pattern); C++20 RAII, `-Wall -Wextra` clean
  on `pdfcompress_core`; fully offline (PDFium fetch at configure time is the sole
  exception); GUI work off the UI thread with cancel support.

## Phase 1 (Goal B) — Structure & Profile-Defaults Verify-and-Complete

**Problem statement.** Structure optimization (object streams, Flate recompress L9,
stream dedup, per-item stripping, linearization option) and the three profile
defaults are already implemented (`PDFOptimizer.h:27-64 forProfile`,
`PDFOptimizer.cpp:68-137 strip branches, 334-398 dedup+writer`, GUI picker
`main.cpp:63-66`, CLI flags). But no test locks the spec defaults
(MaxQuality-strips-nothing / Balanced-strips-JS+metadata-only /
MaxCompression-strips-everything) and dedup/object-stream behavior is only
incidentally covered. Phase 1 verifies the defaults against `forProfile`, adds
verifier tests, and completes small gaps (e.g. `/AA` + `/OpenAction` non-JS handling,
`--linearize` documentation) — it does NOT re-implement stripping.

**Goals**
- Lock `forProfile` defaults per profile with GTest verifier tests (7 strip booleans
  + `linearize=false` + `recompressFlate=true` + `deduplicateStreams=true` per profile).
- Verify object-stream mode (`qpdf_o_generate`), Flate recompress L9, unreferenced
  object pruning, and dedup counting (`streamsDeduplicated`) on the corpus.
- Confirm `run_optimize --strip-*/--no-strip-*` and `--linearize/--no-flate-recompress/--no-dedup`
  round-trip to `OptimizationOptions` correctly.

**Non-Goals**
- No codec, DPI, JBIG2, font, encryption, or GUI redesign work (Phases 2–8).
- No change to strip semantics — only verification + tests + doc gaps. (Locked 2026-10-04: `forProfile` defaults are normative — MaxQuality-strips-nothing / Balanced-strips-JS+metadata-only / MaxCompression-strips-everything, `linearize=false`.)

**Manual test PDFs (Phase 1):** `with_form.pdf`, `with_bookmarks_and_links.pdf`,
`with_javascript.pdf`, `with_metadata.pdf`, `transparency.pdf` (+merged-dedup coverage),
`text_only.pdf` (no-growth control).

### User Journeys (Phase 1)

#### J-B1 — Developer locks profile defaults
- **Preconditions:** Master builds; corpus generated (14 files).
- **Steps:** 1. Read `OptimizationOptions::forProfile` 2. Run new
  `TEST(ProfileDefaults, ...)` per profile 3. Run `run_optimize <pdf> --profile
  <each>` on the four stripping corpus PDFs and compare `OptimizationResult`
  breakdowns.
- **Postconditions:** Defaults table in USAGE.md matches code; tests pin it.
- **Edge Cases:** User passes `--profile balanced --strip-bookmarks` (explicit flag
  overrides profile default) → explicit flag wins and result logs the override.

#### J-B2 — User strips selectively via GUI/CLI
- **Preconditions:** GUI built; `with_javascript.pdf` + `with_metadata.pdf` present.
- **Steps:** 1. Open GUI, pick Balanced, uncheck "Strip JavaScript" 2. Optimize
  3. Re-run via CLI with `--no-strip-js`.
- **Postconditions:** JS preserved when unchecked; metadata still stripped per default;
  result breakdown shows `jsRemoved=0, metadataStripped=true`.
- **Edge Cases:** PDF has `/OpenAction` that is a GoTo (not JavaScript) → stripping JS
  must NOT remove it; verifier asserts GoTo survives.

### Acceptance Criteria (Phase 1)

#### AC-B1 — Profile defaults locked (verify, not build)
- **Given** `OptimizationOptions::forProfile` as on master (MaxQuality all-false;
  Balanced JS+metadata true, rest false; MaxCompression all-true; all
  `linearize=false, recompressFlate=true, deduplicateStreams=true`)
- **When** `TEST(ProfileDefaults, MaxQualityStripsNothing)`,
  `TEST(ProfileDefaults, BalancedStripsJsAndMetadataOnly)`,
  `TEST(ProfileDefaults, MaxCompressionStripsEverything)` run
- **Then** each asserts all 7 strip booleans + 3 structural flags exactly; any
  future default change fails the test and requires a requirements change first.

#### AC-B2 — Stripping end-to-end per corpus type
- **Given** the four stripping corpus PDFs with real dict keys
- **When** optimized under each profile via `run_optimize --profile <p>`
- **Then** MaxQuality removes 0 items on all four; Balanced removes JS (`jsRemoved≥1`
  on `with_javascript.pdf`) and metadata (`metadataStripped=true` on
  `with_metadata.pdf`) and nothing else; MaxCompression removes ≥1 item on each of
  the four; non-JS `/OpenAction` GoTo survives all profiles.
- **Normative — ESC-008 ruling (locked 2026-10-07, no behavior change):** `metadataStripped` means "the strip-metadata option was applied" (option (a)), NOT "metadata entries were actually found and removed". This locks current code behavior (`PDFOptimizer.cpp`: `result.metadataStripped=true` unconditionally whenever `options.stripMetadata` is true) as the specified semantic, per the Phase 1 no-strip-semantics-change lock. Consequences: (1) Balanced reports `metadataStripped=true` on ALL inputs, including metadata-free PDFs (`with_javascript.pdf`, `with_form.pdf`, `with_bookmarks_and_links.pdf`); (2) tests MUST assert `metadataStripped=true` on `with_metadata.pdf` under Balanced/MaxCompression but MUST NOT assert `metadataStripped==false` on metadata-free PDFs under those profiles; (3) the "and nothing else" in the `Then` above scopes to the six removal counts (`==0`) only. An "actually removed" semantic (option (b)) is a deferred product decision for a later phase — it requires a code change plus user approval and is explicitly OUT of Phase 1.

#### AC-B3 — Structural writer options verified
- **Given** `photo_heavy.pdf` (multi-image) and the transparency/dedup PDF
  (`generateSharedXObject()` → `generateTransparency()`)
- **When** optimized with defaults (`deduplicateStreams=true, recompressFlate=true`)
  vs `--no-dedup --no-flate-recompress`
- **Then** default output is ≤ no-opt output; `streamsDeduplicated == 0` on the
  shared-XObject PDF with defaults **and** with `--no-dedup`; `qpdf --check` clean both.
- **Normative — ESC-010 ruling (locked 2026-10-07, no behavior change):** Phase 1 pins
  the **verified inert-dedup behavior**: `streamsDeduplicated == 0` under defaults on
  every Phase 1 fixture, including the shared-XObject PDF. Both causes are verified
  in-tree: (1) the ESC-001 guard at `src/core/PDFOptimizer.cpp:373`
  (`if (it->second.isIndirect()) continue;`) is always taken, because every
  `seenStreams` handle comes from `pdf.getAllObjects()` and the line-338 filter
  (`!obj.isIndirect()`) admits only indirect objects, so `streamsDeduplicated++` at
  line 377 is dead code; (2) the shared-XObject fixture `generateSharedXObject()`
  returns `generateTransparency()` (`tests/test_corpus_generator.cpp:450-451`), which
  places one indirect image object by reference on both pages, so no distinct duplicate
  pair exists — the `obj.getObjGen() != it->second.getObjGen()` guard at line 366 would
  skip it even with a working replace path. The Phase 1 verify-only lock forbids the
  product fix (live dedup replace path) and the corpus change (distinct byte-identical
  streams) that `>= 1` would require, so `streamsDeduplicated ≥ 1` as originally written
  is unsatisfiable in Phase 1.

  **DEFERRED follow-up (explicitly not dropped) — OWNED BY PHASE 2 (user-confirmed
  2026-10-07, ESC-010-FU).** The aspirational `streamsDeduplicated ≥ 1` assertion is
  deferred to **Phase 2 (Goal C)**, the first product-code-carrying phase after Phase 1,
  which owns the fix inside its existing structural-correctness scope. Phase 6 (Goal G —
  Font Dedup + Subsetting) is NOT the owner and stays font-scoped. Deferred work =
  (a) **product:** replace the inert ESC-001 indirect-handle skip at
  `src/core/PDFOptimizer.cpp:373` with a correct **direct-copy `replaceObject`** path in
  `PDFOptimizer.cpp` (pass a direct copy of the replacement stream so QPDF accepts it) so
  byte-identical *distinct* streams dedup and `streamsDeduplicated` increments;
  (b) **corpus:** stage a **`$TMPDIR` fixture containing two distinct byte-identical
  streams** (two separate indirect objects, NOT the reference-shared
  `transparency.pdf`/`generateSharedXObject()` fixture) and re-verify
  `streamsDeduplicated ≥ 1`; and (c) **test:** the Phase-1 P1-T3 test pinning `== 0` must
  be revisited/rewritten to assert `>= 1` in the same phase the fix lands. See Phase 2
  Goals and AC-C5.
- **Test note (revisit trigger):** the current P1-T3 test (`tests/test_structure_writer.cpp`,
  asserting `streamsDeduplicated == 0`) documents this dead-code state and is the correct
  Phase 1 pin, not an accommodation of a defect. It MUST be rewritten to assert `>= 1` in
  Phase 2 (the phase that lands the deferred dedup fix, AC-C5); until then `== 0` is
  normative for Phase 1.

#### AC-B4 — Phase 1 benchmark gate
- **Given** TSV from before Phase 1 on the 14-file corpus
- **When** re-run after Phase 1 (no compression-logic change expected)
- **Then** pdfcompress rows are within ±2% size on every file (writer/flag-plumbing
  only); any larger delta fails the phase.

## Phase 2 (Goal C) — Codec-Selection Correctness

**Problem statement.** `DecisionEngine.cpp:51-71` forces JPEG whenever the quality
slider is active for Screenshot/LineArt (correct only for lossy classes), never
selects the fully-implemented `PngCodec` (dead code — alpha silently flattened via
`TJPF_RGBA`), always routes ScannedText to JP2 regardless of slider, and
`PDFOptimizer.cpp:234-238` skips CMYK entirely (`imagesSkipped++`) to avoid
`JpegCodec` RGBA corruption. Phase 2 scopes the slider to lossy classes only, wires
PNG for alpha/lossless paths, and implements correct CMYK preservation (not blind
transcoding). Phase 2 also owns the deferred structural stream-dedup activation
(ESC-010 follow-up, user-confirmed 2026-10-07): replacing the inert ESC-001
indirect-handle guard with a direct-copy `replaceObject` path (see AC-C5).

**Goals**
- Slider (`qualityHint>0`) affects ONLY Photo/Screenshot/LineArt; Monochrome and
  ScannedText ignore it (Monochrome always lossless; ScannedText profile-driven).
- PNG selected for: alpha images (`hasAlpha`), and Screenshot/LineArt under
  MaxQuality with no slider (replacing zlib where PNG is smaller — size guard decides).
- CMYK images preserved with correct color space (ICC/DeviceCMYK round-trip, no
  RGBA corruption); optimizer reports `imagesSkipped` only when preservation (not
  optimization) was the correct action, with a logged reason. Opt-in transcoding
  via `--transcode-cmyk-to-rgb` (CLI) + GUI checkbox per AC-C3b.
- `run_optimize --quality` + `--profile` semantics documented in USAGE.md.
- **Stream dedup activation (structural correctness — deferred ESC-010 follow-up, owned
  by Phase 2 per user 2026-10-07):** replace the inert ESC-001 indirect-handle guard
  (`src/core/PDFOptimizer.cpp:373`) with a correct **direct-copy `replaceObject`** path so
  two distinct-but-byte-identical indirect streams are actually deduplicated and
  `streamsDeduplicated` increments; add a staged `$TMPDIR` fixture with two distinct
  byte-identical streams (NOT the reference-shared `transparency.pdf` fixture); and rewrite
  the Phase-1 P1-T3 `streamsDeduplicated == 0` test to assert `>= 1` in this phase. No
  font/JBIG2/DPI scope is pulled in.

**Non-Goals**
- No downsampling (Phase 3), no JBIG2 (Phase 5). CMYK→RGB transcoding is IN scope
  ONLY as an opt-in `--transcode-cmyk-to-rgb` flag per AC-C3b (default: preservation-only).

**Manual test PDFs (Phase 2):** `screenshot_flat.pdf`, `line_art.pdf`,
`photo_jpeg.pdf`, `transparency.pdf` (alpha), `cmyk_image.pdf`, `monochrome_bw.pdf`
(control: stays zlib), `grayscale_scan.pdf` (control: stays JP2), plus a new staged
`$TMPDIR` fixture `distinct_duplicate_streams.pdf` — two **distinct** indirect streams
with byte-identical raw data (NOT the reference-shared `transparency.pdf` fixture; not
added to the 14 canonical files) for AC-C5.

### User Journeys (Phase 2)

#### J-C1 — Screenshot compresses losslessly at MaxQuality, lossy with slider
- **Preconditions:** `screenshot_flat.pdf` present; GUI/CLI built.
- **Steps:** 1. `run_optimize shot.pdf --profile max-quality -o q.pdf` 2. Re-run with
  `--quality 70` 3. Compare sizes + SSIM.
- **Postconditions:** (1) uses PNG/zlib (FlateDecode) with SSIM 1.0; (2) uses JPEG
  (DCTDecode) smaller with SSIM ≥ 0.98; filters recorded in result/diagnose.
- **Edge Cases:** PNG output larger than original → size guard keeps original and
  reports `imagesKeptOriginal=1` (never writes larger silently).

#### J-C2 — CMYK PDF survives with color intact
- **Preconditions:** `cmyk_image.pdf` present.
- **Steps:** 1. Optimize Balanced 2. Open before/after in Chrome + Preview 3. `qpdf
  --json` check ColorSpace.
- **Postconditions:** Output ColorSpace still DeviceCMYK (or ICC-based CMYK), visual
  match, no corruption; result logs skipped-with-reason or bytes saved.
- **Edge Cases:** CMYK JPEG with Adobe APP14 transform → decoder must honor transform
  or skip-with-log rather than corrupt.

### Acceptance Criteria (Phase 2)

#### AC-C1 — Slider scoped to lossy classes
- **Given** one PDF per classification (photo, screenshot, line-art, scanned-text,
  monochrome)
- **When** each is optimized with `--profile max-quality --quality 70` vs `--profile
  max-quality` without `--quality`
- **Then** photo/screenshot/line-art outputs differ (slider honored, JPEG path);
  scanned-text outputs are byte-comparable in codec choice (JP2 both, slider ignored);
  monochrome outputs are byte-comparable (zlib/Flate both, slider ignored);
  `TEST(DecisionEngine, SliderOnlyAffectsLossyClasses)` pins this mapping.

#### AC-C2 — PNG wired (dead code eliminated)
- **Given** `transparency.pdf` (hasAlpha) and `screenshot_flat.pdf`
- **When** optimized `--profile max-quality` (no `--quality`)
- **Then** alpha image filter is FlateDecode-or-PNG (never DCTDecode); alpha pixels
  round-trip (render-diff SSIM 1.0 within rounding); `m_pngCodec` is reachable by at
  least one `TEST(DecisionEngine, ...)`; JPEG-with-alpha flattening no longer occurs
  on this path.

#### AC-C3 — CMYK preserved, not corrupted
- **Given** `cmyk_image.pdf`
- **When** optimized under any profile
- **Then** output passes `qpdf --check`, ColorSpace remains CMYK-family, renders match
  (SSIM ≥ 0.98), and result is either `imagesSkipped=1` with reason logged or
  `imageBytesSaved≥0` with color intact — never corrupted channels.

#### AC-C3b — CMYK→RGB transcode opt-in (locked 2026-10-04)
- **Given** `cmyk_image.pdf`
- **When** optimized with `--transcode-cmyk-to-rgb` (or GUI checkbox checked)
- **Then** output ColorSpace is DeviceRGB (or ICC-based sRGB), renders match the
  original with SSIM ≥ 0.98 (standing gate S3 is the color-shift threshold), size
  gate S2 applies, and the result breakdown logs `transcodedCmyk=1`; without the
  flag, AC-C3 preservation behavior applies unchanged. Plan chooses the
  implementation-side color-difference metric (e.g. ΔE sampling); the SSIM gate
  above is the normative acceptance threshold.
- **And** `run_optimize --help` lists `--transcode-cmyk-to-rgb`; USAGE.md documents
  that transcoding may shift spot/press colors and is off by default.

#### AC-C4 — Phase 2 benchmark gate
- **Given** pre-Phase-2 TSV baseline
- **When** re-run after Phase 2
- **Then** screenshot/line-art MaxQuality rows change codec (Flate vs DCT) with SSIM
  1.0; photo rows within ±5% (JPEG params only); cmyk row no longer silently skipped
  without reason; all rows still satisfy S2–S5.

#### AC-C5 — Stream dedup activated (deferred ESC-010 follow-up, owned by Phase 2)
- **Given** a staged `$TMPDIR` fixture containing two **distinct** indirect stream objects
  with byte-identical raw data and matching signatures (same `/Subtype /Width /Height
  /ColorSpace /BitsPerComponent`) — separate from the reference-shared `transparency.pdf`
  fixture, and not added to the 14 canonical corpus files
- **When** optimized with defaults (`deduplicateStreams=true`) and again with `--no-dedup`
- **Then** with defaults `result.streamsDeduplicated >= 1` and the output shares one stream
  object for that pair (`qpdf --check` clean; `qpdf --json` shows a single stream); with
  `--no-dedup` `streamsDeduplicated == 0` and both stream objects remain; output satisfies
  S2–S5; and the Phase-1 test that currently pins `streamsDeduplicated == 0`
  (`tests/test_structure_writer.cpp`, P1-T3) is rewritten to assert `>= 1` against this
  fixture in this same phase.
- **Implementation note:** replace the ESC-001 skip
  (`if (it->second.isIndirect()) continue;` at `src/core/PDFOptimizer.cpp:373`) with a
  direct-copy replacement — pass a direct copy of the replacement stream to
  `QPDF::replaceObject(obj.getObjGen(), directCopy)` so QPDF accepts it — keeping the
  existing try/catch so one bad pair never fails the whole file. `seenStreams` may keep
  storing indirect handles for signature comparison; only the object handed to
  `replaceObject` must be a direct copy.

## Phase 3 (Goal D) — Effective-DPI Downsampling

**Problem statement.** Images are re-encoded at original pixel dimensions
(`PDFOptimizer.cpp:214-215, 273-284` — no resize call; verified RIGHT). Oversampled
scans/photos (e.g. 600 DPI placed at 2 inches = 1200px for 300px of print) bloat
output. Current `PDFInspector.cpp:226-245` DPI calc is a page-size approximation, not
the image CTM, so it cannot drive resampling. Phase 3 computes per-image effective
DPI from the placement matrix and downsamples to profile targets with a floor.

**Goals**
- Effective DPI per image from CTM (image placement matrix, honoring crop/rotation);
  fallback to page-size approximation only when CTM unavailable (logged).
- Profile targets (defaults; CLI-overridable): MaxQuality 300 DPI, Balanced 150 DPI,
  MaxCompression 96 DPI; never upscale; never downsample vector/thumbnail or images
  already below target; minimum dimension floor 32px preserved. (Locked 2026-10-04.)
- High-quality resample (e.g. Lanczos/bicubic) in the decode→analyze→resample→encode
  path; `/Width /Height` + CTM updated consistently so print size is unchanged.

**Non-Goals**
- No change to codec choice logic (Phase 2 owns it); downsampling only changes pixel
  dimensions fed to the existing path.

**Manual test PDFs (Phase 3):** oversampled scan (600 DPI photo placed small),
`photo_heavy.pdf`, `grayscale_scan.pdf`, `line_art.pdf` (edge preservation check),
tiny-icon PDF (floor check: must NOT shrink below legibility).

### User Journeys (Phase 3)

#### J-D1 — Oversampled scan shrinks, print size unchanged
- **Preconditions:** 600-DPI scan PDF present.
- **Steps:** 1. `diagnose` shows effective DPI ~600 2. `run_optimize --profile
  balanced` 3. Compare sizes + print dimensions (`qpdf --json` Width/Height + CTM).
- **Postconditions:** Output ≥40% smaller than Phase-2 baseline on the oversampled
  file; printed size identical; SSIM ≥ 0.98.
- **Edge Cases:** Image with `/Interpolate` + tiny placed size (icon) → below floor:
  kept at original dims, logged `imagesKeptOriginal`.

### Acceptance Criteria (Phase 3)

#### AC-D1 — CTM-based effective DPI
- **Given** a PDF with one 1200×1200 image placed at 2×2in (→600 DPI) and one placed
  at 8×8in (→150 DPI)
- **When** `diagnose` (extended with effective-DPI column) runs
- **Then** it reports ~600 and ~150 (±5%) respectively; page-size approximation is
  no longer the source when CTM exists (`TEST(Inspector, EffectiveDpiFromCtm)`).

#### AC-D2 — Profile targets + floor
- **Given** the oversampled PDF + tiny-icon PDF
- **When** optimized under each profile (`--dpi` unset, then `--dpi 200` override)
- **Then** oversampled output dims ≈ targetDPI/printSize per profile (±10%) and never
  larger than input dims; icon PDF dims unchanged under all profiles;
  `run_optimize --help` lists `--dpi/--no-downsample`.

#### AC-D3 — Phase 3 benchmark gate
- **Given** pre-Phase-3 TSV
- **When** re-run after Phase 3
- **Then** oversampled/scan rows shrink ≥20% vs baseline with SSIM still ≥ gate;
  already-at-target rows within ±3%; text_only/form/bookmark rows unchanged (±2%).

## Phase 4 (Goal E) — GUI Batch UX: Progress, Cancel, Off-Thread, Stats

**Problem statement.** Core batch works (drag-drop `DropHandler/DropOverlay`, sequential
queue) but long jobs block the UI, cannot cancel, and before/after stats are CLI-only
detail. Phase 4 moves work off the UI thread (QThreadPool), adds per-file + total
progress, cancel, and a before/after summary (bytes, %, per-profile note), keeping the
existing picker/slider/checkboxes.

**Goals**
- Off-UI-thread optimization with responsive GUI, per-file progress bar + overall
  batch progress, cancel button that stops cleanly (partial outputs removed or marked).
- Batch summary view: per-file original→optimized bytes + % + result breakdown;
  errors shown per file without aborting the batch.
- Keep existing controls (3-profile picker, slider 1–100 default 70, strip checkboxes,
  linearize checkbox); USAGE.md documents the flow.

**Non-Goals**
- No new compression logic; no target-size ("fit ≤ N MB") mode in any phase
  (locked NOT in scope 2026-10-04 — profile+slider+summary is sufficient).

**Manual test PDFs (Phase 4):** batch of all 14 corpus PDFs incl. `encrypted.pdf`
(error-path display check).

### User Journeys (Phase 4)

#### J-E1 — User drag-drops a batch, watches progress, cancels
- **Preconditions:** GUI running; 14 corpus PDFs in a folder.
- **Steps:** 1. Drop folder 2. Watch per-file + total progress 3. Press Cancel
  mid-batch 4. Re-run to completion.
- **Postconditions:** Cancel stops within 2s, completed files kept, in-flight file
  has no half-written output; full run shows summary table.
- **Edge Cases:** `encrypted.pdf` without password in batch → row shows
  `FAILED (encrypted — password required)` with per-file Continue; batch completes.

### Acceptance Criteria (Phase 4)

#### AC-E1 — Responsive + cancellable batch
- **Given** a 14-file batch optimizing (Balanced)
- **When** the user presses Cancel after ≥1 file completes
- **Then** no new file starts, the in-flight file finishes-or-rolls-back within 2s
  (no truncated PDF left at the output path), UI remains responsive throughout
  (event loop never blocked >100ms — verified by manual test + code inspection that
  work runs on QThreadPool).

#### AC-E2 — Summary stats
- **Given** a completed batch
- **When** the summary view is shown
- **Then** each row shows filename, original bytes, output bytes, % saved, and
  profile; totals row sums all three; failed files show reason from
  `OptimizationResult.errorMessage`.

## Phase 5 (Goal F) — JBIG2 Monochrome

**Problem statement.** Monochrome correctly routes to lossless zlib
(`DecisionEngine.cpp:68-71`) but JBIG2 (10–50% smaller on 1-bit scans) is not
integrated (verified RIGHT; no jbig2enc in `vcpkg.json`, no JBIG2 ref in `src/`).
Phase 5 integrates JBIG2 for true 1-bit images behind a profile rule (licensing
accepted 2026-10-04: jbig2enc Apache-2.0 via vcpkg/vendored source).

**Goals**
- True 1-bit images (`/BitsPerComponent 1` + DeviceGray/CalGray) encode via JBIG2
  (lossless mode default; lossy refinement only with explicit `--jbig2-lossy` flag).
  Encoder: jbig2enc (Apache-2.0) via vcpkg or vendored source — accepted 2026-10-04.
- Non-1-bit grayscale (`monochrome_bw.pdf` 8-bit concept) keeps current zlib path.
- Size guard decides per image (JBIG2 bytes replace only if smaller); fallback to
  zlib/original + log on encode failure.

**Non-Goals**
- No change to non-monochrome paths; no lossy-JBIG2-by-default (must stay lossless
  unless user opts in).

**Manual test PDFs (Phase 5):** true 1-bit scan PDF (new fixture: `/BitsPerComponent
1`), `monochrome_bw.pdf` (8-bit control → still zlib), `grayscale_scan.pdf`
(control → still JP2).

### User Journeys (Phase 5)

#### J-F1 — 1-bit scan uses JBIG2
- **Preconditions:** 1-bit scan PDF present.
- **Steps:** 1. `run_optimize scan1bit.pdf --profile balanced` 2. `qpdf --json` filter
  check 3. Compare vs `--no-jbig2`.
- **Postconditions:** Default output filter is JBIG2Decode and smaller than zlib path;
  renders pixel-identical (SSIM 1.0); flag override works.
- **Edge Cases:** jbig2enc unavailable at build → configure warns, codec disabled,
  monochrome falls back to zlib with `imagesKeptOriginal`/log (build never breaks).

### Acceptance Criteria (Phase 5)

#### AC-F1 — JBIG2 for true 1-bit only
- **Given** a 1-bit PDF and the 8-bit `monochrome_bw.pdf`
- **When** optimized with defaults
- **Then** 1-bit output uses JBIG2Decode and is ≤ zlib-path size; 8-bit output still
  uses FlateDecode; `TEST(DecisionEngine, MonochromeBitDepthRoutesJbig2)` pins routing.

#### AC-F2 — Lossless default + fallback
- **Given** the 1-bit PDF
- **When** optimized default vs `--jbig2-lossy` vs encoder-failure injection
- **Then** default is pixel-identical (SSIM 1.0); `--jbig2-lossy` is smaller-or-equal
  with SSIM ≥ 0.99; encoder failure yields valid PDF via zlib/original + logged reason.

#### AC-F3 — Phase 5 benchmark gate
- **Given** pre-Phase-5 TSV
- **When** re-run after Phase 5
- **Then** 1-bit scan row shrinks ≥10% vs zlib baseline at SSIM 1.0; all non-1-bit
  rows within ±2%.

## Phase 6 (Goal G) — Font Dedup + Subsetting (Selectable Text Preserved)

**Problem statement.** Structure is partially optimized (stream dedup, object streams —
PARTIAL) but fonts are untouched: duplicate font descriptors/embedded subsets bloat
merged PDFs and no subsetting exists (grep font only in inspector emptyPDF). Phase 6
deduplicates identical fonts and subsets embedded fonts to used glyphs, never breaking
selectable text (standing gate S4 is the hard constraint).

**Goals**
- Byte-identical font program dedup (same hash-join pattern as stream dedup, with
  the ESC-001 indirect-handle guard).
- Glyph-subsetting for embedded TrueType/OpenType (used-glyphs only) via
  HarfBuzz `hb-subset` (vcpkg dependency — locked 2026-10-04) behind a
  `deduplicateStreams`-adjacent flag (`--subset-fonts`, default ON for Balanced +
  MaxCompression, OFF for MaxQuality — matrix locked 2026-10-04; behavior matrix
  in USAGE.md).
- `/ToUnicode` preserved whenever subsetting; text extraction before/after identical
  on corpus + manual merged-font PDF.

**Non-Goals**
- No OCR, no font format conversion (OTF→TTF etc.), no unembedding of fonts.

**Manual test PDFs (Phase 6):** merged-duplicate-fonts PDF (shared font XObject),
`text_only.pdf`, `with_form.pdf` (field text must keep working), large mixed-text PDF.

### User Journeys (Phase 6)

#### J-G1 — Merged PDF dedups + subsets fonts, text intact
- **Preconditions:** Merged-font PDF present.
- **Steps:** 1. Extract text before (`pdftotext`/`diagnose`) 2. Optimize Balanced
  3. Extract text after + compare sizes.
- **Postconditions:** Output smaller; extracted text identical; fonts still embedded
  (no tofu in Preview/Chrome).
- **Edge Cases:** Form-field font subset would break editing → form fonts are never
  subset, only deduped; result logs the exemption.

### Acceptance Criteria (Phase 6)

#### AC-G1 — Font dedup
- **Given** a PDF with the same font program embedded twice (distinct objects)
- **When** optimized with defaults
- **Then** output embeds one copy (`qpdf --json` shows shared reference),
  extracted text identical, `qpdf --check` clean.

#### AC-G2 — Subsetting with ToUnicode intact
- **Given** a text PDF embedding a full font but using <30% of glyphs
- **When** optimized `--profile balanced` (subset ON) vs `--profile max-quality`
  (subset OFF)
- **Then** Balanced output is smaller with byte-identical extracted text and a
  present `/ToUnicode`; MaxQuality output preserves the full program.

#### AC-G3 — Phase 6 benchmark gate
- **Given** pre-Phase-6 TSV
- **When** re-run after Phase 6
- **Then** font-heavy rows (text_only, merged-fonts, form) shrink vs baseline with
  text-identical; image-only rows within ±2%.

## Phase 7 (Goal H) — Encrypted PDFs: Password UX + Re-encryption

**Problem statement.** Encrypted PDFs are rejected (`PDFInspector.cpp:88-94`
`FPDF_ERR_PASSWORD` throw; `PDFOptimizer.cpp:61` no-password `processFile` — verified
RIGHT). Corpus uses `test123` only as a fixture. Production needs a password flow:
prompt, pass to QPDF/PDFium, optimize, and write unencrypted unless the user opts to
re-encrypt (default locked 2026-10-04: output unencrypted unless user opts in).

**Goals**
- GUI password dialog on `isEncrypted` (per-file in batch, with per-file cancel that
  marks the row and continues); `run_optimize --password <pw>` (+ `PDFOPTIMIZE_PASSWORD`
  env fallback for scripts — never logged).
- Decrypt → optimize → write unencrypted by default; `--re-encrypt` (CLI) /
  "Re-encrypt with same password" checkbox (GUI) preserves AES-128 when requested.
  (Default locked 2026-10-04: unencrypted output unless opted.)
- Wrong password → clean `errorMessage` (no crash, no retry loop); batch continues.

**Non-Goals**
- No password cracking/recovery; no change to encryption strength beyond preserving
  input parameters on re-encrypt.

**Manual test PDFs (Phase 7):** `encrypted.pdf` (`test123`), wrong-password attempt,
batch mixing encrypted + plain PDFs.

### User Journeys (Phase 7)

#### J-H1 — User optimizes an encrypted PDF
- **Preconditions:** GUI running; `encrypted.pdf` (test123) present.
- **Steps:** 1. Drop file 2. Password dialog appears 3. Enter `test123` 4. Optimize
  5. Open output (no password) 6. Re-run with re-encrypt checked.
- **Postconditions:** (5) output opens without password, smaller; (6) output requires
  the password again.
- **Edge Cases:** Wrong password → inline error `Incorrect password`, 3 attempts then
  row marked failed; batch continues; password string never written to logs/TSV.

### Acceptance Criteria (Phase 7)

#### AC-H1 — Password flow end-to-end
- **Given** `encrypted.pdf` (test123)
- **When** `run_optimize encrypted.pdf --password test123 -o out.pdf` and again with
  `--password wrong` and again without `--password`
- **Then** correct password → success, output unencrypted (`isEncrypted==false`);
  wrong/missing → `success=false`, `errorMessage` set (`Incorrect password` /
  `Encrypted PDF — password required`), no crash, no output written.

#### AC-H2 — Re-encrypt opt-in
- **Given** `encrypted.pdf`
- **When** optimized `--password test123 --re-encrypt`
- **Then** output requires `test123` to open (`isEncrypted==true`), renders match,
  size gate S2 still holds.

## Phase 8 (Goal I) — Performance, Streaming, Packaging & CI Hardening

**Problem statement.**ril Large PDFs (>500MB per original plan §8) risk memory blowup
(whole-document decode hold) and slow single-threaded page loops; packaging (DMG,
code-sign, sandbox entitlements) and CI benchmark gating are ad hoc. Phase 8 makes
large-file behavior safe, parallelizes page work within the existing QThreadPool
direction, and locks CI/packaging so every earlier phase stays green.

**Goals**
- Memory-conscious streaming: one-image-at-a-time discipline kept; peak RSS measured
  and capped (target: <1GB on a 500MB mixed PDF — exact cap locked after first
  measurement; benchmark reports peak RSS per file).
- Parallel page processing (thread pool, deterministic output regardless of thread
  count); no data races on QPDF handles (existing per-image guard pattern extended).
- Packaging: signed/notarized DMG path documented, Qt frameworks bundled, sandbox
  entitlements listed; CI runs corpus + benchmark + S1–S5 gates (missing optional
  tools → SKIPPED).

**Non-Goals**
- No new compression features; performance work must not change output bytes vs
  single-threaded run (determinism gate below).

**Manual test PDFs (Phase 8):** large mixed PDF (≥200MB staged locally, NOT committed),
`photo_heavy.pdf` (parallel determinism check), full 14-file corpus (CI matrix).

### User Journeys (Phase 8)

#### J-I1 — Developer runs CI on a large PDF
- **Preconditions:** Large PDF staged outside git; CI image with optional tools.
- **Steps:** 1. `run_optimize big.pdf` with `/usr/bin/time -l` (RSS) 2. Compare
  single-thread vs pooled output hashes 3. Check CI TSV.
- **Postconditions:** Peak RSS under cap; outputs byte-comparable across thread
  counts; CI green with SKIPPED (not FAILED) for missing tools.
- **Edge Cases:** OOM-risk file (>available RAM) → streams spilled via `$TMPDIR`
  (`NSTemporaryDirectory`), never held fully in RAM; progress still reported.

### Acceptance Criteria (Phase 8)

#### AC-I1 — Streaming memory cap
- **Given** a ≥200MB mixed PDF staged in `$TMPDIR` (never committed)
- **When** optimized while sampling peak RSS
- **Then** peak RSS ≤ locked cap (≤1GB default pending measurement) and output
  satisfies S2–S5; TSV gains a `PeakRSS` column.

#### AC-I2 — Deterministic parallelism
- **Given** `photo_heavy.pdf`
- **When** optimized with `--jobs 1` vs `--jobs 4` (new flag, default = core count)
- **Then** both outputs are valid, both satisfy S2–S5, and rendered pages are
  pixel-identical across thread counts (SSIM 1.0 between the two outputs).

#### AC-I3 — Packaging + CI matrix
- **Given** a clean checkout on macOS
- **When** CI runs `cmake -B build … && cmake --build build && ctest` +
  `benchmark/run_benchmark.sh`
- **Then** all gates pass; `.app` bundles `libpdfium.dylib` (no `DYLD_*` needed to
  launch); DMG/signing steps documented in USAGE.md; missing `gs`/`ocrmypdf` rows
  are SKIPPED.

---

## Constraints (additions for Phases 1–8; Phase 0 constraints still apply)

- **Offline only** extends to all phases: JBIG2 encoder, subsetter, and benchmark
  comparators must run locally; no network calls in app/tests/benchmark (PDFium
  fetch at configure time remains the sole exception).
- **Tech stack hard constraint:** C++20 RAII, `-Wall -Wextra` clean; `core/` +
  `codecs/` (new codecs implement `ImageCodec`) + `tools/` layout; GTest locked (no
  Catch2 migration); QPDF + PDFium retained; vcpkg manifest stays the dependency
  channel (jbig2enc + HarfBuzz hb-subset added via vcpkg or vendored source — no Homebrew-only
  runtime dependency).
- **Safety invariants (all phases):** never overwrite input; never emit larger output
  silently (S2); per-image/page/cleanup fallback + log; `$TMPDIR` for ephemera;
  corpus stays exactly the 14 canonical files (new fixtures beyond the 14 need a
  requirements change); corpus `test123` password is TEST-ONLY (production prompts).
- **Render-diff purity:** all quality numbers stay pure-C++ PDFium; no Python.
- **GUI discipline:** compression off the UI thread with cancel (Phase 4 sets the
  pattern; later phases follow it).
- **Platform (locked 2026-10-04):** macOS-only for Phases 1–8; packaging = signed DMG
  per AC-I3; CI matrix is macOS (no Windows/Linux legs required).

## Glossary (additions B–I)

- **Effective DPI:** pixels ÷ print inches from the image placement CTM (not page
  size); drives Phase 3 downsampling.
- **CTM (Current Transformation Matrix):** PDF graphics-state matrix placing an
  image XObject on the page; source of print size for DPI.
- **JBIG2 (jbig2enc):** lossless/lossy codec for 1-bit bi-level images; Phase 5.
- **CMYK transcode (opt-in):** `--transcode-cmyk-to-rgb` conversion of CMYK images to
  RGB for extra savings; OFF by default, gated by SSIM ≥ 0.98 render match (AC-C3b);
  Phase 2.
- **Font subsetting:** rewriting an embedded font to contain only used glyphs while
  keeping `/ToUnicode` so text stays selectable; Phase 6.
- **Re-encrypt:** writing the optimized PDF with encryption again (same password /
  parameters) instead of the default unencrypted output; Phase 7.
- **Target DPI / DPI floor:** per-profile resampling target (e.g. 300/150/96) and the
  minimum dimension below which downsampling stops; Phase 3.
- **Peak RSS:** maximum resident set size during one optimization; Phase 8 metric.
- **Stream dedup:** `OptimizationResult.streamsDeduplicated` counts stream pairs actually
  replaced. In **Phase 1** it is pinned **`0` under every input, including the
  shared-XObject PDF**, because the ESC-001 indirect-handle guard (`PDFOptimizer.cpp:373`)
  makes the increment dead code and the shared-XObject fixture reference-shares one indirect
  object (no distinct duplicate pair). The aspirational `>= 1` is **deferred to Phase 2**
  (owned by Phase 2 per user 2026-10-07; see AC-B3 and AC-C5): Phase 2 replaces the inert
  ESC-001 guard with a direct-copy `replaceObject` path, adds a `$TMPDIR` distinct
  byte-identical-streams fixture, and rewrites the P1-T3 `== 0` test to `>= 1`. Ruled
  2026-10-07 per ESC-010 / ESC-010-FU.

## Assumptions Log (additions for Phases 1–8)

| Assumption | Rationale | Confirmed? | Date |
|---|---|---|---|
| `forProfile` spec defaults (MaxQuality-strips-nothing / Balanced-strips-JS+metadata / MaxCompression-strips-everything) match user brief and stay locked unless a requirements change is filed | Verified against `PDFOptimizer.h:27-64` 2026-10-04; brief explicitly says verify-not-assume | y | 2026-10-04 |
| Slider scope fix (lossy-only) + PNG wiring + CMYK preservation (+ opt-in `--transcode-cmyk-to-rgb` per AC-C3b) are all in Phase 2, not split | Single DecisionEngine/optimizer area; splitting would triple benchmark churn; transcode locked 2026-10-04 | y | 2026-10-04 |
| Downsample targets 300/150/96 DPI with 32px floor are defaults; `--dpi/--no-downsample` override | Common print/web values; floor prevents icon destruction; confirmed 2026-10-04 | y | 2026-10-04 |
| JBIG2 (jbig2enc Apache-2.0 via vcpkg/vendored) default lossless-only; lossy only via explicit flag | Lossy JBIG2 can alter glyphs; safe default required; licensing accepted 2026-10-04 | y | 2026-10-04 |
| Font subset (HarfBuzz hb-subset) ON for Balanced/MaxCompression, OFF for MaxQuality | Matches strip-philosophy of profiles; subsetter + matrix locked 2026-10-04 | y | 2026-10-04 |
| Re-encrypt default OFF (output unencrypted unless opted) | Matches test-fixture behavior; confirmed 2026-10-04 | y | 2026-10-04 |
| Parallelism default = hardware core count via `--jobs` | Standard practice; determinism gate guards it | n | 2026-10-04 |
| Stream dedup is inert in Phase 1: `streamsDeduplicated == 0` under defaults on every fixture (ESC-001 `isIndirect` guard always taken; shared-XObject fixture reference-shares one indirect object) | Verified against `PDFOptimizer.cpp:338,366,373` and `test_corpus_generator.cpp:450-451`; Phase 1 verify-only lock bars the product + corpus fix | y (Phase 1 behavior) / y (fix owned by Phase 2) | 2026-10-07 |
| The deferred stream-dedup fix (direct-copy `replaceObject` in `PDFOptimizer.cpp` + distinct-byte-identical `$TMPDIR` fixture + P1-T3 test rewrite to `>= 1`) is owned by **Phase 2**, not Phase 6 | User confirmed 2026-10-07 (ESC-010-FU): Phase 2 is the first product-code-carrying phase and the fix sits inside its structural-correctness scope; Phase 6 stays font-scoped | y | 2026-10-07 |

## Risks / Unknowns (additions for Phases 1–8)

| Risk | Severity | Likelihood | Mitigation |
|---|---|---|---|
| JBIG2 encoder license (jbig2enc is Apache-2.0; older JBIG2 libs GPL) blocks bundling | H | L | Accepted 2026-10-04 (Apache-2.0 via vcpkg/vendored); fallback retained: keep zlib with documented gap if integration fails |
| CTM extraction via QPDF/PDFium placement matrix is inaccurate for rotated/cropped images | H | M | AC-D1 fixtures cover 600-vs-150 + rotation; fallback to page-size approx + log |
| Downsampling + recompression double-degrades already-JPEG images | M | H | Downsample in pixel domain before encode; SSIM gate catches over-degradation |
| Font subsetting breaks form editing or CJK ToUnicode | H | M | Never subset form fonts; CJK fixtures in manual tests; text-identity gate per phase |
| Password strings leak into logs/TSV/crash dumps | H | L | Never log password; env-var path; code-review checklist item for Phase 7 |
| CMYK→preserve still larger than original on some files (S2 tension) | M | M | Skip-with-reason counts as S2 explanation; benchmark gate documents it |
| Parallel QPDF handle races cause nondeterministic output | M | M | AC-I2 determinism gate; thread-local buffers, join-before-write |
| Large-PDF RSS cap unachievable without temp-file spill redesign | M | M | Phase 8 measures first, locks cap second; spill to `$TMPDIR` allowed |
| Windows/Linux port requested mid-program (scope creep) | M | L | Locked 2026-10-04: macOS-only for Phases 1–8 |

---

All 8 Phase 1–8 open questions answered 2026-10-04 (see `open_questions.md`); no
blocking gap remains. CMYK-transcode color-shift threshold defined in AC-C3b (SSIM ≥ 0.98
render match; Plan chooses the implementation-side ΔE metric).

ESC-010 (2026-10-07): AC-B3 reworded to pin Phase 1 `streamsDeduplicated == 0` (inert
dedup: ESC-001 indirect guard + reference-shared fixture); the aspirational
`streamsDeduplicated >= 1` is retained as an explicit DEFERRED follow-up now **owned by
Phase 2 (Goal C)** — user-confirmed 2026-10-07 via ESC-010-FU (Phase 6 stays font-scoped)
— AC-B3, Glossary, Assumptions Log, `design_gaps.md`, and Phase 2 Goals/AC-C5 updated. The
P1-T3 test asserting `== 0` must be rewritten to `>= 1` in Phase 2 when the fix lands.

Status: READY_FOR_PLAN
