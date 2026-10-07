# Task Manager Notes — Phase 0

## Graphify
- Ran `graphify query "What tasks are needed to harden corpus generator, benchmark script, and render-diff for Phase 0?"` first before decomposing.
- Graph: 350 nodes / 549 edges, 15 communities. Query returned 73 nodes centered on TestCorpusGenerator, test_corpus_generator.h/.cpp, generate* methods, run_benchmark.sh, PDFium wrapper, codecs. Used to confirm stubs (generateWithForm, generateWithMetadata, generateWithJavaScript, generateMultiImage, generateSharedXObject, generateLargeUncompressed) are real nodes and that run_benchmark.sh and PDFium wrapper are separate communities — validated manual file reads.

## Locked Interpretations Applied
- **RG-001 (14 files)**: AC3 14-type list is authoritative. Implemented as exactly 14 PDFs where `transparency.pdf` embeds both SMask/alpha + shared XObject duplicate-font coverage via same QPDFObjectHandle on two pages (same object ID). No separate `merged_duplicate_fonts.pdf` unless documented as 15th — verifier asserts `==14`. If product owner wants 15 files, file a requirement change; Phase 0 stays at 14.
- **RG-002 (<2KB exemption)**: AC5 ±1% size gate and AC7 size guard exempt PDFs <2KB; only ≥50KB PDFs (photo_heavy, line_art, large_uncompressed, screenshot_flat) are checked for ±1% writer overhead. Tiny synthetic PDFs may show negative reduction due to QPDF writer overhead — allowed but logged.

## Decomposition Rationale
- Phase 0.1 touches only `tests/test_corpus_generator.h/.cpp` — no CMake change, so render-diff source can be parallelized after corpus (different files, but logically dependent).
- Phase 0.2 touches only `benchmark/run_benchmark.sh` — depends on T01 for corpus existence, but stat/ms/TSV logic is independent of render-diff.
- Phase 0.3 split into source (`tools/render_diff.cpp`) and build+integration (`CMakeLists.txt` + `benchmark/run_benchmark.sh` guard). This keeps `touches_files` narrow and allows T02 and T03 to run in same wave (wave 1 after T01): `benchmark/run_benchmark.sh` vs `tools/render_diff.cpp` have no file overlap. T04 then merges CMake wiring and benchmark TSV call with N/A fallback — depends on both T02 and T03, touches `CMakeLists.txt` + `benchmark/run_benchmark.sh` (sequential after T02, not same wave).
- Phase 0.4 split into verifier source (`tests/test_corpus_verifier.cpp`) and CTest gate (`CMakeLists.txt`). T05 (verifier) depends only on T01, so it can run parallel with T02/T03 in wave 1 (no file overlap). T06 (CMake wiring) depends on T04 and T05, touches `CMakeLists.txt` alone — sequential after T04, so no wave has two tasks touching `CMakeLists.txt`.
- **Waves (max_parallel_tasks=4)**: Wave0: T01. Wave1: T02, T03, T05 (3 parallel, no file overlap). Wave2: T04 (CMake+benchmark). Wave3: T06 (CMake). All DEPENDS_ON edges reflect logical contracts even when files don't overlap (e.g., benchmark depends on corpus shape, render-diff integration depends on benchmark TSV columns).

## Assumptions / Risks Flagged (not architecture dependencies)
- QPDF R6 AES-128 API `setR6Encryption` fallback to `setR3Encryption` assumed available — verified via grep in vcpkg qpdf headers; if mismatch, generator tries R6 first. Not an `architecture_dependency` because tech stack unchanged.
- `date +%s%3N` not POSIX on macOS BSD date — benchmark probes `gdate` then fallback `date +%s`×1000. `bc` may be absent — use `awk` fallback. Both documented in plan.md Assumptions.
- `libpdfium.dylib` bundled via `install_name_tool` to `@executable_path/../Frameworks`; if DYLD path wrong on CI, render-diff degrades to `N/A` rather than failing build — handled in T03/T04.
- Encrypted test fixture uses hard-coded password `test123` — test-only; production GUI password prompt is Phase H, out of scope for Phase 0.
- No change to `src/core/PDFOptimizer.cpp` or `src/codecs/*` — enforced by T06 git diff --stat check; DecisionEngine slider fix documented but not implemented (Phase C).

## File Layout Compliance
- Only `tests/`, `benchmark/`, `tools/render_diff.cpp`, `CMakeLists.txt` touched — respects `core/`, `codecs/`, `tools/` layout, C++20 RAII, warnings clean, offline, no network.
- `vcpkg.json` not changed (keeps GTest, no Catch2 switch) — locked 2026-10-03.

