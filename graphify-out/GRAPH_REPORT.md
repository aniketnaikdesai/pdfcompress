# Graph Report - pdfcompress  (2026-10-03)

## Corpus Check
- 60 files · ~121,255 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 7 file(s) not represented in the graph (top: (none) 6, .cmake 1)

## Summary
- 404 nodes · 685 edges · 20 communities (19 shown, 1 thin omitted)
- Extraction: 88% EXTRACTED · 12% INFERRED · 0% AMBIGUOUS · INFERRED: 82 edges (avg confidence: 0.83)
- Token cost: 14,000 input · 3,200 output

## Community Hubs (Navigation)
- Corpus Verifier Tests
- GUI and Drag-Drop
- Image Metadata Model
- Decision Engine and Optimizer Tests
- Core Engine Wiring
- Image Analysis Engine
- PDF Optimization Core
- Unit Tests Suite
- PDF Inspection (PDFium)
- Optimization Results Model
- JPEG 2000 Codec
- Codec Interface and Zlib
- PNG Codec
- Architecture and Pipeline Docs
- Requirements and Review Docs
- Corpus Generation Tools
- Compression Params
- JPEG Codec Wiring
- Build Dependencies
- Benchmark Script

## God Nodes (most connected - your core abstractions)
1. `TEST()` - 44 edges
2. `ImageMetadata` - 27 edges
3. `TestCorpusGenerator` - 27 edges
4. `OptimizationResult` - 21 edges
5. `generateAll` - 18 edges
6. `OptimizationOptions` - 16 edges
7. `DecisionEngine` - 13 edges
8. `AnalysisResult` - 13 edges
9. `analyze` - 13 edges
10. `PDFDocumentInfo` - 13 edges

## Surprising Connections (you probably didn't know these)
- `Phase 4 Compression Decision Engine` --semantically_similar_to--> `Image Classification Heuristics (photo/screenshot/line art/scanned text/monochrome)`  [INFERRED] [semantically similar]
  macOS_PDF_Compressor_Implementation_Plan.md → USAGE.md
- `Phase 2 PDF Inspection (enumerate pages, extract images, metadata)` --semantically_similar_to--> `PDFium Wrapper (Inspection and Rendering)`  [INFERRED] [semantically similar]
  macOS_PDF_Compressor_Implementation_Plan.md → software_architecture_specification.md
- `main()` --calls--> `generateAll`  [INFERRED]
  benchmark/generate_corpus.cpp → tests/test_corpus_generator.h
- `TEST()` --calls--> `encode`  [INFERRED]
  tests/test_codecs.cpp → src/codecs/JpegCodec.h
- `optimizeBalancedChecked()` --calls--> `optimize`  [INFERRED]
  tests/test_corpus_verifier.cpp → src/core/PDFOptimizer.h

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Full Optimization Flow (Inspect -> Analyze -> Decide -> Encode -> Write)** — usage_optimization_pipeline, software_architecture_specification_compression_controller, software_architecture_specification_pdfium_wrapper, software_architecture_specification_qpdf_wrapper, software_architecture_specification_codec_layer [EXTRACTED 0.95]
- **Phase 0 Baseline (verify, corpus-14, benchmark, render-diff, gate)** — docs_requirements_requirements_phase0_baseline, docs_plan_notes_rg_001_14_files, docs_plan_notes_rg_002_small_pdf_exemption, docs_tasks_task_manager_notes_task_split [INFERRED 0.85]

## Communities (20 total, 1 thin omitted)

### Community 0 - "Corpus Verifier Tests"
Cohesion: 0.09
Nodes (54): BenchmarkTsvSsimThresholds, CmykImageHasDeviceCmykFlateDecode, CmykImageSkippedWithoutTranscoding, CorpusGenerator, EncryptedIsPasswordProtected, EncryptedWithoutPasswordFailsSafely, EncryptedWithPasswordOptimizesToUnencrypted, FileCountIs14 (+46 more)

### Community 1 - "GUI and Drag-Drop"
Cohesion: 0.06
Nodes (39): DropOverlay, qapplication, qcheckbox, qcombobox, qdragenterevent, qdragleaveevent, qdragmoveevent, qdropevent (+31 more)

### Community 2 - "Image Metadata Model"
Cohesion: 0.06
Nodes (35): iomanip, colorSpaceToString(), string, ColorSpace, ImageClassification, StreamFilter, string, ImageMetadata (+27 more)

### Community 3 - "Decision Engine and Optimizer Tests"
Cohesion: 0.07
Nodes (32): AnnotationsStripped, BookmarksStripped, CmykSkipped, PDFOptimizerTest, DecisionEngine, m_jp2Codec, m_jpegCodec, m_pngCodec (+24 more)

### Community 4 - "Core Engine Wiring"
Cohesion: 0.15
Nodes (18): algorithm, cstdint, functional, memory, string, vector, fpdf_document_t__, fpdf_page_t__ (+10 more)

### Community 5 - "Image Analysis Engine"
Cohesion: 0.14
Nodes (23): array, AnalysisResult, classification, classificationName, compressionRatio, edgeDensity, entropy, flatRegionRatio (+15 more)

### Community 6 - "PDF Optimization Core"
Cohesion: 0.13
Nodes (17): cstdlib, filesystem, fstream, map, pdfoptimizer, pl_flate, QPDF, QPDFObjectHandle (+9 more)

