# Paint Tool Plan

## Overview

Build a separate, side-loadable Gaffer plugin for high-performance point painting intended for scattering workflows.

The plugin must support:

- direct point painting in `SceneView`
- deforming surface attachment over time
- multi-mesh painting
- per-instance painting
- large strokes, including roughly `200k` points per paint
- external or embedded storage
- editing, layering, export, validation, and bake workflows

This is not a core Gaffer modification. It is a standalone extension.

## Plugin Architecture

Suggested modules:

- `GafferScatterPaint`
- `GafferScatterPaintUI`

Suggested package layout:

- `python/GafferScatterPaint/__init__.py`
- `python/GafferScatterPaintUI/__init__.py`
- `startup/GafferScatterPaint/...`
- `startup/GafferScatterPaintUI/...`
- `src/GafferScatterPaint/...`
- `src/GafferScatterPaintUI/...`
- `include/GafferScatterPaint/...`
- `include/GafferScatterPaintUI/...`
- `resources/...`

Load through:

- `GAFFER_STARTUP_PATHS`
- `PYTHONPATH`
- platform library path setup for compiled modules

## Core Components

### Painted Storage Node

Suggested name:

- `GafferScatterPaint::PaintedPoints`

Responsibilities:

- store authored layers, strokes, selections, and stroke attributes
- store compact attachment records
- support embedded or external storage
- output authored point data for downstream solve/evaluation

Storage must not use one child plug per point.
Use packed array/object-based storage.

### Attachment Solve Node

Suggested name:

- `GafferScatterPaint::AttachedPoints`

Responsibilities:

- resolve point attachment on animated/deforming surfaces
- reconstruct evaluated `P`, `N`, `up`, `orient`, `scale`, and related attrs
- support hybrid anchor solve rules
- provide diagnostics and validation

### SceneView Tool

Suggested name:

- `GafferScatterPaintUI::PaintPointsTool`

Responsibilities:

- paint
- erase
- select
- relax
- reproject
- manage brush preview and interaction modes
- commit strokes as bulk operations

Tool UX must support both:

- dedicated edit mode
- same-tool submodes

## Painting Contracts

### Brush Behavior

Required controls in initial HUD/toolbar:

- brush size
- density
- softness
- spacing
- erase toggle
- frame mode
- target filter
- relax
- reproject

Density means:

- points per brush dab

Brush profile:

- hard and soft

Brush hit default:

- front-most only

Painting through geometry:

- only via target filter
- affects all filtered targets in brush volume

Brush volume definition:

- selectable mode

### Randomization

Required in V1:

- tangent-frame rotation controls
- normal spin
- scale jitter
- width jitter

## Attachment Contracts

Primary attachment model:

- hybrid anchor

Store:

- barycentric attachment data
- rest surface/object-space position
- rest world-space position
- UV fallback data

Desired behavior:

- point stays where it was painted
- follows the surface as it deforms
- recomputes `N` and `up`
- remains attached over frames

No stable-topology-only restriction is acceptable.

When attachment cannot be resolved well enough:

- node errors
- last valid evaluated output remains available when possible
- diagnostics are exposed

### Surface Basis

Selectable mode between:

- viewport/evaluated mesh
- pre-subdivision cage

Default:

- viewport mesh

## Multi-Mesh and Instances

### Paint Scope

- one logical stroke can span multiple meshes
- one paint node can paint across multiple meshes

### Instances

Painting onto instanced geometry is required in V1.

Support:

- per-instance painting

Store instance identity using:

- instance identity/path
- instance transform plus source path

Edits across instances:

- only when explicitly multi-selected

## Frame and Layer Model

Required frame modes:

- `Persistent`
- `Additive`
- `Override`

Scope:

- both layer-level and stroke-level

Mode precedence:

- global node setting with overrides
- both layers and strokes can override

If stroke and layer modes conflict:

- selectable rule

Override semantics:

- whole paint node output on affected frames
- manual reorder defines precedence
- last in manual order wins

Frame layer behavior:

- supports per-frame override
- supports persistent plus additive
- supports frame ranges
- outside active range holds nearest frame

Editing state while layer visibility/mute/solo is animated:

- selectable mode

Default:

- current animated state

## Layer System

Structure:

- layers with strokes

Both layers and strokes are:

- user-nameable
- reorderable

Layer controls required in V1:

- visibility
- mute
- solo

All of the above are animatable.

Mute semantics:

- selectable mode

Solo semantics:

- selectable mode

Layer timing must be editable directly in the paint tool UI.

Merge operations required:

- merge strokes
- merge layers

Merge conflict resolution:

- ask on stroke merge
- prompt on layer merge

## Selection and Editing

Selection model:

- transient tool selection
- stored named selections
- multiple named selection sets

