<p align="center">
  <img src="https://recomblacksmith.github.io/scatter-paint-tools-gaffer/assets/logo/dev-logo.png" alt="Scatter Paint Tools for Gaffer" width="320">
</p>

# Scatter Paint Tools for Gaffer

Paint, scatter, attach, and bake points directly inside Gaffer.

Paint hero detail. Fill broad areas procedurally. Bake clean outputs for the next step.

Scatter Paint Tools is a side-loadable Gaffer plugin set for artists and TDs who need shot-level control over surface detail, procedural dressing, and point-cloud prep inside Gaffer. Paint hero placement by hand, generate broader scatter procedurally, keep points attached to changing geometry, and bake or export clean outputs for layout, dressing, lighting, FX, or downstream handoff.

## Artist Documentation

Read the user-facing install, release, and plugin docs here:

<https://recomblacksmith.github.io/scatter-paint-tools-gaffer/>

## What You Can Do

- paint rocks, grass, leaves, bolts, debris, set dressing, or other shot detail directly in the viewer
- keep painted points attached to animated or changing surfaces
- use layers, strokes, selections, visibility, mute, solo, timing, and color overrides to manage art direction
- relax, reproject, erase, edit, validate, repair, export, and bake painted scatter data
- scatter points or instances procedurally from meshes, masks, attributes, and images
- generate, clean up, and output point clouds for layout, dressing, lighting, or FX handoff

## Plugins

### `gaffer_scatter_paint`

Hand-placed scatter painting for art-directed placement. Use it when a shot needs specific placement: grass touch-up, rocks, leaves, bolts, debris, footprints, dressing fixes, or any detail that needs an artist's eye.

Includes `PaintedPoints`, `AttachedPoints`, `StaticPoints`, a compiled `PaintPointsTool`, cache/export helpers, and demo/benchmark menus.

### `gaffer_scatter_plus`

Procedural scatter across support surfaces. Use it for broader coverage passes, image-driven density, attribute masks, helper-point output, prototype instancing, and scene-scatter debug attributes.

### `gaffer_pointcloud_plus`

Point-cloud generation, cleanup, and output tools for scene-driven `PointsPrimitive` data. Use it to turn geometry into points, move incoming point data into cleaner scene branches, or prep simple point-cache handoffs.

## Repository Layout

- `gaffer_scatter_paint/`
- `gaffer_scatter_plus/`
- `gaffer_pointcloud_plus/`

Each plugin directory contains its own source, startup hooks, Python modules, tests, and plugin-local documentation.

## Build And Runtime

These plugins build as side-loadable Gaffer extensions against a packaged Gaffer runtime.

The supported local and CI entrypoint is:

- `./build-plugins.sh`

Requirements:

- Docker

Default behavior:

- uses the shared builder image `d3smond/scatter-paint-tools-gaffer-build:gaffer-1.6.18.0`
- refreshes that image on demand with `./build-plugins.sh --pull`
- downloads and verifies the pinned packaged Gaffer runtime `1.6.18.0`
- builds all three plugins
- packages a combined side-load bundle under `dist/gaffer`
- runs import smoke tests against that combined bundle

Release behavior:

- publishes one combined toolset release containing all three plugins together
- publishes a versioned Linux archive such as `scatter-paint-tools-gaffer-v0.1.0-linux-gaffer-1.6.18.0.tar.gz`
- unpacks into a versioned folder with `how-to-setup.md` and per-plugin directories under `scatter-paint-tools-gaffer/`

Launch example from an unpacked release:

```bash
PYTHONNOUSERSITE=1 \
GAFFER_SCATTER_PAINT_DIAGNOSTICS=1 \
IECORE_FONT_PATHS="/path/to/gaffer-1.6.18.0-linux-gcc11/fonts" \
LD_LIBRARY_PATH="/path/to/gaffer-1.6.18.0-linux-gcc11/lib" \
PYTHONPATH="/path/to/scatter-paint-tools-gaffer/gaffer_scatter_paint/python:/path/to/scatter-paint-tools-gaffer/gaffer_scatter_plus/python:/path/to/scatter-paint-tools-gaffer/gaffer_pointcloud_plus/python:/path/to/gaffer-1.6.18.0-linux-gcc11/python" \
GAFFER_STARTUP_PATHS="/path/to/scatter-paint-tools-gaffer/gaffer_scatter_paint/startup:/path/to/scatter-paint-tools-gaffer/gaffer_scatter_plus/startup:/path/to/scatter-paint-tools-gaffer/gaffer_pointcloud_plus/startup" \
"/path/to/gaffer-1.6.18.0-linux-gcc11/bin/gaffer"
```

That launches Gaffer with all three plugins loaded from the shipped release layout.

Typical side-loading uses:

- `PYTHONPATH`
- `GAFFER_STARTUP_PATHS`
- the platform library search path required for compiled modules

Examples:

```bash
./build-plugins.sh
./build-plugins.sh --plugin scatter_paint
./build-plugins.sh --plugin all --tests
./build-plugins.sh --build-image
```

Windows local build:

```powershell
.\build-plugins.ps1 -Plugin all
```

The Windows GitHub Actions workflow expects a local self-hosted runner with the default `self-hosted`, `Windows`, and `X64` labels. The machine must have Visual Studio 2022 C++ build tools, Python launcher `py`, network access to download the pinned Gaffer Windows zip, and permission to write `T:\github-runner-cache\scatter-paint-tools-gaffer\gaffer`.

## Developer Documentation

- `gaffer_scatter_paint/README.md`
- `gaffer_scatter_plus/README.md`
- `gaffer_pointcloud_plus/README.md`

Use the plugin READMEs for plugin-specific build, test, launch, and usage notes.

Use the plugin-local docs and design notes for deeper implementation detail.

## Status

This repository is the home for this plugin set.
