# Architecture Proposal — PDF Compressor Phases 1–8 (Goals B–I)

## Codebase Summary (Existing Repo — Factual Inventory)

> Inventory only — no proposal. Graphify invoked first (`graphify-out/graph.json` exists:
> 404 nodes; queries run for components/codecs/integrations and optimizer/decision-engine/CLI
> wiring, verified by targeted file reads below). Phase-0 canonical `docs/architecture.md`
> Codebase Summary is carried forward as verified ground truth; this section adds only
> deltas probed for Phases 1–8. All labels `[existing]` / `[new]` / `[new/changed]` apply
> to the proposal sections that follow, not to this inventory.

### Verified deltas for Phases 1–8 (manual survey 2026-10-04)

| Fact | Evidence | Phase it drives |
|---|---|---|
| Slider scoping gap confirmed: Screenshot/LineArt force JPEG when `qualityHint>0`; ScannedText always JP2; Monochrome always zlib; `m_pngCodec` constructed but never selected | `src/core/DecisionEngine.cpp:13,22-71` | Phase 2 |
| CMYK skipped (`imagesSkipped++`, channels==4 path) to avoid RGBA corruption | `src/core/PDFOptimizer.cpp:236,269` | Phase 2 |
| Stream dedup is **inert**: the ESC-001 `if (it->second.isIndirect()) continue;` guard at `src/core/PDFOptimizer.cpp:373` is always taken (all `seenStreams` handles come from `pdf.getAllObjects()` and the line-338 `!obj.isIndirect()` filter admits only indirect objects), so `streamsDeduplicated++` at line 377 is dead code; and `generateSharedXObject()` returns `generateTransparency()` (one indirect image object shared by reference across pages), so no distinct duplicate pair exists even with a working replace path | `src/core/PDFOptimizer.cpp:336-386`, `tests/test_corpus_generator.cpp:450-451`, `tests/test_structure_writer.cpp:53-54` | Phase 2 (AC-C5) |
| DPI calc is page-size approximation (`calculateDPI(meta, page)`), not CTM; `CompressionParams.targetDPI` carried but unused for resampling | `src/core/PDFInspector.cpp:164,226`, `ImageMetadata.h` | Phase 3 |
| No JBIG2 encoder in tree; only `StreamFilter::JBIG2Decode` enum + string mapping exist (parse support, no encode) | `grep jbig2` → only `PDFInspector.cpp:210`, `ImageMetadata.h:26,108`; nothing in `vcpkg.json`/`CMakeLists.txt` | Phase 5 |
| No HarfBuzz/hb-subset in tree; font handling is inspect-only (emptyPDF Helvetica) | `grep harfbuzz/HarfBuzz` → zero hits | Phase 6 |
| GUI has profile picker + quality slider, but **no** QThreadPool/off-thread work, no progress/cancel, no password dialog | `grep QThreadPool|Cancel` in `src/main.cpp` → zero hits; picker `main.cpp:63-66`, slider `:75-78` | Phases 4, 7 |
| No `--jobs`, `--dpi`, `--transcode-cmyk-to-rgb`, `--jbig2-lossy`, `--subset-fonts`, `--password`, `--re-encrypt`, `--corpus-dir` flags exist yet | `tools/run_optimize.cpp` flag list (AC6 set only) | Phases 2,3,5,6,7,8 |

### Carried-forward inventory (from validated Phase-0 `docs/architecture.md`, still current)

- **Entities:** `ImageMetadata`, `AnalysisResult`, `CompressionParams` (incl. unused `targetDPI`),
  `OptimizationOptions` (`forProfile` + 7 strip booleans + `linearize=false`), `OptimizationResult`
  (granular breakdown incl. `streamsDeduplicated`, `errorMessage`), `PDFDocumentInfo`, QPDF
  stream/trailer keys. Pipeline: Inspect → Analyze → Decide → Encode → size-guarded replace → QPDFWriter.
- **Components:** GUI MainWindow, DropHandler/DropOverlay, PDFInspector (PDFium), ImageAnalyzer,
  DecisionEngine, PDFOptimizer (QPDF, per-image try/catch + size guard), codec layer
  (Jpeg/Jp2/Png/Zlib via `ImageCodec`), `run_optimize` + `diagnose` CLIs, GTest suite, corpus
  generator (14 types, `$TMPDIR`), benchmark script + `tools/render_diff.cpp` (pure-C++ SSIM/PSNR).
- **Integrations:** PDFium (inspect + render), QPDF (rewrite + corpus), libjpeg-turbo, OpenJPEG (J2K),
  libpng, zlib/libdeflate, Qt6, GTest, vcpkg/CMake/Ninja, optional `gs`/`ocrmypdf`/`qpdf` benchmark comparators.

