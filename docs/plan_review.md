# Plan Review — PDF Compressor Phases 1–8 (Goals B–I) — Revision Pass (Phase-2 AC-C5)

## Review Status

- **Proposal reviewed:** `docs/architecture_proposal.md` + `docs/plan_proposal.md`, both `Status: READY_FOR_VALIDATION` (targeted revision pass folding the Phase-2 stream-dedup fix into the plan)
- **Requirements baseline:** `docs/requirements/requirements.md` Part II (Status: `READY_FOR_PLAN`) with the **new Phase-2 AC-C5** and the **reworded AC-B3** (ESC-010 / ESC-010-FU, 2026-10-07); Phase 0 Part I complete and locked
- **Mode:** Existing-repo run — Validation Method step 7 applied and recorded below
- **Graphify:** `graphify-out/graph.json` exists (404 nodes); dedup facts re-verified against current source
- **Code Reviewer escalations:** `docs/code_review/escalations.json` has no `"routed_to": "PLAN"` entry with `"status": "open"` (ESC-007 / ESC-009 → TASK_MANAGER, addressed; ESC-008 / ESC-010 → REQUIREMENTS, addressed) — no escalation re-validation required
- **Decision:** `Status: APPROVED` → `Status: READY_FOR_TASK_MANAGER` written to `docs/plan.md` LAST

## Validation Summary

Targeted revision of the Phases 1–8 proposal: Phase-2 **AC-C5** (deferred ESC-010 stream-dedup activation) folded in, a Phase-1 P1-T3 forward-note added, assumptions + FI-4 updated. One line per validation step:

- **Requirements coverage:** PASS — AC-C5 fully covered (table below); reworded AC-B3 preserved; all other AC-B..AC-I and J-B1..J-I1 coverage unchanged from the pass-1 full review.
- **Architecture completeness:** PASS — dedup product fix (direct-copy `replaceObject`), staged `$TMPDIR` fixture, data-model note, NFR structural-correctness, integration + risk rows all explicit with rationale; no decision left implicit.
- **Architecture consistency:** PASS — no contradictions; the AC-C5 mechanism matches verified in-tree code (`PDFOptimizer.cpp:373`/`:377`, `objGen !=` at `:366`, try/catch `:372-382`); the staged fixture is explicitly distinct from the reference-shared `transparency.pdf` and outside the canonical 14.
- **Plan quality:** PASS — Phase 2 remains a coarse body of work ("Codec-Selection Correctness" + the structural-dedup activation); no task-level breakdown, no per-task test specs, no dependency graph.
- **Task Manager readiness:** PASS — the dedup fix names the file, mechanism, fixture, test rewrite, and gates; Task Manager needs no new architectural decision.
- **Existing-repo compliance:** PASS (5/5) — see below.

## Requirements Coverage (revision scope)

| Requirement | Covered in Architecture | Covered in Plan | Verdict |
|---|---|---|---|
| **AC-C5** stream dedup activated — product fix | Stream dedup replace path `[new/changed]`: direct-copy `replaceObject` replaces the inert ESC-001 guard; `objGen !=` + per-pair try/catch retained | Phase 2 Scope "Product fix" bullet | PASS |
| **AC-C5** staged `$TMPDIR` fixture `distinct_duplicate_streams.pdf` (distinct, not reference-shared, not in canonical 14) | Corpus staged-fixtures row `[existing]`; verified-deltas dedup row | Phase 2 Scope "New staged fixture" bullet + per-phase gates | PASS |
| **AC-C5** P1-T3 test rewrite `== 0` → `>= 1` | Verified-deltas dedup row cites `tests/test_structure_writer.cpp:53-54` | Phase 2 Scope "Test rewrite" bullet + Dependencies + Phase 1 note | PASS |
| **AC-C5** gates: `>= 1` defaults, `== 0 --no-dedup`, `qpdf --check` clean, S2–S5, benchmark before/after | NFR structural-correctness row; dedup risk row | Phase 2 "AC-C5 gates" + Requirements/Journeys covered | PASS |
| **AC-B3** (reworded) Phase 1 pins `== 0` (inert dedup); `>= 1` deferred to Phase 2 | Dedup-inert verified-deltas row; `OptimizationResult` live note | Phase 1 Scope P1-T3 note (verify-only pin) | PASS |
| **Phase 1 stays verify-only; Phase 2 pulls in no font/JBIG2/DPI scope** | Summary + components for Ph.3/5/6 unchanged | Phase 2 Goal/Non-goal text ("No font/JBIG2/DPI scope is pulled in") | PASS |
| **Existing Phases 1–8 content preserved** | Phases 1, 3–8 sections carried unchanged; Phase 2 extended only | Plan Phases 1, 3–8 unchanged; Phase 2 extended only | PASS |
| **FI-4 (Phase 6 stale ESC-001 reference)** | FontDedup row now cites the Phase-2 corrected path (FI-4) | Flagged Issues FI-4 recorded; Phase 6 planned later | PASS — correct flag, NOT a requirements defect; no escalation |

