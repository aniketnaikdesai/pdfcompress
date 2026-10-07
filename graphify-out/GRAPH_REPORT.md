# Graph Report - pdfcompress  (2026-10-07)

## Corpus Check
- 46 files · ~93,417 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 522 nodes · 871 edges · 35 communities (27 shown, 8 thin omitted)
- Extraction: 89% EXTRACTED · 11% INFERRED · 0% AMBIGUOUS · INFERRED: 99 edges (avg confidence: 0.83)
- Token cost: 9,000 input · 2,200 output

## Community Hubs (Navigation)
- Corpus Verifier Tests
- GUI and Drag-Drop
- Decision Engine Tests
- Stripping Profiles and Optimizer
- Corpus Generation Tools
- Architecture and Phase Docs
- Optimizer Tests (CMYK/Alpha)
- Image Analysis Engine
- Optimization Options and Defaults
- CmykHandler
- PDF Inspection (PDFium)
- Image Metadata Model
- PDFDocumentInfo
- JPEG 2000 Codec
- Render-Diff Tool
- Core Engine Wiring
- Compression Params
- Codec Tests
- Image Analyzer Tests
- Zlib Codec
- JPEG Codec
- PNG Codec
- ImageMetadata Helpers
- Requirements Docs
- Codec Interface
- Community 25
- Community 26
- Community 27
- Community 28
- Community 29
- Community 30
- Community 31
- Community 32
- Community 33
- Community 34

## God Nodes (most connected - your core abstractions)
1. `TEST()` - 44 edges
2. `TEST()` - 31 edges
3. `TestCorpusGenerator` - 29 edges
4. `OptimizationResult` - 25 edges
5. `ImageMetadata` - 24 edges
6. `generateAll` - 18 edges
7. `OptimizationOptions` - 17 edges
8. `optimize` - 17 edges
9. `TEST()` - 17 edges
10. `CompressionParams` - 16 edges

## Surprising Connections (you probably didn't know these)
- `Phase 2 PDF Inspection (enumerate pages, extract images, metadata)` --semantically_similar_to--> `PDFium Wrapper (Inspection and Rendering)`  [INFERRED] [semantically similar]
  macOS_PDF_Compressor_Implementation_Plan.md → software_architecture_specification.md
- `Phase 4 Compression Decision Engine` --semantically_similar_to--> `Image Classification Heuristics (5 classes)`  [INFERRED] [semantically similar]
  macOS_PDF_Compressor_Implementation_Plan.md → USAGE.md
- `TEST()` --calls--> `encode`  [INFERRED]
  tests/test_codecs.cpp → src/codecs/JpegCodec.h
- `TEST()` --calls--> `compress`  [INFERRED]
  tests/test_pdf_optimizer.cpp → src/core/DecisionEngine.h
- `optimizeBalancedChecked()` --calls--> `optimize`  [INFERRED]
  tests/test_corpus_verifier.cpp → src/core/PDFOptimizer.h

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Full Optimization Flow (Inspect -> Analyze -> Decide -> Encode -> Write)** — usage_optimization_pipeline, software_architecture_specification_compression_controller, software_architecture_specification_pdfium_wrapper, software_architecture_specification_qpdf_wrapper, software_architecture_specification_codec_layer [EXTRACTED 0.95]
- **Phase 2 codec correctness (slider, PNG, CMYK, dedup)** — usage_slider_scoping, usage_cmyk_transcode, usage_stream_dedup, docs_architecture_reference_rewriter, docs_architecture_cmyk_handler [INFERRED 0.85]

## Communities (35 total, 8 thin omitted)

### Community 0 - "Corpus Verifier Tests"
Cohesion: 0.07
Nodes (64): AllOutputsPassQpdfCheck, main(), BenchmarkTsvSsimThresholds, CmykImageHasDeviceCmykFlateDecode, CmykImageSkippedWithoutTranscoding, CorpusGenerator, DedupCountedWithDefaultsZeroWithout, DefaultOutputNotLargerThanNoOpt (+56 more)

### Community 1 - "GUI and Drag-Drop"
Cohesion: 0.05
Nodes (42): DropOverlay, qapplication, qcheckbox, qcombobox, qdragenterevent, qdragleaveevent, qdragmoveevent, qdropevent (+34 more)

