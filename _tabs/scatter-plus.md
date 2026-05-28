---
title: Scatter Plus
icon: fas fa-seedling
order: 4
---

# Gaffer Scatter Plus

`gaffer_scatter_plus` is the support-surface scatter plugin in the toolset.

## Current Scope

- support-geometry driven scatter generation
- helper-point output at `<outputLocation>/points`
- instanced prototype output at `<outputLocation>/instances/*`
- image-driven distribution and luminance-based prototype selection
- density and decimation from support primitive variables
- support and decimation evaluation in `Object`, `World`, and `Reference` spaces
- per-instance debug attributes for downstream inspection

## Runtime Registration

- `/ScatterPlus/Nodes/Scatter Plus`
- `/ScatterPlus/Demos/Image Scatter`
- `/Tools/Scatter Plus/...`

## Tests

Current coverage exercises:

- output branch creation
- helper-point primvars
- prototype assignment modes
- image distribution
- density primitive variables
- reference-space evaluation
- decimation controls
- collision modes
- demo action wiring
