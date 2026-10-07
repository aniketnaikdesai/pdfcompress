# Plan Review — PDF Compressor Phases 1–8 (Goals B–I)

## Review Status

- **Proposal reviewed:** `docs/architecture_proposal.md` + `docs/plan_proposal.md` marked `Status: READY_FOR_VALIDATION` (validation pass 1, retry 0 of 3)
- **Requirements baseline:** `docs/requirements/requirements.md` Part II (Phases 1–8, AC-B..AC-I, S1–S9, 8 locked answers in `open_questions.md`) `Status: READY_FOR_PLAN`; Phase-0 Part I complete and locked (commits 67a54b9/9441c8c)
- **Mode:** Existing-repo run — Validation Method step 7 applied
- **Graphify evidence:** `graphify-out/graph.json` exists (404 nodes) — proposal Codebase Summary deltas re-verified by Validator with targeted file reads (DecisionEngine.cpp:13,22-71; PDFOptimizer.cpp:236,269; main.cpp:63-66,75-78 + zero QThreadPool/Cancel hits; zero jbig2/HarfBuzz/new-flag hits in `src/`)
- **Code Reviewer escalations:** `docs/code_review/escalations.md` contains no `ESCALATED_TO_PLAN` entry (ESC-001..ESC-006 all `addressed`) — no revised-proposal re-validation required
- **Decision:** `Status: APPROVED` → `Status: READY_FOR_TASK_MANAGER` emitted to `docs/plan.md` LAST, after canonical artifacts materialized

## Validation Summary

The Phases 1–8 proposal is complete, internally consistent, and faithfully scoped to requirements Part II. It verifies-and-completes where code exists (Phase 1: no strip-semantics change) and builds-only where Appendix A proves a gap (slider scoping, dead PNG, CMYK skip, missing DPI resampling, JBIG2, fonts, passwords). All 8 locked open-question answers are honored (forProfile normative, transcode flag + SSIM gate, 300/150/96 + 32px floor, lossless-default jbig2enc, HarfBuzz matrix, unencrypted default, NO target-size, macOS-only). STOP boundaries are present per phase. No invented scope (target-size absent). Flagged Issues FI-1–FI-3 are testability notes already accommodated by the requirements text — no escalation. No `CHANGES_REQUIRED` or `BLOCKED` is warranted.

## Requirements Coverage

Standing gates S1–S9 apply to every phase (proposal states this once normatively and carries per-phase benchmark expectations — correct, not a gap).