---

## Summary

Phases 1–8 build strictly on the locked Phase-0 baseline: Phase 1 verifies-and-locks existing
structure/profile behavior with tests only; Phases 2–3 fix codec routing (slider scoping, PNG wiring,
CMYK preservation + opt-in transcode), activate stream dedup (AC-C5: inert ESC-001 guard → direct-copy
`replaceObject`), and add CTM-based DPI downsampling; Phase 4 moves batch work
off the UI thread with progress/cancel/stats; Phase 5 adds optional JBIG2 for true 1-bit; Phase 6
adds font dedup + HarfBuzz subsetting; Phase 7 adds password UX with unencrypted-by-default output;
Phase 8 hardens streaming memory, deterministic parallelism, and macOS packaging/CI. Two new
third-party deps (jbig2enc optional, HarfBuzz required) arrive via vcpkg/vendored source; everything
else is additive inside `core/` + `codecs/` + `tools/` with per-phase benchmark gates.

## Tech Stack Decisions

| # | Decision | Choice | Rationale | Alternatives considered |
|---|---|---|---|---|
| 1 | Language | **C++20, RAII, `-Wall -Wextra` clean** [existing — hard constraint] | Locked stack; all new code follows existing hygiene | — |
| 2 | GUI / build / PDF libs | **Qt6 / CMake+Ninja+vcpkg / PDFium + QPDF** [existing — hard constraint] | Locked; QThreadPool (already a Qt6 module) is the threading vehicle, not a new dep | std::thread pool — rejected (Qt signal/slot + cancel integrate with QThreadPool) |
| 3 | Image codecs (existing) | **libjpeg-turbo, OpenJPEG J2K, libpng, zlib/libdeflate** [existing] | Unchanged; PNG goes from dead code to selected codec with no new dep | — |
| 4 | Test framework | **GTest + CTest** [existing — locked] | Locked per Phase 0; new routing/DPI/JBIG2/font/password tests are `TEST(Suite,Case)` | — |
| 5 | JBIG2 encoder | **jbig2enc (Apache-2.0) via vcpkg or vendored source, OPTIONAL dep** [new] | Locked 2026-10-04 (OQ F-1); lossless-default; configure warns + falls back to zlib when unavailable so build never breaks | GPL JBIG2 libs — rejected (license); Leptonica wrappers — rejected (extra dep for no gain) |
| 6 | Font subsetter | **HarfBuzz `hb-subset` via vcpkg** [new] | Locked 2026-10-04 (OQ G-4); pure-C++, offline, keeps `/ToUnicode`; QPDF has no subsetter | fontTools (Python) — rejected (offline/pure-C++ constraint); QPDF-only — rejected (no subset API); vendored micro-subsetter — rejected (CJK/ToUnicode risk) |
| 7 | Resampling | **Lanczos/bicubic in pixel domain before encode (header-only or small vendored resampler, or Qt-free stb-style)** [new] | Phase 3 needs quality resample with no new runtime dep; decision deferred to implementation but bounded: offline, deterministic, no network | OpenCV — rejected (heavy dep); PDFium scale-on-render — rejected (changes print size semantics, not source pixels) |
| 8 | Render-diff / benchmark | **Pure-C++ PDFium render-diff + bash TSV matrix** [existing] | Locked; extended with `PeakRSS` column (Phase 8) only | Python — rejected (locked) |
| 9 | Password handling libs | **QPDF + PDFium password pass-through, no new lib** [existing] | Both already support passwords; only plumbing + GUI dialog is new | Keychain storage — rejected (out of scope; per-file prompt + env fallback locked) |
| 10 | Platform | **macOS-only, Phases 1–8** [locked constraint] | OQ I-6; packaging = signed DMG; CI = macOS leg | Portable `gdate`/Linux fallbacks kept in scripts but no CI legs required |

## System Components

