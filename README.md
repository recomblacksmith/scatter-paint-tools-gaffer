<p align="center">
  <img src="https://recomblacksmith.github.io/scatter-paint-tools-gaffer/assets/logo/dev-logo.png" alt="Scatter Paint Tools for Gaffer" width="320">
</p>

# Scatter Paint Tools for Gaffer

Artist-friendly scatter painting and point tools for Gaffer.

Scatter Paint Tools is a side-loadable Gaffer plugin set for artists and TDs who need better control over placed detail, procedural scatter, and point-cloud prep inside shot workflows.

## Artist Documentation

Read the user-facing install, release, and plugin docs here:

<https://recomblacksmith.github.io/scatter-paint-tools-gaffer/>

## What You Can Do

- paint points by hand in the viewer
- stick painted points back onto geometry
- freeze painted results for downstream work
- scatter points or instances procedurally from meshes, masks, and attributes
- generate, clean up, and republish point clouds for layout, dressing, lighting, or FX handoff

## Plugins

### `gaffer_scatter_paint`

Hand-placed scatter painting for art-directed placement. Use it for grass touch-up, rocks, leaves, bolts, debris, and other shot-specific detail that needs direct artist control.

Includes `PaintedPoints`, `AttachedPoints`, `StaticPoints`, a compiled `PaintPointsTool`, cache/export helpers, and demo/benchmark menus.

### `gaffer_scatter_plus`

Procedural scatter across support surfaces. Use it for broader coverage passes, image-driven density, attribute masks, helper-point output, prototype instancing, and scene-scatter debug attributes.

### `gaffer_pointcloud_plus`

Point-cloud generation and republish tools for scene-driven `PointsPrimitive` output. Use it to turn geometry into points, clean up incoming point branches, or prep simple point-cache handoffs.

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

## Developer Documentation

- `gaffer_scatter_paint/README.md`
- `gaffer_scatter_plus/README.md`
- `gaffer_pointcloud_plus/README.md`

Use the plugin READMEs for plugin-specific build, test, launch, and usage notes.

Use the plugin-local docs and design notes for deeper implementation detail.

## Status

This repository is the home for this plugin set.
