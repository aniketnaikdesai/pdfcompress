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

# Task Manager Notes — Phase 2 (Goal C) — Codec-Selection Correctness

Fresh graph written 2026-10-07 (normal_progression, Phase 2 kickoff). Phase 1 graph replaced in place with 7 new `not_started` tasks. No edits to `docs/requirements/*`, `docs/plan.md`, `docs/architecture.md`, `docs/code_review/escalations.json`, or `docs/code_review/review_log.md`. `plan.md` ends with `Status: READY_FOR_TASK_MANAGER` and `plan_review.md` is `APPROVED` — decomposition authorized.

## Decomposition Rationale
- Phase 2 is the first product-code-carrying phase after Phase 1. Tasks are file-scoped to the architecture's named components: `DecisionEngine` routing, `PDFOptimizer` decode path, new `CmykHandler`, `JpegCodec`, `run_optimize`, `main.cpp`, `USAGE.md`.
- **P2-T1 (AC-C1 + AC-C2 routing)** reworks the `DecisionEngine` switch once: slider scoped to lossy classes, PNG/alpha branch, lossless Screenshot/LineArt at MaxQuality. Extends `CompressionParams` with `hasAlpha`/`bitsPerComponent`/`colorSpace` (and a defaulted `compress` parameter/overload) so existing callers keep compiling. Unit tests only — no corpus dependency, so the normative `TEST(DecisionEngine, SliderOnlyAffectsLossyClasses)` is independently verifiable.
- **P2-T2 (AC-C2 integration)** adds `/SMask`/`/Mask` detection in `PDFOptimizer` and plumbs `hasAlpha` into the engine so alpha takes the lossless path; mask streams preserved. Depends on T1 for the engine contract.
- **P2-T3 (AC-C1 controls)** covers grayscale detection (DeviceGray -> ScannedText; monochrome -> Monochrome; slider ignored) and the efficient-image skip (`/BitsPerComponent != 8` kept original + logged reason). Depends on T2 for the metadata/param plumbing.
- **P2-T4 (AC-C3)** replaces the silent `channels==4` CMYK skip with a preserve path + `cmykPreserved` result counter + logged reason. Depends on T3 for colorSpace detection.
- **P2-T5 (AC-C3b)** adds the opt-in CMYK->RGB transcode: new `src/core/CmykHandler.h/.cpp`, `JpegCodec` RGB encode path, `--transcode-cmyk-to-rgb` + GUI checkbox + `transcodedCmyk`. Depends on T4.
- **P2-T6 (AC-C5)** is independent of the codec work: direct-copy `replaceObject` dedup fix, staged `distinct_duplicate_streams.pdf`, and the P1-T3 test rewrite. Runs in wave 0 alongside T1 (disjoint files).
- **P2-T7 (AC-C4 + S1/S6/S7/S8)** is the phase gate: USAGE.md docs, full-suite green, benchmark before/after with both TSVs archived. Depends on all product tasks.

## Dependency vs. touches_files analysis (hot spots)
- `src/core/PDFOptimizer.cpp` is the phase hot spot: T2, T3, T4, T5 and T6 all write it. Build serializes on `touches_files`, so these never share a wave. Logical edges (T1->T2->T3->T4->T5) additionally encode the assumed metadata/params contracts; the edges are real, not file-overlap substitutes.
- `src/core/PDFOptimizer.h` is shared by T4 and T5 (result/option fields) — serialized with the `.cpp` anyway.
- `tests/test_pdf_optimizer.cpp` is appended by T2/T3/T4/T5 — same serialization.
- `src/main.cpp` (T5 only), `tools/run_optimize.cpp` (T5 only), `CMakeLists.txt` (T5 only), `USAGE.md` (T7 only), `test_corpus_generator.*`/`test_structure_writer.cpp` (T6 only) — no contention.
- **No CMake churn from new test files**: all Phase-2 tests are appended to already-wired files (`tests/test_decision_engine.cpp`, `tests/test_image_analyzer.cpp`, `tests/test_pdf_optimizer.cpp`, `tests/test_structure_writer.cpp`), so each task can build and run its own scoped tests without waiting on a wiring task. The only `CMakeLists.txt` edit is T5 adding the new `CmykHandler.cpp` to `CORE_SOURCES`.
- **Waves (max_parallel_tasks=4):** Wave0 = P2-T1 + P2-T6 (disjoint footprints, both dependency-free). Wave1 = P2-T2. Wave2 = P2-T3. Wave3 = P2-T4. Wave4 = P2-T5. Wave5 = P2-T7. No wave contains two tasks with overlapping `touches_files`.
- Max width is 2 because the phase inherently funnels through `PDFOptimizer.cpp`; `max_parallel_tasks` is set to 4 (headroom; matches Phase 1) but the ready frontier never exceeds 2.