| Component | Status | Responsibility & relations |
|---|---|---|
| GUI MainWindow + DropHandler/DropOverlay | [existing] | Picker/slider/checkboxes unchanged; gains batch view + cancel + summary (Ph.4) and password dialog + re-encrypt checkbox (Ph.7), all wired to off-thread worker |
| Batch worker (QThreadPool + QRunnable/signals) | [new] | Owns per-file `optimize()` off UI thread, progress signals, cooperative cancel, partial-output rollback; later phases reuse it (password prompt bridging, `--jobs` pool sizing) |
| `OptimizationOptions::forProfile` + strip/writer paths | [existing] | Normative defaults locked by tests in Ph.1; extended with new fields only (`targetDPI`, transcode/JBIG2/subset/password flags) — no semantic change to strip logic |
| DecisionEngine routing | [new/changed] | Slider scoped to lossy classes; PNG branch for alpha + MaxQuality lossless; 1-bit→JBIG2 branch; CMYK preserve/transcode branch; each branch pinned by `TEST(DecisionEngine,…)` |
| Stream dedup replace path | [new/changed] | Replaces the inert ESC-001 `isIndirect()` skip at `PDFOptimizer.cpp:373` with a direct-copy `replaceObject` (a direct copy of the replacement stream is passed so QPDF accepts it); retains the `objGen !=` guard and per-pair try/catch; `streamsDeduplicated` now increments for distinct byte-identical indirect streams (AC-C5, Phase 2) |
| DPICalculator + Downsampler (focused new files in `core/`) | [new] | CTM→effective-DPI per image (fallback: page-size approx + log); Lanczos/bicubic resample honoring 300/150/96 targets, 32px floor, never-upscale; updates `/Width /Height` + CTM consistently |
| `diagnose` effective-DPI column | [new/changed] | Reports per-image effective DPI (±5% gate source for AC-D1) |
| Jbig2Codec (`ImageCodec` impl) | [new] | Wraps jbig2enc lossless default / `--jbig2-lossy` opt-in; size-guard + zlib fallback; disabled-with-warning when dep absent |
| CmykHandler (preserve path + opt-in transcode path) | [new] | ICC/DeviceCMYK round-trip preservation; `--transcode-cmyk-to-rgb` conversion with ΔE sampling internally, SSIM≥0.98 as normative gate |
| FontDedup + FontSubsetter (hb-subset wrapper) | [new] | Hash-join dedup reusing the Phase-2 corrected direct-copy `replaceObject` path (AC-C5 — the ESC-001 indirect-handle guard it replaces no longer exists after Ph.2; requirements Phase 6 wording staleness flagged as FI-4); subset ON Balanced/MaxCompression, OFF MaxQuality; AcroForm fonts exempt (dedup only); `/ToUnicode` preserved |
| Password flow (GUI dialog + CLI `--password`/`PDFOPTIMIZE_PASSWORD` + re-encrypt) | [new] | Per-file prompt, no caching, never logged; decrypt→optimize→unencrypted default; `--re-encrypt` preserves AES-128 params |
| Parallel page/image scheduler + RSS accounting | [new/changed] | `--jobs` (default core count), thread-local buffers, join-before-write determinism; `PeakRSS` sampling into TSV; `$TMPDIR` spill for OOM-risk files |
| `run_optimize` new flags | [new/changed] | `--transcode-cmyk-to-rgb`, `--dpi/--no-downsample`, `--no-jbig2/--jbig2-lossy`, `--subset-fonts/--no-subset-fonts`, `--password/--re-encrypt`, `--jobs` |
| Corpus (14 files) + staged fixtures | [existing] | Canonical 14 unchanged (requirements change needed for more); 1-bit scan, oversampled, tiny-icon, large-mixed, and `distinct_duplicate_streams.pdf` fixtures staged in `$TMPDIR`, never committed |
| Benchmark script + `render_diff` | [new/changed] | Gains `PeakRSS` column (Ph.8); per-phase before/after TSV comparison is the gate mechanism |

## Data Model

| Entity | Status | Key fields / constraints |
|---|---|---|
| `OptimizationOptions` | [new/changed] | Existing fields unchanged; adds `targetDPI` (default per profile 300/150/96, `--dpi` override, `--no-downsample` disables), `transcodeCmykToRgb=false`, `useJbig2=true/lossy=false`, `subsetFonts` (per-profile matrix), `password` (never logged), `reEncrypt=false`, `jobs=coreCount` |
| `OptimizationResult` | [new/changed] | Adds `transcodedCmyk`, `jbig2Images`, `fontsDeduplicated`, `fontsSubsetted`, `peakRssBytes`; `streamsDeduplicated` becomes live in Phase 2 (AC-C5: was inert `0`, now increments for distinct byte-identical streams); other existing counters unchanged |
| `ImageMetadata` | [new/changed] | Adds `effectiveDpiX/Y` + `dpiSource` (CTM vs fallback) alongside existing page-approx `dpiX/dpiY`; `hasAlpha`, `bitsPerComponent`, `colorSpace` now drive PNG/JBIG2/CMYK branches |
| `AnalysisResult`, `PDFDocumentInfo`, `CompressionParams` | [existing] | Unchanged (params `targetDPI` now actually consumed by downsampler) |
| `BenchmarkRow` TSV | [new/changed] | Existing 9 columns + `PeakRSS` (Ph.8) |

