# Scatter Paint SIMD Plan

## Scope

- Initial shipping target: `linux-x86_64`
- CPU vendors: Intel and AMD
- Build output: one baseline-safe plugin binary with internal runtime dispatch
- Future-ready only: macOS support is planned later, but not implemented in phase 1

## Goals

Reduce the current paint hot paths without changing the Python/Gaffer module loading contract.

Primary timing buckets to improve:

- `sampleBuildConstructMs`
- `sampleCompileMs`
- `surfaceResolveMs`

Keep current module names unchanged:

- `python/GafferScatterPaint/_GafferScatterPaint.so`
- `python/GafferScatterPaintUI/_GafferScatterPaintUI.so`

## Non-Goals

- No separate `.so` per CPU model
- No macOS build lane in phase 1
- No global `-mavx2` build
- No more hot-path source splitting around private Gaffer-heavy code
- No SIMD work on Python-object-heavy loops before those loops are converted to typed C++

## Current Baseline

Recent post-revert baseline for the same paint case was approximately:

- `stroke commit totalMs ~= 162.9`
- `sampleBuildConstructMs ~= 32.3`
- `sampleCompileMs ~= 33.4`
- `surfaceResolveMs ~= 33.3`

These are the numbers to beat.

## High-Level Architecture

- Keep high-level brush orchestration in `PaintedPointsBrushOps.cpp`
- Introduce a narrow internal kernel layer that only works on POD/typed array data
- Select backend once at runtime:
  - `scalar`
  - `sse42`
  - `avx2`

## Runtime Dispatch Model

Use one plugin binary and select optimized kernels internally.

### Backends

- `scalar`
- `sse42`
- `avx2`

### Override Env

- `GAFFER_SCATTER_PAINT_SIMD=auto|scalar|sse42|avx2`

### Detection

Linux x86_64 first, using compiler builtins such as:

- `__builtin_cpu_init()`
- `__builtin_cpu_supports("sse4.2")`
- `__builtin_cpu_supports("avx2")`
- `__builtin_cpu_supports("fma")`

## Phase 1: Typed Sample Expansion

### Problem

`PaintPointsToolSampleUtils.h` still performs expensive Python dict copying and mutation per expanded sample.

### Target

- `app_plugins/gaffer_scatter_paint/src/GafferScatterPaintUI/PaintPointsToolSampleUtils.h`

### Change

Replace Python-dict-per-expanded-sample construction with typed C++ batch structs.

### New Internal Types

Suggested new private header:

- `app_plugins/gaffer_scatter_paint/src/GafferScatterPaintUI/PaintPointsToolSampleBatch.h`

Suggested structs:

- `BrushSampleInput`
- `ExpandedBrushSample`
- `ExpandedBrushSampleBatch`

### Rules

- Python-facing tool API stays the same
- Convert Python input once at the boundary
- Expansion and jitter math run over typed C++ storage

### Success Gate

- `sampleBuildConstructMs` drops materially from current baseline

## Phase 2: Typed Compile Path

### Problem

`compileBrushSample()` still depends on Python-originated field access patterns.

### Target

- `app_plugins/gaffer_scatter_paint/src/GafferScatterPaint/PaintedPointsBrushOps.cpp`

### Change

Add a typed path that consumes typed expanded samples directly.

### Suggested Header

- `app_plugins/gaffer_scatter_paint/src/GafferScatterPaint/BrushSampleTypes.h`

### Suggested Types

- `CompiledBrushSampleInput`
- `CompiledBrushSampleBatchView`

### Rule

Do not expose Gaffer/private anonymous-namespace types in any cross-translation-unit interface.

### Success Gate

- `sampleCompileMs` drops materially from current baseline

## Phase 3: CPU Feature and Dispatch Layer

### New Files

- `app_plugins/gaffer_scatter_paint/src/GafferScatterPaint/BrushCpuFeatures.h`
- `app_plugins/gaffer_scatter_paint/src/GafferScatterPaint/BrushCpuFeaturesLinux.cpp`
- `app_plugins/gaffer_scatter_paint/src/GafferScatterPaint/BrushSimdDispatch.h`
- `app_plugins/gaffer_scatter_paint/src/GafferScatterPaint/BrushSimdDispatch.cpp`

### Responsibilities

#### `BrushCpuFeatures.h`

- feature enum/struct definition

#### `BrushCpuFeaturesLinux.cpp`

- Linux x86_64 feature detection

#### `BrushSimdDispatch.h`

- kernel typedefs / dispatch table

#### `BrushSimdDispatch.cpp`

- one-time backend selection and caching

## Phase 4: Scalar Kernel Extraction

### New Files