Selection tools required:

- brush select
- lasso/marquee select

Editing operations required:

- select and delete strokes
- relax/reproject
- subset delete

Subset delete behavior:

- splits strokes
- resulting pieces get new stroke IDs only

Relax and Reproject scope:

- both selected points and whole stroke

Relax operates in:

- attachment anchor space

Relax default optimization mode:

- selectable between preserve silhouette and even redistribution

Reproject behavior:

- updates all anchors
- may move across meshes

No symmetry required in V1.

## Tablet and Pressure

Tablet pressure support is required in V1.

Pressure affects:

- density
- softness

Pressure mapping mode:

- selectable

Options:

- direct pressure
- pressure curve

Pressure curve/settings live in:

- tool with node-backed defaults

Node-side pressure defaults:

- node-wide defaults
- per-layer overrides

Stroke capture behavior for pressure values:

- selectable between baked and live-referenced behavior

If no pressure device is present:

- fixed defaults
- optional mouse/modifier emulation

## Output Attributes

Required output attrs in V1:

- `P`
- `N`
- `orient`
- `up`
- `width`
- `scale`
- `id`
- `seed`
- `sourcePath`
- `strokeId`

`orient` generation:

- from `N` and `up`

`scale` meaning:

- uniform scalar expanded to `V3`

## ID Contracts

Point IDs must remain stable across:

- cache export/import
- cache IO round-trips

They do not need to remain untouched across all edits.

Rules:

- reorder does not regenerate IDs
- only affected strokes regenerate IDs after edits

## Storage and Cache Contracts

### Storage Modes

Support both:

- embedded in `.gfr`
- external cache files

Switching storage mode:

- explicit migration required
- migration exposed as node action

### Cache Path Resolution

Relative path resolution:

1. script location
2. project root variable

Users can also choose a specific folder.

### External Cache Format

- no extra dependency such as HDF5
- strict versioned format
- plugin-native authored cache schema

If incompatible cache version is found:

- explicit upgrade action
- upgrade writes a new cache
- detailed upgrade report is shown

### Missing Cache

If cache is missing:

- offer relink action

If relink fails:

- stay unresolved
- expose diagnostics

### Backups

- optional backups/versioned writes
- default off

### Locking

External cache writes require:

- session-aware lock

Record:

- user
- host machine
- timestamp
- project/script path

### Compaction

Auto-compaction required:

- immediately after edits
- blocking
- with progress UI

## Validation and Diagnostics

Node-level validation action required in V1:

- `validate cache`
- `validate attachments`

Validation output mode:

- detailed categories

Diagnostics exposed via node plugs and metadata.

Required diagnostics include:

- `invalidPointCount`
- `invalidStrokeCount`
- `failingFrame`
- `failingTargetPaths`
- `lastErrorMessage`
- `cacheMode`
- `cachePath`
- `topologyMismatchCount`

## Export and Interchange

V1 exports required:

- plugin authored cache
- evaluated points geometry
- layer/stroke interchange
- diagnostics report

### Geometry Export

All required in V1:

- Gaffer scene export
- USD
- Alembic

Priority order:

1. Gaffer scene export
2. USD
3. Alembic

### Interchange Format

- plugin cache schema

### Export Presets

Provide:

- minimal
- full
- custom

Generated attrs export should use selectable presets.

## Freeze / Bake

Required in V1.

Behavior:

- freeze/bake attachment to a separate static node

Time modes:

- single frame
- frame range

## Undo Contracts

Stroke painting:

- one undo per stroke

Layer/stroke reorder, rename, visibility, mute, solo:

- one action each

## Batch / Non-Interactive Behavior

Merge behavior in batch contexts:

- configurable

## Recommended Implementation Order

1. Define packed storage schema for layers, strokes, selections, and anchors
2. Implement `PaintedPoints` storage node
3. Implement `AttachedPoints` hybrid attachment solve
4. Implement cache system, versioning, locks, migration, compaction, and diagnostics
5. Implement `PaintPointsTool` interaction modes
6. Implement layer system, selections, relax/reproject, and merge actions
7. Implement validation actions
8. Implement exports:
   - Gaffer scene export
   - USD
   - Alembic
   - authored cache/interchange
   - diagnostics report
9. Implement freeze/bake to separate static node
10. Stress test very large strokes, multi-mesh cases, deforming meshes, and per-instance workflows

## Summary

This plugin is a production-scale paint system, not a lightweight viewport helper.

The defining contracts are:

- separate side-loadable plugin
- high-performance C++ hot path
- multi-mesh and per-instance painting
- hybrid surface attachment over time
- layers with strokes
- full editing model
- strict cache/version/lock system
- diagnostics and validation built in
- export and bake workflows included in V1
