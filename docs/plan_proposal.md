# Plan Proposal — PDF Compressor Phase 0: Verification and Baseline

## Phases (Phase 0 only — no Phases A–I detail)

> Sequencing is coarser than Task Manager's task graph. Each phase groups work Task Manager will further decompose. All phases respect: `core/`, `codecs/`, `tools/` layout; modern C++20, RAII, warnings clean; offline; no behavior change to `src/core/PDFOptimizer.cpp` decision logic or `src/codecs/*`.

### Phase 0.1 — Corpus Hardening (14 deterministic types, QPDF-only, TMPDIR)

- **Goal:** Replace the six stubs and missing types in `tests/test_corpus_generator.cpp/.h` so every AC3 logical type generates a real QPDF PDF with the dictionary keys that exercise the corresponding `PDFOptimizer` strip/skip path; keep storage ephemeral in `$TMPDIR/pdfcompress_test_corpus` and licensed-free.
- **Covers:** AC3 (normative 14 types, QPDF-only, `$TMPDIR`, test password `test123` 128-bit AES), AC5 (no behavior change — only `tests/`/`benchmark/` changed), AC7 (encrypted fixture rejected gracefully without password, valid after `QPDF::processFile`), J1 steps 3 & 5 (corpus + Preview check), risks R1/R2/R3 in requirements.md; design_gaps #2 #5 #6 #7.
- **Dependencies:** None — first phase; reads Appendix A verification report as ground truth.

**Scope (reviewable diff):**
- `tests/test_corpus_generator.h/.cpp` — replace `generateWithForm()`, `generateWithMetadata()`, `generateWithJavaScript()`, `generateMultiImage()`→`generatePhotoHeavy()` (3 distinct images 300×200/200×150/250×180 multi-page), `generateSharedXObject()`→`generateMergedDuplicateFonts()` (shared indirect XObject via same `QPDFObjectHandle` reference on 2 pages), `generateLargeUncompressed()` (≥50KB Flate stream to avoid writer-overhead), add `generateLineArt()` (300×300 edge-heavy limited palette, Flate quadrants with high-contrast), `generateTransparency()` (SMask/alpha `/SMask << /S /Alpha >>` + `/Group << /S /Transparency >>`), `generateEncrypted()` (QPDF R6 AES-128 `test123`, no owner), add declared-but-missing `generateWithJavaScript()` impl. Fix `generateCmykImage()` to use `/FlateDecode` with real compressed bytes (or valid JPEG bytes) instead of raw DCTDecode (currently invalid JPEG, causes -1% growth). Apply `QPDFWriter::setDeterministicID(true)` in every generator. Update `generateAll()` to emit exactly 14 PDFs matching AC3 1–14 authoritative list; optionally emit `transparency.pdf` + `merged_duplicate_fonts.pdf` as 14th+15th but document that `merged_*` is counted as #14 per AC3 note — prefer exactly 14 to satisfy `ls *.pdf | wc -l == 14`.
- `CMakeLists.txt` — no change except ensuring `generate_corpus` target still links `test_corpus_generator.cpp`.
- `benchmark/generate_corpus.cpp` — keep (prints `Corpus generated at:`; Phase 0.1 benefits from it).

**Out of scope for this phase:** Any change to `PDFOptimizer.cpp` strip defaults, `DecisionEngine` wiring, DPI calc, codecs.

- **Verification gate:** `./build/generate_corpus | tee corpus_info.txt` then `ls $TMPDIR/pdfcompress_test_corpus/*.pdf | wc -l` == 14; each PDF passes `QPDF().processFile(path)`; key checks: `with_form.pdf` has `/AcroForm`, `with_bookmarks_and_links.pdf` has `/Outlines`+`/Annots`+`/Dests`, `with_javascript.pdf` has `/Names/JavaScript`+`/OpenAction`, `with_metadata.pdf` has `/Info`+`/Metadata`, `encrypted.pdf` has `isEncrypted==true` without password and optimizes to unencrypted with password `test123`, `cmyk_image.pdf` has `/DeviceCMYK` and optimizer `imagesSkipped==1`, `transparency.pdf` has `/SMask` or `/Group /Transparency`, `merged_duplicate_fonts.pdf` shares same object ID on 2 pages.

### Phase 0.2 — Benchmark Hardening (valid QPDF invocation + 5-tool matrix + idempotency)

