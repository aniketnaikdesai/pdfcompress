# Plan — PDF Compressor (Phase 0 Baseline + Phases 1–8)

> **Phase 0 (0.1–0.4) below is retained from the validated Phase-0 canonical
> plan** as the completed baseline. **Phases 1–8 extend it** per
> `docs/requirements/requirements.md` Part II (Status: READY_FOR_PLAN) and
> standing gates S1–S9 (applied to every phase; per-phase sections state only
> their phase-specific benchmark expectation). Phase numbering/names follow
> requirements.md exactly: Ph.1=B structure, Ph.2=C codecs, Ph.3=D DPI, Ph.4=E
> batch UX, Ph.5=F JBIG2, Ph.6=G fonts, Ph.7=H passwords, Ph.8=I
> perf/packaging. Target-size mode is NOT in scope in any phase (OQ E-2).
> macOS-only (OQ I-6).
>
> **2026-10-07 revision:** Phase-2 **AC-C5** (deferred ESC-010 stream-dedup
> activation — inert ESC-001 guard → direct-copy `replaceObject`, staged
> `distinct_duplicate_streams.pdf`, P1-T3 rewrite to `>= 1`) folded into Phase 2.
> Phase 1 stays verify-only; no font/JBIG2/DPI scope pulled in. Re-validated
> `APPROVED`.

## Phases (Phase 0 — retained baseline)

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
- **Requirements / Journeys covered:** AC4 render-diff bullet (pure C++ PDFium, N/A fallback, no Python), AC7(d) SSIM thresholds ≥0.98 Balanced/MaxQuality, ≥0.95 MaxCompression — measured by helper, `N/A` allowed only if PDFium unavailable; J1 step 4; Assumptions Log render-diff row.
- **Dependencies:** Phase 0.2 (TSV columns already exist; this phase fills SSIM/PSNR).
- **Scope (new logic in focused files only):**
  - **New file** `tools/render_diff.cpp` (canonical) — C++20, RAII (`FPDF_DOCUMENT`, `FPDF_PAGE`, `FPDF_BITMAP` via unique_ptr with custom deleters), offline. API: `int main(argc,argv)` takes `original.pdf optimized.pdf`; uses `FPDF_InitLibraryWithConfig` → `FPDF_LoadDocument` → `FPDF_LoadPage(doc,0)` → `FPDF_GetPageWidth/Height` → 150 DPI bitmap `w=pageWidthPt/72*150`, `h=pageHeightPt/72*150` → `FPDFBitmap_Create(w,h,0)` → `FPDFBitmap_FillRect(...0xFFFFFFFF)` → `FPDF_RenderPageBitmap` → BGRA buffers for both PDFs → compute SSIM (windowed mean/variance/covariance, C1/C2) and PSNR (MSE→10*log10(255^2/MSE)) in C++. If either `FPDF_LoadDocument` fails (encrypted without password, missing dylib), print `N/A N/A` to stdout and exit 0; never crash. No `python3`/`pip`/`scikit-image`.
  - `CMakeLists.txt` — add `add_executable(render_diff tools/render_diff.cpp)` linked `PRIVATE pdfcompress_core pdfium`, under `if(BUILD_TESTING)` or alongside `diagnose`.
  - `benchmark/run_benchmark.sh` — after each tool call, invoke `"$BUILD_DIR/render_diff" "$pdf" "$out_file"` (guarded: `if [ -x "$BUILD_DIR/render_diff" ]; then ... else SSIM=N/A PSNR=N/A; fi`) and fill TSV columns; header note `render-diff unavailable (PDFium C++ only)` if fallback.
- **Design note — DecisionEngine slider fix (Phase C out of scope):** Per Appendix A claim 2 PARTIAL/WRONG, slider currently forces JPEG for Screenshot/LineArt (and Photo) but correctly does NOT affect Monochrome/ScannedText. Phase 0 does NOT change `DecisionEngine.cpp:51-71`; correct fix (Phase 2) will scope slider to only lossy classes and wire PNG/zlib for MaxQuality without slider — no code proposed here. `git diff --stat` must show changes only in `tools/render_diff.cpp`, `CMakeLists.txt`, and `benchmark/run_benchmark.sh`.
- **Verification gate:** `cmake --build` succeeds `-Wall -Wextra -Wpedantic` clean; `./build/render_diff $TMPDIR/text_only.pdf $TMPDIR/text_only_optimized.pdf` prints `SSIM PSNR` or `N/A N/A`; `run_benchmark.sh` TSV now has SSIM/PSNR columns populated (or `N/A`); no Python spawned; existing tests still pass.

