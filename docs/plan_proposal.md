# Plan Proposal — PDF Compressor Phases 1–8 (Goals B–I)

> Normative basis: `docs/requirements/requirements.md` Part II (Status: READY_FOR_PLAN) + standing
> gates S1–S9 (applied to every phase below and not repeated per phase except the phase-specific
> benchmark expectation). Phase numbering/names follow requirements.md exactly: Ph.1=B structure,
> Ph.2=C codecs, Ph.3=D DPI, Ph.4=E batch UX, Ph.5=F JBIG2, Ph.6=G fonts, Ph.7=H passwords,
> Ph.8=I perf/packaging. Target-size mode is NOT in scope in any phase (OQ E-2). macOS-only (OQ I-6).

## Phases

### Phase 1 (Goal B) — Structure & Profile-Defaults Verify-and-Complete

- **Goal:** Lock existing `forProfile` defaults and writer behavior with verifier tests; close doc
  gaps only. No strip-semantics change (locked: MaxQuality-strips-nothing /
  Balanced-strips-JS+metadata-only / MaxCompression-strips-everything, `linearize=false`).
- **Requirements covered:** AC-B1 (defaults tests), AC-B2 (per-corpus stripping incl. non-JS
  `/OpenAction` GoTo survival), AC-B3 (dedup/object-stream/Flate flags), AC-B4 (±2% benchmark gate);
  J-B1, J-B2.
- **Dependencies:** None (first; builds on locked Phase-0 baseline + 14-file corpus).
- **Scope:** New `tests/test_profile_defaults.cpp` (`TEST(ProfileDefaults,…)` ×3 + CLI round-trip
  for `--strip-*/--no-strip-*`, `--linearize/--no-flate-recompress/--no-dedup`); `/AA` +
  non-JS `/OpenAction` handling completion; `--linearize` USAGE.md gap; no `DecisionEngine`/codec/DPI change.
  The P1-T3 dedup test (`tests/test_structure_writer.cpp`, `streamsDeduplicated == 0`) is the correct
  Phase-1 pin (verify-only lock) and is rewritten to `>= 1` in Phase 2 (AC-C5) when the deferred fix lands.
- **Per-phase gates:** Benchmark before/after within ±2% every row (no compression change expected);
  manual PDFs: `with_form`, `with_bookmarks_and_links`, `with_javascript`, `with_metadata`,
  `transparency`, `text_only` (no-growth control); S1–S9.
- **STOP boundary:** STOP after verifier tests green + USAGE.md defaults table + both TSVs archived —
  no Phase 2 work until AC-B1/B2/B3 pass and any default dispute is filed as a requirements change.

### Phase 2 (Goal C) — Codec-Selection Correctness

- **Goal:** Scope slider to lossy classes, wire PNG for alpha/lossless, preserve CMYK correctly with
  opt-in transcode, and activate stream dedup via **reference-rewriting** (replace the inert ESC-001
  guard with a cycle-safe referrer walk that repoints references to the kept duplicate, then lets the
  duplicate drop — `replaceObject` cannot alias duplicate streams; see Scope).
- **Requirements covered:** AC-C1 (slider lossy-only + `TEST(DecisionEngine, SliderOnlyAffectsLossyClasses)`),
  AC-C2 (PNG for alpha/MaxQuality lossless, SSIM 1.0, `m_pngCodec` reachable), AC-C3 (CMYK preserved,
  never corrupted), AC-C3b (`--transcode-cmyk-to-rgb` + GUI checkbox, SSIM≥0.98, `transcodedCmyk=1`),
  AC-C4 benchmark gate, AC-C5 (stream dedup activated — deferred ESC-010 follow-up: `streamsDeduplicated >= 1`
  with defaults on a staged distinct-byte-identical fixture, `== 0` with `--no-dedup`, `qpdf --check` clean,
  S2–S5, benchmark before/after); J-C1, J-C2.
- **Dependencies:** Phase 1 (profile defaults locked first; routing builds on them; the Phase-1 P1-T3
  `streamsDeduplicated == 0` test is rewritten here).