### Community 2 - "Decision Engine Tests"
Cohesion: 0.05
Nodes (40): DecisionEngineTest, MaxQualityLosslessUsesPng, MonochromeUsesZlib, PhotoUsesJpegInBalanced, PngSelectedForAlpha, SliderOnlyAffectsLossyClasses, DecisionEngine, m_jp2Codec (+32 more)

### Community 3 - "Stripping Profiles and Optimizer"
Cohesion: 0.09
Nodes (28): BalancedRemovesOnlyJsAndMetadata, ExplicitFlagWinsOverProfileDefault, filesystem, fstream, GoToOpenActionSurvivesAllProfiles, gtest, MaxCompressionRemovesSomethingEverywhere, MaxQualityRemovesNothing (+20 more)

### Community 4 - "Corpus Generation Tools"
Cohesion: 0.09
Nodes (21): cstddef, cstdlib, iostream, map, pl_flate, QPDF, QPDFObjectHandle, QPDFObjGen (+13 more)

### Community 5 - "Architecture and Phase Docs"
Cohesion: 0.08
Nodes (27): CMake Build System (vcpkg toolchain, Qt6, PDFium fetch, GTest, render_diff), CmykHandler (ICC/DeviceCMYK preserve + Adobe APP14 + opt-in transcode), PngCodec emits valid zlib stream under /FlateDecode (ESC-011 fix), ReferenceRewriter (cycle-safe referrer repoint for dedup), ESC-001: PDFOptimizer dedup replaceObject with indirect handle (blocking, fixed), ESC-002: photo_jpeg FlateDecode vs AC3 DCTDecode (fixed via TurboJPEG bytes), ESC-005: foreign tree mods gate gap (resolved via commit-over-stash), ESC-006: T06-T04 diff-grep vacuous post-commit (reword to file-content grep) (+19 more)

### Community 6 - "Optimizer Tests (CMYK/Alpha)"
Cohesion: 0.10
Nodes (25): AlphaLosslessReencodeIsEmbeddableFlate, AnnotationsStripped, BitPackedImageKeptOriginalAndValid, BookmarksStripped, CmykImagePreservedUnderAllProfiles, CmykPreservationReportedAndLogged, CmykSkipped, CmykSkippedOutputIsValid (+17 more)

### Community 7 - "Image Analysis Engine"
Cohesion: 0.11
Nodes (25): array, ImageClassification, ImageMetadata, AnalysisResult, classification, classificationName, compressionRatio, edgeDensity (+17 more)

### Community 8 - "Optimization Options and Defaults"
Cohesion: 0.10
Nodes (20): BalancedStripsJsAndMetadataOnly, MaxCompressionStripsEverything, MaxQualityStripsNothing, ProfileDefaults, CompressionProfile, OptimizationOptions, deduplicateStreams, linearize (+12 more)

### Community 9 - "CmykHandler"
Cohesion: 0.15
Nodes (19): CmykHandler, hasAdobeApp14, kMaxDeltaE, sampleDeltaE, transcodeToRgb, size_t, DeltaEStats, maxDeltaE (+11 more)

### Community 10 - "PDF Inspection (PDFium)"
Cohesion: 0.16
Nodes (18): cmath, FPDF_DOCUMENT, fpdf_edit, FPDF_PAGE, fpdfview, InspectionProgressCallback, ColorSpace, StreamFilter (+10 more)

### Community 11 - "Image Metadata Model"
Cohesion: 0.11
Nodes (18): ImageClassification, ImageMetadata, bitsPerComponent, classification, colorSpace, compressedSize, dpiX, dpiY (+10 more)

### Community 12 - "PDFDocumentInfo"
Cohesion: 0.17
Nodes (10): iomanip, PDFDocumentInfo, filePath, fileSizeBytes, images, isEncrypted, pageCount, pdfVersion (+2 more)

### Community 13 - "JPEG 2000 Codec"
Cohesion: 0.20
Nodes (11): openjpeg, OPJ_BOOL, OPJ_OFF_T, OPJ_SIZE_T, vector, encode, Jp2StreamData, buffer (+3 more)

### Community 14 - "Render-Diff Tool"
Cohesion: 0.31
Nodes (9): algorithm, bgraToGrayscale(), computePSNR(), computeSSIM(), string, vector, main(), PdfiumLibraryGuard (+1 more)