- **Goal:** Make `benchmark/run_benchmark.sh` conform to Appendix B normative spec: correct QPDF invocation, full tool matrix with graceful degradation, pure-C++ timing/metrics, idempotency, TSV output.
- **Covers:** AC4 (normative benchmark), AC5 (compare TSV before/after within ±1% pdfcompress rows), J3 (SKIPPED not FAILED, skip re-benchmark), design_gaps #4 #7, risk R2/R7.
- **Dependencies:** Phase 0.1 (corpus must exist and be correct before benchmarking).

**Scope (focused file):**
- `benchmark/run_benchmark.sh` — `set -euo pipefail`, arg `BUILD_DIR` default `build`; pre-step `(cd "$BUILD_DIR" && ./generate_corpus) | tee corpus_info.txt` and parse `Corpus generated at:` for `CORPUS_DIR`; header `File\tOriginal Size\tTool\tOutput Size\tReduction %\tTime (ms)\tSSIM\tPSNR\tStatus`; loop `"$CORPUS_DIR"/*.pdf` skipping `*_optimized*` `*.qpdf*` `*.opt*` `*.gs*` `*.ocrmypdf*`; tools in order: 1) `pdfcompress` via `(cd "$BUILD_DIR" && ./run_optimize "$pdf")`, 2) `qpdf --recompress --compression-level=9 --object-streams=generate`, 3) `gs -sDEVICE=pdfwrite -dPDFSETTINGS=/ebook -dCompatibilityLevel=1.7 -dNOPAUSE -dBATCH -sOutputFile=`, 4) `gs ... /screen`, 5) `ocrmypdf --optimize 3 --skip-text` — each `command -v` guarded → `SKIPPED (not installed)` row; `orig_size` via `stat -f%z`/`stat -c%s`; `out_size` same; `reduction%` via `awk`/`bc`; wall time via `date +%s%3N` or `gdate` or `date +%s`×1000; output files `<name>.<tool>.pdf` alongside corpus; never overwrite input; wrap `run_optimize` with `|| true` so encrypted PDF doesn't abort `set -e`; write `benchmark_results_YYYYMMDD_HHMMSS.tsv` to cwd and `build/` and `column -t -s $'\t'`.

**Out of scope:** Pure-C++ SSIM/PSNR computation (Phase 0.3); slider/DecisionEngine docs are notes only.