- **Scope:** `DecisionEngine.cpp` branch rework (lossy-only slider; PNG branch; ScannedText profile-driven,
  Monochrome always lossless); new `CmykHandler` (ICC/DeviceCMYK preserve + Adobe APP14 safety +
  skip-with-reason logging; opt-in transcode with internal ΔE sampling); `run_optimize --transcode-cmyk-to-rgb`;
  GUI checkbox; USAGE.md slider/profile/CMYK docs. **Plus the AC-C5 structural stream-dedup activation:**
  - **Product fix (reference-rewriting):** in `src/core/PDFOptimizer.cpp` replace the inert ESC-001
    indirect-handle skip at line 373 (`if (it->second.isIndirect()) continue;`) and the `replaceObject`
    call at line 376 with a **reference-rewriting** path — a new `ReferenceRewriter` helper walks every
    object (`pdf.getAllObjects()` + trailer) and its direct nested dicts/arrays, finds every indirect
    reference to the duplicate's `objGen`, and repoints it to the kept object via `replaceKey` /
    `setArrayItem`; the now-unreferenced duplicate then drops under the already-set
    `setPreserveUnreferencedObjects(false)` (or an explicit `removeObject`). Keep the byte-identical
    signature detection (FNV hash + the five dict keys), the `obj.getObjGen() != it->second.getObjGen()`
    guard (line 366), and the per-pair try/catch (lines 372-382) so one bad pair never fails the whole
    file. **Why not `replaceObject`:** QPDF 12.3.2 `QPDF::replaceObject` rejects any indirect handle except
    a self-stream (`libqpdf/QPDF_objects.cc:1968`), and streams are always indirect (`QPDF::newStream()` →
    `makeIndirectObject`), so a "direct copy" replacement cannot be constructed — verified infeasible at
    build time. `replaceKey`/array-item replacement *do* accept indirect references, which is why
    reference-rewriting works. The walk must be cycle-safe (visit each `getAllObjects()` entry once; never
    follow indirect references during traversal — recurse only into direct containers) and must not repoint
    a reference that would change semantics (require `/Filter` + `/DecodeParms` to match alongside the
    signature).
  - **New staged fixture:** `distinct_duplicate_streams.pdf` in `$TMPDIR` — two **distinct** indirect stream
    objects with byte-identical raw data and matching signatures (same `/Subtype /Width /Height /ColorSpace
    /BitsPerComponent`, `/Filter`, `/DecodeParms`). **Both streams must actually be referenced** (e.g.
    `/Resources /XObject << /Im1 A /Im2 B >>` with both drawn on the page) — a duplicate with no referrer
    cannot be deduped, so an unreferenced second stream would leave `streamsDeduplicated == 0` and is not a
    valid fixture. This is NOT the reference-shared `transparency.pdf` / `generateSharedXObject()` fixture
    and is NOT added to the 14 canonical corpus files.
  - **Test rewrite:** the Phase-1 P1-T3 test (`tests/test_structure_writer.cpp`, currently asserting
    `streamsDeduplicated == 0`) is rewritten to assert `>= 1` against the new staged fixture, retaining a
    `== 0` assertion for the `--no-dedup` run (P1-T3-T01/T03 continue to cover the shared-XObject PDF).
- **Per-phase gates:** Benchmark: screenshot/line-art MaxQuality rows change codec at SSIM 1.0, photos ±5%,
  CMYK row reason-logged, all rows S2–S5; manual PDFs: `screenshot_flat`, `line_art`, `photo_jpeg`,
  `transparency` (alpha), `cmyk_image`, `monochrome_bw` + `grayscale_scan` controls, plus the staged
  `distinct_duplicate_streams.pdf` for AC-C5. **AC-C5 gates:** with defaults `result.streamsDeduplicated >= 1`
  on the staged fixture and the output shares one stream object for that pair (`qpdf --check` clean;
  `qpdf --json` shows a single stream); with `--no-dedup` `streamsDeduplicated == 0` and both stream objects
  remain; S2–S5 hold; benchmark before/after on the same corpus with dedup-bearing rows ≤ baseline (additive,
  no regression on other rows).
- **STOP boundary:** STOP after AC-C1/C2/C3/C3b/C5 tests + CMYK Chrome+Preview visual check + TSV delta —
  no downsampling work until codec routing and stream dedup are pinned (Phase 3 consumes its output dims).

### Phase 3 (Goal D) — Effective-DPI Downsampling

- **Goal:** CTM-based effective DPI + profile-target downsampling with floor, print size unchanged.
- **Requirements covered:** AC-D1 (`diagnose` effective-DPI column, 600-vs-150 ±5%,
  `TEST(Inspector, EffectiveDpiFromCtm)`), AC-D2 (300/150/96 targets, 32px floor, never-upscale,
  `--dpi/--no-downsample`), AC-D3 (≥20% on oversampled rows, ±3% at-target, ±2% text rows); J-D1.
