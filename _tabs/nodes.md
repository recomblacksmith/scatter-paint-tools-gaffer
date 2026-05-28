---
title: Nodes
icon: fas fa-project-diagram
order: 6
---

# Scatter Paint Nodes

The scatter-paint workflow centers on three nodes.

## `PaintedPoints`

Authoring node for:

- layers, strokes, point records, selection sets, diagnostics, upgrades, and backup data
- tool-driven paint, erase, relax, reproject, and edit actions
- cache actions such as validate, migrate, relink, upgrade, compact, export, and bake
- authored color precedence `point > stroke > layer > node default`

## `AttachedPoints`

Solve/evaluation node for:

- resolving painted points back onto scene geometry
- emitting evaluated output points at an output location
- reporting resolved/unresolved counts, topology mismatches, failing targets, and solve status
- applying fallback behavior such as `keepLastValidOutput`, `strictUnresolved`, and optional cross-mesh reprojection support

## `StaticPoints`

Frozen-output node for:

- authored bakes
- evaluated bakes
- selection-only bakes
- frame-sampled baked playback

## Common Graph Shapes

```text
source scene -> PaintedPoints -> AttachedPoints -> preview/render/export
```

Bake paths:

```text
PaintedPoints -> StaticPoints
PaintedPoints -> AttachedPoints -> StaticPoints
```