### Phase 0.4 — Test Wiring & Acceptance Gating (CTest, verifier tests)

- **Goal:** Ensure every new corpus type and benchmark metric is gated by CTest so a fresh checkout's `ctest --test-dir build --output-on-failure` is the single trusted baseline signal (J1/J2).
- **Requirements / Journeys covered:** AC2 (GTest + CTest discover, `TEST(Suite,Case)` naming, one-line `CMakeLists.txt` add), AC3 verifier `QPDF::processFile` + key assertions, AC6 (CLI flags still listable), AC7 quality gates (valid PDF, size guard, text selectable, SSIM threshold).
- **Dependencies:** Phases 0.1–0.3 (corpus, script, helper must exist to test).
- **Scope:**
  - `tests/test_corpus_generator.cpp` (already touched in 0.1) — add `TEST(CorpusGenerator, EachTypeHasExpectedKey)` style checks (14 tests or parameterized) that after `generateAll()` assert `#files==14` and per-file `hasKey` checks; keep `CMakeLists.txt` `add_executable(pdfcompress_tests ...)` one-line discipline.
  - Ensure `CMakeLists.txt` `enable_testing()` + `include(GoogleTest)` + `gtest_discover_tests(pdfcompress_tests)` remains; `BUILD_TESTING=ON` default.
  - Manual gate: `ctest --test-dir build --output-on-failure -V` → 14+ tests pass; `diagnose` and `run_optimize --help` still list flags; optimized PDFs `QPDF().processFile` succeed and `diagnose` shows fonts preserved.
- **Verification gate:** `cmake -B build -DCMAKE_TOOLCHAIN_FILE=third_party/vcpkg/scripts/buildsystems/vcpkg.cmake && cmake --build build && ctest --test-dir build --output-on-failure` passes; `benchmark/run_benchmark.sh [build_dir]` completes with TSV; `git diff --stat` shows only `tests/`, `benchmark/`, `tools/render_diff*`, `CMakeLists.txt` (per AC5).

## Phases (Phases 1–8 — Goals B–I)

> Standing gates S1–S9 (requirements.md Part II) apply to EVERY phase below and are not repeated per phase except the phase-specific benchmark expectation. Sequencing is benchmark-isolating: codec routing (2) → dims (3) → UI (4) → 1-bit (5) → fonts (6) → passwords (7) → perf/packaging (8), so each TSV delta is attributable to one cause.

### Phase 1 (Goal B) — Structure & Profile-Defaults Verify-and-Complete

- **Goal:** Lock existing `forProfile` defaults and writer behavior with verifier tests; close doc gaps only. No strip-semantics change (locked: MaxQuality-strips-nothing / Balanced-strips-JS+metadata-only / MaxCompression-strips-everything, `linearize=false`).
- **Requirements / Journeys covered:** AC-B1 (defaults tests), AC-B2 (per-corpus stripping incl. non-JS `/OpenAction` GoTo survival), AC-B3 (dedup/object-stream/Flate flags), AC-B4 (±2% benchmark gate); J-B1, J-B2.
- **Dependencies:** None (first; builds on locked Phase-0 baseline + 14-file corpus).
- **Scope:** New `tests/test_profile_defaults.cpp` (`TEST(ProfileDefaults,…)` ×3 + CLI round-trip for `--strip-*/--no-strip-*`, `--linearize/--no-flate-recompress/--no-dedup`); `/AA` + non-JS `/OpenAction` handling completion; `--linearize` USAGE.md gap; no `DecisionEngine`/codec/DPI change. The P1-T3 dedup test (`tests/test_structure_writer.cpp`, `streamsDeduplicated == 0`) is the correct Phase-1 pin (verify-only lock) and is rewritten to `>= 1` in Phase 2 (AC-C5) when the deferred fix lands.
- **Per-phase gates:** Benchmark before/after within ±2% every row (no compression change expected); manual PDFs: `with_form`, `with_bookmarks_and_links`, `with_javascript`, `with_metadata`, `transparency`, `text_only` (no-growth control); S1–S9.
- **STOP boundary:** STOP after verifier tests green + USAGE.md defaults table + both TSVs archived — no Phase 2 work until AC-B1/B2/B3 pass and any default dispute is filed as a requirements change.