- **Dependencies:** Phase 2 (downsampler feeds the corrected codec path; dims change would confound
  Phase-2 codec gates if reordered).
- **Scope:** New `core/DPICalculator` (CTM extraction honoring crop/rotation; fallback + log) and
  `core/Downsampler` (Lanczos/bicubic, floor, never-upscale/vector-exempt, `/Width /Height`+CTM update);
  `diagnose` DPI column; CLI flags; USAGE.md targets/floor docs.
- **Per-phase gates:** Benchmark gate AC-D3; manual PDFs: staged oversampled 600-DPI scan (in `$TMPDIR`,
  not committed), `photo_heavy`, `grayscale_scan`, `line_art` (edge check), tiny-icon PDF (floor check).
- **STOP boundary:** STOP after AC-D1/D2 fixtures (incl. rotation case) + oversampled ≥40% J-D1 check at
  SSIM≥0.98 + TSV delta — later phases assume dims are final.

### Phase 4 (Goal E) — GUI Batch UX: Progress, Cancel, Off-Thread, Stats

- **Goal:** Responsive cancellable batch with per-file/total progress and before/after summary; existing
  picker/slider/checkboxes untouched.
- **Requirements covered:** AC-E1 (cancel ≤2s, no half-written output, loop never blocked >100ms),
  AC-E2 (per-file bytes/%/profile + totals + `errorMessage` rows); J-E1 (incl. encrypted-without-password
  row showing `FAILED (encrypted — password required)`).
- **Dependencies:** Phases 1–3 (batch displays their result fields; stable single-file behavior first).
- **Scope:** GUI-side only + thin worker: QThreadPool batch worker, progress signals, cancel with
  finish-or-rollback, summary view, per-file error rows; no compression-logic change; USAGE.md flow docs.
- **Per-phase gates:** Benchmark: no output-bytes change vs pre-Phase-4 (UI-only; rows within ±1%);
  manual: 14-file batch incl. `encrypted.pdf` error path, mid-batch cancel, Preview+Chrome check.
- **STOP boundary:** STOP after manual cancel-timing check + summary screenshot/TSV + S1–S9 — the worker/
  cancel pattern is the template Phases 5–8 reuse, so it must be reviewed before codec/font/password
  work lands on top.

### Phase 5 (Goal F) — JBIG2 Monochrome

- **Goal:** True 1-bit images via lossless JBIG2 (jbig2enc, optional dep); 8-bit grayscale keeps zlib.
- **Requirements covered:** AC-F1 (1-bit→JBIG2Decode ≤ zlib size; 8-bit→Flate; routing test),
  AC-F2 (lossless default SSIM 1.0; `--jbig2-lossy` ≥0.99; failure→zlib/original + log), AC-F3 (1-bit row
  ≥10% smaller at SSIM 1.0, others ±2%); J-F1 (incl. `--no-jbig2` override).
- **Dependencies:** Phases 2 (routing point), 4 (batch shows `jbig2Images`/fallback reasons).
- **Scope:** New `codecs/Jbig2Codec` (`ImageCodec` impl); vcpkg/vendored jbig2enc wiring with
  configure-warn + disable path; `--no-jbig2/--jbig2-lossy` flags; `1-bit` fixture staged in `$TMPDIR`
  (canonical 14 unchanged); USAGE.md licensing + policy docs.
- **Per-phase gates:** Benchmark gate AC-F3; manual PDFs: staged true-1-bit scan (`/BitsPerComponent 1`),
  `monochrome_bw` (8-bit control), `grayscale_scan` (JP2 control).
- **STOP boundary:** STOP after 1-bit SSIM-1.0 + fallback-injection test + license note in USAGE.md —
  lossy path must never be default; any license failure keeps zlib and is documented, not silent.

### Phase 6 (Goal G) — Font Dedup + Subsetting

- **Goal:** Identical-font dedup + hb-subset subsetting (ON Balanced/MaxCompression, OFF MaxQuality)
  with selectable text always intact.
- **Requirements covered:** AC-G1 (single embedded copy, text-identical, `qpdf --check` clean),
  AC-G2 (Balanced smaller + text-identical + `/ToUnicode` present; MaxQuality full program), AC-G3
  (font-heavy rows shrink, image rows ±2%); J-G1 (form fonts exempt: dedup only + logged).
- **Dependencies:** Phases 1 (structure baseline), 4 (batch reporting); independent of Phases 2–3–5
  codec paths but sequenced after them to isolate benchmark deltas.
