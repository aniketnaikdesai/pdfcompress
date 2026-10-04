# Plan Review — PDF Compressor Phase 0

## Review Status

- **Proposal reviewed:** `docs/architecture_proposal.md` + `docs/plan_proposal.md` marked `Status: READY_FOR_VALIDATION` (validation pass 1, retry 0 of 3)
- **Requirements baseline:** `docs/requirements/requirements.md` `Status: READY_FOR_PLAN` (Phase 0 only)
- **Mode:** Existing-repo run — Validation Method step 7 applied
- **Graphify evidence:** `graphify-out/graph.json` exists (350 nodes / 549 edges, 15 communities) — Codebase Summary cross-checked against `GRAPH_REPORT.md` and manual file survey
- **Decision:** `Status: APPROVED` → `Status: READY_FOR_TASK_MANAGER` emitted to `docs/plan.md` LAST, after canonical artifacts materialized

## Validation Summary

Phase 0 proposal correctly hardens only the verification baseline (corpus/benchmark/render-diff/tests) with no compression-pipeline behavior change, respects the locked tech stack and file layout, and faithfully carries the Appendix A verification report (claims 4,5,6,7,10 already FIXED) as ground truth. The four phases 0.1–0.4 are ordered, coarse, testable, and Task-Manager-ready; minor location ambiguity (`tools/render_diff.cpp` vs `benchmark/render_diff.cpp`) is resolved canonically to `tools/render_diff.cpp`. One requirements wording issue (AC3 14 vs 15 count) is locked to exactly 14 files (RG-001) without requiring a requirements escalation. No `CHANGES_REQUIRED` or `BLOCKED` is warranted.

## Requirements Coverage

| Requirement / Journey | Covered in Architecture | Covered in Plan | Verdict |
|---|---|---|---|
| **AC1** Verification report completeness (11 claims RIGHT/WRONG/PARTIAL + file:line) | Existing Verification Baseline section carries claims 4,5,6,7,10 as WRONG/outdated (already fixed); full 12-row table retained in requirements Appendix A as ground truth | Phase 0.1 reads Appendix A as ground truth, no re-verification proposed | PASS — not re-proposed, correctly treated as fact |
| **AC2** Test framework GTest + CTest `BUILD_TESTING=ON` `gtest_discover_tests` 12+ tests, one-line `CMakeLists.txt` add | Tech Stack #7 (GTest locked, Catch2 rejected), System Components Test Harness [existing] | Phase 0.4 Test Wiring & Acceptance Gating — `enable_testing()` + `gtest_discover_tests` + `TEST(Suite,Case)` naming + `BUILD_TESTING=ON` | PASS |
| **AC3** Corpus 14 deterministic types QPDF-only `$TMPDIR/pdfcompress_test_corpus` encrypted `test123` 128-bit AES | Tech Stack #8 QPDF-only, #9 TMPDIR, #10 encrypted R6/R3 `test123`; Data Model `CorpusFile` 1–14 authoritative enumeration with status labels | Phase 0.1 Corpus Hardening — replaces 6 stubs + 3 missing types, `setDeterministicID(true)`, exactly 14 PDFs, verification gate `wc -l ==14` + per-file `hasKey` | PASS |
| **AC4** Benchmark 5-tool matrix `qpdf --recompress` valid, `gs /ebook /screen`, `ocrmypdf`, pure-C++ PDFium render-diff page1 150 DPI SSIM/PSNR N/A fallback, ms timing, TSV header, idempotency | Tech Stack #11–#16 (valid QPDF, GS, ocrmypdf, pure-C++ render helper, ms timing, TSV+idempotency), Integration Points for gs/ocrmypdf/qpdf, Data Model `BenchmarkRow` TSV | Phase 0.2 Benchmark Hardening (5-tool matrix, `command -v` guards, SKIPPED, ms, idempotency) + Phase 0.3 Render-Diff (pure-C++ helper `tools/render_diff.cpp`, SSIM/PSNR, N/A fallback) | PASS |
| **AC5** No behavior change — diff only `tests/`, `benchmark/`, `CMakeLists.txt`, `docs/`; ±1% pdfcompress rows | Non-Functional Correctness (size guard unchanged), Architecture Risks over-scope creep mitigation + docs diff constraint | Phase 0.1–0.4 each declare `git diff --stat` constraint; Phase 0.1 uses ≥50KB streams to make ±1% meaningful | PASS (with RG-002 note exempting <2KB overhead) |
| **AC6** CLI tools `run_optimize` flags (`--profile/--quality/--strip-*/--linearize/--no-flate-recompress/--no-dedup`) + `diagnose` | System Components CLI tools [existing] verified, no extension | Phase 0.4 manual gate `run_optimize --help` + `diagnose` font preservation | PASS |
| **AC7** Quality gates: valid PDF, size guard, text selectable, SSIM ≥0.98/0.95, encrypted `test123` graceful reject | Non-Functional Security/Encryption + Correctness, Tech Stack #10, Data Model `PDFDocumentInfo` isEncrypted | Phase 0.1 verification gate (QPDF processFile, isEncrypted==true, `imagesSkipped==1` for CMYK), Phase 0.3 SSIM thresholds, Phase 0.4 acceptance gating | PASS |
| **J1** Developer verifies baseline (read report, ctest, generate_corpus, benchmark, Preview check) | Non-Functional Performance/Determinism/Observability | Phase 0.1 (corpus), 0.2 (benchmark), 0.3 (render-diff), 0.4 (ctest single signal) | PASS |
| **J2** Headless CI `ctest --test-dir build -R PDFOptimizerTest` | Tech Stack #7, System Components Test Harness | Phase 0.4 | PASS |
| **J3** Benchmark against external tools (corpus, Table review, skip `*_optimized*`) | Integration Points optional tools, Architecture Risks idempotency | Phase 0.2 | PASS |
| **Non-functional: Offline, RAII, no silent growth, C++ hygiene, CLI parity, file layout** | Non-Functional Approach (offline, determinism, hygiene, observability) + System Components layout | Phases 0.1–0.4 scope constraints (only `tests/benchmark/tools` + focused new file) | PASS |
| **Verification report consistency** — claims 4,5,6,7,10 WRONG already fixed must not be re-proposed | Existing Verification Baseline section explicitly lists claims 4,5,6,7,10 as WRONG/outdated already FIXED; System Components GUI/Optimizer/Codec notes say "No change in Phase 0" | Phases 0.1–0.4 Out-of-scope explicitly blocks re-implementing per-item stripping, metadata stripping, linearization, profile picker, tests/benchmark — only hardening | PASS |