### Phase 2 (Goal C) — Codec-Selection Correctness

- **Goal:** Scope slider to lossy classes, wire PNG for alpha/lossless, preserve CMYK correctly with opt-in transcode, and activate stream dedup (replace the inert ESC-001 guard with a direct-copy `replaceObject` path).
- **Requirements / Journeys covered:** AC-C1 (slider lossy-only + `TEST(DecisionEngine, SliderOnlyAffectsLossyClasses)`), AC-C2 (PNG for alpha/MaxQuality lossless, SSIM 1.0, `m_pngCodec` reachable), AC-C3 (CMYK preserved, never corrupted), AC-C3b (`--transcode-cmyk-to-rgb` + GUI checkbox, SSIM≥0.98, `transcodedCmyk=1`), AC-C4 benchmark gate, AC-C5 (stream dedup activated — deferred ESC-010 follow-up: `streamsDeduplicated >= 1` with defaults on a staged distinct-byte-identical fixture, `== 0` with `--no-dedup`, `qpdf --check` clean, S2–S5, benchmark before/after); J-C1, J-C2.
- **Dependencies:** Phase 1 (profile defaults locked first; routing builds on them; the Phase-1 P1-T3 `streamsDeduplicated == 0` test is rewritten here).
- **Scope:** `DecisionEngine.cpp` branch rework (lossy-only slider; PNG branch; ScannedText profile-driven, Monochrome always lossless); new `CmykHandler` (ICC/DeviceCMYK preserve + Adobe APP14 safety + skip-with-reason logging; opt-in transcode with internal ΔE sampling); `run_optimize --transcode-cmyk-to-rgb`; GUI checkbox; USAGE.md slider/profile/CMYK docs. **Plus the AC-C5 structural stream-dedup activation:**
  - **Product fix:** in `src/core/PDFOptimizer.cpp` replace the inert ESC-001 indirect-handle skip at line 373 (`if (it->second.isIndirect()) continue;`) with a **direct-copy `replaceObject`** path — pass a direct copy of the replacement stream to `QPDF::replaceObject(obj.getObjGen(), directCopy)` so QPDF accepts it — so two distinct-but-byte-identical indirect streams actually dedup and `streamsDeduplicated` increments. Preserve the existing `obj.getObjGen() != it->second.getObjGen()` guard (line 366) and the per-pair try/catch (lines 372-382) so one bad pair never fails the whole file; `seenStreams` may keep storing indirect handles for signature comparison — only the object handed to `replaceObject` must be a direct copy.
  - **New staged fixture:** `distinct_duplicate_streams.pdf` in `$TMPDIR` — two **distinct** indirect stream objects with byte-identical raw data and matching signatures (same `/Subtype /Width /Height /ColorSpace /BitsPerComponent`). This is NOT the reference-shared `transparency.pdf` / `generateSharedXObject()` fixture and is NOT added to the 14 canonical corpus files.
  - **Test rewrite:** the Phase-1 P1-T3 test (`tests/test_structure_writer.cpp`, currently asserting `streamsDeduplicated == 0`) is rewritten to assert `>= 1` against the new staged fixture, retaining a `== 0` assertion for the `--no-dedup` run (P1-T3-T01/T03 continue to cover the shared-XObject PDF).
- **Per-phase gates:** Benchmark: screenshot/line-art MaxQuality rows change codec at SSIM 1.0, photos ±5%, CMYK row reason-logged, all rows S2–S5; manual PDFs: `screenshot_flat`, `line_art`, `photo_jpeg`, `transparency` (alpha), `cmyk_image`, `monochrome_bw` + `grayscale_scan` controls, plus the staged `distinct_duplicate_streams.pdf` for AC-C5. **AC-C5 gates:** with defaults `result.streamsDeduplicated >= 1` on the staged fixture and the output shares one stream object for that pair (`qpdf --check` clean; `qpdf --json` shows a single stream); with `--no-dedup` `streamsDeduplicated == 0` and both stream objects remain; S2–S5 hold; benchmark before/after on the same corpus with dedup-bearing rows ≤ baseline (additive, no regression on other rows).
- **STOP boundary:** STOP after AC-C1/C2/C3/C3b/C5 tests + CMYK Chrome+Preview visual check + TSV delta — no downsampling work until codec routing and stream dedup are pinned (Phase 3 consumes its output dims).

### Phase 3 (Goal D) — Effective-DPI Downsampling

