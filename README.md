# Scatter Paint Tools for Gaffer

Standalone source repository for side-loadable Gaffer plugins focused on authored scatter painting, scene scatter generation, and point-cloud workflows.

## Plugins

### `gaffer_scatter_paint`

Authored scatter-paint workflow for Gaffer with `PaintedPoints`, `AttachedPoints`, `StaticPoints`, a compiled `PaintPointsTool`, cache/export helpers, and demo/benchmark menus.

### `gaffer_scatter_plus`

Support-surface scatter plugin built around helper-point output, prototype instancing, image-driven distribution, and scene-scatter debug attributes.

### `gaffer_pointcloud_plus`

Point-cloud generation and republish plugin for scene-driven `PointsPrimitive` output, including geometry, primitive-center, and file-style playback workflows.

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

## Documentation

- `gaffer_scatter_paint/README.md`
- `gaffer_scatter_plus/README.md`
- `gaffer_pointcloud_plus/README.md`

Use the plugin READMEs for plugin-specific build, test, launch, and usage notes.

Use the plugin-local docs and design notes for deeper implementation detail.

## Status

This repository is the canonical standalone home for this plugin set.
