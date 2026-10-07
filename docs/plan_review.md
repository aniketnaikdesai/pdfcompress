# Plan Review — PDF Compressor Phases 1–8 (Goals B–I) — Revision Pass (Phase-2 AC-C5 mechanism redesign)

## Review Status

- **Proposal reviewed:** `docs/architecture_proposal.md` + `docs/plan_proposal.md`, both `Status: READY_FOR_VALIDATION` (targeted revision pass: Phase-2 AC-C5 mechanism changed to **reference-rewriting**)
- **Requirements baseline:** `docs/requirements/requirements.md` Part II (Status: `READY_FOR_PLAN`) with the **refreshed Phase-2 AC-C5** (reference-rewriting implementation note) and the reworded AC-B3 (ESC-010 / ESC-010-FU, 2026-10-07); Phase 0 Part I complete and locked
- **Mode:** Existing-repo run — Validation Method step 7 applied and recorded below
- **Graphify:** `graphify-out/graph.json` exists (404 nodes); dedup facts re-verified against current source (targeted file reads)
- **Code Reviewer escalations:** `docs/code_review/escalations.json` has no `"routed_to": "PLAN"` entry with `"status": "open"` — no escalation re-validation required
- **Decision:** `Status: APPROVED` → `Status: READY_FOR_TASK_MANAGER` written to `docs/plan.md` LAST

## Validation Summary

Targeted revision of the Phases 1–8 proposal: the Phase-2 AC-C5 mechanism was corrected from the infeasible direct-copy `replaceObject` to **reference-rewriting** (cycle-safe referrer walk + `replaceKey`/`setArrayItem` repoint, then drop-unreferenced), with `requirements.md` AC-C5 refreshed to match. One line per validation step:

- **Requirements coverage:** PASS — AC-C5 fully covered with its outcome unchanged (table below); reworded AC-B3 preserved; all other AC-B..AC-I / J-B1..J-I1 coverage unchanged from prior passes.
- **Architecture completeness:** PASS — new `ReferenceRewriter` component `[new]`, "Stream dedup replace path" `[new/changed]`, Tech Stack decision #28, NFR structural-correctness row, integration row, and three dedup risk rows all explicit with rationale.
- **Architecture consistency:** PASS — the mechanism matches verified in-tree code (`PDFOptimizer.cpp:373` guard, `:376` `replaceObject`, `:366` `objGen !=`, `:372-382` try/catch, `:399` `setPreserveUnreferencedObjects(false)`) and the verified QPDF 12.3.2 API (`QPDF_objects.cc:1968` rejects indirect handles; `replaceKey`/`setArrayItem` accept indirect values).
- **Plan quality:** PASS — Phase 2 remains a coarse body of work; the AC-C5 sub-bullets state *what* (fix, fixture, test), not tasks; no task-level breakdown, dependency graph, or per-task test spec.
- **Task Manager readiness:** PASS — file, mechanism, fixture, test rewrite, and gates are fully specified; no new architectural decision is left to Task Manager.
- **Existing-repo compliance:** PASS (5/5) — see below.

## Requirements Coverage (revision scope)