- **Goal:** CTM-based effective DPI + profile-target downsampling with floor, print size unchanged.
- **Requirements / Journeys covered:** AC-D1 (`diagnose` effective-DPI column, 600-vs-150 ±5%, `TEST(Inspector, EffectiveDpiFromCtm)`), AC-D2 (300/150/96 targets, 32px floor, never-upscale, `--dpi/--no-downsample`), AC-D3 (≥20% on oversampled rows, ±3% at-target, ±2% text rows); J-D1.
- **Dependencies:** Phase 2 (downsampler feeds the corrected codec path; dims change would confound Phase-2 codec gates if reordered).
- **Scope:** New `core/DPICalculator` (CTM extraction honoring crop/rotation; fallback + log) and `core/Downsampler` (Lanczos/bicubic, floor, never-upscale/vector-exempt, `/Width /Height`+CTM update); `diagnose` DPI column; CLI flags; USAGE.md targets/floor docs.
- **Per-phase gates:** Benchmark gate AC-D3; manual PDFs: staged oversampled 600-DPI scan (in `$TMPDIR`, not committed), `photo_heavy`, `grayscale_scan`, `line_art` (edge check), tiny-icon PDF (floor check).
- **STOP boundary:** STOP after AC-D1/D2 fixtures (incl. rotation case) + oversampled ≥40% J-D1 check at SSIM≥0.98 + TSV delta — later phases assume dims are final.

### Phase 4 (Goal E) — GUI Batch UX: Progress, Cancel, Off-Thread, Stats

- **Goal:** Responsive cancellable batch with per-file/total progress and before/after summary; existing picker/slider/checkboxes untouched. No target-size ("fit ≤ N MB") mode — locked NOT in scope.
- **Requirements / Journeys covered:** AC-E1 (cancel ≤2s, no half-written output, loop never blocked >100ms), AC-E2 (per-file bytes/%/profile + totals + `errorMessage` rows); J-E1 (incl. encrypted-without-password row showing `FAILED (encrypted — password required)`).
- **Dependencies:** Phases 1–3 (batch displays their result fields; stable single-file behavior first).
- **Scope:** GUI-side only + thin worker: QThreadPool batch worker, progress signals, cancel with finish-or-rollback, summary view, per-file error rows; no compression-logic change; USAGE.md flow docs.
- **Per-phase gates:** Benchmark: no output-bytes change vs pre-Phase-4 (UI-only; rows within ±1%); manual: 14-file batch incl. `encrypted.pdf` error path, mid-batch cancel, Preview+Chrome check.
- **STOP boundary:** STOP after manual cancel-timing check + summary screenshot/TSV + S1–S9 — the worker/cancel pattern is the template Phases 5–8 reuse, so it must be reviewed before codec/font/password work lands on top.

### Phase 5 (Goal F) — JBIG2 Monochrome

- **Goal:** True 1-bit images via lossless JBIG2 (jbig2enc, optional dep); 8-bit grayscale keeps zlib. Lossless default; lossy only via explicit flag.
- **Requirements / Journeys covered:** AC-F1 (1-bit→JBIG2Decode ≤ zlib size; 8-bit→Flate; routing test), AC-F2 (lossless default SSIM 1.0; `--jbig2-lossy` ≥0.99; failure→zlib/original + log), AC-F3 (1-bit row ≥10% smaller at SSIM 1.0, others ±2%); J-F1 (incl. `--no-jbig2` override).
- **Dependencies:** Phases 2 (routing point), 4 (batch shows `jbig2Images`/fallback reasons).
- **Scope:** New `codecs/Jbig2Codec` (`ImageCodec` impl); vcpkg/vendored jbig2enc wiring with configure-warn + disable path; `--no-jbig2/--jbig2-lossy` flags; `1-bit` fixture staged in `$TMPDIR` (canonical 14 unchanged); USAGE.md licensing + policy docs.
- **Per-phase gates:** Benchmark gate AC-F3; manual PDFs: staged true-1-bit scan (`/BitsPerComponent 1`), `monochrome_bw` (8-bit control), `grayscale_scan` (JP2 control).
- **STOP boundary:** STOP after 1-bit SSIM-1.0 + fallback-injection test + license note in USAGE.md — lossy path must never be default; any license failure keeps zlib and is documented, not silent.

### Phase 6 (Goal G) — Font Dedup + Subsetting

