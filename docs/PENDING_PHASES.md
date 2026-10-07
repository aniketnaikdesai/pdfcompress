# PDF Compressor — Phases Overview & Pending Work

> Living roadmap. Source of truth for phase scope is `docs/plan.md` (canonical, Validator-owned);
> this document is a human-readable summary generated from it. Last updated: 2026-10-07.

## Status at a glance

| Phase | Goal | Status |
|-------|------|--------|
| **0** | Verification & baseline | ✅ **Complete** |
| **1** | Control, safety & structure (verify-and-complete) | ✅ **Complete** |
| **2** | Per-image decisions (codec selection correctness) | ✅ **Complete** (10/10 tasks; ESC-011…015 fixed; one cosmetic follow-up ESC-016) |
| **3** | Effective-DPI downsampling | ⬜ Pending |
| **4** | GUI batch UX (progress, cancel, off-thread, stats) | ⬜ Pending |
| **5** | JBIG2 monochrome | ⬜ Pending |
| **6** | Font dedup + subsetting | ⬜ Pending |
| **7** | Encrypted PDFs (password UX + re-encryption) | ⬜ Pending |
| **8** | Performance, streaming, packaging & CI hardening | ⬜ Pending |

Locked global constraints: fully offline · C++20 RAII, `-Wall -Wextra` clean · keep `core/`, `codecs/`, `tools/` layout · new codecs implement `ImageCodec` · per-step wrapping with fallback + log · never overwrite input · CLI flag for every option · USAGE.md updated per user-visible change · GTest + CTest · macOS-only for Phases 1–8.

---

## ✅ Phase 0 — Verification & Baseline (complete)

Proved which USAGE.md claims were true/false against the code, and built the measurement baseline. No behavior change.
- **Delivered:** 14-type deterministic QPDF test corpus (`$TMPDIR`, licensed-free); 5-tool benchmark matrix (`pdfcompress`, `qpdf`, Ghostscript `/ebook` + `/screen`, `ocrmypdf`) with graceful SKIPPED; pure-C++ PDFium render-diff (SSIM/PSNR, N/A fallback); GTest corpus verifier + CTest wiring; granular `OptimizationResult`.
- **Committed:** `67a54b9`, `9441c8c`, `7da7cd3`.

---

## ✅ Phase 1 — Control, Safety & Structure (complete)

Verify-and-complete only — **no strip-semantics change** (locked defaults: Max Quality strips nothing / Balanced strips JS + metadata only / Max Compression strips everything; `linearize=false`).
- **Delivered:** `tests/test_profile_defaults.cpp`, `test_stripping_profiles.cpp`, `test_structure_writer.cpp` wired into CTest (44 tests green); USAGE.md defaults table + full flag table + measured benchmark numbers replacing the "20–80%" claim.
- **Committed:** `ec9d32d`.

---

## ✅ Phase 2 — Per-Image Decisions / Codec-Selection Correctness (complete)

**Goal:** Scope the JPEG-quality slider to lossy classes only, wire PNG for alpha/lossless, preserve CMYK correctly with an opt-in transcode, and activate stream dedup via reference-rewriting.

**Requirements covered:** AC-C1 (slider lossy-only), AC-C2 (PNG for alpha + Max Quality lossless, SSIM 1.0), AC-C3 (CMYK preserved, never corrupted), AC-C3b (`--transcode-cmyk-to-rgb` + GUI checkbox), AC-C4 (benchmark gate), AC-C5 (stream dedup activated).