## No ESC-PLAN escalation
- Every Phase-2 technology and component boundary is settled in `docs/architecture.md` + `docs/plan.md` (Validator readiness YES): DecisionEngine routing, the new `CmykHandler` component, the direct-copy `replaceObject` dedup path, PNG/JPEG/JP2/zlib codecs, and the staged `$TMPDIR` fixture are all named. The plan explicitly delegates implementation-side selections (CMYK delta-E metric; resampler) to Task Manager/Build bounded by the SSIM gates.
- No undecided datastore/interface/component/tech choice blocks decomposition; no ESC-PLAN entry written; `"status": "READY_FOR_BUILD"` set.

## Interpretation notes (decomposition-level, bounded by the ACs)
- **PNG embedding:** AC-C2 fixes the output `/Filter` as FlateDecode (never DCTDecode) and requires `m_pngCodec` reachable. The task contracts state that requirement behaviorally; the exact byte layout (PNG-predictor Flate vs plain zlib) is left to Build within the AC, since PDF has no PNG stream filter and the requirement already pins FlateDecode.
- **`cmykPreserved` counter (T4):** added to make "skip-with-reason logged" (AC-C3) independently testable in GTest. `imagesSkipped==1` remains and the existing `CmykSkipped` test is preserved.
- **`CmykHandler` location (T5):** placed at `src/core/CmykHandler.h/.cpp` — it is optimizer-facing color logic, not an `ImageCodec` (architecture reserves `codecs/` for `ImageCodec` implementations). Flagged for Plan/Code-Reviewer if a different path was intended.
- **Efficient-image skip (T3):** implemented as "`/BitsPerComponent != 8` is kept original with a logged reason", consistent with S9 resilience and the 8-bit decode assumption; the staged 1-bit fixture lives in `$TMPDIR` and is not added to the canonical 14.

## Flagged observations (for Requirements Analyst / Plan — no escalation status; user relays)
- **grayscale_scan classification (testability):** the Phase-0 corpus fixture `grayscale_scan.pdf` (DeviceGray, `i % 256` ramp) does not currently satisfy `ImageAnalyzer::classify`'s ScannedText rule (which needs `meta.dpiX >= 150`, and `PDFOptimizer` never sets `meta.dpiX`), so AC-C1's "scanned-text control stays JP2" and plan.md's "grayscale_scan (control: stays JP2)" are not reachable without the grayscale-detection work P2-T3 owns. P2-T3 addresses it, but if the product owner intended the fixture itself to classify as ScannedText without an analyzer change, that is a fixture/requirements nuance to confirm. Flagged, not resolved.
- **AC-C1 "byte-comparable in codec choice" vs size guard:** for synthetic corpus images the size guard may keep the original stream (e.g. a JP2 larger than the source), so the applied `/Filter` can differ from the chosen codec. P2-T1/T3 assert *slider-invariance of codec choice* rather than a specific applied filter; if the AC intends the applied filter to be JP2 specifically, that reading should be confirmed.

## Graph validation
- `python3 ~/.config/opencode/scripts/check_tasks.py` -> `OK: 7 tasks validated, execution_graph.json written.` Exit 0. Only warnings: P2-T5's two not-yet-existing output files (`src/core/CmykHandler.h/.cpp`) are in `touches_files` but not `read_files` (expected — pure-output files).
- 7 tasks, 10 DEPENDS_ON edges, acyclic; every `dependencies` id exists; every acceptance criterion has >=1 matching test; every code task has non-empty `touches_files`; every context pack well-formed. `"status": "READY_FOR_BUILD"`.

## 2026-10-07 — P2-T6 rescope to the feasible reference-rewriting mechanism (AC-C5)

Acting as Task Manager per Orchestrator rework instruction (normal_progression, P2-T6 rescope after an approved Plan/architecture revision). No edits to `docs/requirements/*`, `docs/plan.md`, `docs/architecture.md`, `docs/code_review/escalations.json`, or `docs/code_review/review_log.md`. No done task's fields or results were changed — only **P2-T6** was rewritten. The canonical artifacts already carry the decision (`plan.md` 2026-10-07 revision + Assumptions; `architecture.md` ReferenceRewriter row; `requirements.md` AC-C5 implementation note), so this is a decompose-the-approved-decision pass, **not** an ESC-PLAN escalation.

