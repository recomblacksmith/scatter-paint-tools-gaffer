# Build Notes

This plugin is built through the Docker-first workflow in this repository.

## Supported Build Path

Use:

```bash
cd /home/des/_git/scatter-paint-tools-gaffer
./build-plugins.sh --plugin scatter_paint
```

This is the supported build process.

It:

- uses the shared builder image `d3smond/scatter-paint-tools-gaffer-build:gaffer-1.6.18.0`
- downloads and verifies the pinned packaged Gaffer runtime `1.6.18.0`
- builds `gaffer_scatter_paint`
- packages the combined side-load bundle under `dist/gaffer`
- runs the smoke-test step against that bundle

Use `./build-plugins.sh --pull` if you want to refresh the builder image before the run.

## Expected Output

The Docker build assembles a combined side-load bundle under `dist/gaffer`.

Expected payload for scatter paint includes:

- `dist/gaffer/python/GafferScatterPaint/`
- `dist/gaffer/python/GafferScatterPaintUI/`
- `dist/gaffer/python/GafferScatterPaintTest/`
- `dist/gaffer/startup/GafferScatterPaint/`
- `dist/gaffer/startup/GafferScatterPaintUI/`
- `dist/gaffer/startup/GafferSceneUI/`
- `dist/gaffer/startup/gui/`
- `dist/gaffer/graphics/`
- `dist/gaffer/demo/`

## Current Verified Runtime Slice

The compiled runtime now covers more than import-only smoke tests:

- `PaintedPoints` supports schema-backed repair backup persistence via `pointBackups`
- `PaintedPoints`, `AttachedPoints`, and `StaticPoints` share authored-color output behavior, including schema-v2 color persistence, `scatterColor`, and `debugColor`-driven `Cs` display switching
- `AttachedPoints` supports the packaged-runtime attachment regression harness used by the current shipped plugin
- the runtime includes active layer/stroke filtering, layer visibility and solo/mute handling, layer and stroke frame-range evaluation, override precedence, `keepLastValidOutput`, `strictUnresolved`, hybrid stored fallback anchors, optional triangle-failure fallback when `allowCrossMeshReproject` is enabled, per-instance path/transform resolve using stored `instanceSourcePathId` + `instanceId`, and deforming-surface topology-generation handling
- `PaintPointsTool` supports verified compiled action coverage for `commitDemoStroke()` and `eraseLastStroke()` through the packaged Gaffer UI test harness
- `PaintPointsTool` also includes compiled hit-based drag authoring, current-frame syncing, selection-driven relax/reproject actions, layer/stroke edit actions, and benchmark/demo actions exposed from the startup menus
- the interactive release path uses a runtime overlay for normal append paint and normal `eraseSpace == 0` erase instead of repacking the durable cache blob on every release
- `PaintedPoints` covers export and bake helpers exercised by the regression suite, including authored/interchange/diagnostics exports, evaluated points/scene exports, embedded-external cache migration, relink/upgrade paths, named selection sets, selection-driven relax/reproject, and authored/evaluated `StaticPoints` bake creation
- the packaged plugin test suite `GafferScatterPaintTest` passes against the pinned `1.6.18.0` runtime in the verified slice used here, including compiled and Python-fallback hard-error coverage, cache migration/export coverage, authored-color coverage, attachment regressions, tool regressions, and GL-gated interaction coverage

## Side-loading Contract

At runtime the combined bundle is discoverable with:

- `PYTHONPATH`
- `GAFFER_STARTUP_PATHS`
- platform library search path for compiled modules

For direct Python smoke tests outside the normal Gaffer launcher, point at the packaged Gaffer runtime and the combined bundle:

```bash
PYTHONNOUSERSITE=1 \
QT_QPA_PLATFORM=offscreen \
IECORE_FONT_PATHS="/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/fonts" \
LD_LIBRARY_PATH="/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/lib" \
PYTHONPATH="$(pwd)/dist/gaffer/python:/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/python" \
GAFFER_STARTUP_PATHS="$(pwd)/dist/gaffer/startup" \
/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/bin/python -c "import Gaffer, GafferUI, GafferScene, GafferSceneUI, GafferScatterPaint, GafferScatterPaintUI"
```

When launching the actual `gaffer` GUI with the same environment, the plugin auto-loads its startup bridges from `GAFFER_STARTUP_PATHS`, which registers:

- `PaintPointsTool` on `GafferSceneUI.SceneView`
- `/Tools/Scatter Paint` in the ScriptWindow menu
- `/Scatter/Painted Points`, `/Scatter/Attached Points`, and `/Scatter/Static Points` in the node menu
- the plugin-local `graphics/gafferScatterPaintUIPaintPointsTool.png` icon for `PaintPointsTool`; startup prepends that graphics directory to `GAFFERUI_IMAGE_PATHS`

Set `GAFFER_SCATTER_PAINT_BUILD_DEMO=1` to auto-build the scatter-paint demo graph on ScriptWindow creation via `startup/gui/scatterPaintDemo.py`.

## TypeId Caveat

Plugin node/tool `TypeId` ranges must not overlap ids already claimed by the target Gaffer runtime. The scatter-paint plugin uses `130000-130199` to avoid collisions with built-in `GafferSceneUI` ids in the `121000-121199` range.

## Current Boundaries

1. Node and tool contracts are shared across compiled and Python compatibility paths.
2. Startup compatibility shims remain part of the current launch surface.
3. The current Docker packaging flow builds in the plugin trees and then assembles the combined side-load bundle under `dist/gaffer`.
4. Larger paint sessions and viewer interaction paths are covered by the current regression surface, including GL-gated skips in headless runs.