- **Scope:** Font walker (AcroForm-reference detection for the subset exemption), hash-join dedup with
  indirect-handle guard, `hb-subset` wrapper, `--subset-fonts/--no-subset-fonts` (profile-matrix defaults),
  text-extraction identity checks in tests; USAGE.md behavior matrix.
- **Per-phase gates:** Benchmark gate AC-G3; manual PDFs: merged-duplicate-fonts PDF, `text_only`,
  `with_form` (field editing still works), large mixed-text + CJK fixture.
- **STOP boundary:** STOP after text-identity diffs (before/after extraction) + Preview/Chrome tofu check +
  TSV delta — any glyph loss or form breakage blocks Phases 7–8.

### Phase 7 (Goal H) — Encrypted PDFs: Password UX + Re-encryption

- **Goal:** Per-file password prompt (GUI) + `--password`/`PDFOPTIMIZE_PASSWORD` (CLI); unencrypted output
  by default; opt-in re-encrypt.
- **Requirements covered:** AC-H1 (correct→unencrypted success; wrong/missing→`success=false` +
  `errorMessage`, no crash/output), AC-H2 (`--re-encrypt` → still-encrypted, S2 holds); J-H1
  (3-attempt inline error, batch continues, secret never logged).
- **Dependencies:** Phase 4 (per-file dialog + batch-continue pattern); orthogonal to Phases 2–3–5–6
  (decrypt happens before the shared optimize path).
- **Scope:** GUI password dialog (per-file, cancellable, no caching), QPDF/PDFium password pass-through,
  `--password/--re-encrypt` + env fallback, wrong-password `errorMessage` paths, GUI checkbox copy;
  USAGE.md security notes (never-logged, test-`test123`-vs-production distinction).
- **Per-phase gates:** Benchmark: corpus rows unchanged except `encrypted.pdf` (now optimizable with password);
  manual PDFs: `encrypted.pdf` correct/wrong/missing + mixed batch; Preview open-with/without-password checks.
- **STOP boundary:** STOP after wrong-password + missing-password + re-encrypt matrix + log/TSV secret-scan
  (no secret present) — password handling must be audited before packaging/CI work.

### Phase 8 (Goal I) — Performance, Streaming, Packaging & CI Hardening

- **Goal:** Memory-safe large files, deterministic parallelism, signed macOS packaging, green CI matrix.
- **Requirements covered:** AC-I1 (≥200MB staged `$TMPDIR` file, peak RSS ≤ measured-then-locked cap,
  TSV `PeakRSS` column), AC-I2 (`--jobs 1` vs `4` pixel-identical, SSIM 1.0), AC-I3 (clean-checkout CI
  green, bundled `libpdfium.dylib`, DMG/signing documented, missing tools SKIPPED); J-I1 (`$TMPDIR` spill).
- **Dependencies:** All Phases 1–7 (final hardening over the complete pipeline; RSS/parallelism measured
  on final behavior).
- **Scope:** Streaming discipline audit + `$TMPDIR` spill, `--jobs` scheduler (thread-local buffers,
  join-before-write), RSS sampling + TSV column, macOS DMG/sign/entitlements docs, CI job
  (corpus + benchmark + S1–S5); no compression-feature change (output bytes identical vs single-thread).
- **Per-phase gates:** Full-corpus S1–S9 + determinism gate + RSS-cap gate; manual: ≥200MB mixed PDF
  (staged, never committed), `photo_heavy` 1-vs-4, full 14-file CI matrix.
- **STOP boundary:** STOP (program checkpoint) after CI green on clean checkout + RSS cap locked from
  measurement + packaging verified on a second machine — release readiness, not just code-complete.

## Assumptions

- Phase ordering is benchmark-isolating: codec routing (2) → dims (3) → UI (4) → 1-bit (5) → fonts (6)
  → passwords (7) → perf/packaging (8), so each TSV delta is attributable to one cause. (Not in
  requirements.md; sequencing rationale.)
- Staged fixtures beyond the canonical 14 (true-1-bit scan, oversampled 600-DPI, tiny-icon, ≥200MB mixed,
  CJK, and the Phase-2 `distinct_duplicate_streams.pdf`) live in `$TMPDIR`, are never committed, and need no
  requirements change (consistent with the `$TMPDIR`-ephemera constraint; design gap #10's locked direction;
  the canonical 14 stay exactly 14 per the safety invariant).
- Phase 8 RSS cap is measure-first-then-lock (≤1GB starting hypothesis per AC-I1); the measurement run
  itself is a plan step, not a pre-declared pass/fail number.
