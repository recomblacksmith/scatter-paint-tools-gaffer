# Build Notes

This plugin is a side-loadable Gaffer extension with an active Python backend and a working compiled build against either a packaged Gaffer runtime or the repo's local `temp/gaffer` checkout.

## Current state

The repository now contains the plugin directory layout, an active Python backend for cache/storage/evaluation scaffolding, and a local `scons` build for the compiled extension modules.

The build supports two target layouts:

- a packaged/self-contained Gaffer runtime containing `include/`, `lib/`, `python/`, and `bin/python`
- a local source + build pairing

For the local source build, it depends on two trees:

- `temp/gaffer` for Gaffer source headers, startup files, and build scripts
- `temp/gaffer-build` for installed dependency headers, shared libraries, Python packages, and built Gaffer libraries

This repo-root `devbox.json` currently provides the expected toolchain:

- `python311`
- `scons`
- `gcc`
- `inkscape`
- `mesa_glu`
- `pkg-config`

For reference, GafferHQ's official release/container build environment is documented in `GafferHQ/build`, while the dependency source/binary project is `GafferHQ/dependencies`.

## Working commands

Build against a packaged Gaffer runtime:

```bash
cd app_plugins/gaffer_scatter_paint
scons GAFFER_ROOT="/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11" -j8
```

Wrapper form:

```bash
python app_plugins/gaffer_scatter_paint/dev.py \
  --gaffer-root /home/des/Downloads/gaffer-1.6.18.0-linux-gcc11 \
  build
```

In this mode the plugin build derives these ABI-sensitive values from `GAFFER_ROOT/bin/python` and `GAFFER_ROOT/lib`:

- Python include directory
- Python shared library name
- Boost.Python shared library name
- extension suffix
- runtime `rpath`

Install the Gaffer dependency tree:

```bash
cd temp/gaffer
python .github/workflows/main/installDependencies.py --dependenciesDir ../gaffer-build
```

Apply local build-tree fixes:

```bash
python app_plugins/gaffer_scatter_paint/bootstrap_build_tree.py
```

Build Gaffer:

```bash
cd temp/gaffer
scons build BUILD_DIR="$(pwd)/../gaffer-build" WARNINGS_AS_ERRORS=0 -j8
```

Build the plugin:

```bash
cd app_plugins/gaffer_scatter_paint
scons GAFFER_ROOT="$(pwd)/../../temp/gaffer" GAFFER_BUILD_DIR="$(pwd)/../../temp/gaffer-build" -j8
```

Wrapper form:

```bash
python app_plugins/gaffer_scatter_paint/dev.py build
```

## Why the bootstrap step exists

Today the generated `temp/gaffer-build` tree needs two reproducible local fixes:

1. `temp/gaffer-build/include/IECore/MurmurHash.h` needs `#include <cstdint>` so GCC 15 parses the header correctly.
2. `temp/gaffer-build/include/GL` and `temp/gaffer-build/lib` need Linux GL/GLU headers and shared libraries linked in from the devbox packages.

`app_plugins/gaffer_scatter_paint/bootstrap_build_tree.py` applies those fixes. Re-run it any time `temp/gaffer-build` is recreated.

## Expected integration

The compiled path is expected to produce:

- a `GafferScatterPaint` Python extension module at `python/GafferScatterPaint/_GafferScatterPaint.so`
- a `GafferScatterPaintUI` Python extension module at `python/GafferScatterPaintUI/_GafferScatterPaintUI.so`
- any required shared libraries beside those modules

## Current verified runtime slice

The compiled runtime now covers more than import-only smoke tests:

- `PaintedPoints` supports schema-backed repair backup persistence via `pointBackups`
- `PaintedPoints`, `AttachedPoints`, and `StaticPoints` now share authored-color output behavior, including schema-v2 color persistence, `scatterColor` authored-color output, and `debugColor`-driven `Cs` display switching
- `AttachedPoints` supports the packaged-runtime attachment regression harness for the current compiled Milestone 4 slice
- that slice now includes active layer/stroke filtering, layer visibility and solo/mute handling, layer and stroke frame-range evaluation, override precedence, `keepLastValidOutput` / `strictUnresolved`, hybrid stored fallback anchors, optional triangle-failure fallback when `allowCrossMeshReproject` is enabled, first-pass per-instance path/transform resolve using stored `instanceSourcePathId` + `instanceId`, and a deforming-surface topology-generation pass that allows stable barycentric resolves across frames and stale-topology triangle failures to stay resolved from stored fallback data while still surfacing topology mismatch diagnostics
- `PaintPointsTool` supports a verified compiled action-parity slice for `commitDemoStroke()` / `eraseLastStroke()` through the packaged Gaffer UI test harness
- `PaintPointsTool` also includes a first-pass compiled hit-based drag-authoring slice wired through `SceneView` signals, plus current-frame syncing for `frameStart` / `frameEnd`; its GL-backed interaction regression is present in the suite and skips automatically in headless/offscreen runs
- the packaged plugin test suite `GafferScatterPaintTest` currently passes against `/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11` in the verified slice used here, including compiled and Python-fallback hard-error coverage for `strictUnresolved`, export-preset coverage, the compiled `PaintPointsTool` action-parity regression, the frame-sync regression, the GL-gated hit-interaction regression, and the broader combined sweep of `AttachmentRegressionTest`, `PaintPointsToolTest`, and `PaintedPointsCacheActionsTest` (`125` tests, `15` skipped)
- with `strictUnresolved` enabled, unresolved attachments now raise an evaluation error while leaving diagnostics plugs populated; `keepLastValidOutput` can still preserve and reuse the last valid solve payload behind that error state

This is still not full Milestone 4 parity. Current compiled `AttachedPoints` behavior is intentionally narrower than the spec's eventual hybrid/per-instance/deforming-surface contract, and the next work is attachment/tool UX follow-up rather than build bring-up.

## Side-loading contract

At runtime the plugin should be discoverable with:

- `PYTHONPATH`
- `GAFFER_STARTUP_PATHS`
- platform library search path for compiled modules

For direct Python smoke tests outside the normal Gaffer launcher, also point at the built Gaffer runtime:

```bash
PYTHONPATH="$(pwd)/app_plugins/gaffer_scatter_paint/python:$(pwd)/temp/gaffer-build/python" \
LD_LIBRARY_PATH="$(pwd)/temp/gaffer-build/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
"$(pwd)/temp/gaffer-build/bin/python" -c "import GafferScatterPaint, GafferScatterPaintUI"
```

For a packaged Gaffer runtime:

```bash
PYTHONNOUSERSITE=1 \
QT_QPA_PLATFORM=offscreen \
IECORE_FONT_PATHS="/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/fonts" \
LD_LIBRARY_PATH="/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/lib" \
PYTHONPATH="$(pwd)/app_plugins/gaffer_scatter_paint/python:/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/python" \
GAFFER_STARTUP_PATHS="$(pwd)/app_plugins/gaffer_scatter_paint/startup" \
/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/bin/python -c "import Gaffer, GafferUI, GafferScene, GafferSceneUI, GafferScatterPaint, GafferScatterPaintUI"
```

When launching the actual `gaffer` GUI with the same environment, the plugin now auto-loads its startup bridges from `GAFFER_STARTUP_PATHS`, which registers:

- `PaintPointsTool` on `GafferSceneUI.SceneView`
- `/Tools/Scatter Paint` in the ScriptWindow menu
- `/Scatter/Painted Points`, `/Scatter/Attached Points`, and `/Scatter/Static Points` in the node menu
- an explicit built-in tool icon override (`gafferSceneUISelectionTool.png`) for `PaintPointsTool`, so Viewer startup does not rely on a plugin-local `gafferScatterPaintUIPaintPointsTool.png` file

Set `GAFFER_SCATTER_PAINT_BUILD_DEMO=1` to auto-build the scatter-paint demo graph on ScriptWindow creation via `startup/gui/scatterPaintDemo.py`.

Wrapper forms:

```bash
python app_plugins/gaffer_scatter_paint/dev.py \
  --gaffer-root /home/des/Downloads/gaffer-1.6.18.0-linux-gcc11 \
  smoke-test --ui

python app_plugins/gaffer_scatter_paint/dev.py \
  --gaffer-root /home/des/Downloads/gaffer-1.6.18.0-linux-gcc11 \
  launch

python app_plugins/gaffer_scatter_paint/dev.py \
  --gaffer-root /home/des/Downloads/gaffer-1.6.18.0-linux-gcc11 \
  launch --demo
```

## TypeId caveat

Plugin node/tool `TypeId` ranges must not overlap ids already claimed by the target Gaffer runtime. The scatter-paint plugin now uses `130000-130199` to avoid collisions with built-in `GafferSceneUI` ids in the `121000-121199` range.

## Near-term plan

1. Keep node and tool contracts stable in headers and Python startup.
2. Port the active Python backend responsibilities into compiled nodes/tooling.
3. Replace the generated-tree bootstrap workaround with a cleaner upstreamable fix.
4. Extend smoke tests from import coverage to full viewer/tool registration coverage.