### Community 7 - "Unit Tests Suite"
Cohesion: 0.09
Nodes (20): CodecTest, DecisionEngineTest, gtest, imageanalyzer, ImageAnalyzerTest, jp2codec, jpegcodec, JpegEncode (+12 more)

### Community 8 - "PDF Inspection (PDFium)"
Cohesion: 0.16
Nodes (18): cmath, FPDF_DOCUMENT, fpdf_edit, FPDF_PAGE, fpdfview, InspectionProgressCallback, ColorSpace, StreamFilter (+10 more)

### Community 9 - "Optimization Results Model"
Cohesion: 0.11
Nodes (19): string, OptimizationResult, annotationsRemoved, bookmarksRemoved, errorMessage, formsRemoved, imageBytesSaved, imagesKeptOriginal (+11 more)

### Community 10 - "JPEG 2000 Codec"
Cohesion: 0.16
Nodes (12): openjpeg, OPJ_BOOL, OPJ_OFF_T, OPJ_SIZE_T, vector, Jp2Codec, encode, Jp2StreamData (+4 more)

### Community 11 - "Codec Interface and Zlib"
Cohesion: 0.17
Nodes (9): cstring, ImageCodec, encode, name, vector, string, ZlibCodec, encode (+1 more)

### Community 12 - "PNG Codec"
Cohesion: 0.18
Nodes (10): png, png_bytep, png_size_t, png_structp, vector, custom_png_flush(), custom_png_write_data(), string (+2 more)

### Community 13 - "Architecture and Pipeline Docs"
Cohesion: 0.17
Nodes (12): CMake Build System (vcpkg toolchain, Qt6, PDFium fetch, GTest, render_diff), ESC-001: PDFOptimizer dedup replaceObject with indirect handle (blocking, fixed), ESC-005: foreign tree mods gate gap (resolved via commit-over-stash), ESC-006: T06-T04 diff-grep vacuous post-commit (reword to file-content grep), Design Gap: PNG codec implemented but never selected by DecisionEngine, Task Split Fix: T07 optimizer dedup fix + T08 test expectations (ESC-004), Phase 2 PDF Inspection (enumerate pages, extract images, metadata), Codec Layer (libjxl, OpenJPEG, libpng, jbig2enc, libdeflate) (+4 more)

### Community 14 - "Requirements and Review Docs"
Cohesion: 0.18
Nodes (11): Drag-and-Drop Batch Processing (DropHandler/DropOverlay, buffer fixes), ESC-002: photo_jpeg FlateDecode vs AC3 DCTDecode (fixed via TurboJPEG bytes), RG-001: exactly 14 corpus files (transparency embeds SMask + shared XObject), RG-002: under-2KB PDFs exempt from size gate, Phase 0 Verification and Baseline (tests, corpus, benchmark, no behavior change), Verification Report (11 brief claims checked RIGHT/WRONG/PARTIAL with file:line), Phase 4 Compression Decision Engine, Compression Profiles (Max Quality / Balanced / Max Compression + JPEG slider) (+3 more)

### Community 15 - "Corpus Generation Tools"
Cohesion: 0.25
Nodes (5): main(), iostream, stdexcept, test_corpus_generator, turbojpeg

### Community 16 - "Compression Params"
Cohesion: 0.22
Nodes (9): CompressionParams, channels, height, profile, qualityHint, width, CompressionProfile, vector (+1 more)

### Community 17 - "JPEG Codec Wiring"
Cohesion: 0.29
Nodes (4): string, JpegCodec, CompressionProfile, DecisionEngine::DecisionEngine()

### Community 18 - "Build Dependencies"
Cohesion: 0.50
Nodes (3): dependencies, name, version-string

## Knowledge Gaps
- **93 isolated node(s):** `public`, `filesDropped`, `m_overlay`, `public`, `width` (+88 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 207 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **1 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `optimize` connect `Decision Engine and Optimizer Tests` to `Corpus Verifier Tests`, `Core Engine Wiring`, `Image Analysis Engine`, `PDF Optimization Core`, `Optimization Results Model`?**
  _High betweenness centrality (0.153) - this node is a cross-community bridge._
- **Why does `TEST()` connect `Corpus Verifier Tests` to `Decision Engine and Optimizer Tests`, `PDF Optimization Core`?**
  _High betweenness centrality (0.134) - this node is a cross-community bridge._
- **Why does `ImageMetadata` connect `Image Metadata Model` to `PDF Inspection (PDFium)`, `Core Engine Wiring`, `Image Analysis Engine`?**
  _High betweenness centrality (0.110) - this node is a cross-community bridge._
- **Are the 17 inferred relationships involving `TEST()` (e.g. with `optimize` and `generateAll`) actually correct?**
  _`TEST()` has 17 INFERRED edges - model-reasoned connections that need verification._
- **What connects `public`, `filesDropped`, `m_overlay` to the rest of the system?**
  _93 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Corpus Verifier Tests` be split into smaller, more focused modules?**
  _Cohesion score 0.08897243107769423 - nodes in this community are weakly interconnected._
- **Should `GUI and Drag-Drop` be split into smaller, more focused modules?**
  _Cohesion score 0.06236786469344609 - nodes in this community are weakly interconnected._