**Delivered so far (done):**
- **P2-T1** — slider affects only Photo/Screenshot/LineArt; Monochrome (zlib/Deflate) and ScannedText (JPEG 2000) ignore it; PNG selected for alpha and Max Quality lossless.
- **P2-T2** — `/SMask`/`/Mask` detection → alpha forces the lossless path; masks preserved (never flattened).
- **P2-T3** — `DeviceGray` scans classify as Scanned Text; non-8-bit (bit-packed) images kept original with a logged reason.
- **P2-T4** — CMYK preserved by default (DeviceCMYK / ICCBased-CMYK) with a logged reason instead of a silent skip.
- **P2-T5** — opt-in CMYK→RGB transcode: `CmykHandler` (Adobe APP14 safety + CIE76 ΔE gate), `JpegCodec::encodeCmykAsRgb`, `--transcode-cmyk-to-rgb`, GUI checkbox.
- **P2-T6** — stream dedup activated via **reference-rewriting** (`ReferenceRewriter` repoints duplicate-stream referrers to the kept object; `replaceObject` can't alias streams), plus the `distinct_duplicate_streams.pdf` fixture.

**Remediation (done — code-review findings ESC-011…015):**
- **P2-T8** — verifier SSIM gate scoped to `pdfcompress` rows; benchmark loop restricted to the canonical 14 files.
- **P2-T9** — PNG codec output contract fixed: the FlateDecode-labelled lossless stream is now a valid zlib stream, never a PNG container.
- **P2-T10** — dedup signature extended with `/SMask`/`/Mask`/`/Type`/`/Intent` so masked byte-identical streams never merge.
- **P2-T7** — Phase-2 USAGE.md behavior matrix + full-suite gate: **62/62 green**; benchmark delta shows only the expected transparency dedup gain (11.63% → 17.46%).

**Closed:** **ESC-016** (stale PngCodec libpng labelling) and **ESC-017** (`CodecInterface.h` name() example) — both fixed; libpng retained as a listed (now-unused) stack dependency pending a future flagged cleanup.

**Status:** all 12 tasks done; all escalations ESC-007…017 addressed; `ctest` 62/62.

**New components/flags:** `CmykHandler`, `ReferenceRewriter`, `--transcode-cmyk-to-rgb`.

---

## ⬜ Phase 3 — Effective-DPI Downsampling (pending)

**Goal:** CTM-based effective DPI + per-profile target downsampling with a pixel floor; print size unchanged.

**Includes:**
- New `core/DPICalculator` — walk page content streams + Form XObjects, track the CTM (`cm`/`q`/`Q`) and `Do`, use the largest placement; honor crop/rotation; fallback + log.
- New `core/Downsampler` — high-quality resampler (Lanczos/bicubic), never upscale, vector-exempt, update `/Width`/`/Height` + CTM.
- `diagnose` effective-DPI column; `--dpi` / `--no-downsample` flags.
- USAGE.md targets/floor docs.

**Targets (locked):** 300 DPI Max Quality / 150 Balanced / 96 Max Compression; 32 px floor; resample only above target × 1.25.

**Acceptance:** `diagnose` 600-vs-150 within ±5%; ≥20% size reduction on oversampled rows; ±3% at-target rows; ±2% text rows.

**Depends on:** Phase 2 (downsampler feeds the corrected codec path).

---

## ⬜ Phase 4 — GUI Batch UX: Progress, Cancel, Off-Thread, Stats (pending)

**Goal:** Responsive, cancellable batch with per-file/total progress and a before/after summary; existing picker/slider/checkboxes untouched. **No target-size ("fit ≤ N MB") mode — locked out of scope.**

**Includes:**
- `QThreadPool` batch worker; progress signals; cancel with finish-or-rollback; summary view; per-file error rows.
- No compression-logic change; USAGE.md flow docs.

**Acceptance:** cancel ≤ 2 s with no half-written output; UI loop never blocked > 100 ms; per-file bytes/%/profile + totals + `errorMessage` rows (incl. encrypted-without-password `FAILED (encrypted — password required)`).

**Depends on:** Phases 1–3.

---

## ⬜ Phase 5 — JBIG2 Monochrome (pending)

**Goal:** True 1-bit images via lossless JBIG2 (jbig2enc, **optional dependency**); 8-bit grayscale keeps zlib. Lossless by default; lossy only via an explicit flag.

**Includes:**
- New `codecs/Jbig2Codec` (`ImageCodec` implementation).
- jbig2enc (Apache-2.0) via vcpkg/vendored, optional at CMake configure time with a warn-and-disable path.
- `--no-jbig2` / `--jbig2-lossy` flags; `/JBIG2Globals` handling.
- A 1-bit fixture staged in `$TMPDIR` (canonical 14 unchanged); USAGE.md licensing + policy docs.

**Acceptance:** 1-bit → JBIG2Decode ≤ zlib size at SSIM 1.0; 8-bit → Flate; lossy opt-in ≥ 0.99 SSIM; failure → zlib/original + log.

**Depends on:** Phases 2, 4.

---

## ⬜ Phase 6 — Font Dedup + Subsetting (pending)

**Goal:** Identical-font dedup + `hb-subset` subsetting (ON Balanced/Max Compression, OFF Max Quality) with selectable text always intact.

**Includes:**
- Font walker (AcroForm-reference detection for the subset exemption); hash-join dedup with indirect-handle guard; `hb-subset` wrapper (HarfBuzz via vcpkg).
- `--subset-fonts` / `--no-subset-fonts` flags (profile-matrix defaults).
- Text-extraction identity checks in tests; USAGE.md behavior matrix.

**Acceptance:** single embedded copy, text-identical, `qpdf --check` clean; Balanced smaller + `/ToUnicode` present; font-heavy rows shrink while image rows stay ±2%.

**Note:** this phase also carries the previously-deferred stream-dedup follow-up is NOT here (that moved to Phase 2, AC-C5). Design gap #15 flags that Phase 6's wording still references the old dedup guard — refresh when planning.

**Depends on:** Phases 1, 4.

---

## ⬜ Phase 7 — Encrypted PDFs: Password UX + Re-encryption (pending)

**Goal:** Per-file password prompt (GUI) + `--password` / `PDFOPTIMIZE_PASSWORD` (CLI); **unencrypted output by default**; opt-in re-encrypt.

**Includes:**
- GUI password dialog (per-file, cancellable, no caching); QPDF/PDFium password pass-through.
- `--password` / `--re-encrypt` + env fallback; wrong-password `errorMessage` paths; GUI checkbox copy.
- USAGE.md security notes (secret never logged; test `test123` vs production distinction).

**Acceptance:** correct password → unencrypted success; wrong/missing → `success=false` + `errorMessage`, no crash/output; `--re-encrypt` → still-encrypted with S2 intact.

**Depends on:** Phase 4.

---

## ⬜ Phase 8 — Performance, Streaming, Packaging & CI Hardening (pending)

**Goal:** Memory-safe large files, deterministic parallelism, signed macOS packaging, green CI. No compression-feature change (output bytes identical vs single-thread).

**Includes:**
- Streaming discipline audit + `$TMPDIR` spill for large files.
- `--jobs` scheduler (thread-local buffers, join-before-write).
- RSS sampling + benchmark TSV `PeakRSS` column.
- macOS DMG / signing / entitlements docs (secrets documented, never hardcoded); CI job (corpus + benchmark + S1–S5).

**Acceptance:** ≥ 200 MB staged file with peak RSS ≤ measured-then-locked cap; `--jobs 1` vs `4` pixel-identical (SSIM 1.0); clean-checkout CI green with bundled `libpdfium.dylib`; missing tools SKIPPED.

**Depends on:** all Phases 1–7.

---

## Standing acceptance gates (every phase)

- **S1** all existing + new tests pass (full `ctest` green).
- **S2** output size never larger than input unless reported.
- **S3** page renders match the original within the verification threshold (SSIM).
- **S4** text stays selectable (text is never rasterized).
- **S5** output opens in at least Preview and one other viewer.
- **S6–S9** per-phase benchmark before/after table; CLI flags for every option; USAGE.md updated.

## Process note — recurring task-split gap

Three times now (Phase-0 ESC-004/005, Phase-2 ESC-014) a phase-gate task carried a "full suite green" AC before every gate-relevant file was assigned an owner, so the gate could not converge. Future decompositions must enumerate the gate's input files and pre-assign each before writing the gate AC.