## Integration Points

| Integration | Status | Purpose |
|---|---|---|
| PDFium (inspect + render + password) | [existing] | Adds CTM/bounds queries for effective DPI + `FPDF_LoadDocument(password)` path |
| QPDF (rewrite + password + deterministic write) | [existing] | Adds password pass-through, re-encrypt write, font-object walk for dedup/subset, and a direct-copy `replaceObject` for live stream dedup (Ph.2, AC-C5) |
| libjpeg-turbo / OpenJPEG / libpng / zlib | [existing] | PNG becomes live-selected; no version changes |
| jbig2enc (Apache-2.0, optional) | [new] | 1-bit lossless/lossy encode; absent → warn + zlib fallback |
| HarfBuzz hb-subset (required from Ph.6) | [new] | Embedded TTF/OTF subsetting with `/ToUnicode` intact |
| `gs` / `ocrmypdf` / `qpdf` CLI comparators | [existing] | Unchanged benchmark role |
| macOS packaging (signed DMG, entitlements, bundled `libpdfium.dylib`) | [new/changed] | Documented path; `.app` launches without `DYLD_*` |

## Non-Functional Approach

| Requirement | Approach |
|---|---|
| **Offline** (all phases) | No network in app/tests/benchmark; jbig2enc + HarfBuzz via vcpkg/vendored at configure time (same exception class as PDFium fetch) |
| **Performance** | One-image-at-a-time kept; Ph.3 resample bounded by target DPI; Ph.8 `--jobs` parallelism + RSS cap (measure-first, lock-second) + `$TMPDIR` spill |
| **Determinism** | `setDeterministicID` retained; Ph.8 AC-I2 (1-vs-4-jobs pixel-identical) gates parallelism; no run is larger silently (S2) |
| **Structural correctness (Ph.2 dedup)** | Live dedup replaces only byte-identical *distinct* streams (matching signature + `memcmp` + `objGen !=` guard); direct-copy `replaceObject`; per-pair try/catch falls back to original so one bad pair never fails the file; `qpdf --check` clean under defaults and `--no-dedup` (AC-C5) |
| **Security** | Passwords never logged/TSV'd; env fallback; 3-attempt GUI limit; re-encrypt preserves input AES-128 params; lossy-JBIG2 and CMYK-transcode both opt-in (safe defaults) |
| **Accessibility/correctness** | S3 render gates + S4 text-identity (extract-before/after) enforced per phase; form fonts never subset; tiny-icon floor prevents legibility loss |
| **Hygiene/UX** | RAII, `-Wall -Wextra` clean, reviewable diffs, `core/codecs/tools` layout, `USAGE.md` per change, GUI never blocked >100ms (QThreadPool + cancel) |

## Architecture Risks

| Risk | Severity | Mitigation |
|---|---|---|
| CTM inaccurate for rotated/cropped images (H/M) | H | AC-D1 fixtures incl. rotation; fallback to page-approx + log; SSIM gate catches over-shrink |
| Font subset breaks CJK/ToUnicode or form editing (H/M) | H | Never subset form fonts; CJK manual fixtures; byte-identical text-extraction gate; OFF for MaxQuality |
| Password leaks into logs/dumps (H/L) | H | Never-log rule + code-review checklist; env path; `TEST` asserts no secret in stdout/TSV |
| jbig2enc unavailable/fails to build (H/L) | H | Optional dep: configure warning + zlib fallback; accepted Apache-2.0 license |
| Double-degrade: downsample + JPEG recompress (M/H) | M | Resample in pixel domain pre-encode; SSIM gates per phase |
| QPDF handle races in parallel mode (M/M) | M | Thread-local buffers, join-before-write, AC-I2 determinism gate |
| Dedup direct-copy `replaceObject` throws or corrupts a stream on an unusual/encrypted object (M/M) | M | Keep the existing per-pair try/catch (one bad pair never fails the whole file); replace only on signature + raw-byte match with the `objGen !=` guard retained; `qpdf --check` clean gate under defaults and `--no-dedup` (AC-C5); staged fixture keeps the change isolated from the canonical 14 |
| RSS cap unachievable without spill redesign (M/M) | M | Measure-first-then-lock; `$TMPDIR` spill allowed; cap is a plan step, not a guess |
| Scope creep: target-size mode, JXL, Windows/Linux (M/L) | M | Locked OUT (OQ E-2/I-6, feature #5 rejected); Validator to reject out-of-scope additions |
