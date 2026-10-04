# Plan — PDF Compressor Phase 0: Verification and Baseline

## Phases (Phase 0 only — no Phases A–I detail)

> Sequencing is coarser than Task Manager's task graph. Each phase groups work Task Manager will further decompose. All phases respect: `core/`, `codecs/`, `tools/` layout; C++20 RAII, warnings clean; offline; no behavior change to `src/core/PDFOptimizer.cpp` decision logic or `src/codecs/*`.

### Phase 0.1 — Corpus Hardening (14 deterministic types, QPDF-only, TMPDIR)

- **Goal:** Replace the six stubs and missing types in `tests/test_corpus_generator.cpp/.h` so every AC3 logical type generates a real QPDF PDF with the dictionary keys that exercise the corresponding `PDFOptimizer` strip/skip path; keep storage ephemeral in `$TMPDIR/pdfcompress_test_corpus` and licensed-free.
- **Requirements / Journeys covered:** AC3 (normative 14 types, QPDF-only, `$TMPDIR`, test password `test123` 128-bit AES), AC5 (no behavior change — only `tests/`/`benchmark/` changed), AC7 (encrypted fixture rejected gracefully without password, valid after `QPDF::processFile`), J1 steps 3 & 5 (corpus + Preview check), risks R1/R2/R3; design_gaps #2 #5 #6 #7.
- **Dependencies:** None — first phase; reads Appendix A verification report as ground truth.
- **Scope (reviewable diff):**
  - `tests/test_corpus_generator.h/.cpp` — replace `generateWithForm()`, `generateWithMetadata()`, `generateWithJavaScript()`, `generateMultiImage()`→`generatePhotoHeavy()` (3 distinct images 300×200/200×150/250×180 multi-page), `generateSharedXObject()`→`generateMergedDuplicateFonts()` (shared indirect XObject via same `QPDFObjectHandle` on 2 pages), `generateLargeUncompressed()` (≥50KB Flate stream to avoid writer-overhead), add `generateLineArt()` (300×300 edge-heavy limited palette, Flate quadrants), `generateTransparency()` (SMask/alpha `/SMask << /S /Alpha >>` + `/Group << /S /Transparency >>`), `generateEncrypted()` (QPDF R6 AES-128 `test123` no owner), add declared-but-missing `generateWithJavaScript()` impl. Fix `generateCmykImage()` to use `/FlateDecode` with real compressed bytes instead of raw DCTDecode. Apply `QPDFWriter::setDeterministicID(true)` in every generator. Update `generateAll()` to emit exactly 14 PDFs matching AC3 1–14 authoritative list (see plan_notes RG-001 for 14-file interpretation).
  - `CMakeLists.txt` — no change except ensuring `generate_corpus` target still links `test_corpus_generator.cpp`.
  - `benchmark/generate_corpus.cpp` — keep (prints `Corpus generated at:`; benefits from corrected generator).
- **Out of scope:** Any change to `PDFOptimizer.cpp` strip defaults, `DecisionEngine` wiring, DPI calc, codecs.
- **Verification gate:** `./build/generate_corpus | tee corpus_info.txt` then `ls $TMPDIR/pdfcompress_test_corpus/*.pdf | wc -l` == 14; each PDF passes `QPDF().processFile(path)`; key checks: `with_form.pdf` has `/AcroForm`, `with_bookmarks_and_links.pdf` has `/Outlines`+`/Annots`+`/Dests`, `with_javascript.pdf` has `/Names/JavaScript`+`/OpenAction`, `with_metadata.pdf` has `/Info`+`/Metadata`, `encrypted.pdf` has `isEncrypted==true` without password and optimizes to unencrypted with password `test123`, `cmyk_image.pdf` has `/DeviceCMYK` and optimizer `imagesSkipped==1`, `transparency.pdf` has `/SMask` or `/Group /Transparency`, `merged_duplicate_fonts.pdf` coverage via shared object ID if separate file (or embedded in transparency per RG-001).

### Phase 0.2 — Benchmark Hardening (valid QPDF invocation + 5-tool matrix + idempotency)

