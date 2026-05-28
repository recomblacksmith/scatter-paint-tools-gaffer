# Gaffer Scatter Paint

Side-loadable Gaffer plugin for authored surface scatter painting in Gaffer.

## Current Scope

This plugin currently ships:

- `PaintedPoints` for authored scatter data, cache actions, export, and bake workflows
- `AttachedPoints` for evaluating authored points back onto scene geometry with diagnostics and fallback behavior
- `StaticPoints` for frozen baked output
- `PaintPointsTool` for paint, erase, selection, relax, reproject, and layer/stroke editing workflows inside `SceneView`

Current verified workflows include:

- authored layers, strokes, selection sets, and cache-backed point records
- cache validate, migrate, relink, upgrade, compact, export, and bake actions
- authored color precedence `point > stroke > layer > node default`
- `debugColor` display switching while keeping authored color in `scatterColor`
- layer visibility, mute, solo, mode, and frame-range filtering
- attachment diagnostics, `strictUnresolved`, `keepLastValidOutput`, fallback anchors, and per-instance resolve support
- compiled and Python-fallback regression coverage for cache and attachment behavior

Supported build target:

- the Docker-first `./build-plugins.sh` flow in this repository

## Build

For detailed build and integration notes, see `gaffer_scatter_paint/docs/build.md`.

Build, package, and smoke-test through Docker:

```bash
cd /home/des/_git/scatter-paint-tools-gaffer
./build-plugins.sh --plugin scatter_paint
```

This is the supported build path. It uses the shared builder image, downloads and verifies the pinned packaged Gaffer runtime, builds the plugin, packages the combined side-load bundle under `dist/gaffer`, and runs the smoke-test step.

## Side-loading

Point these paths at the combined bundle produced by `./build-plugins.sh`:

- `PYTHONPATH += dist/gaffer/python`
- `GAFFER_STARTUP_PATHS += dist/gaffer/startup`
- `LD_LIBRARY_PATH += <gaffer runtime>/lib` on Linux when launching outside Gaffer's managed runtime

Combined bundle smoke test after `./build-plugins.sh`:

```bash
PYTHONNOUSERSITE=1 \
QT_QPA_PLATFORM=offscreen \
IECORE_FONT_PATHS="/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/fonts" \
LD_LIBRARY_PATH="/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/lib" \
PYTHONPATH="$(pwd)/dist/gaffer/python:/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/python" \
GAFFER_STARTUP_PATHS="$(pwd)/dist/gaffer/startup" \
/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/bin/python -c "import GafferScatterPaint, GafferScatterPaintUI"
```

Use the same combined bundle paths when launching Gaffer or debugging imports manually.

## Demo Menus

After side-loading the plugin startup paths, Gaffer registers:

- `/Scatter/...` node menu entries for `PaintedPoints`, `AttachedPoints`, and `StaticPoints`
- `/Tools/Scatter Paint/...` for graph creation, validation, repair, and inspection actions
- `/ScatterPaint/Demos/...` for focused demo graphs
- `/ScatterPaint/Benchmark/...` for deterministic stroke/perf checks

Current demo builders include:

- `Build Demo Graph`
- `Paint + Erase Basics`
- `Paint + Erase Cyclo`
- `Paint Through + Erase Space`
- `Relax + Reproject`
- `Layer Modes + Timing`
- `Layer + Stroke Editing`
- `Box Instance Scatter`

Set `GAFFER_SCATTER_PAINT_BUILD_DEMO=1` before launch if you want the demo graph auto-built for new script windows.

## Tests

Local regression command against the combined bundle:

```bash
PYTHONNOUSERSITE=1 \
QT_QPA_PLATFORM=offscreen \
IECORE_FONT_PATHS="/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/fonts" \
LD_LIBRARY_PATH="/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/lib" \
PYTHONPATH="/home/des/_git/scatter-paint-tools-gaffer/dist/gaffer/python:/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/python" \
GAFFER_STARTUP_PATHS="/home/des/_git/scatter-paint-tools-gaffer/dist/gaffer/startup" \
"/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/bin/python" -m unittest GafferScatterPaintTest
```

The regression suite covers cache actions, attachment solve behavior, authored color output, export/bake flows, tool action parity, current-frame syncing, and GL-gated viewport interaction. Some `PaintPointsTool` tests still skip automatically in headless or offscreen mode because `SceneView` interaction needs a GL-backed context.

## More Detail

- `gaffer_scatter_paint/docs/build.md`
- `gaffer_scatter_paint/docs/nodes.md`
- `gaffer_scatter_paint/docs/cache-schema.md`
- `gaffer_scatter_paint/docs/stats.md`
- `gaffer_scatter_paint/docs/status.md`
