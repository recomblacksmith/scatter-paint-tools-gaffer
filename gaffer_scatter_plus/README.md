# Gaffer Scatter Plus

Side-loadable Gaffer plugin for support-surface scatter workflows built around helper points, prototype instancing, and optional image input.

## Current Scope

`ScatterPlus` is the shipped node in this plugin.

Current workflows include:

- support-geometry driven scatter generation
- helper-point output at `<outputLocation>/points`
- instanced prototype output at `<outputLocation>/instances/*`
- image-driven distribution and luminance-based prototype selection
- density/decimation driven by support primitive variables
- support and decimation evaluation in `Object`, `World`, and `Reference` spaces
- per-instance debug attributes for downstream inspection

Startup currently registers:

- `/ScatterPlus/Nodes/Scatter Plus`
- `/ScatterPlus/Demos/Image Scatter`
- `/Tools/Scatter Plus/...`

## Build

Build, package, and smoke-test through Docker:

```bash
cd /home/des/_git/scatter-paint-tools-gaffer
./build-plugins.sh --plugin scatter_plus
```

This is the supported build path. It contributes `gaffer_scatter_plus` into the combined side-load bundle under `dist/gaffer`.

## Side-loading

Typical runtime setup points at the combined bundle produced by `./build-plugins.sh`:

- `PYTHONPATH += dist/gaffer/python`
- `GAFFER_STARTUP_PATHS += dist/gaffer/startup`
- `LD_LIBRARY_PATH += <gaffer runtime>/lib` on Linux when launching outside a packaged runtime

Packaged-runtime launch form used locally:

```bash
PYTHONNOUSERSITE=1 \
IECORE_FONT_PATHS="/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/fonts" \
LD_LIBRARY_PATH="/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/lib" \
PYTHONPATH="/home/des/_git/scatter-paint-tools-gaffer/dist/gaffer/python:/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/python" \
GAFFER_STARTUP_PATHS="/home/des/_git/scatter-paint-tools-gaffer/dist/gaffer/startup" \
"/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/bin/gaffer"
```

Headless import smoke test:

```bash
PYTHONNOUSERSITE=1 \
LD_LIBRARY_PATH="/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/lib" \
PYTHONPATH="/home/des/_git/scatter-paint-tools-gaffer/dist/gaffer/python:/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/python" \
GAFFER_STARTUP_PATHS="/home/des/_git/scatter-paint-tools-gaffer/dist/gaffer/startup" \
"/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/bin/python" -c "import GafferScatterPlus, GafferScatterPlusUI"
```

## Demo

`/ScatterPlus/Demos/Image Scatter` builds a ready-to-inspect graph with:

- a subdivided support plane
- a checkerboard image input
- a small prototype library
- `ScatterPlus` configured for image distribution and luminance-based prototype assignment
- a downstream `GafferScene.OpenGLAttributes` preview node

## Tests

Packaged-runtime unittest entrypoint used for local verification:

```bash
PYTHONNOUSERSITE=1 \
IECORE_FONT_PATHS="/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/fonts" \
LD_LIBRARY_PATH="/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
PYTHONPATH="/home/des/_git/scatter-paint-tools-gaffer/dist/gaffer/python:/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/python" \
GAFFER_STARTUP_PATHS="/home/des/_git/scatter-paint-tools-gaffer/dist/gaffer/startup" \
"/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/bin/python" -m unittest GafferScatterPlusTest
```

The current test coverage exercises output branch creation, helper-point primvars, prototype assignment modes, image distribution, density primitive variables, reference-space evaluation, decimation controls, collision modes, and demo action wiring.

## Design Notes

- `gaffer_scatter_plus/gaffer-scatter-plus.md`