## Architecture Findings

- **Technology stack:** Complete and respects hard constraint. All Current Tech Stack items (C++20/Qt6/CMake/Ninja/vcpkg/PDFium/QPDF/libjpeg-turbo/OpenJPEG/libpng/zlib/GTest) appear as [existing] before any [new] additions (Tech Stack #1–#9). No replacement. Alternatives for rejected choices (Catch2, Python scikit-image, ImageMagick, `tests/corpus/` fixtures) are documented. Rationale present for every decision.
- **Major components / responsibilities / relationships:** Inventory covers GUI, Drag-Drop, PDFInspector, ImageAnalyzer, DecisionEngine, PDFOptimizer, Codec layer, CLI tools, Tests, Benchmark & corpus, Build — with paths, LOC, and relations (Inspect→Analyze→Decide→Encode→Write). New/changed components isolated to `TestCorpusGenerator` [new/changed], `run_benchmark.sh` [new/changed], `render_diff` [new].
- **Data model:** Entities + key fields + status labels present (OptimizationOptions/Result, ImageMetadata, AnalysisResult, PDFDocumentInfo, logical CorpusFile 14 and BenchmarkRow TSV). Relationships implicit via pipeline wiring.
- **External integrations:** Labeled [existing]/[new]/[new/changed]; purposes defined; optional tools correctly `command -v` guarded.
- **Interfaces / boundaries:** QPDF stream dict keys, OptimizationOptions strip booleans driving optimizer branches, ImageCodec interface, CLI flags — all specified.
- **Security / performance / scalability / operational:** Non-Functional Approach covers offline, determinism, ms timing, C++ hygiene, encryption `test123` fixture vs production prompt, writer overhead handling. Scalability out of scope for Phase 0 synthetic corpus.
- **Architecture risks:** Table present with severity + mitigation for vacuously-passing stubs, QPDF encrypt API mismatch, PDFium unavailable, `bc`/BSD date portability, tiny-PDF overhead, TMPDIR race, scope creep into Phase C. Rationale tied to requirements risks.
- **Completeness verdict:** No meaningful decision missing. Tech Stack #17 explicitly scopes DecisionEngine slider fix to Phase C and forbids PNG wiring in Phase 0 — correctly prevents over-scope.

## Plan Findings

- **Phase ordering:** 0.1 Corpus → 0.2 Benchmark matrix → 0.3 Render-diff (fills SSIM/PSNR) → 0.4 Test wiring & acceptance gating. Each phase depends only on earlier phases that must logically precede it. Correct.
- **Goals / coverage / dependencies:** Each phase has explicit Goal, Covers (AC/J/risk/design_gap), Dependencies, Scope (file-level, reviewable), Out-of-scope, and Verification gate. All AC/J are covered by exactly one phase without duplication.
- **Abstraction level:** Phases describe bodies of work ("Corpus Hardening", "Benchmark Hardening", "Render-Diff Hardening", "Test Wiring") at the correct coarse granularity for Task Manager to decompose. No per-task breakdown, no task-level dependency graph, no per-task test specifications. One allowed exception: verification gates list `TEST(Suite,Case)` naming convention and `ls *.pdf | wc -l ==14` checks — these correctly describe acceptance gates, not task graphs.
- **Scope integrity:** Plan does not add product scope (PNG/JBIG2/DPI downsampling/font dedup/password GUI all explicitly deferred to Phases C/D/F/G/H per verification report). Does not remove approved scope (all 14 corpus types accounted for). Does not reinterpret requirements (AC3 14-file interpretation locked in RG-001 with justification, not silent). Does not convert unresolved requirement into assumption. `git diff --stat` constraint correctly enforced per AC5.
- **File layout / hygiene / offline:** All new logic in focused files (`tests/test_corpus_generator.*`, `benchmark/run_benchmark.sh`, `tools/render_diff.cpp`, `CMakeLists.txt` wiring) respecting `core/`/`codecs/`/`tools/` layout; RAII, `-Wall -Wextra -Wpedantic`, offline, no Python — all stated.

## Existing Repo Compliance

*Applies because `docs/requirements/requirements.md` contains "This is an existing repository" and "Treat the existing tech stack as a hard constraint".*

| Check | Result | Evidence |
|---|---|---|
| **Codebase Summary is present and substantive** — inventories existing data models (entities + key fields), major components/modules + responsibilities, existing integrations | **PASS** | `architecture_proposal.md` § Codebase Summary has 3 tables: 7 entities with locations/fields, 11 components with paths/LOC/responsibilities, 9 integrations with usage; verified against `graphify-out/graph.json` (350 nodes / 549 edges) and `GRAPH_REPORT.md` (TestCorpusGenerator, OptimizationResult, PDFInspector communities) + manual survey of `src/` `tests/` `benchmark/` `tools/` `CMakeLists.txt` `vcpkg.json` `cmake/FetchPDFium.cmake` |
| **The existing stack is respected** — every Current Tech Stack item appears as existing/constrained before new additions; nothing replaced unless flagged | **PASS** | Requirements Current Tech Stack: C++20, Qt6, CMake+Ninja, vcpkg, PDFium (FetchPDFium chromium/7920 libpdfium.dylib), QPDF, libjpeg-turbo, OpenJPEG, libpng, zlib/libdeflate, GTest — all appear in Tech Stack #1–#9 as [existing — hard constraint] before #10–#17 [new]; no existing item replaced or deprecated; Flagged Issues F1–F5 contain no stack replacement |
| **Labels are present** — System Components, Data Model, Integration Points each mark every item `[existing]` / `[new]` / `[new/changed]` | **PASS** | System Components: 7× [existing], 1× [new/changed], 1× [existing], 1× [new/changed], 1× [new] — all labeled. Data Model: 4× [existing], 2× [new]/[new/changed] — all labeled. Integration Points: 3× [existing], 3× [new]/[new/changed] — all labeled. Existing Verification Baseline also labeled. |
| **Additive-only** — existing components not redesigned beyond what requirement calls for; new/changed work clearly separated | **PASS** | GUI, PDFInspector, ImageAnalyzer, DecisionEngine, PDFOptimizer (core logic), Codec layer, CLI tools unchanged in Phase 0 (each marked [existing] with "No change" note). Only `TestCorpusGenerator` [new/changed], `run_benchmark.sh` [new/changed], `render_diff` [new] are modified/added — all in `tests/`/`benchmark/`/`tools/` per AC5 diff constraint |
| **Codebase Summary is consistent with the proposal** — components/entities treated as `[existing]` actually appear in Codebase Summary | **PASS** | Every `[existing]` System Component (GUI/MainWindow, PDFInspector, ImageAnalyzer, DecisionEngine, PDFOptimizer, Codec layer, run_optimize/diagnose, Tests, Build) and every `[existing]` Data Model entity (ImageMetadata, AnalysisResult, OptimizationOptions, etc.) appears in Codebase Summary tables with matching paths/keys. No phantom existing component. |

**Existing Repo Compliance overall: PASS (5/5)**

## Required Changes

None — proposal is APPROVED. The following items were normalized in canonical artifacts without introducing new architectural decisions:

- Resolved `tools/render_diff.cpp` vs `benchmark/render_diff.cpp` alternative to canonical `tools/render_diff.cpp` (preferred in proposal; same constraints apply).
- Locked AC3 14-file interpretation to exactly 14 files with `transparency.pdf` embedding both SMask and shared XObject (RG-001) — proposal already recommended this; Validator made it the validated baseline.
- Carried five requirements assumptions (QPDF R6/R3 fallback, BSD `date`/`gdate`, `bc`→`awk`, licensed-free synthetic content, dylib `N/A` fallback) to `docs/plan.md` Assumptions — these were discovery notes, not new decisions.

## Requirements Escalations

No `Status: ESCALATED_TO_REQUIREMENTS` emitted. Flagged Issues F1–F5 were evaluated:

- **F1** (AC3 14 vs 15 count): Requirements wording contradiction — resolved by Validator locking exactly 14 files per AC3 normative note ("if strict 14-file limit, `transparency.pdf` must contain both") and recording as `docs/plan_notes.md` RG-001 `Status: open` with locked direction. Does not block Task Manager; if product owner later wants 15 files, a follow-up requirement change can be filed. No escalation needed.
- **F2** (monochrome 1-bit concept): Correctly flagged as implementation note — 8-bit ≤4-color generation is an acceptable trigger for Monochrome→zlib; true 1-bit bit-packing is optional. Documented in RG-002, no escalation.
- **F3** (SSIM≥0.98 trivial on synthetic flat images): Correctly flagged as risk — retained as CI gate but noted that meaningful gating needs photo-heavy content (Phase D+). No escalation.
- **F4** (CMYK DCTDecode vs FlateDecode): Correctly flagged as bug fix within Phase 0.1 scope — FlateDecode validates the `channels==4` skip path. No escalation.
- **F5** (AC5 ±1% on tiny 667-byte PDFs): Correctly flagged — mitigated by ≥50KB realistic corpus content and exempting `<2KB` inputs per Constraints. No escalation.

## Task Manager Readiness

> Can Task Manager create a valid task graph from these planning artifacts without making a new architectural decision?

**YES.** All required technologies are specified (QPDF, PDFium C++, GTest/vcpkg, GS/ocrmypdf optional with guards), component boundaries are unambiguous (`TestCorpusGenerator` is the only changed core-adjacent component; `render_diff` is a focused new binary in `tools/`), every integration has a defined approach (QPDF R6/R3, `command -v` guards, `FPDF_RenderPageBitmap` at 150 DPI, SSIM/PSNR in-process, TSV + idempotency), the 14-file corpus Data Model is fully enumerated with dictionary keys to assert, and phases provide coarse goals + dependencies + verification gates at the right abstraction. No architectural decision is left to Task Manager.

## Final Decision

**Status: APPROVED**

Canonical artifacts written:

- `docs/architecture.md` — complete (Codebase Summary, Summary, Tech Stack Decisions, System Components, Data Model, Integration Points, Non-Functional Approach, Architecture Risks)
- `docs/plan.md` — complete (Phases 0.1–0.4 with goals/requirements/dependencies/scope/gates, Assumptions) — ends with `Status: READY_FOR_TASK_MANAGER`
- `docs/plan_notes.md` — `RG-001` (14-file interpretation locked) + `RG-002` (F2–F5 informational), both `Status: open`, no `ESCALATED_TO_REQUIREMENTS`
- `docs/plan_review.md` — this file, `Status: APPROVED`

Task Manager may proceed upon detecting `Status: READY_FOR_TASK_MANAGER` in `docs/plan.md`. No further Plan validation pass is required for Phase 0.