- **Goal:** Make `benchmark/run_benchmark.sh` conform to Appendix B normative spec: correct QPDF invocation, full tool matrix with graceful degradation, pure-C++ timing/metrics, idempotency, TSV output.
- **Requirements / Journeys covered:** AC4 (normative benchmark), AC5 (TSV before/after within ±1% pdfcompress rows on ≥50KB PDFs, exempt <2KB), J3 (SKIPPED not FAILED, skip re-benchmark), design_gaps #4 #7, risks R2/R7.
- **Dependencies:** Phase 0.1 (corpus must exist and be correct before benchmarking).
- **Scope (focused file):**
  - `benchmark/run_benchmark.sh` — `set -euo pipefail`, arg `BUILD_DIR` default `build`; pre-step `(cd "$BUILD_DIR" && ./generate_corpus) | tee corpus_info.txt` and parse `Corpus generated at:` for `CORPUS_DIR`; header `File\tOriginal Size\tTool\tOutput Size\tReduction %\tTime (ms)\tSSIM\tPSNR\tStatus`; loop `"$CORPUS_DIR"/*.pdf` skipping `*_optimized*` `*.qpdf*` `*.opt*` `*.gs*` `*.ocrmypdf*`; tools in order: 1) `pdfcompress` via `(cd "$BUILD_DIR" && ./run_optimize "$pdf")`, 2) `qpdf --recompress --compression-level=9 --object-streams=generate`, 3) `gs -sDEVICE=pdfwrite -dPDFSETTINGS=/ebook -dCompatibilityLevel=1.7 -dNOPAUSE -dBATCH -sOutputFile=`, 4) `gs ... /screen`, 5) `ocrmypdf --optimize 3 --skip-text` — each `command -v` guarded → `SKIPPED (not installed)` row; `orig_size` via `stat -f%z`/`stat -c%s`; `reduction%` via `awk`/`bc`; wall time via `date +%s%3N` or `gdate` or `date +%s`×1000; output files `<name>.<tool>.pdf` alongside corpus; never overwrite input; wrap `run_optimize` with `|| true` so encrypted PDF doesn't abort `set -e`; write `benchmark_results_YYYYMMDD_HHMMSS.tsv` to cwd and `build/` and `column -t -s $'\t'`.
- **Out of scope:** Pure-C++ SSIM/PSNR computation (Phase 0.3); slider/DecisionEngine docs are notes only.
- **Verification gate:** Run with and without `gs`/`ocrmypdf`/`qpdf`; TSV has one row per tool per PDF; missing tools show `SKIPPED`; `qpdf` rows no longer `FAILED`; `*_optimized*` files not re-benchmarked; TSV header present; `ctest` still passes.

### Phase 0.3 — Pure-C++ Render-Diff Hardening (PDFium, no Python)

- **Goal:** Add a focused helper binary that renders page 1 at 150 DPI purely in C++ via PDFium and computes SSIM/PSNR in-process; wire it into `run_benchmark.sh` so every TSV row carries `SSIM`/`PSNR` or `N/A` with header note when PDFium unavailable — no Python dependency, per 2026-10-03 decision.
- **Requirements / Journeys covered:** AC4 render-diff bullet (pure C++ PDFium, N/A fallback, no Python), AC7(d) SSIM thresholds ≥0.98 Balanced/MaxQuality, ≥0.95 MaxCompression — measured by helper, `N/A` allowed only if PDFium unavailable; J1 step 4; Assumptions Log render-diff row; feature_suggestions #1.
- **Dependencies:** Phase 0.2 (TSV columns already exist; this phase fills SSIM/PSNR).
- **Scope (new logic in focused files only):**
  - **New file** `tools/render_diff.cpp` (canonical — `benchmark/render_diff.cpp` alternative rejected) — C++20, RAII (`FPDF_DOCUMENT`, `FPDF_PAGE`, `FPDF_BITMAP` via unique_ptr with custom deleters), offline. API: `int main(argc,argv)` takes `original.pdf optimized.pdf`; uses `FPDF_InitLibraryWithConfig` → `FPDF_LoadDocument` → `FPDF_LoadPage(doc,0)` → `FPDF_GetPageWidth/Height` → 150 DPI bitmap `w=pageWidthPt/72*150`, `h=pageHeightPt/72*150` → `FPDFBitmap_Create(w,h,0)` → `FPDFBitmap_FillRect(...0xFFFFFFFF)` → `FPDF_RenderPageBitmap` → BGRA buffers for both PDFs → compute SSIM (windowed mean/variance/covariance, C1/C2) and PSNR (MSE→10*log10(255^2/MSE)) in C++. If either `FPDF_LoadDocument` fails (encrypted without password, missing dylib), print `N/A N/A` to stdout and exit 0; never crash. No `python3`/`pip`/`scikit-image`.
  - `CMakeLists.txt` — add `add_executable(render_diff tools/render_diff.cpp)` linked `PRIVATE pdfcompress_core pdfium`, under `if(BUILD_TESTING)` or alongside `diagnose`.
  - `benchmark/run_benchmark.sh` — after each tool call, invoke `"$BUILD_DIR/render_diff" "$pdf" "$out_file"` (guarded: `if [ -x "$BUILD_DIR/render_diff" ]; then ... else SSIM=N/A PSNR=N/A; fi`) and fill TSV columns; header note `render-diff unavailable (PDFium C++ only)` if fallback.
  - Optional `tests/test_render_diff.cpp` (Task Manager may add as `RenderDiff.SSIMThreshold` asserting SSIM≥0.98 on `text_only.pdf` pair) — not required in Phase 0 plan but architecture supports it.
