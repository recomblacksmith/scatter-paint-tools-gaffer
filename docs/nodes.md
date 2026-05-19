# Scatter Paint Nodes

This plugin's core workflow is split into two main nodes:

- `PaintedPoints` stores authored scatter data.
- `AttachedPoints` evaluates that authored data into usable output points in the scene.

## `PaintedPoints`

`PaintedPoints` is the authoring node.

What it does:

- stores the painted scatter cache: layers, strokes, point records, selection sets, diagnostics, upgrades, and backup data
- receives paint/erase/edit actions from the `PaintPointsTool`
- keeps brush defaults and paint settings such as size, density, softness, spacing, points-per-dab, filters, cache mode, and color defaults
- supports cache actions such as validate, migrate, relink, upgrade, compact, export, and bake
- owns authored color overrides with precedence `point > stroke > layer > node default`

How to think about it:

- `PaintedPoints` is the editable source-of-truth for what the artist painted
- it is where strokes are created, stored, modified, deleted, and serialized
- it does not mainly exist to produce the final attached scatter result; it exists to hold the authored scatter state

Important outputs/state it carries:

- `cacheBlob` for the packed cache payload
- metadata plugs such as authored point count, invalid counts, failing targets, cache version, and validation summary
- layer and selection metadata used by the tool and downstream solve

## `AttachedPoints`

`AttachedPoints` is the solve/evaluation node.

What it does:

- reads authored points from the scatter workflow and evaluates them against the current input scene
- resolves each painted point back onto its target surface using stored attachment data such as path, instance info, triangle index, and barycentric coordinates
- outputs the evaluated scatter points at an output location such as `/scatter`
- reports diagnostics such as resolved/unresolved counts, topology mismatches, failing targets, and solve status
- supports fallback behavior through plugs like `keepLastValidOutput`, `strictUnresolved`, and `allowCrossMeshReproject`
- can switch `Cs` into debug attachment coloring with `debugColor`, while keeping authored color in `scatterColor`

How to think about it:

- `AttachedPoints` is the runtime "make my painted data stick to the surface now" node
- when the source surface deforms, changes frame, or partially breaks, this node is responsible for re-solving the painted points and surfacing failures
- it is the node you use to preview and render the evaluated scatter result, not to author strokes directly

## Short version

- use `PaintedPoints` to paint and manage scatter data
- use `AttachedPoints` to evaluate that painted data onto geometry and get the final attached point output

In practice, the common graph is:

`source scene -> PaintedPoints -> AttachedPoints -> preview/render/export`