- **Goal:** Identical-font dedup + hb-subset subsetting (ON Balanced/MaxCompression, OFF MaxQuality) with selectable text always intact.
- **Requirements / Journeys covered:** AC-G1 (single embedded copy, text-identical, `qpdf --check` clean), AC-G2 (Balanced smaller + text-identical + `/ToUnicode` present; MaxQuality full program), AC-G3 (font-heavy rows shrink, image rows ±2%); J-G1 (form fonts exempt: dedup only + logged).
- **Dependencies:** Phases 1 (structure baseline), 4 (batch reporting); independent of Phases 2–3–5 codec paths but sequenced after them to isolate benchmark deltas.
- **Scope:** Font walker (AcroForm-reference detection for the subset exemption), hash-join dedup with indirect-handle guard, `hb-subset` wrapper, `--subset-fonts/--no-subset-fonts` (profile-matrix defaults), text-extraction identity checks in tests; USAGE.md behavior matrix.
- **Per-phase gates:** Benchmark gate AC-G3; manual PDFs: merged-duplicate-fonts PDF, `text_only`, `with_form` (field editing still works), large mixed-text + CJK fixture.
- **STOP boundary:** STOP after text-identity diffs (before/after extraction) + Preview/Chrome tofu check + TSV delta — any glyph loss or form breakage blocks Phases 7–8.

### Phase 7 (Goal H) — Encrypted PDFs: Password UX + Re-encryption

- **Goal:** Per-file password prompt (GUI) + `--password`/`PDFOPTIMIZE_PASSWORD` (CLI); unencrypted output by default; opt-in re-encrypt.
- **Requirements / Journeys covered:** AC-H1 (correct→unencrypted success; wrong/missing→`success=false` + `errorMessage`, no crash/output), AC-H2 (`--re-encrypt` → still-encrypted, S2 holds); J-H1 (3-attempt inline error, batch continues, secret never logged).
- **Dependencies:** Phase 4 (per-file dialog + batch-continue pattern); orthogonal to Phases 2–3–5–6 (decrypt happens before the shared optimize path).
- **Scope:** GUI password dialog (per-file, cancellable, no caching), QPDF/PDFium password pass-through, `--password/--re-encrypt` + env fallback, wrong-password `errorMessage` paths, GUI checkbox copy; USAGE.md security notes (never-logged, test-`test123`-vs-production distinction).
- **Per-phase gates:** Benchmark: corpus rows unchanged except `encrypted.pdf` (now optimizable with password); manual PDFs: `encrypted.pdf` correct/wrong/missing + mixed batch; Preview open-with/without-password checks.
- **STOP boundary:** STOP after wrong-password + missing-password + re-encrypt matrix + log/TSV secret-scan (no secret present) — password handling must be audited before packaging/CI work.

### Phase 8 (Goal I) — Performance, Streaming, Packaging & CI Hardening

- **Goal:** Memory-safe large files, deterministic parallelism, signed macOS packaging, green CI matrix. No compression-feature change (output bytes identical vs single-thread).
- **Requirements / Journeys covered:** AC-I1 (≥200MB staged `$TMPDIR` file, peak RSS ≤ measured-then-locked cap, TSV `PeakRSS` column), AC-I2 (`--jobs 1` vs `4` pixel-identical, SSIM 1.0), AC-I3 (clean-checkout CI green, bundled `libpdfium.dylib`, DMG/signing documented, missing tools SKIPPED); J-I1 (`$TMPDIR` spill).
- **Dependencies:** All Phases 1–7 (final hardening over the complete pipeline; RSS/parallelism measured on final behavior).
- **Scope:** Streaming discipline audit + `$TMPDIR` spill, `--jobs` scheduler (thread-local buffers, join-before-write), RSS sampling + TSV column, macOS DMG/sign/entitlements docs, CI job (corpus + benchmark + S1–S5); no compression-feature change.
- **Per-phase gates:** Full-corpus S1–S9 + determinism gate + RSS-cap gate; manual: ≥200MB mixed PDF (staged, never committed), `photo_heavy` 1-vs-4, full 14-file CI matrix.
- **STOP boundary:** STOP (program checkpoint) after CI green on clean checkout + RSS cap locked from measurement + packaging verified on a second machine — release readiness, not just code-complete.

## Assumptions

### Phase 0 (retained)

