---
title: Build
icon: fas fa-hammer
order: 1
---

# Build

The supported build path for this toolset is Docker-first and packaged-runtime-only.

## Entry Point

Use:

```bash
./build-plugins.sh
```

Useful variants:

```bash
./build-plugins.sh --plugin scatter_paint
./build-plugins.sh --plugin scatter_plus
./build-plugins.sh --plugin pointcloud_plus
./build-plugins.sh --plugin all --tests
./build-plugins.sh --pull
./build-plugins.sh --build-image
```

## What The Build Does

- uses `d3smond/scatter-paint-tools-gaffer-build:gaffer-1.6.18.0`
- downloads and verifies packaged Gaffer `1.6.18.0`
- builds the selected plugin or the full toolset
- assembles one combined side-load payload under `dist/gaffer`
- runs smoke imports against the combined payload

## Expected Output

The build writes one combined runtime payload to:

```text
dist/gaffer/
```

That merged payload includes:

- `python/`
- `startup/`
- `graphics/` from scatter paint
- `demo/` from scatter paint

## Side-loading Contract

Typical runtime setup points at the combined bundle:

- `PYTHONPATH += dist/gaffer/python`
- `GAFFER_STARTUP_PATHS += dist/gaffer/startup`
- `LD_LIBRARY_PATH += <gaffer runtime>/lib` on Linux when launching outside a packaged runtime

## Smoke Test Shape

```bash
GAFFER_ROOT=/path/to/gaffer-1.6.18.0-linux-gcc11

PYTHONNOUSERSITE=1 \
QT_QPA_PLATFORM=offscreen \
IECORE_FONT_PATHS="$GAFFER_ROOT/fonts" \
LD_LIBRARY_PATH="$GAFFER_ROOT/lib" \
PYTHONPATH="$(pwd)/dist/gaffer/python:$GAFFER_ROOT/python" \
GAFFER_STARTUP_PATHS="$(pwd)/dist/gaffer/startup" \
"$GAFFER_ROOT/bin/python" -c "import GafferScatterPaint, GafferScatterPaintUI, GafferScatterPlus, GafferScatterPlusUI, GafferPointCloudPlus, GafferPointCloudPlusUI"
```