- `app_plugins/gaffer_scatter_paint/src/GafferScatterPaint/BrushResolveKernels.h`
- `app_plugins/gaffer_scatter_paint/src/GafferScatterPaint/BrushResolveScalar.cpp`

### Responsibility

Move math-dense loops into scalar reference kernels first.

### First Kernel Candidates

- candidate distance evaluation
- triangle hit / barycentric tests
- candidate filtering over typed arrays

### Rule

- Keep orchestration in `PaintedPointsBrushOps.cpp`
- Scalar remains the correctness oracle

## Phase 5: SSE4.2 Backend

### New File

- `app_plugins/gaffer_scatter_paint/src/GafferScatterPaint/BrushResolveSSE42.cpp`

### Compile Flags

- `-msse4.2`

### Scope

Vectorize only clean, stable math kernels:

- point/triangle math
- distance checks
- candidate filtering loops

### Rule

Fallback to scalar for tails and awkward branches.

## Phase 6: AVX2/FMA Backend

### New File

- `app_plugins/gaffer_scatter_paint/src/GafferScatterPaint/BrushResolveAVX2.cpp`

### Compile Flags

- `-mavx2 -mfma`

### Rule

- Runtime-selected only
- Never required for module load
- Same kernel API as scalar and SSE

## Phase 7: Build System Changes

### Target

- `app_plugins/gaffer_scatter_paint/sconstruct_ext.py`

### Required Changes

Add new kernel files to the core build and compile ISA-specific files with per-file flags.

### Important Rules

- Keep default files on current baseline flags
- Do not set AVX2 flags globally
- If needed, use cloned SCons envs for ISA-specific source files

### Expected Shape

- baseline env for common files
- cloned env for SSE files
- cloned env for AVX2 files
- all linked into the same `_GafferScatterPaint.so`

## Phase 8: Benchmark and Correctness Harness

Reuse the existing timing output from:

- `app_plugins/gaffer_scatter_paint/src/GafferScatterPaintUI/PaintPointsToolStroke.cpp`

### Compare Modes

- `scalar`
- `sse42`
- `avx2`

### Must Validate

- committed point count
- authored point records
- layer/stroke behavior
- paint and erase parity
- timing differences

### Hardware Validation

- older Linux x86_64 CPU without AVX2
- modern Intel AVX2 CPU
- modern AMD AVX2 CPU

## Phase Gates

### Gate 1: Typed Expansion

Target:

- `sampleBuildConstructMs < 20ms`

### Gate 2: Typed Compile

Target:

- `sampleCompileMs < 22ms`

### Gate 3: Scalar Kernel Extraction

Target:

- no correctness drift
- no regression

### Gate 4: SSE4.2

Target:

- measurable `surfaceResolveMs` reduction on supported CPUs

### Gate 5: AVX2/FMA

Target:

- `surfaceResolveMs < 20ms` on strong AVX2 hardware

### Stretch Goal

Bring total stroke commit well below the current ~163ms baseline for the same stroke/settings case.

## API Boundary Rules

### Allowed Across SIMD/Kernel Boundaries

- plain structs
- contiguous float/int arrays
- triangle index buffers
- output hit/result buffers

### Forbidden Across SIMD/Kernel Boundaries

- `boost::python` objects
- Gaffer object ownership types
- anonymous-namespace helper types
- private TU-local geometry helper types

## Risks

### 1. SIMD before de-Pythonizing

This is the biggest risk. If Python object churn remains, SIMD gains will underdeliver.

### 2. Numeric drift

Triangle hit and barycentric code may drift subtly between scalar and SIMD paths.

### 3. Marshaling overhead

Poor batch/data layout can erase SIMD wins.

### 4. ABI mistakes

Do not leak private or TU-local helper types into new interfaces.

## Recommended Execution Order

1. typed sample expansion
2. typed compile path
3. scalar kernel API extraction
4. Linux CPU detection and dispatch
5. SSE4.2 backend
6. AVX2/FMA backend
7. benchmark and regression tightening

## Future macOS Support

Do not implement macOS yet, but keep the design ready for it.

### Rule

Separate:

- platform-specific CPU detection
- ISA-specific kernels
- platform-neutral dispatch interface

### Likely Later Additions

- `BrushCpuFeaturesMac.cpp`
- possibly `BrushResolveNEON.cpp` for macOS arm64

## Summary

Phase 1 should focus on removing Python object churn in sample expansion and compile. SIMD should be added only after the hot loops are running over typed C++ data. Ship one Linux x86_64 plugin binary with runtime dispatch for Intel and AMD CPUs using scalar, SSE4.2, and AVX2/FMA backends.
