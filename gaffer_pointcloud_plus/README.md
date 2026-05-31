# Gaffer PointCloud Plus

Side-loadable Gaffer plugin for point-cloud generation, cleanup, and output workflows.

Use PointCloud Plus to create clean `PointsPrimitive` output from scene geometry, generate primitive-center points, or move incoming point data into cleaner scene branches with simple playback controls. It is built for practical point handoffs into layout, lighting, FX, and cache-driven workflows.

## Current Scope

`PointCloudPlus` is the shipped node in this plugin.

Current workflows include:

- random surface point generation
- primitive-center point generation
- output of upstream `PointsPrimitive` data
- file-style playback remapping through `frame`, `frameOffset`, and `animationBehavior`
- injecting point output at a chosen location while preserving the surrounding source scene

Startup currently registers:

- `/PointCloud/PointCloud Plus`
- `/PointCloudPlus/Nodes/PointCloud Plus`
- `/PointCloudPlus/Demos/Geometry Basic`
- `/PointCloudPlus/Demos/Primitive Center`
- `/PointCloudPlus/Demos/File Output`
- `/Tools/PointCloud Plus/...`

## Build

Build, package, and smoke-test through Docker:

```bash
cd /home/des/_git/scatter-paint-tools-gaffer
./build-plugins.sh --plugin pointcloud_plus
```

This is the supported build path. It contributes `gaffer_pointcloud_plus` into the combined side-load bundle under `dist/gaffer`.

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
"/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/bin/python" -c "import GafferPointCloudPlus, GafferPointCloudPlusUI"
```

## Demos

The startup menus currently expose three focused demo builders:

- `Geometry Basic`
- `Primitive Center`
- `File Output`

Those demo graphs wrap the output with `GafferScene.OpenGLAttributes` so the emitted points preview cleanly in `SceneView`.

## Tests

Packaged-runtime unittest entrypoint used for local verification:

```bash
PYTHONNOUSERSITE=1 \
IECORE_FONT_PATHS="/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/fonts" \
LD_LIBRARY_PATH="/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/lib" \
PYTHONPATH="/home/des/_git/scatter-paint-tools-gaffer/dist/gaffer/python:/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/python" \
GAFFER_STARTUP_PATHS="/home/des/_git/scatter-paint-tools-gaffer/dist/gaffer/startup" \
"/home/des/Downloads/gaffer-1.6.18.0-linux-gcc11/bin/python" -m unittest GafferPointCloudPlusTest
```

The current test coverage exercises geometry mode, primitive-center mode, file-output mode, auto-discovery of input point locations, playback remapping, intermediate branch creation, preserved ancestor transforms and attributes, and demo action wiring.

## Design Notes

- `gaffer_pointcloud_plus/gaffer-pointcloud-plus.md`