- **Design note — DecisionEngine slider fix (Phase C out of scope):** Per Appendix A claim 2 PARTIAL/WRONG, slider currently forces JPEG for Screenshot/LineArt (and Photo) but correctly does NOT affect Monochrome/ScannedText. Phase 0 does NOT change `DecisionEngine.cpp:51-71`; correct fix (Phase C) will scope slider to only lossy classes and wire PNG/zlib for MaxQuality without slider — no code proposed here. `git diff --stat` must show changes only in `tools/render_diff.cpp`, `CMakeLists.txt`, and `benchmark/run_benchmark.sh`.
- **Verification gate:** `cmake --build` succeeds `-Wall -Wextra -Wpedantic` clean; `./build/render_diff $TMPDIR/text_only.pdf $TMPDIR/text_only_optimized.pdf` prints `SSIM PSNR` or `N/A N/A`; `run_benchmark.sh` TSV now has SSIM/PSNR columns populated (or `N/A`); no Python spawned; existing 12+ tests still pass.

### Phase 0.4 — Test Wiring & Acceptance Gating (CTest, verifier tests)

- **Goal:** Ensure every new corpus type and benchmark metric is gated by CTest so a fresh checkout's `ctest --test-dir build --output-on-failure` is the single trusted baseline signal (J1/J2).
- **Requirements / Journeys covered:** AC2 (GTest + CTest discover, `TEST(Suite,Case)` naming, one-line `CMakeLists.txt` add), AC3 verifier `QPDF::processFile` + key assertions, AC6 (CLI flags still listable), AC7 quality gates (valid PDF, size guard, text selectable, SSIM threshold).
- **Dependencies:** Phases 0.1–0.3 (corpus, script, helper must exist to test).
- **Scope:**
  - `tests/test_corpus_generator.cpp` (already touched in 0.1) — add `TEST(CorpusGenerator, EachTypeHasExpectedKey)` style checks (14 tests or parameterized) that after `generateAll()` assert `#files==14` and per-file `hasKey` checks; keep `CMakeLists.txt` `add_executable(pdfcompress_tests ...)` one-line discipline.
  - Ensure `CMakeLists.txt` `enable_testing()` + `include(GoogleTest)` + `gtest_discover_tests(pdfcompress_tests)` remains; `BUILD_TESTING=ON` default.
  - Manual gate: `ctest --test-dir build --output-on-failure -V` → 14+ tests pass; `diagnose` and `run_optimize --help` still list flags; optimized PDFs `QPDF().processFile` succeed and `diagnose` shows fonts preserved.
- **Verification gate:** `cmake -B build -DCMAKE_TOOLCHAIN_FILE=third_party/vcpkg/scripts/buildsystems/vcpkg.cmake && cmake --build build && ctest --test-dir build --output-on-failure` passes; `benchmark/run_benchmark.sh [build_dir]` completes with TSV; `git diff --stat` shows only `tests/`, `benchmark/`, `tools/render_diff*`, `CMakeLists.txt` (per AC5).

## Assumptions

- QPDF `R6` AES-128 encryption API is available via `QPDFWriter::setR6Encryption` fallback to `setR3Encryption`; if headers differ, generator tries R6 first. (Not in requirements.md; discovered during inventory.)
- `date +%s%3N` is not POSIX on macOS BSD `date`; benchmark will probe `gdate` (coreutils) then fallback to `date +%s` ×1000 so timing remains ms without assuming GNU date.
- `bc` may be absent on minimal CI images; `awk` is used as fallback for `reduction%` calc.
- Corpus PDFs remain synthetic and licensed-free because they are built from QPDF `emptyPDF` + generated pixel buffers and Helvetica Type1 — no embedded third-party font/image is copied.
- `libpdfium.dylib` is bundled in `build/` for both GUI and helper; if `DYLD_LIBRARY_PATH` / `@executable_path` is wrong on CI, render-diff degrades to `N/A` rather than failing the build.
- The 14-file enumeration in AC3 is authoritative; for strict 14, `merged_duplicate_fonts` coverage is embedded inside `transparency.pdf` via shared XObject + SMask on same PDF (RG-001) — Task Manager will assert `==14`.

## Flagged Issues

_Flagged Issues F1–F5 from the proposal have been reviewed by the Validator. F1 is recorded as a planning note (RG-001) with a locked interpretation (exactly 14 files); see `docs/plan_notes.md`. F2–F5 are correctly flagged as implementation notes / risks, not requirements escalations, and are addressed by the architecture's mitigations. No ESCALATED_TO_REQUIREMENTS is required. The 5-tool matrix, QPDF-only/TMPDIR/test123, pure-C++ render-diff, and RAII/offline/file-layout constraints are all correctly scoped._

Status: READY_FOR_TASK_MANAGER
