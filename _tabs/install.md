---
title: Install
icon: fas fa-download
order: 1
---

# Install

This page is the quickest way to get the plugins loaded in Gaffer.

If you are just trying to use the tools, download the release package. You do not need to build each plugin by hand.

## Download

Latest Linux release file:

```text
scatter-paint-tools-gaffer-v0.1.3-linux-gaffer-1.6.18.0.tar.gz
```

Download it from:

- <https://github.com/recomblacksmith/scatter-paint-tools-gaffer/releases>

After you unpack it, you should have a folder like this:

```text
scatter-paint-tools-gaffer-v0.1.3-linux-gaffer-1.6.18.0/
  how-to-setup.md
  scatter-paint-tools-gaffer/
```

The folder you care about is:

```text
scatter-paint-tools-gaffer/
```

That is the plugin folder you point Gaffer at.

## Short Version

1. download the archive
2. unpack it somewhere stable
3. point Gaffer at the three plugin `python` folders
4. point Gaffer at the three plugin `startup` folders

## What Is Inside

Inside `scatter-paint-tools-gaffer/` you will find:

- `gaffer_scatter_paint/`
- `gaffer_scatter_plus/`
- `gaffer_pointcloud_plus/`

Each plugin folder carries its own `python/` and `startup/` paths.

## Gaffer Paths

The usual setup is:

- `PYTHONPATH += /path/to/scatter-paint-tools-gaffer/gaffer_scatter_paint/python`
- `PYTHONPATH += /path/to/scatter-paint-tools-gaffer/gaffer_scatter_plus/python`
- `PYTHONPATH += /path/to/scatter-paint-tools-gaffer/gaffer_pointcloud_plus/python`
- `GAFFER_STARTUP_PATHS += /path/to/scatter-paint-tools-gaffer/gaffer_scatter_paint/startup`
- `GAFFER_STARTUP_PATHS += /path/to/scatter-paint-tools-gaffer/gaffer_scatter_plus/startup`
- `GAFFER_STARTUP_PATHS += /path/to/scatter-paint-tools-gaffer/gaffer_pointcloud_plus/startup`

If you launch Gaffer by hand on Linux, you may also need the Gaffer runtime libs available in `LD_LIBRARY_PATH`, depending on how your Gaffer package is set up.

Once the paths are in place, you should see the scatter nodes, tools, demos, and menus show up inside Gaffer.

## Launch Example

This is the tested Linux launch pattern for loading all three plugins from the shipped release bundle:

```bash
PYTHONNOUSERSITE=1 \
GAFFER_SCATTER_PAINT_DIAGNOSTICS=1 \
IECORE_FONT_PATHS="/path/to/gaffer-1.6.18.0-linux-gcc11/fonts" \
LD_LIBRARY_PATH="/path/to/gaffer-1.6.18.0-linux-gcc11/lib" \
PYTHONPATH="/path/to/scatter-paint-tools-gaffer/gaffer_scatter_paint/python:/path/to/scatter-paint-tools-gaffer/gaffer_scatter_plus/python:/path/to/scatter-paint-tools-gaffer/gaffer_pointcloud_plus/python:/path/to/gaffer-1.6.18.0-linux-gcc11/python" \
GAFFER_STARTUP_PATHS="/path/to/scatter-paint-tools-gaffer/gaffer_scatter_paint/startup:/path/to/scatter-paint-tools-gaffer/gaffer_scatter_plus/startup:/path/to/scatter-paint-tools-gaffer/gaffer_pointcloud_plus/startup" \
"/path/to/gaffer-1.6.18.0-linux-gcc11/bin/gaffer"
```

Swap in your real paths and Gaffer should come up with all three plugins live.

## For Studio Setup

If someone asks what this package is, the short version is:

- one plugin bundle
- three tools inside it
- drop it somewhere stable
- add each plugin's `python` and `startup` folders to the usual Gaffer paths

## For Developers

If you do want to build locally instead of downloading a release:

```bash
./build-plugins.sh
```

That path is mainly for development work, not day-to-day artist setup.