### Why the rescope (cause, per the approved revision — not re-diagnosed)
- The original P2-T6 mechanism was a direct-copy `QPDF::replaceObject`. Verified infeasible against in-tree QPDF 12.3.2: `replaceObject` rejects any indirect handle except a self-stream (`QPDF_objects.cc:1967-1968`), and PDF streams are always indirect (`QPDF::newStream()` → `makeIndirectObject`), so no "direct copy" replacement can be constructed. P2-T6 was `blocked` because its own mechanism could never satisfy its ACs.
- Plan + Validator + Requirements re-approved **reference-rewriting** as the feasible mechanism (`requirements.md` AC-C5 refreshed; `design_gaps.md` #14; Plan FI-5 ADDRESSED). Outcome is unchanged: `streamsDeduplicated>=1` with defaults, `==0` with `--no-dedup`, `qpdf --check` clean, S2–S5, benchmark before/after.

### What changed in P2-T6 (task-graph.json)
- **Title/description:** direct-copy `replaceObject` → reference-rewriting (cycle-safe referrer walk + `replaceKey`/`setArrayItem` repoint; duplicate drops under `setPreserveUnreferencedObjects(false)`). Keeps the byte-identical signature detection (FNV hash + five dict keys) and additionally requires `/Filter` + `/DecodeParms` to match; keeps the `objGen !=` guard and per-pair try/catch.
- **AC1/AC2/AC3 + tests T01/T02/T03** updated to the new mechanism; **test IDs preserved verbatim** (T01/T02/T03), 1:1 AC↔test link kept. AC3 still rewrites the P1-T3 `==0` test to `>=1`.
- **New component in scope:** `src/core/ReferenceRewriter.{h,cpp}` `[new]` (architecture-named). `touches_files` now: `src/core/ReferenceRewriter.h`, `src/core/ReferenceRewriter.cpp`, `src/core/PDFOptimizer.cpp`, `CMakeLists.txt`, `tests/test_corpus_generator.h`, `tests/test_corpus_generator.cpp`, `tests/test_structure_writer.cpp`.
- **`CMakeLists.txt` added to `touches_files` (beyond the four groups named in the instruction):** `CORE_SOURCES` is an explicit list (`CMakeLists.txt:51-62`), so the new `ReferenceRewriter.cpp` will not compile without a one-line add. This mirrors P2-T5's `CmykHandler.cpp` wiring and is required for the task's own tests to build/run. Flagged here for visibility; P2-T5 is `done`, so there is no contention on `CMakeLists.txt`.
- **Fixture:** `tests/test_corpus_generator.*` gains `generateDistinctDuplicateStreams()` — `distinct_duplicate_streams.pdf` staged in `$TMPDIR`, two **distinct** indirect byte-identical image streams, **both actually referenced** (`/Resources /XObject << /Im1 A /Im2 B >>`, both drawn), NOT the reference-shared `transparency.pdf`, NOT added to the canonical 14.
- **Status:** `blocked` → `not_started` (only Build advances it thereafter). **Size:** `M` → `L` (new component + referrer walk + fixture + test rewrite exceeds the 15–45 min M band).
- **Dependencies:** kept `[]`. P2-T6's correctness assumes no codec contract from P2-T1..T5 (the dedup loop is self-contained; all of T1..T5 are `done` anyway), so no edge is warranted. P2-T7 already depends on P2-T6 and stays the phase gate.

### Parallelism note (per the instruction's "consider whether ReferenceRewriter can be built in parallel")
- `ReferenceRewriter.{h,cpp}` are new files with no overlap, so they *could* be built in parallel with another task. But **no other Phase-2 task remains** (P2-T1..T5 are `done`; P2-T7 depends on P2-T6), so there is nothing to parallelize against. Splitting P2-T6 into a standalone `ReferenceRewriter` task would add a second task with an artificial dependency edge and a mid-wave handoff, for zero scheduling benefit, while fragmenting a component that has exactly one consumer. Kept as one task with two AC clusters (mechanism+fixture, then test rewrite) — independently verifiable via `ctest -R StructureWriter`. `PDFOptimizer.cpp` remains the only contention point and no other not-started task touches it.

### Graph validation
- `python3 ~/.config/opencode/scripts/check_tasks.py` → `OK: 7 tasks validated, execution_graph.json written.` Exit 0. Warnings only: P2-T5's and P2-T6's new pure-output files (`CmykHandler.*`, `ReferenceRewriter.*`) are in `touches_files` but not `read_files` (expected — they don't exist yet). 7 tasks, 10 DEPENDS_ON edges, acyclic; every AC has ≥1 matching test; every code task has non-empty `touches_files`; every context pack well-formed.
- `"status": "READY_FOR_BUILD"` confirmed (top-level). P2-T6 is the sole remaining Phase-2 product task; P2-T7 is the phase gate.

## 2026-10-07 — ESC-014 escalation-driven revision: close the Phase-2 task-split gap (ESC-011/012/013/015)

Acting as Task Manager per Orchestrator rework instruction (reviewer_escalation ESC-014; user approved handling). No edits to `docs/requirements/*`, `docs/plan.md`, `docs/architecture.md`, `docs/code_review/escalations.json`, `docs/code_review/review_log.md`, or any `docs/build/results/<id>.json`. Only `docs/tasks/task-graph.json` was edited (plus `execution_graph.json`, regenerated by `check_tasks.py`). Diagnosis was already done by Code Reviewer — not re-diagnosed here; the fixes below map 1:1 to the recorded escalation observations.

### ESC-014 — the gap and the fix (this is the 3rd task-split gap)
- **Cause (per ESC-014, not re-diagnosed):** P2-T7's T03 acceptance ("full ctest suite 100% green") could not be satisfied within P2-T7's footprint (`touches_files: [USAGE.md]`). The failing test lives in `tests/test_corpus_verifier.cpp` (ESC-012) and the contaminating loop in `benchmark/run_benchmark.sh` (ESC-013) — neither file was owned by any Phase-2 task (P2-T1..P2-T7). Re-dispatching P2-T7 therefore could never converge.
- **Recurring pattern (must not repeat):** this is the **third** task-split gap in the project — Phase-0 **ESC-004/ESC-005** (no task owned `src/core/PDFOptimizer.cpp`; no task owned the `tests/test_pdf_optimizer.cpp` expectations), and now Phase-2 **ESC-014** (no task owned the Phase-0 verifier/benchmark artifacts that a phase-gate task's "full suite green" AC depends on). **Root cause: a phase-gate task was given a "whole suite / whole artifact set is green" acceptance without first assigning every gate-relevant file to an owning task.** Future decompositions MUST pre-assign ownership of every file that any gate (ctest, benchmark, qpdf check) touches before writing the gate task's AC — enumerate the gate's inputs (all test files, all scripts it invokes) and confirm each is in some task's `touches_files`, or explicitly carve it out of the gate. A gate AC is only as satisfiable as the narrowest un-owned file it silently depends on.

### New tasks added (all `not_started`; only Build advances status)
- **P2-T8 — ESC-012 + ESC-013 (verifier SSIM gate + benchmark canonical-14).** Sole owner of `tests/test_corpus_verifier.cpp` and `benchmark/run_benchmark.sh`. ACs: (1) the `CorpusGenerator.BenchmarkTsvSsimThresholds` >=0.98 gate applies only to rows whose `Tool == "pdfcompress"` (third-party qpdf/gs/ocrmypdf sub-0.98 rows no longer fail the product verifier); (2) `benchmark/run_benchmark.sh` loops the canonical 14-file list, excluding non-canonical fixtures and test byproducts; (3) full ctest green. `dependencies: []` — the verifier/benchmark scope is independent of product code.
- **P2-T9 — ESC-011 (PNG-under-FlateDecode bug).** Sole owner of `src/core/DecisionEngine.cpp`, `src/codecs/PngCodec.cpp`, `src/codecs/CodecInterface.h` (+ tests `tests/test_codecs.cpp`, `tests/test_pdf_optimizer.cpp`). ACs: the FlateDecode-labelled lossless output is a valid, embeddable zlib/Flate stream (never a full PNG container) that inflates under `/FlateDecode` with `qpdf --check` clean; `m_pngCodec` stays selected for the alpha and MaxQuality-lossless branches (AC-C2 reachability preserved); existing DecisionEngine/CodecTest cases still pass. `dependencies: []`.
- **P2-T10 — ESC-015 (dedup signature alpha-safety).** Sole owner of `src/core/PDFOptimizer.cpp` (+ `tests/test_structure_writer.cpp`, `tests/test_corpus_generator.h/.cpp` for the masked-duplicate fixture). ACs: two byte-identical streams differing only by `/SMask`/`/Mask` are never merged; the signature keys on `/SMask`, `/Mask`, `/Type`, `/Intent`; matching pairs still dedup (`streamsDeduplicated>=1` with defaults, `==0` with `--no-dedup`); P2-T6/P1-T3 dedup tests still pass. `dependencies: []`.

### Decision: ESC-011 is a NEW fix task, not an amendment of P2-T1
- Chose a new task (**P2-T9**) over reopening `done` P2-T1. Rationale: P2-T1's contract is *routing* (which codec is selected); the ESC-011 defect is the *codec output contract* (PngCodec returns a PNG container while the reported filter is FlateDecode). Bundling a codec-output fix into the already-verified routing task would conflate two acceptance clusters, reset a done task, and re-run P2-T1's tests for no reason. This mirrors the Phase-0 precedent (ESC-004 → new `T07-optimizer-dedup-fix` rather than amending T05). P2-T1's ACs/tests/results are untouched; P2-T1-T02 (`PngSelectedForAlpha`) remains valid and is re-run by P2-T9-T02.
- **Mechanism is Build-bounded, not an architecture escalation:** `requirements.md` AC-C2 pins the output filter as FlateDecode and requires `m_pngCodec` reachable; `plan.md`/`architecture.md` name the PNG branch but leave the exact byte layout (plain zlib vs IDAT+predictor) to Build within the AC — as already recorded in the Phase-2 kickoff notes. P2-T9's contracts state the requirement behaviourally (valid zlib stream, never a PNG container) and forbid routing alpha through `ZlibCodec` (which would make `m_pngCodec` unreachable). No ESC-PLAN entry.

### Dependency / wave analysis (no overlapping `touches_files` in any wave)
- `dependencies` encode the *logical* contract: **P2-T7 now depends on P2-T8, P2-T9 and P2-T10** (in addition to P2-T1..P2-T6), because its T03 "full suite green" gate and T04 benchmark-delta AC are only satisfiable once the verifier/benchmark scope (T8) and the product fixes (T9/T10) have landed. These edges are real (gate correctness assumes those tasks' changes), not file-overlap substitutes.
- `touches_files` are disjoint across the three new tasks: T8 = verifier+benchmark script; T9 = DecisionEngine/PngCodec/CodecInterface + codec/optimizer tests; T10 = PDFOptimizer + structure-writer/corpus-generator. `src/core/PDFOptimizer.cpp` remains the phase hot spot and is touched only by T10, so nothing serializes against it.
- **Waves (max_parallel_tasks=4):** Wave A = **P2-T8 + P2-T9 + P2-T10** in parallel (width 3, disjoint footprints, all dependency-free). Wave B = **P2-T7** (the phase gate, now unblocked once T8/T9/T10 are `done`). No wave contains two tasks with overlapping `touches_files`.
- P2-T7 `status` reset `blocked` → `not_started` (Task Manager only ever sets `not_started`; Build advances it thereafter). P2-T7's ACs/tests/footprint (`USAGE.md`) are unchanged — its gate was already correct; only its dependencies were incomplete.

### Graph validation
- `python3 ~/.config/opencode/scripts/check_tasks.py` → `OK: 10 tasks validated, execution_graph.json written.` Exit 0. Warnings only: P2-T5's and P2-T6's pre-existing pure-output files (`CmykHandler.*`, `ReferenceRewriter.*`) in `touches_files` but not `read_files` (expected; untouched by this revision).
- 10 tasks, 13 DEPENDS_ON edges, acyclic; every `dependencies` id exists; every acceptance criterion has >=1 matching test; every code task has non-empty `touches_files`; every context pack well-formed (all `read_files` paths exist). Top-level `"status": "READY_FOR_BUILD"`.
- Open escalations now each have an owning task: ESC-011 → P2-T9, ESC-012/ESC-013 → P2-T8, ESC-014 → this split (P2-T8/P2-T9/P2-T10 + re-opened P2-T7), ESC-015 → P2-T10. `escalations.json` was not edited (Code Reviewer owns it and flips entries on its own re-verification).

## 2026-10-07 — ESC-016 escalation-driven close-out: stale libpng labelling + unused dependency (P2-T11)

Acting as Task Manager per Orchestrator rework instruction (reviewer_escalation ESC-016; user approved fixing it). No edits to `docs/requirements/*`, `docs/plan.md`, `docs/architecture.md`, `docs/code_review/escalations.json`, `docs/code_review/review_log.md`, or any `docs/build/results/<id>.json`. Only `docs/tasks/task-graph.json` was edited (one task appended) plus `execution_graph.json` (regenerated by `check_tasks.py`) and this notes file.

### What was added
- **P2-T11** — "Phase 2 close-out: drop stale libpng labelling and the unused libpng dependency (ESC-016)". `dependencies: []`, `status: not_started`, `size: S`, `task_type: doc`. Sole Phase-2 close-out task.
- **ACs (3):** (1) `PngCodec.h` comment describes a lossless zlib/Flate encoder and `name()` is no longer `"libpng"` (class name retained per AC-C2); (2) USAGE.md's three libpng mentions removed and the PNG/alpha lossless path described via the zlib/Flate encoder; (3) no libpng dependency/build reference remains in `PngCodec.h`/`vcpkg.json`/`CMakeLists.txt` and configure+build+full ctest stay green (62/62).
- **Tests (4, ≥1 per AC):** T01 grep `PngCodec.h` + read comment/`name()`; T02 grep `USAGE.md`; T03 grep the three build files + re-configure/build/full ctest; T04 scoped ctest (`CodecTest|DecisionEngine|PDFOptimizerTest|StructureWriter`) for no regression. T01/T02 are grep-based manual checks; T03/T04 carry the "full ctest still 62/62" gate.

### Deviation from the escalation's stated footprint — CMakeLists.txt (justified)
- ESC-016 states: *"CMakeLists.txt has no libpng reference"* (and `review_log.md:1171` repeats it). **This is factually wrong against the current tree.** Verified by grep:
  - `CMakeLists.txt:16` → `find_package(PNG REQUIRED)`
  - `CMakeLists.txt:76` → `PNG::PNG` (linked into `pdfcompress_core`)
- Therefore libpng is **not trivially removable from `vcpkg.json` alone**: dropping the manifest entry while `find_package(PNG REQUIRED)` remains would fail `cmake` configure (vcpkg manifest mode would no longer install libpng). The instruction's own condition ("check nothing else uses it before removing the manifest entry") is what surfaced this.
- Decision: the task owns **four** files — `src/codecs/PngCodec.h`, `USAGE.md`, `vcpkg.json`, **and `CMakeLists.txt`** — and removes libpng end-to-end (the two CMake lines + the manifest entry). Rationale: this is the minimal set that makes the removal correct and keeps the task's own build/ctest AC satisfiable; without the CMake edit the task would repeat the ESC-014 "gate footprint can't satisfy its own AC" failure mode. `find_package(ZLIB REQUIRED)` (:21) and `ZLIB::ZLIB` (:78) are kept; zlib also remains transitively available via qpdf.
- This is a **scope correction caused by an inaccurate escalation premise**, not scope creep. It is recorded here and in P2-T11's context pack (contract line 7). Code Reviewer is asked to note the corrected premise when re-verifying ESC-016.

### Pre-verification performed before writing the task (evidence, so Build need not re-derive)
- No file under `src/` or `tests/` includes `png.h` or references any `png_*` symbol (P2-T9 already replaced the PngCodec container writer with `<zlib.h>` `compress2`). `PngCodec.cpp` includes only `zlib.h`.
- `.name()` is **not called anywhere** in `src/`, `tools/` or `tests/` → changing the label breaks no test.
- libpng literal occurrences and ownership: `PngCodec.h:7,:10` (P2-T11), `USAGE.md:16,:60,:305` (P2-T11), `vcpkg.json:6` (P2-T11), `CMakeLists.txt:16,:76` (P2-T11); `PngCodec.cpp:14` is an accurate historical comment; `CodecInterface.h:38` uses "libpng" only as a doc-comment example.
- Mechanism is not architectural: P2-T9's switch to zlib was already approved (ESC-011 addressed); this task only updates descriptive text and drops the now-dead build dependency. No ESC-PLAN entry.

### Dependency / wave analysis
- `dependencies: []` is correct: the change is independent of every other task's contracts (no interface/API/schema assumption), and all other Phase-2 tasks are `done`. No `DEPENDS_ON` edge is warranted.
- `touches_files` overlap check: none of the four files is touched by any other **not-started** task (P2-T1..P2-T10 are all `done`, so Build will not serialize against them). `CMakeLists.txt` was last owned by done P2-T5/P2-T6; `USAGE.md` by done P2-T7; `PngCodec.h` and `vcpkg.json` were not owned by any prior Phase-2 task.
- **Wave:** P2-T11 is the only `not_started` task, so the ready frontier is width 1, then the build/ctest gate inside the task itself. No wave contains two tasks with overlapping `touches_files`.

### Flagged observations (not escalated; cosmetic, outside ESC-016's scope — user/Orchestrator may route)
- **Residual stale libpng mentions not owned by P2-T11** (out of ESC-016's declared scope; left untouched):
  - `src/codecs/CodecInterface.h:38` — doc example lists `"libpng"` as a codec name (stale; should read e.g. `"TurboJPEG", "flate", "OpenJPEG"`). Not named by ESC-016; intentionally not widened into this task.
  - `PngCodec.cpp:14` — historical comment naming the old libpng writer; accurate as history, kept.
  - `docs/architecture.md:54,:69,:117,:138,:225,:234`, `docs/requirements/requirements.md:29`, `docs/plan_review.md`, `docs/architecture_proposal.md`, `docs/PENDING_PHASES.md`, and root legacy docs (`software_architecture_specification.md`, `macOS_PDF_Compressor_Implementation_Plan.md`, `current_context.txt`) still list libpng in the stack. These are Validator/Requirements/draft artifacts — not Task-Manager-owned, not edited here. **After P2-T11 lands, `architecture.md`'s "libpng [existing]" rows become stale** (PNG encode is zlib since P2-T9); flag for Validator if a consistency pass is wanted.

### Graph validation
- `python3 ~/.config/opencode/scripts/check_tasks.py` → `OK: 11 tasks validated, execution_graph.json written.` Exit 0. Only the two pre-existing warnings remain (P2-T5/P2-T6 pure-output files in `touches_files` but not `read_files`).
- 11 tasks, 13 DEPENDS_ON edges, acyclic; every `dependencies` id exists; every acceptance criterion has ≥1 matching test; every code task has non-empty `touches_files`; every context pack well-formed (all `read_files` paths exist; `size` = S; `needs_graphify` = false). `execution_graph.json` has 11 Task nodes; P2-T11 has no edges.
- Top-level `"status": "READY_FOR_BUILD"` confirmed.
- Open escalations now each have an owning task: ESC-011 → P2-T9, ESC-012/ESC-013 → P2-T8, ESC-014 → P2-T8/T9/T10, ESC-015 → P2-T10, **ESC-016 → P2-T11**. `escalations.json` was not edited (Code Reviewer owns it and flips entries on its own re-verification).

## 2026-10-07 — P2-T11 NARROWED to a labelling-only fix (libpng dependency RETAINED)

Acting as Task Manager per Orchestrator rework instruction (normal_progression; narrow a scoped task). No edits to `docs/requirements/*`, `docs/plan.md`, `docs/architecture.md`, `docs/code_review/escalations.json`, `docs/code_review/review_log.md`, or any done task's fields/results. Only `docs/tasks/task-graph.json` (P2-T11 rewritten), `execution_graph.json` (regenerated by `check_tasks.py`), and this notes file changed.

### Why the narrowing (cause)
- ESC-016 was approved by the user framed as **COSMETIC** (stale libpng labelling). The previous P2-T11 **overshot** that frame: it removed libpng end-to-end (`PngCodec.h`, `USAGE.md`, `vcpkg.json`, `CMakeLists.txt`).
- Removing libpng is **not cosmetic**. `requirements.md:29` lists libpng in the project's **hard-constraint tech stack** ("Treat the existing tech stack above as a hard constraint — do not suggest replacing it"), and `docs/architecture.md` names libpng in **6** places (codec-layer row, integration rows, tech-stack decision #6/#20, data-model row). Pulling the dependency out therefore forces a **Requirements flag + Validator architecture refresh** (a downstream 2-agent cascade) for what is only a nice-to-have.
- Decision: keep P2-T11 inside its approved cosmetic boundary. Fix the stale **labelling** only; RETAIN the dependency and defer its actual removal as a separate cleanup.

### What changed in P2-T11
- **`touches_files` narrowed**: `["src/codecs/PngCodec.h", "USAGE.md"]` (was `+ vcpkg.json, CMakeLists.txt`). `vcpkg.json` and `CMakeLists.txt` are **explicitly out of footprint**.
- **AC1 / AC2 unchanged** (PngCodec.h comment + `name()`; the three USAGE.md mentions) — these are the cosmetic fix and stay.
- **AC3 rewritten.** Removed the "no libpng dependency or build reference remains in vcpkg.json / CMakeLists.txt" requirement. New AC3: the change is documentation/labelling-only; configure + build + full ctest stay green (**62/62**), **and libpng remains declared in `vcpkg.json` and linked in `CMakeLists.txt`** (intentionally RETAINED, must NOT be removed by this task).
- **Tests updated, 1:1 AC↔test link preserved.** T01/T02 (grep labels: `PngCodec.h`, `USAGE.md`) kept verbatim in intent. T03's grep now targets only `src/codecs/PngCodec.h USAGE.md`, keeps the full-cmake-build + full-ctest 62/62 gate, and **additionally asserts libpng is still present in `vcpkg.json` / `CMakeLists.txt`** (retention check). T04 (scoped ctest no-regression) unchanged. vcpkg/CMake removal assertions deleted.
- **Description / expected_artifacts / inputs / outputs / context** updated to match: `expected_artifacts` now only `PngCodec.h` + `USAGE.md`; contracts state "KEEP `libpng`; this task MUST NOT edit `vcpkg.json` or `CMakeLists.txt`"; a new contract line records the retention rationale; `requirements.md:25-33` added to `read_files` so Build sees the hard-constraint text.

### Rationale recorded for Code Reviewer (for re-checking ESC-016)
- **libpng is genuinely unused but RETAINED.** Since P2-T9, `PngCodec::encode` is a plain zlib/Flate encoder (`<zlib.h> compress2`); no file under `src/` or `tests/` includes `png.h` or references any `png_*` symbol. `CMakeLists.txt:16` (`find_package(PNG REQUIRED)`) and `:76` (`PNG::PNG`) and `vcpkg.json:6` (`"libpng"`) are now **dead** references — but they are retained deliberately.
- **Removal is a flagged-requirements change, not cosmetic.** Because `requirements.md:29` lists libpng in the hard-constraint stack and `architecture.md` names it in 6 places, dropping it requires the Requirements Analyst to author a flagged change plus the Validator to refresh architecture — a 2-agent cascade. That is **deferred as a separate cleanup, explicitly out of ESC-016's scope**.
- **What P2-T11 now does instead** is only stop the code and docs from *claiming* a libpng dependency: `PngCodec.h` comment + `name()` and the three `USAGE.md` mentions move to zlib/Flate wording. This is the whole of the approved cosmetic fix.
- Code Reviewer is asked to note: (a) the earlier P2-T11's CMake-footprint deviation (recorded in the prior notes section) is **superseded** by this narrowing; (b) `vcpkg.json`/`CMakeLists.txt` libpng references are **expected to remain** after P2-T11 — their presence is correct, not a defect.

### Residual stale libpng mentions (unchanged from the prior notes; not P2-T11's scope)
- `src/codecs/CodecInterface.h:38` — doc-comment example still lists `"libpng"` (stale, out of ESC-016 scope).
- `src/codecs/PngCodec.cpp:14` — historical comment naming the old libpng writer (accurate as history; kept).
- `docs/architecture.md`, `docs/requirements/requirements.md:29`, and root legacy docs still list libpng in the stack — Validator/Requirements-owned. Since the dependency is now RETAINED, `architecture.md`'s "libpng [existing]" rows remain true for the dependency (only the code-behaviour wording is stale). No Validator pass requested by this narrowing.

### Dependency / wave analysis
- `dependencies: []` unchanged and still correct: the change is independent of every other task's contracts, and all other Phase-2 tasks are `done`.
- No overlap: neither `src/codecs/PngCodec.h` nor `USAGE.md` is touched by any other **not-started** task. P2-T11 remains the only `not_started` task, so the ready frontier is width 1.
- `size: S`; `needs_graphify: false`.

### Graph validation
- `python3 ~/.config/opencode/scripts/check_tasks.py` → `OK: 11 tasks validated, execution_graph.json written.` Exit 0. Only the two pre-existing warnings remain (P2-T5/P2-T6 pure-output files in `touches_files` but not `read_files`).
- 11 tasks, 13 DEPENDS_ON edges, acyclic; every acceptance criterion has ≥1 matching test; every code task has non-empty `touches_files`; every context pack well-formed.
- Top-level `"status": "READY_FOR_BUILD"` confirmed. Open escalations still each have an owning task: ESC-011 → P2-T9, ESC-012/ESC-013 → P2-T8, ESC-014 → P2-T8/T9/T10, ESC-015 → P2-T10, **ESC-016 → P2-T11** (now cosmetic-only). `escalations.json` was not edited (Code Reviewer owns it and flips entries on its own re-verification).

## 2026-10-07 — ESC-017 escalation-driven close-out: stale 'libpng' codec-name example (P2-T12)

Acting as Task Manager per Orchestrator rework instruction (reviewer_escalation ESC-017; user approved handling). No edits to `docs/requirements/*`, `docs/plan.md`, `docs/architecture.md`, `docs/code_review/escalations.json`, `docs/code_review/review_log.md`, or any `done` task's fields/results. Only `docs/tasks/task-graph.json` was edited (one task appended), plus `execution_graph.json` (regenerated by `check_tasks.py`) and this notes file.

### What was added
- **P2-T12** — "Phase 2 close-out residual: refresh stale 'libpng' codec-name example in `CodecInterface.h` name() doc comment (ESC-017)". `dependencies: []`, `status: not_started`, `size: S`, `task_type: doc`, `needs_graphify: false`.
- **Sole footprint:** `touches_files: ["src/codecs/CodecInterface.h"]` — the one file ESC-017 names. No other file (vcpkg.json / CMakeLists.txt / PngCodec.h / USAGE.md) is touched; the libpng build dependency stays RETAINED per the P2-T11 narrowing.
- **ACs (2):** (1) `src/codecs/CodecInterface.h:38` no longer contains `"libpng"` and its examples list only codec-name strings actual codecs return (`"TurboJPEG"`, `"OpenJPEG"`, `"zlib"`/`"flate"`); (2) documentation/comment-only change — build + full ctest stay green (**62/62**), no codec-behaviour/interface/class-name change.
- **Tests (3, ≥1 per AC):** T01 `grep -n 'libpng' src/codecs/CodecInterface.h` + read the comment (manual); T02 full `ctest --test-dir build --output-on-failure` (62/62 gate); T03 scoped `ctest -R CodecTest` to confirm the codec suite is unaffected. 1:1 AC↔test link preserved.

### Why a new task rather than amending P2-T11
- P2-T11 (`done`) deliberately declared `CodecInterface.h:38` **out of its footprint** as a residual stale mention (notes 2026-10-07, "Residual stale libpng mentions"). ESC-017 is exactly that residual, surfaced separately by Code Reviewer. Adding P2-T12 keeps P2-T11's verified scope intact (no `done` task reset) and gives the one remaining file its own owner — the same "new task, don't reopen a done task" precedent used for **P2-T9** (ESC-011) and Phase-0 **T07** (ESC-004).

### Dependency / wave analysis
- `dependencies: []` is correct: a comment-only edit assumes no interface/contract/schema from any other task, and every other Phase-2 task is `done`. No `DEPENDS_ON` edge is warranted (an unnecessary edge would only remove parallelism).
- `touches_files` overlap: `src/codecs/CodecInterface.h` is touched by no other **not-started** task (`P2-T1..P2-T11` are all `done`; P2-T9 historically listed `CodecInterface.h` "only if a defaulted flag was needed", but it is `done`, so nothing serializes).
- **Wave:** P2-T12 is the only `not_started` task → ready frontier width 1. No wave contains two tasks with overlapping `touches_files`.

### Relationship to ESC-017
- ESC-017 is `routed_to: BUILD`, severity `nice-to-have`, `status: open`; the user approved handling. The graph now has an **owning task** for it (ESC-017 → P2-T12). `escalations.json` was **not** edited — Code Reviewer owns it and flips entries on its own re-verification.
- Scope kept strictly to the escalation's cosmetic ask ("Documentation-only; no behaviour, test or architecture impact. Do not rename the PngCodec class per AC-C2"): one doc-comment token in one header.

### Graph validation
- `python3 ~/.config/opencode/scripts/check_tasks.py` → `OK: 12 tasks validated, execution_graph.json written.` Exit 0. Only the two pre-existing warnings remain (P2-T5/P2-T6 pure-output files in `touches_files` but not `read_files`). **No warning for P2-T12** (`CodecInterface.h` is in `read_files`).
- 12 tasks, 13 DEPENDS_ON edges (unchanged; P2-T12 has no edges), acyclic; every `dependencies` id exists; every acceptance criterion has ≥1 matching test; every code/doc task has non-empty `touches_files`; every context pack well-formed (all `read_files` paths exist; `size` = S; `needs_graphify` = false). Top-level `"status": "READY_FOR_BUILD"` confirmed.
