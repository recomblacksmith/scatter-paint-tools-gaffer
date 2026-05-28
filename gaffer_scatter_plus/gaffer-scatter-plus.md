# Gaffer Scatter Plus Design Overview

`ScatterPlus` is the current scene-scatter plugin in this repository.

It focuses on support-surface driven scattering, prototype selection, helper-point publishing, and instanced output that can be side-loaded into Gaffer as a standalone plugin.

## Current plugin role

- generate scatter points from support geometry
- publish helper-point data under `<outputLocation>/points`
- build instanced output under `<outputLocation>/instances/*`
- support prototype selection from a prototype scene
- support image-driven density and luminance-based prototype workflows
- preserve per-instance debug attributes for inspection

## Current node

The shipped node is `ScatterPlus`.

Its public interface currently covers these workflow groups:

- support selection and output location
- prototype scene and prototype-index assignment
- transform controls for position, orientation, scale, and normal alignment
- variance controls for position, rotation, scale, seed, and time offset
- decimation and collision-related controls
- support/reference-space evaluation controls
- optional image input for density and luminance-driven selection

## Output model

The current output structure is intentionally split into two branches:

1. helper points at `<outputLocation>/points`
2. instances at `<outputLocation>/instances/*`

The helper points are not just temporary internals. They publish scatter data that is useful for inspection, debugging, and downstream processing.

Current helper-point data includes:

- `prototypeIndex`
- quaternion `orientation`
- `scale`
- `referencePosition`
- `scatter_position`
- `scatter_rotation`
- `scatter_scale`
- `scatter_normal`
- `scatter_rotation_order`
- `scatter_time_offset`

The plugin also preserves per-instance scene attributes used for debugging and inspection:

- `scatterPlus:prototypePath`
- `scatterPlus:prototypeIndex`
- `scatterPlus:seed`
- `scatterPlus:id`
- `scatterPlus:timeOffset`

## Supported workflows

The plugin currently supports:

- support-geometry based scatter generation
- prototype instancing from a prototype scene
- image-distribution workflows
- luminance-driven prototype assignment
- density/decimation input from support primitive variables
- support evaluation in `Object`, `World`, and `Reference` spaces

The startup integration currently registers:

- `/ScatterPlus/Nodes/Scatter Plus`
- `/ScatterPlus/Demos/Image Scatter`
- `/Tools/Scatter Plus/...`

## Current behavior

`ScatterPlus` is a practical scatter workflow node for Gaffer. It keeps the support scene visible, publishes helper-point data, and adds instanced scatter output as a new subtree.

The current node and tests cover:

- output branches for helper points and instances
- prototype-scene selection and prototype-index assignment
- image-driven density and luminance-based prototype workflows
- support evaluation in `Object`, `World`, and `Reference` spaces
- decimation controls and collision-related controls already present on the node
- helper-point publishing and per-instance debug attributes used for inspection

## Current boundaries

This document describes the shipped scatter workflow that is present in the repository now. It does not describe a broader general-purpose scattering system beyond the controls, menus, demos, and regression coverage already included with `ScatterPlus`.

## Relationship to the README

Use `gaffer_scatter_plus/README.md` for build, runtime, test, and launch commands.

Use this document for the current design and workflow shape of `ScatterPlus`.