| Requirement | Covered in Architecture | Covered in Plan | Verdict |
|---|---|---|---|
| **AC-C5** product fix — reference-rewriting replaces the inert ESC-001 guard | Stream dedup replace path `[new/changed]` + `ReferenceRewriter` `[new]` + Tech Stack #28 | Phase 2 Scope "Product fix (reference-rewriting)" | PASS |
| **AC-C5** cycle-safe referrer walk; `replaceKey`/`setArrayItem`; drop-unreferenced | `ReferenceRewriter` row (never follows indirect refs; visit-once); NFR row | Phase 2 Scope product-fix bullet | PASS |
| **AC-C5** keep FNV signature + `objGen !=` guard + per-pair try/catch | Stream dedup replace path row; risk rows | Phase 2 Scope product-fix bullet | PASS |
| **AC-C5** require `/Filter` + `/DecodeParms` match (no decode-affecting repoint) | `ReferenceRewriter` row + risk row "Repoint changes semantics…" | Phase 2 Scope product-fix bullet (explicit) | PASS |
| **AC-C5** outcome unchanged: `>= 1` defaults, `== 0 --no-dedup`, `qpdf --check` clean, S2–S5, benchmark before/after | NFR structural-correctness row; dedup risk rows | Phase 2 "AC-C5 gates" + Requirements/Journeys covered | PASS |
| **AC-C5** staged `distinct_duplicate_streams.pdf` with **both streams actually referenced** | Corpus staged-fixtures row; risk row "unreferenced duplicate…" | Phase 2 Scope "New staged fixture" bullet | PASS |
| **AC-C5** P1-T3 test rewrite `== 0` → `>= 1` | Verified-deltas dedup row cites `tests/test_structure_writer.cpp:53-54` | Phase 2 Scope "Test rewrite" bullet + Dependencies + Phase 1 note | PASS |
| **AC-B3** (reworded) Phase 1 pins `== 0`; `>= 1` deferred to Phase 2 | Dedup-inert verified-deltas row; `OptimizationResult` live note | Phase 1 Scope P1-T3 note (verify-only pin) | PASS |
| **Phase 1 stays verify-only; Phases 3–8 intact; no unrelated scope** | Phases 1, 3–8 components unchanged; Phase 2 dedup rows only | Plan Phases 1, 3–8 unchanged; Phase 2 extended only | PASS |
| **FI-5 (stale AC-C5 implementation note)** | Requirements AC-C5 refreshed to reference-rewriting | Plan Flagged Issues FI-5 marked ADDRESSED | PASS — resolved |
| **FI-4 (Phase 6 stale ESC-001 reference)** | FontDedup row now cites the Phase-2 reference-rewriting path (FI-4) | Plan Flagged Issues FI-4 recorded; Phase 6 planned later | PASS — correct flag, non-blocking, no escalation |

## Architecture Findings

- **Mechanism feasibility confirmed against the in-tree QPDF 12.3.2.** `QPDF::replaceObject` throws for any indirect handle except a self-stream (`libqpdf/QPDF_objects.cc:1968`), and streams are always indirect, so the earlier direct-copy path was infeasible. `replaceKey` (`QPDFObjectHandle.hh:944`) and `setArrayItem` (`:917`) accept `QPDFObjectHandle` values including indirect references (used with indirect values at `QPDFObjectHandle.cc:1231,1250`), so repointing is a valid alias mechanism. The duplicate drops under the already-present `writer.setPreserveUnreferencedObjects(false)` (`PDFOptimizer.cpp:399`), or via `QPDF::removeObject` (`QPDF.hh:754`).
- **Detection retained correctly.** The five dict-key signature + FNV hash (`2166136261u`/`16777619u`) + `memcmp` + `objGen !=` guard (`:366`) + per-pair try/catch (`:372-382`) are preserved; the new requirement that `/Filter` + `/DecodeParms` also match closes the only decode-affecting repoint hazard.
- **Fixture validity.** `distinct_duplicate_streams.pdf` is specified as two *distinct* byte-identical indirect streams, both actually referenced, staged in `$TMPDIR`, NOT the reference-shared `transparency.pdf` / `generateSharedXObject()` fixture (which returns `generateTransparency()`, `tests/test_corpus_generator.cpp:450-451`) and NOT added to the canonical 14.
- **Consistency.** Data model (`streamsDeduplicated` live), NFR, integration, and risk rows all agree; no section contradicts another.

## Plan Findings

- Phase 2 is a single coarse body of work; the AC-C5 sub-bullets describe *what* (fix, fixture, test), not *tasks*. No task-level dependency graph or per-task test spec.
- Phase 1 remains verify-only; the P1-T3 note is a forward-reference, not new Phase-1 work. Phases 3–8 are unchanged.
- STOP boundary includes AC-C5; benchmark before/after retained. No added, removed, or reinterpreted scope.

## Existing Repo Compliance