- QPDF `R6` AES-128 encryption API is available via `QPDFWriter::setR6Encryption` fallback to `setR3Encryption`; if headers differ, generator tries R6 first. (Not in requirements.md; discovered during inventory.)
- `date +%s%3N` is not POSIX on macOS BSD `date`; benchmark will probe `gdate` (coreutils) then fallback to `date +%s` ×1000 so timing remains ms without assuming GNU date.
- `bc` may be absent on minimal CI images; `awk` is used as fallback for `reduction%` calc.
- Corpus PDFs remain synthetic and licensed-free because they are built from QPDF `emptyPDF` + generated pixel buffers and Helvetica Type1 — no embedded third-party font/image is copied.
- `libpdfium.dylib` is bundled in `build/` for both GUI and helper; if `DYLD_LIBRARY_PATH` / `@executable_path` is wrong on CI, render-diff degrades to `N/A` rather than failing the build.
- The 14-file enumeration in AC3 is authoritative; for strict 14, `merged_duplicate_fonts` coverage is embedded inside `transparency.pdf` via shared XObject + SMask on same PDF (RG-001) — Task Manager will assert `==14`.

### Phases 1–8 (additions — sequencing rationale, not requirements changes)

- Phase ordering is benchmark-isolating: codec routing (2) → dims (3) → UI (4) → 1-bit (5) → fonts (6) → passwords (7) → perf/packaging (8), so each TSV delta is attributable to one cause. (Not in requirements.md; sequencing rationale.)
- Staged fixtures beyond the canonical 14 (true-1-bit scan, oversampled 600-DPI, tiny-icon, ≥200MB mixed, CJK, and the Phase-2 `distinct_duplicate_streams.pdf`) live in `$TMPDIR`, are never committed, and need no requirements change (consistent with the `$TMPDIR`-ephemera constraint; design gap #10's locked direction; the canonical 14 stay exactly 14 per the safety invariant).
- Phase 8 RSS cap is measure-first-then-lock (≤1GB starting hypothesis per AC-I1); the measurement run itself is a plan step, not a pre-declared pass/fail number.
- Resampler choice (Lanczos vs bicubic) and CMYK ΔE sampling metric are implementation-side selections bounded by SSIM gates (AC-C3b/AC-D3); Task Manager/Build choose within those bounds.
- Phase numbering follows requirements.md Part II exactly (Ph.4 = Goal E batch UX). No new phase is introduced for any shorthand; all scope maps to existing AC.

## Flagged Issues

_Phases 0 F1–F5 reviewed in the Phase-0 cycle: F1 locked as planning note RG-001 (exactly 14 files, see `docs/plan_notes.md`); F2–F5 correctly flagged as implementation notes / risks, not requirements escalations, and addressed by the architecture's mitigations. No ESCALATED_TO_REQUIREMENTS was required._

_Phases 1–8 FI-1–FI-3 from the proposal evaluated in this cycle (see `docs/plan_review.md` Requirements Escalations): FI-1 (AC-E1 timing testability), FI-2 (Phase 6 form-font detector unspecified), FI-3 (Phase 8 RSS cap pending measurement) are all testability/implementation notes already accommodated by the requirements text (AC-E1 manual+inspection verification, AC-I1 measure-then-lock, AcroForm-reference walk within the specified exemption) — surfaced, not reinterpreted, no escalation._

- **FI-4 — Phase 6 (Goal G) font-dedup wording still references the ESC-001 indirect-handle guard that Phase 2 removes (added 2026-10-07 revision).** Requirement/journey: Phase 6 Goals ("same hash-join pattern as stream dedup, with the ESC-001 indirect-handle guard") and `design_gaps.md` #15. Why it can't be satisfied as written: Phase 2's AC-C5 replaces the inert ESC-001 `isIndirect()` guard at `PDFOptimizer.cpp:373` with a direct-copy `replaceObject` path, so after Phase 2 the ESC-001 guard no longer exists and Phase 6 would cite a removed mechanism. This is a genuine requirements-staleness gap surfaced while folding in the Phase-2 addition, not a Phase-2 defect (Phase 2 stays requirements-bound; no font work is pulled in). Not a blocking requirements defect requiring escalation — Phase 6 is planned later; when Phase 6 is planned the Phase 6 Goal wording should cite the Phase-2 direct-copy dedup path as the pattern to reuse. Until then `docs/architecture.md` labels Phase 6's dedup as reusing the Phase-2 corrected pattern. Flagged, not silently rewritten.

Status: READY_FOR_TASK_MANAGER
