---
title: Nodes
icon: fas fa-project-diagram
order: 6
---

# Scatter Paint Nodes

Scatter Paint is built around three main nodes.

You do not need to understand every internal detail to use them. The simple version is:

- one node stores painted work
- one node evaluates that work on geometry
- one node bakes the result

## `PaintedPoints`

This is the authoring node.

It is where your painted scatter data lives.

It handles:

- layers, strokes, point records, selection sets, diagnostics, upgrades, and backup data
- tool-driven paint, erase, relax, reproject, and edit actions
- cache actions such as validate, migrate, relink, upgrade, compact, export, and bake
- color coming from point, stroke, layer, or node defaults

## `AttachedPoints`

This is the evaluation node.

It takes painted data and puts it back onto scene geometry so you can preview, render, or export it.

It handles:

- resolving painted points back onto scene geometry
- emitting evaluated output points at an output location
- reporting resolved/unresolved counts, topology mismatches, failing targets, and solve status
- keeping useful fallback behavior when the source changes or partially breaks

## `StaticPoints`

This is the bake node.

Use it when you want a lighter, frozen result.

It supports:

- authored bakes
- evaluated bakes
- selection-only bakes
- frame-sampled baked playback

## Common Graph Shapes

```text
source scene -> PaintedPoints -> AttachedPoints -> preview/render/export
```

Common bake paths:

```text
PaintedPoints -> StaticPoints
PaintedPoints -> AttachedPoints -> StaticPoints
```