- **Verification gate:** Run on machine with and without `gs`/`ocrmypdf`/`qpdf`; TSV has one row per tool per PDF; missing tools show `SKIPPED`; `qpdf` rows are no longer `FAILED`; `*_optimized*` files are not re-benchmarked; TSV header present; `ctest` still passes (script doesn't change `pdfcompress_core`).

### Phase 0.3 — Pure-C++ Render-Diff Hardening (PDFium, no Python)

- **Goal:** Add a focused helper binary that renders page 1 at 150 DPI purely in C++ via PDFium and computes SSIM/PSNR in-process; wire it into `run_benchmark.sh` so every TSV row carries `SSIM`/`PSNR` or `N/A` with header note when PDFium unavailable — no Python dependency, per 2026-10-03 decision and feature_suggestions #1.
- **Covers:** AC4 render-diff bullet (pure C++ PDFium, N/A fallback, no Python `scikit-image`), AC7 (d) SSIM thresholds ≥0.98 Balanced/MaxQuality, ≥0.95 MaxCompression — measured by this helper, `N/A` allowed only if PDFium unavailable; J1 step 4; Assumptions Log render-diff row.
- **Dependencies:** Phase 0.2 (TSV columns already exist; this phase fills SSIM/PSNR).

**Scope (new logic in focused files only):**
- **New file** `tools/render_diff.cpp` (preferred; alternative `benchmark/render_diff.cpp` — either is focused and reviewable) — C++20, RAII (`FPDF_DOCUMENT`, `FPDF_PAGE`, `FPDF_BITMAP` via unique_ptr with custom deleters), offline. API: `int main(argc, argv)` takes `original.pdf optimized.pdf`; uses `FPDF_InitLibraryWithConfig` → `FPDF_LoadDocument(path,nullptr)` → `FPDF_LoadPage(doc,0)` → `FPDF_GetPageWidth/Height` → 150 DPI bitmap size `w = pageWidthPt/72*150`, `h = pageHeightPt/72*150` → `FPDFBitmap_Create(w,h,0)` → `FPDFBitmap_FillRect(...0xFFFFFFFF)` → `FPDF_RenderPageBitmap(bitmap,page,0,0,w,h,0,0)` → read BGRA buffers for both PDFs → compute SSIM (windowed mean/variance/covariance, constants C1/C2 per standard) and PSNR (MSE→10*log10(255^2/MSE)) in C++. If either `FPDF_LoadDocument` fails (e.g., encrypted without password, missing dylib), print `N/A N/A` to stdout and exit 0 with note; never crash. No `python3`/`pip`/`scikit-image` invoked.
- `CMakeLists.txt` — add `add_executable(render_diff tools/render_diff.cpp)` linked `PRIVATE pdfcompress_core pdfium` (or `qpdf::libqpdf` only if needed), placed under `if(BUILD_TESTING)` or alongside `diagnose` — either is minimal.
- `benchmark/run_benchmark.sh` — after each tool's `run_tool` call, invoke `"$BUILD_DIR/render_diff" "$pdf" "$out_file"` (guarded: `if [ -x "$BUILD_DIR/render_diff" ]; then ... else SSIM=N/A PSNR=N/A; fi`) and fill TSV columns; header note `render-diff unavailable (PDFium C++ only)` if fallback.
- Optional `tests/test_render_diff.cpp` (maps to feature_suggestions #1 — candidate for Task Manager to add as CTest `RenderDiff.SSIMThreshold` asserting SSIM≥0.98 on `text_only.pdf` pair) — leave to Task Manager; Phase 0 plan does not require it but architecture supports it.

**Design note — DecisionEngine slider fix (Phase C out of scope):**
Per Appendix A claim 2 verdict PARTIAL/WRONG, the slider currently forces JPEG for Screenshot/LineArt (and Photo) but correctly does NOT affect Monochrome/ScannedText. Phase 0 does NOT change `DecisionEngine.cpp:51-71`; the plan documents that a correct fix (Phase C) will scope the slider to only lossy classes (Photo/Screenshot/LineArt) and wire PNG/zlib for MaxQuality without slider — but no code is proposed here. `git diff --stat` against master in this phase must show changes only in `tools/render_diff.cpp`, `CMakeLists.txt` (one-line add), and `benchmark/run_benchmark.sh`.

- **Verification gate:** `cmake --build` succeeds with `-Wall -Wextra -Wpedantic` clean; `./build/render_diff $TMPDIR/text_only.pdf $TMPDIR/text_only_optimized.pdf` prints `SSIM PSNR` or `N/A N/A`; `run_benchmark.sh` TSV now has SSIM/PSNR columns populated (or `N/A`); no Python process spawned (verify via `ps`/`strace`); existing 12+ tests still pass.

### Phase 0.4 — Test Wiring & Acceptance Gating (CTest, verifier tests)

- **Goal:** Ensure every new corpus type and benchmark metric is gated by CTest so a fresh checkout's `ctest --test-dir build --output-on-failure` is the single trusted baseline signal (J1/J2).
- **Covers:** AC2 (GTest + CTest discover, `TEST(Suite,Case)` naming, one-line `CMakeLists.txt` add), AC3 verifier `QPDF::processFile` + key assertions, AC6 (CLI flags still listable), AC7 quality gates (valid PDF, size guard, text selectable, SSIM threshold).
- **Dependencies:** Phases 0.1–0.3 (corpus, script, helper must exist to test).

**Scope:**
- `tests/test_corpus_generator.cpp` (already touched in 0.1) — add `TEST(CorpusGenerator, EachTypeHasExpectedKey)` style checks (14 tests or parameterized) that after `generateAll()` assert `#files==14` and per-file `hasKey` checks; keep `CMakeLists.txt` `add_executable(pdfcompress_tests ...)` one-line discipline.
- Ensure `CMakeLists.txt` `enable_testing()` + `include(GoogleTest)` + `gtest_discover_tests(pdfcompress_tests)` remains; `BUILD_TESTING=ON` default.
- Manual gate: `ctest --test-dir build --output-on-failure -V` → 14+ tests pass; `diagnose` and `run_optimize --help` still list flags; optimized PDFs `QPDF().processFile` succeed and `diagnose` shows fonts preserved.

- **Verification gate:** `cmake -B build -DCMAKE_TOOLCHAIN_FILE=third_party/vcpkg/scripts/buildsystems/vcpkg.cmake && cmake --build build && ctest --test-dir build --output-on-failure` passes; `benchmark/run_benchmark.sh [build_dir]` completes with TSV; `git diff --stat` shows only `tests/`, `benchmark/`, `tools/render_diff*`, `CMakeLists.txt` (per AC5).

---

## Assumptions

- QPDF `R6` AES-128 encryption API is available via `QPDFWriter::setR6Encryption` fallback to `setR3Encryption`; if headers differ, generator tries R6 first. (Not in requirements.md; discovered during inventory.)
- `date +%s%3N` is not POSIX on macOS BSD `date`; benchmark will probe `gdate` (coreutils) then fallback to `date +%s` ×1000 so timing remains ms without assuming GNU date.
- `bc` may be absent on minimal CI images; `awk` is used as fallback for `reduction%` calc (consistent with `arch` choice).
- Corpus PDFs remain synthetic and licensed-free because they are built from QPDF `emptyPDF` + generated pixel buffers and Helvetica Type1 — no embedded third-party font/image is copied.
- `libpdfium.dylib` is bundled in `build/` for both GUI and helper; if `DYLD_LIBRARY_PATH` / `@executable_path` is wrong on CI, render-diff degrades to `N/A` rather than failing the build.
- The 14-file enumeration in AC3 is authoritative; if strict 14 is required, `merged_duplicate_fonts` coverage is embedded inside `transparency.pdf` via shared XObject + SMask on same PDF, otherwise two separate files with doc note still satisfy “14 logical types” — Task Manager will pick one interpretation and the verifier test will assert `==14`.

## Flagged Issues

| # | Requirement / Journey | Why it can't be satisfied as written | Suggested direction |
|---|---|---|---|
| F1 | AC3 — `transparency.pdf` + `merged_duplicate_fonts.pdf` counting | AC3 wording is self-contradictory: it lists 14 enumerated items where #14 “covers both alpha and duplicate-font” yet also says “generate both `transparency.pdf` and `merged_duplicate_fonts.pdf` and treat the latter as the 14th file” and later “alternatively generate 15 files and document…” — exact file count (`14` vs `15`) and naming cannot both be satisfied without an interpretation choice. | Task Manager + Validator should lock one interpretation (recommended: exactly 14 files where `transparency.pdf` contains both SMask and a shared XObject, so `merged_duplicate_fonts` case is covered without a 15th file). Validator should record the chosen count in `plan_notes.md` escalation per AC3 normative note. |
| F2 | AC3 — `monochrome_bw.pdf` “1-bit concept” via DeviceGray 8-bit | AC3 defines monochrome as 100×100 DeviceGray (1-bit concept) but QPDF/`createPdfWithImage` uses 8 bpc Flate with 1 channel — true 1-bit `DeviceGray` with `/BitsPerComponent 1` and bit-packed data would need a different stream layout and test-corpus mirroring. Generating 8-bit grayscale is a valid classification trigger (≤4 colors) but not literally 1-bit. | Generate `monochrome_bw.pdf` with `/BitsPerComponent 1` and bit-packed 0/1 data via QPDF if feasible in Phase 0, otherwise keep 8-bit with ≤4 unique colors and document “1-bit concept exercised via ≤4 colors → Monochrome path” in plan_notes; true 1-bit bit-packing is not required for AC7 but should be noted. |
| F3 | AC7 (d) — SSIM ≥0.98 threshold on synthetic corpus | Synthetic corpus images are uniform/flat (e.g., `screenshot_flat` quadrants, `grayscale_scan` solid gray) — any codec will yield SSIM≈1.0, so the threshold is trivially satisfied and does not prove perceptual quality on real PDFs. Real render-diff gating requires photo-heavy natural images. | Keep threshold as CI gate for Phase 0 (it will pass), but document that meaningful quality gating needs a future real-image corpus (Phase D+); consider adding one non-synthetic photo path using valid JPEG bytes of a tiny natural gradient to make the gate non-vacuous. |
| F4 | AC3 — `generateCmykImage` DCTDecode vs FlateDecode | Design gap #6: `cmyk_image.pdf` currently uses DCTDecode with raw pixels (invalid JPEG) — QPDF stream is corrupt and optimizer skips it, so “CMYK skip/preserve” is not truly exercised. Fixing to FlateDecode changes the filter that `PDFOptimizer` will see, but `channels==4` path skips regardless of filter, so the fix is safe. | Replace with FlateDecode + `compress()` of CMYK bytes (as done for other types) per this plan; this validates the skip path without needing valid JPEG encoding. |
| F5 | AC5 — `git diff --stat` within ±1% size delta on tiny PDFs | AC5 requires pdfcompress rows within ±1% before/after Phase 0, but the current 667-byte `text_only.pdf` shows -93% (writer overhead dominates). No behavior change can make writer overhead vanish; the ±1% bound cannot be met on tiny PDFs. | Corpus fix in Phase 0.1 should use larger realistic content (≥50KB) for benchmark-measured PDFs; AC5 gate should be evaluated on `photo_heavy`/`line_art` sized PDFs, not on `text_only`. Validator should exempt `<2KB` inputs from the ±1% check and rely on `imageBytesSaved` reporting per Constraints. |

Status: READY_FOR_VALIDATION
