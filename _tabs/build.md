---
title: Build
icon: fas fa-hammer
order: 1
---

# Build

This toolset is meant to be built in one step.

You do not need to set up a local compiler toolchain or build each plugin separately.

## Entry Point

Run:

```bash
./build-plugins.sh
```

Useful options:

```bash
./build-plugins.sh --plugin scatter_paint
./build-plugins.sh --plugin scatter_plus
./build-plugins.sh --plugin pointcloud_plus
./build-plugins.sh --plugin all --tests
./build-plugins.sh --pull
./build-plugins.sh --build-image
```

## What The Build Does

- uses the shared project build image `d3smond/scatter-paint-tools-gaffer-build:gaffer-1.6.18.0`
- downloads the matching Gaffer package automatically
- builds the selected tool, or the whole toolset
- assembles one combined payload under `dist/gaffer`
- runs a quick import test before finishing

## Expected Output

The finished build is written to:

```text
dist/gaffer/
```

That folder is the toolset you load into Gaffer.

It includes:

- `python/`
- `startup/`
- `graphics/` for Scatter Paint icons
- `demo/` for Scatter Paint demo content

## Loading The Toolset In Gaffer

When you launch Gaffer manually, point it at the combined toolset bundle:

- `PYTHONPATH += dist/gaffer/python`
- `GAFFER_STARTUP_PATHS += dist/gaffer/startup`
- `LD_LIBRARY_PATH += <gaffer runtime>/lib` on Linux if needed

## Quick Validation Example

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

If that import works, the built toolset is ready to load.