- Resampler choice (Lanczos vs bicubic) and CMYK ΔE sampling metric are implementation-side selections
  bounded by SSIM gates (AC-C3b/AC-D3); Task Manager/Build choose within those bounds.
- Phase numbering follows requirements.md Part II exactly (Ph.4 = Goal E batch UX). The task brief's
  shorthand ("lossless-JPEG in Phase 4") is interpreted as Phase 2/4 existing AC (JPEG quality guard +
  size guard), not a separate scope — no new phase is introduced.
- The Phase-2 AC-C5 mechanism is **reference-rewriting**, not the requirements note's direct-copy
  `replaceObject` (infeasible against QPDF 12.3.2 — verified at build time). The user approved this
  Plan-side redesign with AC-C5's *outcome* unchanged; FI-5 surfaces the requirements-text refresh. The
  `distinct_duplicate_streams.pdf` fixture's two distinct byte-identical streams are both referenced, so
  the referrer walk has a referrer to repoint.

## Flagged Issues

- **FI-1 — AC-E1 "event loop never blocked >100ms" is not machine-testable as written.**
  Requirement/journey: Phase 4 AC-E1. Why unsatisfiable as written: no harness measures main-thread
  block time; "verified by manual test + code inspection" cannot gate CI. Suggested direction: Validator
  records an automated QTimer-watchdog self-test (or documents AC-E1's timing clause as manual-only with
  code-inspection evidence) — surfaced here, not reinterpreted.
- **FI-2 — Phase 6 form-font exemption lacks a specified detector.**
  Requirement/journey: AC-G1/J-G1 ("form fonts never subset"). Why: requirements mandate the exemption
  but no detection mechanism (AcroForm font-reference walk is the plan's chosen approach, not a specified
  one). Suggested direction: accept AcroForm-reference walk + logged exemption as satisfying; if stricter
  field-level provenance is required, file a requirements clarification.
- **FI-3 — Phase 8 RSS cap is a target pending measurement, not a committable threshold.**
  Requirement/journey: AC-I1. Why: "≤1GB default pending measurement" cannot be asserted until the first
  large-file run. Suggested direction: keep AC-I1's measure-then-lock sequence as the normative path
  (plan Phase 8 does this); Validator should treat a measured-and-documented cap as satisfying.
- **FI-4 — Phase 6 (Goal G) font-dedup wording still references the ESC-001 indirect-handle guard that
  Phase 2 removes.** Requirement/journey: Phase 6 Goals ("same hash-join pattern as stream dedup, with the
  ESC-001 indirect-handle guard") and `design_gaps.md` #15. Why it can't be satisfied as written: Phase 2's
  AC-C5 replaces the inert ESC-001 `isIndirect()` guard at `PDFOptimizer.cpp:373` with a reference-rewriting
  path (`ReferenceRewriter`), so after Phase 2 the ESC-001 guard no longer exists and Phase 6 would cite a
  removed mechanism. This is a genuine requirements-staleness gap surfaced while folding in the Phase-2
  addition, not a Phase-2 defect (Phase 2 stays requirements-bound; no font work is pulled in). Suggested
  direction: Requirements Analyst refreshes the Phase 6 Goal wording (when Phase 6 is planned) to cite the
  Phase-2 reference-rewriting dedup path as the pattern to reuse; until then the architecture proposal
  labels Phase 6's dedup as reusing the Phase-2 corrected pattern. No reinterpretation applied here —
  flagged, not silently rewritten.
- **FI-5 — `requirements.md` AC-C5 implementation note and `design_gaps.md` #14 still name the infeasible
  direct-copy `replaceObject` mechanism.** Requirement/journey: Phase 2 AC-C5 "Implementation note"
  (the paragraph after AC-C5) and `design_gaps.md` #14. Why it can't be satisfied as written: the note
  prescribes passing "a direct copy of the replacement stream to `QPDF::replaceObject(...)`", but QPDF
  12.3.2 `replaceObject` rejects indirect handles and no direct stream can exist (verified at build time;
  see Codebase Summary), so that mechanism is infeasible. AC-C5's *outcome* is unchanged and is satisfied
  by the Plan's user-approved reference-rewriting redesign; only the mechanism wording is stale. Suggested
  direction: Requirements Analyst refreshes the AC-C5 implementation note and `design_gaps.md` #14 to name
  reference-rewriting (referrer walk + `replaceKey`/`setArrayItem` repoint + drop-unreferenced) so the
  normative text matches the approved architecture. No outcome or scope change — surfaced here, not
  silently rewritten.

Status: READY_FOR_VALIDATION