*Applies because `docs/requirements/requirements.md` contains "This is an existing repository".*

| Check | Result | Evidence |
|---|---|---|
| Codebase Summary present and substantive | **PASS** | `architecture_proposal.md` Codebase Summary: verified-deltas table (incl. the new AC-C5 mechanism-discovery row with QPDF file:line evidence) + carried-forward inventory of entities/components/integrations |
| Existing stack respected (existing/constrained first, nothing replaced) | **PASS** | Tech Stack Decisions list C++20/Qt6/CMake+vcpkg/PDFium/QPDF/libjpeg-turbo/OpenJPEG/libpng/zlib/GTest as `[existing — hard constraint]` before `[new]` additions; AC-C5 replaces no stack item (QPDF-API-only change) |
| `[existing]` / `[new]` / `[new/changed]` labels present | **PASS** | System Components / Data Model / Integration Points all labeled; new `ReferenceRewriter` is `[new]`, "Stream dedup replace path" is `[new/changed]` |
| Additive-only | **PASS** | AC-C5 changes only the dedup replace path; Phase 0 sections untouched; no redesign beyond the requirement |
| Codebase Summary consistent with the proposal | **PASS** | `[existing]` items (QPDF, PDFOptimizer, corpus 14, GTest) appear in the inventory; the dedup delta and mechanism row cite real files/lines verified against the tree |

**Existing Repo Compliance overall: PASS (5/5)**

## Required Changes

None — `Status: APPROVED`.

## Requirements Escalations

None. **FI-5** (AC-C5 implementation note naming the infeasible direct-copy `replaceObject`) is **resolved**: `requirements.md` AC-C5 and `design_gaps.md` #14 were refreshed to name reference-rewriting with the outcome unchanged. **FI-4** (Phase 6 Goal wording still cites the removed ESC-001 guard) remains a correct, **non-blocking** staleness flag deferred to Phase 6 planning — the architecture already labels Phase 6's dedup as reusing the Phase-2 corrected `ReferenceRewriter` path, so no `ESCALATED_TO_REQUIREMENTS` is emitted and `docs/plan_notes.md` needs no new entry.

## Task Manager Readiness

**YES.** Can Task Manager create a valid task graph without a new architectural decision? The AC-C5 fix is fully specified: file (`src/core/PDFOptimizer.cpp`), new helper (`core/ReferenceRewriter.{h,cpp}`), mechanism (reference-rewriting replacing the `:373` guard and the `:376` `replaceObject` call; cycle-safe referrer walk + `replaceKey`/`setArrayItem`; `/Filter` + `/DecodeParms` match; retain signature/`objGen !=`/try-catch), fixture (`$TMPDIR/distinct_duplicate_streams.pdf`, both streams referenced), test rewrite (`tests/test_structure_writer.cpp` `== 0` → `>= 1`), and gates (`>= 1` defaults / `== 0 --no-dedup` / `qpdf --check` clean / S2–S5 / benchmark before-after). No architectural decision is left to Task Manager.

## Final Decision

**Status: APPROVED**

Canonical artifacts updated:

- `docs/architecture.md` — Phase-2 AC-C5 mechanism corrected to reference-rewriting: revision note, Summary, AC-C5 mechanism-discovery delta row, Tech Stack decision #28, "Stream dedup replace path" `[new/changed]` + new `ReferenceRewriter` `[new]` component, FontDedup wording, QPDF integration row, NFR structural-correctness row, and the dedup risk rows; Phase 0 retained
- `docs/plan.md` — Phase 2 Goal + Scope (product fix, staged fixture) corrected to reference-rewriting; assumption added; FI-4 reworded and FI-5 marked ADDRESSED; ends with `Status: READY_FOR_TASK_MANAGER`
- `docs/plan_notes.md` — unchanged (no new requirements note; no escalation)
- `docs/plan_review.md` — this file, `Status: APPROVED`

Task Manager may proceed upon detecting `Status: READY_FOR_TASK_MANAGER` in `docs/plan.md`.

Status: APPROVED