### Community 15 - "Core Engine Wiring"
Cohesion: 0.25
Nodes (7): cstdint, functional, memory, string, vector, fpdf_document_t__, fpdf_page_t__

### Community 16 - "Compression Params"
Cohesion: 0.18
Nodes (11): CompressionParams, bitsPerComponent, channels, colorSpace, hasAlpha, height, profile, qualityHint (+3 more)

### Community 17 - "Codec Tests"
Cohesion: 0.20
Nodes (9): CodecTest, jp2codec, jpegcodec, JpegEncode, pngcodec, PngEncodeIsFlateDecodable, TEST(), zlibcodec (+1 more)

### Community 18 - "Image Analyzer Tests"
Cohesion: 0.20
Nodes (9): GrayscaleDeviceImageClassifiedAsScannedText, imageanalyzer, ImageAnalyzerTest, MonochromeDeviceGrayStaysMonochrome, NullPixelsReturnsUnknown, PhotoClassification, ScreenshotClassification, SolidColorIsMonochrome (+1 more)

### Community 19 - "Zlib Codec"
Cohesion: 0.29
Nodes (5): cstring, vector, string, ZlibCodec, encode

### Community 20 - "JPEG Codec"
Cohesion: 0.36
Nodes (5): vector, JpegCodec, encode, encodeCmykAsRgb, stdexcept

### Community 21 - "PNG Codec"
Cohesion: 0.29
Nodes (5): vector, string, PngCodec, encode, zlib

### Community 22 - "ImageMetadata Helpers"
Cohesion: 0.33
Nodes (7): colorSpaceToString(), string, ColorSpace, StreamFilter, string, summary, streamFilterToString()

### Community 23 - "Requirements Docs"
Cohesion: 0.33
Nodes (6): RG-001: exactly 14 corpus files (transparency embeds SMask + shared XObject), RG-002: under-2KB PDFs exempt from size gate, Phase 0 Verification and Baseline (tests, corpus, benchmark, no behavior change), Verification Report (11 brief claims checked RIGHT/WRONG/PARTIAL with file:line), Phase-2 opt-in CMYK->RGB transcode (--transcode-cmyk-to-rgb), Optimization Pipeline (inspect-analyze-decide-encode-write)

### Community 24 - "Codec Interface"
Cohesion: 0.33
Nodes (4): ImageCodec, encode, name, Jp2Codec

### Community 25 - "Community 25"
Cohesion: 0.40
Nodes (4): src_codecs_jp2codec, src_codecs_zlibcodec, CompressionProfile, DecisionEngine::DecisionEngine()

### Community 26 - "Community 26"
Cohesion: 0.50
Nodes (3): dependencies, name, version-string

## Knowledge Gaps
- **110 isolated node(s):** `fpdf_document_t__`, `fpdf_page_t__`, `filesDropped`, `m_overlay`, `public` (+105 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 269 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **8 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `optimize` connect `Decision Engine Tests` to `Corpus Verifier Tests`, `Stripping Profiles and Optimizer`, `Corpus Generation Tools`, `Optimizer Tests (CMYK/Alpha)`, `Optimization Options and Defaults`, `CmykHandler`, `JPEG Codec`?**
  _High betweenness centrality (0.159) - this node is a cross-community bridge._
- **Why does `TEST()` connect `Corpus Verifier Tests` to `Decision Engine Tests`, `Stripping Profiles and Optimizer`?**
  _High betweenness centrality (0.096) - this node is a cross-community bridge._
- **Why does `TEST()` connect `Optimizer Tests (CMYK/Alpha)` to `Corpus Verifier Tests`, `Optimization Options and Defaults`, `Decision Engine Tests`?**
  _High betweenness centrality (0.077) - this node is a cross-community bridge._
- **Are the 17 inferred relationships involving `TEST()` (e.g. with `optimize` and `generateAll`) actually correct?**
  _`TEST()` has 17 INFERRED edges - model-reasoned connections that need verification._
- **Are the 9 inferred relationships involving `TEST()` (e.g. with `compress` and `optimize`) actually correct?**
  _`TEST()` has 9 INFERRED edges - model-reasoned connections that need verification._
- **What connects `fpdf_document_t__`, `fpdf_page_t__`, `filesDropped` to the rest of the system?**
  _110 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Corpus Verifier Tests` be split into smaller, more focused modules?**
  _Cohesion score 0.07236544549977386 - nodes in this community are weakly interconnected._