| Requirement / Journey | Covered in Architecture | Covered in Plan | Verdict |
|---|---|---|---|
| **AC-B1** Profile defaults locked (MaxQuality-strips-nothing / Balanced-JS+metadata-only / MaxCompression-everything, `linearize=false`) | `forProfile` + strip paths [existing] normative, locked by tests | Phase 1 `TEST(ProfileDefaults,…)` ×3 + CLI round-trip | PASS |
| **AC-B2** Stripping end-to-end + non-JS `/OpenAction` GoTo survival | Strip/writer paths [existing], no semantic change | Phase 1 per-corpus × per-profile matrix | PASS |
| **AC-B3** Structural writer options (dedup/object-stream/Flate flags) | Writer paths [existing] | Phase 1 default vs `--no-dedup --no-flate-recompress` | PASS |
| **AC-B4** ±2% benchmark gate | Benchmark [new/changed] gate mechanism | Phase 1 STOP includes both TSVs archived | PASS |
| **J-B1/J-B2** Lock defaults; selective strip via GUI/CLI | forProfile [existing]; GUI checkboxes [existing] | Phase 1 scope + explicit-flag-wins edge case | PASS |
| **AC-C1** Slider scoped to lossy classes | DecisionEngine routing [new/changed] + pinned test | Phase 2 + STOP pins routing before Phase 3 | PASS |
| **AC-C2** PNG wired, dead code eliminated, SSIM 1.0 | PNG live-selected [existing→selected]; Jbig2Codec separate | Phase 2 alpha + MaxQuality-lossless scope | PASS |
| **AC-C3** CMYK preserved, never corrupted | CmykHandler preserve path [new] | Phase 2 ICC/APP14 + skip-with-reason | PASS |
| **AC-C3b** Opt-in `--transcode-cmyk-to-rgb`, SSIM≥0.98, `transcodedCmyk=1`, off by default | CmykHandler transcode path [new]; ΔE internal, SSIM normative | Phase 2 flag + GUI checkbox + USAGE.md spot-color warning | PASS |
| **AC-C4** Phase 2 benchmark gate | Benchmark gate mechanism | Phase 2 codec-change expectations | PASS |
| **J-C1/J-C2** Screenshot lossy/lossless; CMYK visual intact | Routing + CmykHandler | Phase 2 size-guard + Chrome+Preview check | PASS |
| **AC-D1** CTM effective DPI ±5%, `diagnose` column, `TEST(Inspector, EffectiveDpiFromCtm)` | DPICalculator [new]; diagnose column [new/changed] | Phase 3 fixtures incl. rotation | PASS |
| **AC-D2** 300/150/96 targets, 32px floor, never-upscale, `--dpi/--no-downsample` | Downsampler [new]; `targetDPI` consumed | Phase 3 override + floor checks | PASS |
| **AC-D3** ≥20% oversampled / ±3% at-target / ±2% text | Benchmark gate mechanism | Phase 3 STOP ≥40% J-D1 check at SSIM≥0.98 | PASS |
| **J-D1** Oversampled shrinks, print size unchanged | `/Width /Height`+CTM update | Phase 3 edge: icon below floor kept | PASS |
| **AC-E1** Responsive + cancellable (≤2s, no half-write, >100ms clause) | Batch worker [new] QThreadPool + rollback | Phase 4 manual cancel-timing check | PASS (FI-1 note: timing clause verified manual+inspection per requirements text) |
| **AC-E2** Summary stats + `errorMessage` rows | Batch worker + result fields | Phase 4 totals + per-file error rows | PASS |
| **J-E1** Drop batch, watch, cancel; encrypted row FAILED-continue | Batch worker; password flow (Ph.7) for the row | Phase 4 14-file batch incl. encrypted path | PASS |
| **AC-F1** JBIG2 true-1-bit only (8-bit stays Flate) | Jbig2Codec [new] + routing branch | Phase 5 `TEST(DecisionEngine, MonochromeBitDepthRoutesJbig2)` | PASS |
| **AC-F2** Lossless default SSIM 1.0; `--jbig2-lossy` ≥0.99; failure→zlib+log | Lossless default + fallback in Jbig2Codec | Phase 5 failure-injection test | PASS |
| **AC-F3** 1-bit ≥10% smaller, others ±2% | Benchmark gate mechanism | Phase 5 STOP SSIM-1.0 + license note | PASS |
| **J-F1** 1-bit uses JBIG2; `--no-jbig2` override; dep-absent fallback | Optional-dep wiring + warn path | Phase 5 `$TMPDIR` staged fixture, 14 unchanged | PASS |
| **AC-G1** Font dedup single copy, text-identical | FontDedup [new] hash-join + indirect-handle guard | Phase 6 shared-reference check | PASS |
| **AC-G2** Subset ON Balanced/MaxCompression, OFF MaxQuality, `/ToUnicode` | FontSubsetter hb-subset [new] + matrix | Phase 6 extraction-identity gate | PASS |
| **AC-G3** Font-heavy shrink, image rows ±2% | Benchmark gate mechanism | Phase 6 STOP tofu + text-identity diffs | PASS |
| **J-G1** Dedup+subset intact; form fonts dedup-only + logged | AcroForm exemption in FontSubsetter | Phase 6 field-editing manual check | PASS (FI-2 note: AcroForm-reference walk is plan's bounded choice within the specified exemption) |
| **AC-H1** Password flow (correct→unencrypted; wrong/missing→`errorMessage`, no crash/output) | Password flow [new] QPDF/PDFium pass-through | Phase 7 3-case matrix + secret-scan | PASS |
| **AC-H2** `--re-encrypt` opt-in still-encrypted, S2 holds | Re-encrypt write path | Phase 7 AES-128 params preserved | PASS |
| **J-H1** Dialog, 3 attempts, batch continues, never logged | GUI dialog + no-cache + never-log | Phase 7 mixed-batch manual test | PASS |
| **AC-I1** ≥200MB `$TMPDIR` file, RSS ≤ measured-then-locked cap, `PeakRSS` column | Scheduler + RSS accounting [new/changed] | Phase 8 measure-first-then-lock step | PASS (FI-3 note: cap-locking run is a plan step per requirements text) |
| **AC-I2** `--jobs 1` vs `4` pixel-identical SSIM 1.0 | Join-before-write determinism | Phase 8 1-vs-4 manual check | PASS |
| **AC-I3** Clean-checkout CI green, bundled dylib, DMG/sign docs, SKIPPED | macOS packaging [new/changed] | Phase 8 second-machine verify STOP | PASS |
| **J-I1** CI on large PDF; `$TMPDIR` spill | `$TMPDIR` spill path | Phase 8 never-commit staging | PASS |
| **Locked decisions (8 answers):** forProfile normative; transcode+SSIM; 300/150/96+32px; jbig2enc lossless-default; HarfBuzz matrix; unencrypted default; NO target-size; macOS-only | All present (§Tech Stack 22–27, NFR security, risks scope-creep row) | Phase 1 no-semantics-change; Ph.2 C3b; Ph.3 targets; Ph.5 policy; Ph.6 matrix; Ph.7 default; Ph.4 explicitly excludes target-size; Ph.8 macOS | PASS — all 8 honored, target-size absent everywhere |
| **Non-functional:** Offline, RAII, S2 size guard, S3/S4 gates, S7 CLI parity (flag per new option), S8 USAGE.md, S9 hygiene/no-overwrite/fallback | NFR table (offline/configure-time exception, determinism, security, hygiene) | Per-phase manual PDFs + S1–S9 invocation; CLI flags per phase; USAGE.md per change | PASS |

## Architecture Findings

- **Technology stack:** Complete. All Current Tech Stack items appear as [existing — hard constraint] first (decisions 18–21), then only two new third-party deps: jbig2enc OPTIONAL (22, Apache-2.0, configure-warn + zlib fallback) and HarfBuzz hb-subset (23, required from Ph.6). Resampler (24) is header-only/vendored with no runtime dep — not a stack replacement. No existing item replaced or deprecated. Rationale + alternatives documented for every decision (std::thread rejected for QThreadPool; GPL JBIG2 rejected; fontTools/QPDF-only/micro-subsetter rejected; OpenCV/PDFium-scale rejected; keychain rejected).
- **Major components / responsibilities / relationships:** Every Phases 1–8 component labeled; responsibilities and relations explicit (batch worker reused by Ph.5–8; decrypt-before-shared-path ordering for Ph.7; benchmark-isolating sequencing). New work is additive in `core/` + `codecs/` + `tools/` + GUI worker.
- **Data model:** `OptimizationOptions`/`OptimizationResult`/`ImageMetadata` deltas enumerated field-by-field with defaults (`transcodeCmykToRgb=false`, `useJbig2=true/lossy=false`, `reEncrypt=false`, `jobs=coreCount`, 300/150/96 + 32px); existing fields declared unchanged. `BenchmarkRow` gains only `PeakRSS`.
- **External integrations:** All labeled; jbig2enc [new] optional vs HarfBuzz [new] required distinction matches requirements (OQ F-1 vs G-4); macOS packaging [new/changed] matches OQ I-6.
- **Interfaces / boundaries:** `ImageCodec` interface respected (Jbig2Codec impl); CLI flag per new option (S7); `/ToUnicode` preservation; never-log password boundary; QPDF indirect-handle guard carried from ESC-001.
- **Security / performance / scalability / operational:** NFR table covers offline (configure-time exception class), one-image-at-a-time + `--jobs` + RSS measure-then-lock + `$TMPDIR` spill, determinism gate AC-I2, 3-attempt limit + never-log + AES-128 preservation, opt-in safe defaults for lossy paths, S3/S4 per-phase gates, form-font exemption, icon floor.
- **Architecture risks:** Table present with severity + mitigation for CTM/rotation, CJK/form subset, password leaks, jbig2enc availability, double-degrade, QPDF races, RSS cap, and scope creep (target-size/JXL/Win-Linux locked OUT). Rationale tied to requirements risks.
- **Completeness verdict:** No meaningful decision missing. Two implementation-bounded selections are explicitly deferred within gates (Lanczos-vs-bicubic; CMYK ΔE sampling metric with SSIM≥0.98 normative) — these are Build-level choices inside Validator-accepted bounds, not architectural decisions left to Task Manager. Phase 8 RSS cap measure-then-lock is a sequenced plan step per AC-I1, not a missing threshold.

## Plan Findings

- **Phase ordering:** 1 (lock defaults) → 2 (routing) → 3 (dims) → 4 (batch worker template) → 5 (1-bit) → 6 (fonts) → 7 (passwords) → 8 (harden). Dependencies stated per phase; codec-before-dims and worker-before-passwords orderings are justified (dims would confound codec gates; Ph.7 reuses Ph.4 dialog pattern). Correct.
- **Goals / coverage / dependencies:** Each phase has Goal, Requirements/Journeys covered, Dependencies, Scope, Per-phase gates, STOP boundary. All AC-B..AC-I and all journeys J-B1..J-I1 are covered exactly once. Locked values restated per phase (defaults, targets, matrix, unencrypted default).
- **Abstraction level:** Phases describe bodies of work ("Codec-Selection Correctness", "Effective-DPI Downsampling") at the correct coarse granularity. `TEST(Suite,Case)` names cited are normative AC pins from requirements, not task breakdowns. No per-task specs, no task dependency graphs. File-level scope notes (`core/DPICalculator`, `codecs/Jbig2Codec`) are component boundaries for Task Manager to decompose, not tasks.
- **Phase 1 purity:** Phase 1 scope is tests + `/AA` + non-JS `/OpenAction` completion + docs; explicitly no `DecisionEngine`/codec/DPI change and no strip-semantics change. PASS.
- **STOP boundaries:** Present in all 8 phases with concrete exit criteria (TSV deltas, visual checks, secret-scan, second-machine verify). PASS.
- **Scope integrity:** No added scope (target-size explicitly excluded in Phase 4; no JXL; no Windows/Linux; no OCR; no font conversion; no password cracking). No removed scope (every AC has an owner phase). No reinterpretation (FI items surfaced, not redefined; ΔE/SSIM split follows requirements' "Plan chooses the implementation-side metric" note). Staged `$TMPDIR` fixtures beyond the 14 need no requirements change per the ephemera constraint — correctly classified as assumption, not scope change.

## Existing Repo Compliance

*Applies because `docs/requirements/requirements.md` contains "This is an existing repository" with the tech stack as a hard constraint.*

| Check | Result | Evidence |
|---|---|---|
| **Codebase Summary is present and substantive** — inventories existing data models (entities + key fields), major components/modules + responsibilities, existing integrations | **PASS** | Phase-0 inventory carried forward (7 entities with locations/fields, 11 components with paths/LOC, 9 integrations) + Phases 1–8 delta table with `file:line` per row (DecisionEngine.cpp:13,22-71; PDFOptimizer.cpp:236,269; PDFInspector.cpp:164,226; main.cpp:63-66; run_optimize.cpp flag list). Validator re-verified each delta against the tree; `graphify-out/graph.json` (404 nodes) as discovery evidence |
| **The existing stack is respected** — every Current Tech Stack item appears as existing/constrained before new additions; nothing replaced unless flagged | **PASS** | C++20/Qt6/CMake+Ninja/vcpkg/PDFium/QPDF/libjpeg-turbo/OpenJPEG/libpng/zlib/GTest all [existing — hard constraint] (decisions 18–21) before [new] 22–24; resampler adds no runtime dep; Flagged Issues contain no stack replacement |
| **Labels are present** — System Components, Data Model, Integration Points each mark every item `[existing]` / `[new]` / `[new/changed]` | **PASS** | Components 13/13 labeled; Data Model 5/5 labeled; Integrations 7/7 labeled. No unlabeled item. |
| **Additive-only** — existing components not redesigned beyond what requirement calls for; new/changed work clearly separated | **PASS** | GUI picker/slider/checkboxes, forProfile strip semantics, existing codec paths unchanged (extended only with new fields/branches per AC); new files/components isolated (DPICalculator, Downsampler, Jbig2Codec, CmykHandler, FontDedup/Subsetter, batch worker, password flow, scheduler) |
| **Codebase Summary is consistent with the proposal** — components/entities treated as `[existing]` actually appear in the Codebase Summary | **PASS** | Every `[existing]` item (GUI, forProfile paths, PDFium/QPDF/codec libs, corpus 14, comparators) appears in the carried-forward inventory or delta table; no phantom existing component; `[new]` items (jbig2enc, HarfBuzz) correctly absent from the existing tree per verification greps |

**Existing Repo Compliance overall: PASS (5/5)**

## Required Changes

None — proposal is APPROVED. The following items were normalized in canonical artifacts without introducing new architectural decisions:

- Phase-0 architecture/plan sections retained verbatim as ground truth; Phases 1–8 appended under clearly-delimited extension headings (no duplication of overlapping stack entries — cross-referenced instead).
- Proposal Assumptions carried to `docs/plan.md` Assumptions as sequencing rationale (benchmark-isolating order, `$TMPDIR` staging, measure-then-lock, bounded resampler/ΔE selections, phase numbering) — none changes requirements meaning.
- Resampler and ΔE-metric deferrals recorded as implementation-bounded selections inside SSIM gates (per AC-C3b/AC-D3), not as open architectural decisions.

## Requirements Escalations

No `Status: ESCALATED_TO_REQUIREMENTS` emitted. Flagged Issues FI-1–FI-3 evaluated (all `Requirement/Journey` + `Why` + `Suggested Direction` present in proposal — correctly surfaced, not reinterpreted):

- **FI-1** (AC-E1 "event loop never blocked >100ms" not machine-testable): NOT a requirements defect — requirements text already specifies the verification method ("verified by manual test + code inspection that work runs on QThreadPool"). Phase 4 plan follows exactly that method (manual cancel-timing check). No escalation; Task Manager treats the timing clause as manual-gate.
- **FI-2** (Phase 6 form-font exemption lacks a specified detector): NOT a requirements defect blocking Task Manager — requirements mandate the exemption outcome ("form fonts are never subset"), and the AcroForm-reference walk is the plan's bounded implementation choice to achieve it. If stricter field-level provenance is later required, a follow-up requirements change can be filed. No escalation.
- **FI-3** (Phase 8 RSS cap pending measurement): NOT a requirements defect — AC-I1 explicitly defines the measure-then-lock sequence ("exact cap locked after first measurement"), and Phase 8 plans that run as a step with a ≤1GB starting hypothesis. No escalation.

`docs/plan_notes.md` needs no new entry: RG-001/RG-002 (Phase 0) remain the only requirements notes, both `Status: open` with locked directions and no escalation. FI-1–FI-3 require no new note beyond this review record.

## Task Manager Readiness

> Can Task Manager create a valid task graph from these planning artifacts without making a new architectural decision?

**YES.** All required technologies are specified (QThreadPool vehicle, jbig2enc optional wiring with fallback, HarfBuzz via vcpkg, header-only resampler class, QPDF/PDFium password pass-through, macOS DMG path); every component boundary is unambiguous (new files named: `core/DPICalculator`, `core/Downsampler`, `codecs/Jbig2Codec`, CmykHandler, FontDedup/Subsetter, batch worker, scheduler); every integration has a defined approach; the Data Model deltas are field-complete with defaults; phases provide coarse goals + coverage + dependencies + STOP gates at the right abstraction. The only deferred selections (resampler algorithm, ΔE sampling) are explicitly bounded by normative SSIM gates for Build to choose within. No architectural decision is left to Task Manager.

## Final Decision

**Status: APPROVED**

Canonical artifacts written:

- `docs/architecture.md` — complete (Phase-0 inventory + deltas retained; Summary; Tech Stack 1–27; System Components; Data Model; Integration Points; Non-Functional Approach; Architecture Risks)
- `docs/plan.md` — complete (Phases 0.1–0.4 retained; Phases 1–8 with goals/coverage/dependencies/scope/gates/STOP; Assumptions; Flagged Issues disposition) — ends with `Status: READY_FOR_TASK_MANAGER`
- `docs/plan_notes.md` — unchanged (RG-001/RG-002 stand; no new requirements notes needed, no escalation)
- `docs/plan_review.md` — this file, `Status: APPROVED`

Task Manager may proceed upon detecting `Status: READY_FOR_TASK_MANAGER` in `docs/plan.md`. No further Plan validation pass is required for Phases 1–8.

Status: APPROVED