All other AC-B..AC-I and journeys J-B1..J-I1: coverage unchanged from the pass-1 full review and still PASS (the Phase-2 addition is additive to AC-C1..AC-C4).

## Architecture Findings

- **Dedup mechanism:** Correct and code-grounded. `PDFOptimizer.cpp:373` guard confirmed (`if (it->second.isIndirect()) continue;`), `streamsDeduplicated++` at `:377`, `objGen !=` guard at `:366`, try/catch at `:372-382`. The direct-copy `replaceObject` fix is the minimal change that makes the counter live while preserving the "one bad pair never fails the file" invariant.
- **Fixture isolation:** `distinct_duplicate_streams.pdf` is explicitly two *distinct* byte-identical indirect streams (not the reference-shared `transparency.pdf` / `generateSharedXObject()`), staged in `$TMPDIR`, and NOT added to the canonical 14 — matches AC-C5 and the safety invariant.
- **Consistency:** Data model (`streamsDeduplicated` live), NFR (structural correctness), integration (QPDF direct-copy), and risk rows all agree; no section contradicts another.
- **No new architecture decision:** the proposal supplies mechanism, fixture, test, and gates; nothing is deferred to Task Manager.

## Plan Findings

- Phase 2 is a single coarse body of work; the AC-C5 sub-bullets describe *what* (fix, fixture, test), not *tasks*. No task-level dependency graph or per-task test spec.
- Phase 1 remains verify-only; the P1-T3 note is a forward-reference, not new Phase-1 work.
- STOP boundary updated to include AC-C5; benchmark before/after retained.
- No added, removed, or reinterpreted scope.

## Existing Repo Compliance

*Applies because `docs/requirements/requirements.md` contains "This is an existing repository".*

| Check | Result | Evidence |
|---|---|---|
| Codebase Summary present and substantive | **PASS** | `architecture_proposal.md` Codebase Summary (verified deltas + carried-forward inventory of entities/components/integrations) now includes the dedup-inert row; `docs/architecture.md` carries the full Phase-0 inventory |
| Existing stack respected (existing/constrained first, nothing replaced) | **PASS** | Tech Stack Decisions list C++20/Qt6/CMake+vcpkg/PDFium/QPDF/libjpeg-turbo/OpenJPEG/libpng/zlib/GTest as `[existing — hard constraint]` before `[new]` additions; AC-C5 replaces no stack item (uses the existing QPDF API) |
| `[existing]` / `[new]` / `[new/changed]` labels present | **PASS** | System Components / Data Model / Integration Points all labeled; the new "Stream dedup replace path" row is `[new/changed]` |
| Additive-only | **PASS** | AC-C5 changes only the dedup replace path (already `[new/changed]`); Phase 0 sections untouched; no redesign beyond the requirement |
| Codebase Summary consistent with the proposal | **PASS** | `[existing]` items (QPDF, PDFOptimizer, corpus 14, GTest) appear in the inventory; the dedup delta cites real files/lines verified against the tree |

**Existing Repo Compliance overall: PASS (5/5)**

## Required Changes

None — `Status: APPROVED`.

## Requirements Escalations

None. **FI-4** (Phase 6 font-dedup wording still references the removed ESC-001 guard) is a correct staleness flag, **not** a blocking requirements defect: Phase 6 is planned later, the architecture already labels Phase 6's dedup as reusing the Phase-2 corrected path, and the Phase 6 wording can be refreshed when Phase 6 is planned. No `ESCALATED_TO_REQUIREMENTS` emitted. `docs/plan_notes.md` needs no new entry (RG-001/RG-002 stand; FI-4 is recorded in `docs/plan.md` Flagged Issues, not a requirements note).

## Task Manager Readiness

**YES.** Can Task Manager create a valid task graph without a new architectural decision? The AC-C5 fix is fully specified: file (`src/core/PDFOptimizer.cpp`), mechanism (direct-copy `replaceObject` replacing the `:373` guard, retaining `objGen !=` + try/catch), fixture (`$TMPDIR/distinct_duplicate_streams.pdf`), test rewrite (`tests/test_structure_writer.cpp` `== 0` → `>= 1`), and gates (`>= 1` defaults / `== 0 --no-dedup` / `qpdf --check` clean / S2–S5 / benchmark before-after). No architectural decision is left to Task Manager.

## Final Decision

**Status: APPROVED**

Canonical artifacts updated:

- `docs/architecture.md` — Phases 1–8 sections extended with the dedup component change, staged fixture, data-model note, NFR structural-correctness row, and integration + risk rows; Phase 0 retained
- `docs/plan.md` — Phase 2 extended with AC-C5 (goal, coverage, dependencies, scope, gates, STOP); Phase 1 P1-T3 note; assumptions + FI-4 added; ends with `Status: READY_FOR_TASK_MANAGER`
- `docs/plan_notes.md` — unchanged (no new requirements note; no escalation)
- `docs/plan_review.md` — this file, `Status: APPROVED`

Task Manager may proceed upon detecting `Status: READY_FOR_TASK_MANAGER` in `docs/plan.md`.

Status: APPROVED