## Verification Gates
- Corpus: `./build/generate_corpus | tee corpus_info.txt && ls $TMPDIR/pdfcompress_test_corpus/*.pdf | wc -l ==14 && qpdf --check` per file.
- Benchmark: `bash benchmark/run_benchmark.sh build` with/without gs/ocrmypdf/qpdf, check SKIPPED vs FAILED, TSV header, idempotency, dual write, `|| true`.
- Render-diff: `cmake --build build --target render_diff && ./build/render_diff orig.pdf opt.pdf` prints parsable SSIM/PSNR or N/A.
- Tests: `cmake -B build -DCMAKE_TOOLCHAIN_FILE=third_party/vcpkg/scripts/buildsystems/vcpkg.cmake && cmake --build build && ctest --test-dir build --output-on-failure -V` → 14+ tests pass.


## 2026-10-04 — Escalation-driven revision (ESC-001, ESC-004)

Acting as Task Manager per user instruction, without invoking any other agent. No changes to `docs/code_review/escalations.md` (Code Reviewer owns it).

### What changed and why
- **ESC-004 (bad task split, blocking, routed to TASK_MANAGER):** Code Reviewer proved no task in the graph owned `src/core/PDFOptimizer.cpp`, so re-dispatching T05 could never resolve ESC-001. Fixed by adding **T07-optimizer-dedup-fix** (`touches_files: ["src/core/PDFOptimizer.cpp"]`, `dependencies: ["T01-corpus-hardening"]`, `status: not_started`), scoped strictly to the minimal ESC-001 crash fix (isIndirect skip + try/catch, objGen != retained; no codec/DecisionEngine/stripping changes). Narrow footprint overlaps with nothing, so it parallelizes freely.
- **ESC-001 (code bug, blocking, routed to BUILD):** added the missing logical edge — **T05-test-verifier** now `dependencies: ["T01-corpus-hardening", "T07-optimizer-dedup-fix"]` (was T01 only). T05-T04 assumes the dedup contract T07 establishes; file non-overlap (`tests/test_corpus_verifier.cpp` vs `src/core/PDFOptimizer.cpp`) never implied logical independence. T05 `status` left as `blocked` (only Build flips statuses).
- **T06 gate carve-out (consequence of ESC-001/ESC-004):** **T06-ctest-wiring-gate** AC4 and test T06-T04 amended — `src/core/PDFOptimizer.cpp` diff is now allowed **only** for the ESC-001 dedup guard; image decode/encode branches, DecisionEngine wiring, stripping logic, and all of `src/codecs/*` remain forbidden. T06 `dependencies` extended to `["T05-test-verifier", "T04-render-diff-build", "T07-optimizer-dedup-fix", "T08-test-expectations-fix"]`.
- **3 pre-existing `tests/test_pdf_optimizer.cpp` failures** (review R-002 note, outside all footprints, blocking T06-T02's 100%-pass gate): added **T08-test-expectations-fix** (`touches_files: ["tests/test_pdf_optimizer.cpp"]`, `dependencies: ["T01-corpus-hardening"]`, `status: not_started`) to correct the three wrong assertions to match verified behavior — BookmarksStripped/AnnotationsStripped pass explicit strip options (defaults strip JS+metadata only), CmykSkipped asserts `imagesSkipped==1` instead of `imagesProcessed==0`. No product-code changes in T08. This is within Phase 0 acceptance ("all existing and new tests pass"), not new scope.
- **Execution graph** (`execution_graph.json`): added Task nodes T07, T08 and DEPENDS_ON edges T01→T07, T01→T08, T07→T05, T07→T06, T08→T06 (12 edges total, validated acyclic; every edge references an existing task id).

### Assumption log
- The ESC-001 working-tree fix (dedup guard) may already be present from manual intervention; T07 instructs Build to verify every T07 test and reconcile drift rather than revert. Statuses of T01–T05 left untouched (Build owns the `status` field); new tasks initialized to `not_started` per protocol.
- T02/T03/T04 untouched (review-verified clean). ESC-002/ESC-003 remain `addressed`, no graph action needed.
- Waves with the new tasks (max_parallel_tasks=4): T07 and T08 can run in parallel (disjoint footprints, both depend only on done T01); T05 unblocks once T07 is done; T06 runs last.

### Requirements ambiguity flagged (not resolved)
- None new. RG-001 (14 files) and RG-002 (<2KB exemption) locks unchanged.

# Task Manager Notes — Phase 1 (Goal B)

## Decomposition Rationale
- Phase 1 is verify-and-complete with NO strip-semantics change, so all product code is read-only; the graph creates 3 new test files + 2 wiring/docs tasks. No `src/` file is touched by any task.
- P1-T1/T2/T3 each own exactly one new test file (no touches overlap) and have no interdependencies → Wave 1 runs all three in parallel. T2's 5th AC covers the plan's CLI round-trip + J-B1 explicit-flag-wins edge case via optimizer-API semantics (mirrors run_optimize parsing); T2-T04 stages its GoTo/AA fixture in $TMPDIR inside the test, never committed.
- P1-T4 is the sole owner of the CMakeLists.txt edit (depends on T1–T3 so the files exist to wire) → Wave 2. P1-T5 is the sole owner of the USAGE.md edit (depends on T4 so binaries are built for the benchmark gate) → Wave 3.
- **Waves (max_parallel_tasks=4):** Wave1: P1-T1, P1-T2, P1-T3 (disjoint footprints). Wave2: P1-T4 (CMakeLists.txt). Wave3: P1-T5 (USAGE.md + benchmark). No wave has overlapping touches_files.
- AC-B4 benchmark gate: since Phase 1 makes no compression change there is no pre-existing pre/post pair to diff unless an archived TSV exists; P1-T5 compares two consecutive runs (±2% stability proof) or against an archived pre-Phase-1 TSV when present. This is a bounded verification choice within AC-B4, not an architectural decision.

## No escalation
- All Phase-1 technology and component boundaries are settled in docs/architecture.md + docs/plan.md (Validator: Task Manager readiness YES). No undecided choice encountered; no ESC-PLAN entry. No requirements ambiguity found beyond what plan_review.md already dispositioned (FI-1–FI-3 are later phases).

## 2026-10-07 — Escalation-driven revision (ESC-007, ESC-009; ESC-008 as input)

Acting as Task Manager per Orchestrator rework instruction. No changes to `docs/code_review/escalations.json` (Code Reviewer owns it); no changes to requirements or canonical plan files. P1-T1 left `done`; P1-T4/P1-T5 dependencies untouched (T4 on T1–T3, T5 on T4 — still sound); `touches_files` unchanged on all revised tasks (test-only edits, Phase 1 verify-only lock holds, no product-code or corpus change).

### ESC-007 (P1-T2) — narrowed test expectation, no product change
- **Cause (per Code Reviewer diagnosis, not re-diagnosed):** P1-T2-T02 asserted `EXPECT_FALSE(metadataStripped)` on metadata-free PDFs under Balanced, but `PDFOptimizer.cpp:119-125` sets `metadataStripped=true` unconditionally whenever `stripMetadata=true` (Balanced default) — so the test was stricter than its own task AC and contradicted locked `forProfile` behavior.
- **Ruling applied:** ESC-008, now normative in `requirements.md` (AC-B2 note + glossary): `metadataStripped` means "option applied" (intent) — true on ALL inputs under Balanced/MaxCompression; tests must assert true on `with_metadata.pdf` and must NOT assert false on metadata-free PDFs; "nothing else" scopes to the six removal counts.
- **Change:** P1-T2 AC2 reworded to scope "nothing else" explicitly to the six removal counters and to forbid asserting `metadataStripped` on metadata-free PDFs; P1-T2-T02 description/expected-result narrowed identically (1:1 AC–test link preserved verbatim); added the intent-semantics contract line to the context pack. `touches_files` still exactly `tests/test_stripping_profiles.cpp`. Status reset `blocked` → `not_started`.

### ESC-009 (P1-T3) — rescoped to converging current-behavior pin
- **Cause (per Code Reviewer diagnosis, not re-diagnosed):** P1-T3-T02 asserted `streamsDeduplicated>=1` with defaults, but two independent facts outside P1-T3's footprint make that unachievable: (1) the ESC-001 `isIndirect` guard skips all `seenStreams` handles (always indirect → counter is dead code); (2) `generateSharedXObject()` shares one indirect object by reference, so no distinct duplicate pair exists to dedup. Product-code and corpus changes are both barred by the Phase 1 verification-only lock, so re-running the old task could never converge.
- **Chosen rescope (branch 1 of the authorized options) with rationale:** pin the verified current behavior — AC2 + P1-T3-T02 now assert `streamsDeduplicated==0` in BOTH runs (defaults and `--no-dedup`), with the inert-dedup rationale recorded in the AC itself. Rationale for this branch over splitting into a new executable task: any new Phase 1 task asserting `>=1` would need `PDFOptimizer.cpp` and/or corpus-fixture writes, which the verify-only lock forbids — it could not converge either. The aspirational `>=1` assertion is therefore recorded here as a **deferred follow-up-phase item** (requires a product dedup fix, e.g. direct-copy replace path, plus a distinct-duplicate-streams corpus fixture), not as an executable Phase 1 task; no product/codec/corpus changes bundled. Description + T02 + context contract updated; 1:1 AC–test link preserved; `touches_files` still exactly `tests/test_structure_writer.cpp`. Status reset `blocked` → `not_started`.
- **Flag for Requirements Analyst (no escalation status; user relays):** requirements AC-B3 still states `streamsDeduplicated≥1` on the shared-XObject PDF with defaults, which the reference-shared fixture cannot satisfy under any replace-based dedup (ESC-009 observation). Either AC-B3 needs rewording for Phase 1 (pin `==0`, defer `>=1`), or the follow-up phase that owns the dedup fix must also own the fixture + AC-B3 re-verification. Task Manager does not resolve this — flagged only.

### Waves / dependencies
- Unchanged: Wave1 P1-T2 + P1-T3 (disjoint footprints, no interdependency); Wave2 P1-T4 (CMakeLists.txt, deps T1–T3); Wave3 P1-T5 (USAGE.md, dep T4). No new edges; graph remains acyclic (validated by `check_tasks.py`).

## 2026-10-07 — Post-reconciliation rework (ESC-009 / ESC-010 root now fixed)

Acting as Task Manager per Orchestrator rework instruction (normal_progression, post-reconciliation). No changes to `docs/requirements/*`, `docs/code_review/escalations.json`, or the canonical plan files. No change made to `docs/tasks/task-graph.json` or `docs/tasks/execution_graph.json` — none was warranted (see below). Phase 1 graph remains 5 tasks, all `done`, metadata `"status": "READY_FOR_BUILD"`.

### P1-T3 consistency check — PASS (no revision needed)
Requirements Analyst reworded **AC-B3** (requirements.md:352-391) to pin the verified behavior: `streamsDeduplicated == 0` on the shared-XObject PDF **with defaults AND with `--no-dedup`**, `qpdf --check` clean on both, default output ≤ no-opt output — exactly what P1-T3 already asserts.
- **Task AC ↔ requirement: aligned.** P1-T3's three ACs (default ≤ no-opt; `==0` in both runs with the inert-dedup rationale; both outputs valid) map 1:1 to reworded AC-B3. No AC edit required.
- **Test ↔ AC: aligned.** `tests/test_structure_writer.cpp:53-54` asserts `EXPECT_EQ(rDefault.streamsDeduplicated, 0)` and `EXPECT_EQ(rNoDedup.streamsDeduplicated, 0)` — the `==0` pin the rescoped AC and the AC-B3 test note (requirements.md:388-391) both call for.
- **Footprint: aligned.** `touches_files` remains exactly `["tests/test_structure_writer.cpp"]`, which is now sufficient: reworded AC-B3 no longer demands product (`PDFOptimizer.cpp`) or corpus (`test_corpus_generator.cpp`) changes in Phase 1. The prior ESC-009 mismatch ("footprint can't satisfy its AC") is therefore **moot** — the AC was narrowed to what a test-only footprint can converge on, rather than widening the footprint.

### ESC-009 disposition (recorded here; Code Reviewer owns the escalation file)
ESC-009 (routed TASK_MANAGER, kept `open` by Code Reviewer precisely because AC-B3 still promised `>=1`) is **resolved at its root** by the AC-B3 reword: P1-T3 no longer promises behavior its footprint cannot deliver. No further Task-Manager action. Code Reviewer may flip ESC-009 to `addressed` on its own re-verification; Task Manager does not edit `escalations.json`.

### Deferred follow-up — owning phase pending ESC-010-FU (NOT tasked into Phase 1)
The aspirational `streamsDeduplicated >= 1` assertion remains a real follow-up, not a dropped one. It requires **both** (a) a product fix (replace the ESC-001 `isIndirect` skip at `PDFOptimizer.cpp:373` with a direct-copy `replaceObject` so the counter is live) **and** (b) a corpus fixture of genuinely distinct byte-identical streams (not a reference-shared one). Its owning phase is **pending ESC-010-FU** (open_questions.md row ESC-010-FU): **Phase 6 (Goal G) recommended**, **Phase 2 fallback** if Plan scopes Phase 6 font-only. Recorded in requirements.md AC-B3 deferred follow-up and design_gaps.md row 14.
- **Task Manager deliberately does NOT add a Phase-1 task for this.** Phase 1 is verify-only and real-done; a Phase-1 task asserting `>=1` could not converge (it needs product + corpus writes the Phase-1 lock forbids). The fix must be slotted by Plan into the named later phase once ESC-010-FU is confirmed, at which point the P1-T3 test is rewritten to `>= 1` **in the same phase that lands the fix** (per AC-B3 test note), not before.

### Graph validation
- `python3 ~/.config/opencode/scripts/check_tasks.py` → `OK: 5 tasks validated, execution_graph.json written.` (3 warnings only: the three new test files are pure-output files, expected). Exit 0.
- No dependency edges, `touches_files`, ACs, or statuses changed; graph remains acyclic and all-done. `"status": "READY_FOR_BUILD"` confirmed; Phase 1 is complete.
