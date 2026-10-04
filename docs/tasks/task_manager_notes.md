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
