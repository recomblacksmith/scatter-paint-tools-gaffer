---
title: Scatter Paint
icon: fas fa-paintbrush
order: 3
---

# Gaffer Scatter Paint

`gaffer_scatter_paint` is the authored scatter-paint workflow in this toolset.

## Shipped Surface

- `PaintedPoints`
- `AttachedPoints`
- `StaticPoints`
- `PaintPointsTool`

## Current Verified Workflows

- authored layers, strokes, selection sets, and cache-backed point records
- cache validate, migrate, relink, upgrade, compact, export, and bake actions
- authored color precedence `point > stroke > layer > node default`
- `debugColor` display switching while keeping authored color in `scatterColor`
- layer visibility, mute, solo, mode, and frame-range filtering
- attachment diagnostics, `strictUnresolved`, `keepLastValidOutput`, fallback anchors, and per-instance resolve support
- compiled and Python-fallback regression coverage for cache and attachment behavior

## Runtime Registration

After side-loading the combined payload, Gaffer registers:

- `/Scatter/...` node menu entries for `PaintedPoints`, `AttachedPoints`, and `StaticPoints`
- `/Tools/Scatter Paint/...` graph, validation, repair, and inspection actions
- `/ScatterPaint/Demos/...`
- `/ScatterPaint/Benchmark/...`

## More Detail

Use the other docs tabs for deeper scatter-paint internals:

- Build
- Nodes
- Cache Schema
- Status
- Stats
