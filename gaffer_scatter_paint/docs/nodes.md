# Scatter Paint Nodes

This plugin's shipped workflow centers on three nodes:

- `PaintedPoints` stores authored scatter data.
- `AttachedPoints` evaluates that authored data into usable output points in the scene.
- `StaticPoints` is the bake/export destination node for frozen authored or evaluated output.

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
- layer, stroke, and selection metadata used by the tool and downstream solve

Current authored workflows covered by the node and its tests:

- layer/stroke visibility, mute, solo, ordering, and frame-range controls
- named selection sets plus current point/stroke selection state
- selection-driven relax and reproject actions
- export helpers for authored cache, interchange blobs, diagnostics, evaluated points, and scene files
- bake helpers that create `StaticPoints` nodes from authored, selected, or evaluated scatter data

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

Current evaluated workflows covered by the node and its tests:

- layer/stroke filtering and override precedence
- strict vs non-strict unresolved handling with last-valid-output fallback
- per-instance attachment resolve using stored instance identity
- hybrid stored fallback anchors and optional cross-mesh reprojection behavior
- authored-color output via `scatterColor` with optional debug coloring in `Cs`

## `StaticPoints`

`StaticPoints` is the frozen-output node.

What it does:

- stores already-baked point data in a lightweight node-local payload
- outputs that data at a chosen location without re-running live attachment solve
- supports authored bakes, evaluated bakes, and selection-only bakes produced from `PaintedPoints`
- can store frame-sampled baked data for simple range playback

How to think about it:

- use `StaticPoints` when you want a stable scene result that no longer depends on the live authoring graph
- it is the handoff node for exports, range bakes, or lighter downstream scene graphs

## Short version

- use `PaintedPoints` to paint and manage scatter data
- use `AttachedPoints` to evaluate that painted data onto geometry and get the final attached point output
- use `StaticPoints` when you want to freeze authored or evaluated results for handoff/export

In practice, the common graph is:

`source scene -> PaintedPoints -> AttachedPoints -> preview/render/export`

And the common bake paths are:

`PaintedPoints -> StaticPoints`

or

`PaintedPoints -> AttachedPoints -> StaticPoints`
