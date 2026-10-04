# Code Review Escalations — Phase 0, Waves 1–4 (2026-10-04)

## ESC-001 — PDFOptimizer dedup `replaceObject` with indirect handle (T05-T04)
- Linked task: T05-test-verifier (failing test T05-T04,
  GTest `CorpusGenerator.StrippingRegressionSizeGateWithExemption`)
- Routed to: BUILD
- Category: bug — Severity: blocking
- Observed vs expected: Optimizing the new `transparency.pdf` (first corpus PDF with a
  byte-identical duplicate stream pair) returns `success=false` with
  `QPDF::replaceObject called with indirect object handle`, thrown at
  `src/core/PDFOptimizer.cpp:367` (`pdf.replaceObject(obj.getObjGen(), it->second)`).
  Expected per AC7 + T05 criterion 4: Balanced optimize of any corpus PDF succeeds
  (and the verifier's `EXPECT_TRUE(result.success)` holds). The replacement handle
  `it->second` is indirect by construction (loop stores only indirect streams, line
  338/361); QPDF requires a direct replacement object. 13/13 other corpus files
  optimize fine. Full reproduction + code evidence in `review_log.md` entry F-001
  and `docs/build/logs/T05-test-verifier.md` ("Blocking issue"). Verifier file needs
  no change — it passes unmodified once the optimizer is fixed.
- Suggested direction (non-binding): pass a direct copy of `it->second` as the
  replacement, or skip pairs whose replacement is indirect; keep the `objGen !=`
  guard. Minimal crash fix restoring AC7, not a decision-logic change (see F-001 for
  the AC5 interaction).
- Status: addressed
- Verified: Wave 7 re-verified fixed by reviewer 2026-10-04 — `src/core/PDFOptimizer.cpp:372-382` holds the guard (`it->second.isIndirect()` skip + `try/catch`, `objGen !=` retained line 366); reviewer-ran `DYLD_LIBRARY_PATH=build ./build/run_optimize transparency.pdf` prints `=== Optimization Successful ===` (no `indirect object handle`), `qpdf --check` clean, and full `ctest` is `100% tests passed, 0 failed out of 33` (1 allowed skip); `build_log.md` wave 7 records T07/T05 `done`.

## ESC-002 — Corpus `photo_jpeg` uses FlateDecode, not AC3-specified DCTDecode
- Linked task: T01-corpus-hardening (done; gap its tests did not catch)
- Routed to: BUILD
- Category: gap — Severity: should-fix
- Observed vs expected: `photo_jpeg.pdf` is generated `/FlateDecode`
  (`tests/test_corpus_generator.cpp:98`), but AC3 item 2 requires 200×150 DeviceRGB
  **DCTDecode** exercising the JPEG path. No corpus PDF now exercises the optimizer's
  `/DCTDecode` → TurboJPEG branch (`PDFOptimizer.cpp:244-258`). Detail in
  `review_log.md` entry F-002 (B1).
- Suggested direction (non-binding): generate valid JPEG bytes for the 200×150 photo
  (e.g. TurboJPEG compress in the generator) and keep `/DCTDecode`; or record a locked
  deviation — do not silently keep Flate.
- Status: addressed
- Verified: Wave 5 T01 rework verified fixed by reviewer 2026-10-04 — `grep` shows `turbojpeg.h`/`tjCompress2` in `tests/test_corpus_generator.cpp`, live `qpdf --json photo_jpeg.pdf` contains `DCTDecode`+`DeviceRGB`, raw bytes contain `FFD8FF`, `qpdf --check` clean, and reviewer-ran `run_optimize photo_jpeg.pdf` reports `Optimization Successful (1 optimized)` exercising the DCT branch; `build_log.md` wave 5 records T01 `done` 13/13 plus ESC-002 checks.

## ESC-003 — Stray benchmark TSV / corpus_info byproducts in repo root
- Linked task: T04-render-diff-build (done; hygiene its tests did not gate)
- Routed to: BUILD
- Category: quality — Severity: nice-to-have
- Observed vs expected: untracked `benchmark_results_202*.tsv` (root + `build/`) and
  `corpus_info*.txt` left in the tree (`git status` `??`), which will pollute the T06
  `git diff --stat` gate and TSV newest-file discovery. Expected: removed or ignored.
  Detail in `review_log.md` entry F-003 (B4).
- Suggested direction (non-binding): delete strays; add `benchmark_results_*.tsv` and
  `corpus_info*.txt` to `.gitignore`.
- Status: addressed
- Verified: Wave 5 T04 rework verified fixed by reviewer 2026-10-04 — `.gitignore` now ends with `benchmark_results_*.tsv` + `corpus_info*.txt` (ESC-003 comment), `git check-ignore -v` hits both patterns, and reviewer-ran `git status --short` lists no `?? benchmark_results_*` / `?? corpus_info*`; `build_log.md` wave 5 records T04 `done` 5/5.

## ESC-004 — No task owns `src/core/PDFOptimizer.cpp`, so ESC-001 has no fixer (T05-T04)
- Linked task: T05-test-verifier (status `blocked`; failing test T05-T04, GTest `CorpusGenerator.StrippingRegressionSizeGateWithExemption`; see also ESC-001 which stays open)
- Routed to: TASK_MANAGER
- Category: gap (task-breakdown gap) — Severity: blocking
- Observed vs expected: ESC-001's minimal dedup crash fix lives in `src/core/PDFOptimizer.cpp` (~lines 359-370), but no task in `docs/tasks/task-graph.json` lists that file in `touches_files` — verified: T01 `tests/test_corpus_generator.*`, T02 `benchmark/run_benchmark.sh`, T03 `tools/render_diff.cpp`, T04 `CMakeLists.txt`+`benchmark/run_benchmark.sh`, T05 `tests/test_corpus_verifier.cpp`, T06 `CMakeLists.txt`. T05's wave-5 worker correctly refused the out-of-footprint edit twice, so re-dispatching T05 can never resolve ESC-001; Build has no dispatchable task for the fix. Expected: a task (new or amended) whose footprint includes `src/core/PDFOptimizer.cpp` for the minimal crash fix only. Complication to reconcile: T06-T04's `git diff --stat` gate currently requires zero diff on `src/core/PDFOptimizer.cpp` (AC5 "no changes to decision logic") — the crash fix will violate that gate as written, so the gate wording needs a carve-out for the crash fix (restoring AC7 is not a decision-logic change; see `review_log.md` F-001/R-001). Architecture and requirements need no change: the dedup-by-hash design is sound and AC7 already demands success. Detail in `review_log.md` entry R-001/R-004.
- Suggested direction (non-binding): add a small product-fix task owning `src/core/PDFOptimizer.cpp` limited to the ESC-001 minimal fix (direct copy or skip-indirect, keep `objGen !=` guard), and amend the T06 gate to allow that diff; do not bundle broader optimizer changes.
- Status: addressed
- Verified: Task Manager added T07-optimizer-dedup-fix (owns `src/core/PDFOptimizer.cpp`) + T08-test-expectations-fix (owns `tests/test_pdf_optimizer.cpp`), wired `T05→T07` (`T05.dependencies` now includes `T07-optimizer-dedup-fix`) and `T06→T07/T08`, and amended T06-T04 gate with the ESC-001 carve-out (`git diff src/core/PDFOptimizer.cpp | grep -q 'isIndirect'`, dedup-block-only); `task-graph.json` confirms all three, `build_log.md` wave 7 records T07/T08 `done`.

## ESC-005 — T06-T04 diff guard fails on pre-existing foreign tree mods (no task owns them)
- Linked task: T06-ctest-wiring-gate (status `blocked`; failing test T06-T04, `git diff --stat` guard)
- Routed to: TASK_MANAGER
- Category: gap (task-breakdown / gate-scope gap) — Severity: blocking
- Observed vs expected: T06-T01/T06-T02/T06-T03 PASS (reviewer-confirmed wiring greps + `ctest 100% tests passed, 0 failed out of 33`); T06-T04 FAILS because the working tree holds pre-existing uncommitted modifications outside every Phase-0 task footprint: `src/core/PDFOptimizer.h`, `src/main.cpp`, `tools/run_optimize.cpp`, `vcpkg.json` (plus `.gitignore`, whose ESC-003 hygiene edit was authorized but is still absent from the gate's allowed-paths list). Expected per amended T06-T04: diff only in `tests/`, `benchmark/`, `tools/render_diff.cpp`, `CMakeLists.txt`, `docs/` + the ESC-001 dedup carve-out. The T06 worker correctly wired the verifier (one-line CMake addition), kept all assertions intact, and refused to revert or silently absorb the foreign files. Note on scope: `git diff src/core/PDFOptimizer.cpp` vs master is ~286 lines (full `OptimizationOptions`/per-item-strip refactor), not just the T07 dedup block — the refactor predates Phase 0 tasks and Appendix A already treats per-item stripping as "FIXED on master", so the gate's `master` baseline is stale for this tree. Detail in `review_log.md` entry W7-004 and `docs/build/logs/T06-direct-fix.md`.
- Suggested direction (non-binding): decide explicitly — commit the in-flight product work, stash it out of the Phase-0 tree, or amend the T06-T04 allowed-paths/baseline (e.g. carve-out the known files or re-baseline vs the current tree) — do NOT silently absorb unrelated in-flight work into Phase 0 to make the gate pass.
- Status: addressed
- Verified: Verify-and-close pass 2026-10-04 — user chose commit-over-stash; local commits 67a54b9 (baseline: stripping options, CLI flags, GUI picker, gtest, incl. ESC-001 guard) + 9441c8c (Phase-0: corpus, benchmark, render-diff, verifier, CTest wiring, .gitignore hygiene) now hold every alleged-foreign file (each load-bearing per stash-rejection evidence). Reviewer re-ran `ctest`: `100% tests passed, 0 failed out of 33` (1 allowed skip); guard present `src/core/PDFOptimizer.cpp:373-382`; `git diff 34e6301..HEAD -- src/codecs/` empty; gtest/no-catch2 confirmed; worktree `git diff --name-only` clean except orchestrator-owned `docs/orchestrator_state.json` (+ personal untracked `.graphifyignore`, `current_context.txt`, `graphify-out.old/`). Substance resolved. Follow-on gate-wording gap filed as ESC-006 (not re-diagnosed here).

## ESC-006 — T06-T04 verbatim `git diff` carve-out grep is vacuous post-commit (gate-wording gap)
- Linked task: T06-ctest-wiring-gate (status `blocked`; failing test T06-T04, `git diff --stat` guard)
- Routed to: TASK_MANAGER
- Category: gap (task-breakdown / gate-wording gap) — Severity: blocking
- Observed vs expected: T06-T04's expected_result requires `git diff src/core/PDFOptimizer.cpp | grep -q 'isIndirect'` — i.e. the ESC-001 guard visible as an *uncommitted* worktree diff. Post-commit (67a54b9 holds the guard at `src/core/PDFOptimizer.cpp:373-382`, verified present by reviewer) the worktree diff is empty, so that grep leg now FAILS vacuously: the guard exists in the file but not in `git diff` output. Likewise the allowed-paths leg (`git diff --name-only | grep -qvE ...`) now passes trivially. The substance ESC-005 complained about (foreign tree mods) is resolved by the commits; what remains is purely wording: the gate pins the carve-out to `git diff` output instead of committed file content. T06's own wiring work is correct (T06-T01/T06-T02/T06-T03 PASS, reviewer re-ran full `ctest` green 33/33) — no Build/Plan/Requirements defect.
- Suggested direction (non-binding): reword T06-T04's carve-out leg to grep file content (e.g. `grep -q 'isIndirect' src/core/PDFOptimizer.cpp` plus a dedup-block-scope check) instead of `git diff` output; keep the `git diff src/codecs/` empty leg and gtest/no-catch2 legs as-is. Do not weaken the gate — pin it to the committed guard.
- Status: addressed
- Verified: Direct close-out 2026-10-04 (user-authorized, no agents) — T06 AC4/T06-T04 reworded to tree-content checks; all legs executed literally and PASS (no modified tracked files outside docs/, no stray untracked, guard in file, codecs untouched, gtest/no-catch2); ctest 100% 0-failed/33. Gate substance satisfied; verbatim diff-grep retired as vacuous post-commit.