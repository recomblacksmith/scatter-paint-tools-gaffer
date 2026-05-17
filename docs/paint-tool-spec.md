**Spec**

**1. Plugin / Module Layout**

1. Plugin name: `GafferScatterPaint`
2. UI module name: `GafferScatterPaintUI`
3. Side-loading mechanism:
   - `GAFFER_STARTUP_PATHS`
   - `PYTHONPATH`
   - platform library path for compiled modules
4. Repo/package layout:
   - `include/GafferScatterPaint/`
   - `include/GafferScatterPaintUI/`
   - `src/GafferScatterPaint/`
   - `src/GafferScatterPaintUI/`
   - `python/GafferScatterPaint/__init__.py`
   - `python/GafferScatterPaintUI/__init__.py`
   - `startup/GafferScatterPaint/`
   - `startup/GafferScatterPaintUI/`
   - `resources/icons/`
   - `resources/shaders/`
   - `resources/examples/`
5. Compiled libraries:
   - `GafferScatterPaint` contains nodes, data objects, cache IO, validation, export, bake
   - `GafferScatterPaintUI` contains tool, overlay gadgets, UI actions, metadata registration
6. Python startup responsibilities:
   - register node metadata
   - register NodeMenu entries
   - register `SceneView` tool
   - register custom widgets and actions
7. Public API surface:
   - only public Gaffer/GafferScene/GafferUI/GafferSceneUI APIs
   - avoid `Private/` headers
8. Core source groups:
   - `Data/`
   - `Nodes/`
   - `IO/`
   - `Validation/`
   - `Export/`
   - `UI/Tool/`
   - `UI/Widgets/`
   - `UI/Actions/`

**2. Node Classes And Plugs**

**2.1 `GafferScatterPaint::PaintedPoints`**

1. Purpose:
   - authoritative authored data model
   - stores layers, strokes, selections, cache settings, edit defaults
   - outputs authored point records and optionally lightweight preview scene
2. Main plugs:
   - `in` : `GafferScene::ScenePlug`
   - `out` : `GafferScene::ScenePlug`
   - `enabled` : `BoolPlug`
   - `targetFilter` : `StringPlug`
   - `targetSetFilter` : `StringPlug`
   - `surfaceMode` : `IntPlug`
   - enum values:
     - `ViewportMesh`
     - `PreSubdivCage`
   - `paintThroughMode` : `IntPlug`
   - enum values:
     - `FrontMostOnly`
     - `FilteredBrushVolume`
   - `cacheMode` : `IntPlug`
   - enum values:
     - `Embedded`
     - `External`
   - `cachePath` : `StringPlug`
   - `cachePathMode` : `IntPlug`
   - enum values:
     - `Absolute`
     - `RelativeToScript`
     - `RelativeToProject`
   - `projectRoot` : `StringPlug`
   - `lockMode` : `IntPlug`
   - enum value:
     - `SessionAware`
   - `backupEnabled` : `BoolPlug`
   - `backupPolicy` : `IntPlug`
   - enum values:
     - `Off`
     - `On`
   - `compactionMode` : `IntPlug`
   - enum value:
     - `ImmediateBlocking`
   - `globalModePrecedence` : `IntPlug`
   - enum values:
     - `StrokeWins`
     - `LayerWins`
   - `pressureDefaults` : compound plug group
   - `pressureDefaults.enabled` : `BoolPlug`
   - `pressureDefaults.mappingMode` : `IntPlug`
   - enum values:
     - `Direct`
     - `Curve`
   - `pressureDefaults.densityCurve` : spline/compound plug
   - `pressureDefaults.softnessCurve` : spline/compound plug
   - `brushDefaults` : compound plug group
   - `brushDefaults.size`
   - `brushDefaults.density`
   - `brushDefaults.softness`
   - `brushDefaults.spacing`
   - `brushDefaults.rotationMode`
   - `brushDefaults.scaleJitter`
   - `brushDefaults.widthJitter`
3. Layer container:
   - `layers` : custom compound array or opaque authored-data plug
   - per layer logical fields:
     - `name`
     - `enabled`
     - `visible`
     - `mute`
     - `solo`
     - `mode`
     - `modeOverrideEnabled`
     - `pressureOverrideEnabled`
     - `pressureDefaults`
     - `timeMode`
     - `frameStart`
     - `frameEnd`
     - `holdOutsideRange`
     - `strokes`
4. Stroke logical fields:
   - `id`
   - `name`
   - `layerId`
   - `mode`
   - `modeOverrideEnabled`
   - `createdTime`
   - `orderIndex`
   - `frameMode`
   - `frameStart`
   - `frameEnd`
   - `anchorStorageRef`
   - `pointCount`
   - `targetSummary`
   - `selectionMaskRef`
5. Selection storage:
   - `selectionSets` : authored-data plug
   - multiple named sets
   - current selection persists if requested
6. Diagnostics plugs:
   - `invalidPointCount`
   - `invalidStrokeCount`
   - `failingFrame`
   - `failingTargetPaths`
   - `lastErrorMessage`
   - `topologyMismatchCount`
   - `validationSummary`
   - `cacheResolvedPath`
   - `cacheVersion`
   - `cacheLockedBy`
   - `cacheLockedHost`
   - `cacheLockedTime`
   - `cacheLockedScript`
7. Actions exposed on node:
   - `migrateCacheMode`
   - `relinkCache`
   - `upgradeCache`
   - `validateCache`
   - `validateAttachments`
   - `compactCache`
   - `exportAuthoredCache`
   - `exportInterchange`
   - `exportDiagnostics`
   - `freezeBakeToStaticNode`

**2.2 `GafferScatterPaint::AttachedPoints`**

1. Purpose:
   - resolves authored anchors against current scene at evaluation time
   - produces evaluated points scene
2. Main plugs:
   - `in` : `ScenePlug`
   - `points` : input connection from `PaintedPoints.out`
   - `out` : `ScenePlug`
   - `enabled` : `BoolPlug`
   - `outputLocation` : `StringPlug`
   - `pointType` : `StringPlug`
   - `includeAttributes` : `StringPlug`
   - `exportPreset` : `IntPlug`
   - enum values:
     - `Minimal`
     - `Full`
     - `Custom`
   - `surfaceSolveMode` : `IntPlug`
   - enum values:
     - `Hybrid`
   - `allowCrossMeshReproject` : `BoolPlug`
   - `keepLastValidOutput` : `BoolPlug`
   - `strictUnresolved` : `BoolPlug`
3. Output attrs guaranteed:
   - `P`
   - `N`
   - `up`
   - `orient`
   - `width`
   - `scale`
   - `id`
   - `seed`
   - `sourcePath`
   - `strokeId`
4. Internal evaluation responsibilities:
   - resolve instance identity
   - choose anchor path
   - reconstruct frame
   - filter by layer/stroke frame state
   - apply mute/solo/visibility rules
   - apply override precedence
5. Diagnostics plugs mirror `PaintedPoints` plus:
   - `resolvedPointCount`
   - `unresolvedPointCount`
   - `lastValidFrame`
   - `solveStatus`
   - `validationCategories`

**2.3 `GafferScatterPaint::StaticPoints`**

1. Purpose:
   - output static baked points
   - result of freeze/bake action
2. Main plugs:
   - `out`
   - `enabled`
   - `pointData`
   - `outputLocation`
   - `pointType`
3. This is the destination for bake operations, not the authored layer store.

**2.4 Optional helper nodes**

1. `GafferScatterPaint::PaintExport`
   - wraps export actions for non-interactive pipelines
2. `GafferScatterPaint::PaintValidation`
   - batch validation node/utility facade
3. `GafferScatterPaint::PaintInterchange`
   - import/export interchange helper

**3. Cache Schema**

**3.1 Storage modes**

1. Embedded mode:
   - packed binary blob stored in node-owned data plug
2. External mode:
   - versioned plugin-native file format
   - no HDF5 dependency
3. Switching mode:
   - explicit node action only
   - user migration required
   - no automatic transparent switch

**3.2 File structure**

1. Header block:
   - magic
   - schema version
   - plugin version
   - endianness
   - content flags
   - checksum
2. Lock metadata block:
   - user
   - host
   - timestamp
   - script/project path
3. Node metadata block:
   - cache mode
   - relative path base mode
   - export preset defaults
4. Layer table:
   - layer ids
   - names
   - order
   - anim flags
   - mode defaults
   - pressure defaults
   - visibility/mute/solo animation refs
5. Stroke table:
   - stroke ids
   - layer ids
   - names
   - order
   - mode
   - frame mode
   - frame start/end
   - created time
   - point range refs
   - target refs
6. Point record arrays:
   - stroke-local or chunk-local contiguous arrays
   - chunked for large strokes
7. Named selection sets:
   - set name
   - point membership refs
   - stroke membership refs
8. Diagnostics snapshot block:
   - optional cached validation results
9. Upgrade history block:
   - source version
   - upgraded version
   - timestamp
   - notes/report ref

**3.3 Point record schema**

1. Core identity fields:
   - `pointId`
   - `strokeId`
   - `layerId`
   - `targetPathId`
   - `instanceId`
   - `instanceSourcePathId`
2. Hybrid anchor fields:
   - `triangleIndex`
   - `barycentric`
   - `restObjectP`
   - `restWorldP`
   - `restUV`
   - `restNormal`
   - `restUp`
3. Output/value fields:
   - `width`
   - `uniformScale`
   - `seed`
   - `normalSpin`
   - tangent-frame rotation values
   - baked pressure values when chosen
4. Status fields:
   - `valid`
   - `lastValidFrame`
   - `anchorModeUsed`
   - `topologyGeneration`
5. Optional per-point baked attrs:
   - baked `orient`
   - baked `scale`
   - baked `pressureDensity`
   - baked `pressureSoftness`

**3.4 Path and identity dictionaries**

1. Dictionary tables:
   - scene paths
   - instance source paths
   - layer names
   - stroke names
2. Path ids used in records to minimize size.

**3.5 Chunking**

1. Strokes are chunked internally.
2. Chunking is required for:
   - `200k`+ point strokes
   - partial rewrite
   - selection updates
   - compaction
3. Reordering does not rewrite point ids.

**3.6 Locking**

1. Session-aware write lock file or lock section.
2. Lock must prevent concurrent writers.
3. Must support stale-lock detection or explicit unresolved state.
4. Read-only access allowed while unlocked or with compatible policy.

**3.7 Upgrade behavior**

1. Incompatible version does not auto-evaluate.
2. Node exposes explicit `upgrade` action.
3. Upgrade writes a new cache file.
4. Old cache remains untouched.
5. Upgrade emits detailed report:
   - version delta
   - schema changes
   - data-loss warnings
   - migrated sections
   - failed sections

**3.8 Compaction**

1. Trigger:
   - immediately after edits
2. Behavior:
   - blocking
   - progress UI required
3. Cleanup targets:
   - stale stroke chunks
   - deleted selection data
   - superseded chunk generations
4. Backups:
   - optional
   - off by default

**4. Tool Modes And Commands**

**4.1 Main tool**

1. Class: `GafferScatterPaintUI::PaintPointsTool`
2. Registers on `GafferSceneUI::SceneView`
3. Supports both:
   - single tool with submodes
   - dedicated edit mode entry path

**4.2 Modes**

1. `Paint`
   - add points
   - points per dab
   - hard/soft brush
   - tangent-frame randomization
   - optional tablet pressure on density/softness
2. `Erase`
   - visible-space erase
   - attachment-space erase
   - both selectable
3. `SelectBrush`
   - brush subset selection
4. `SelectLasso`
   - lasso/marquee subset selection
5. `Relax`
   - selected points or whole stroke
   - anchor-space operation
   - selectable objective:
     - preserve silhouette
     - even redistribution
6. `Reproject`
   - selected points or whole stroke
   - updates all anchors
   - may cross meshes
   - cross-instance only when explicitly multi-selected
7. `LayerEdit`
   - rename layer
   - reorder layer
   - visibility/mute/solo
   - mode precedence override
   - timing controls
8. `StrokeEdit`
   - rename stroke
   - reorder stroke
   - merge stroke
   - delete stroke
   - split by subset delete

**4.3 Toolbar / HUD controls**

1. `size`
2. `density`
3. `softness`
4. `spacing`
5. `erase toggle`
6. `frame mode`
7. `target filter`
8. `relax`
9. `reproject`
10. `surface basis`
11. `paint-through mode`
12. `pressure mapping mode`
13. `pressure curve access`
14. `mode precedence rule`
15. `mute behavior`
16. `solo behavior`

**4.4 Frame modes**

1. `Persistent`
   - always contributes
2. `Additive`
   - adds over persistent content for frame or range
3. `Override`
   - replaces whole node output for affected frame/range
   - precedence defined by manual ordering
4. Range behavior:
   - supports single frame
   - supports frame range
   - outside range holds nearest frame

**4.5 Command set**

1. `createLayer`
2. `deleteLayer`
3. `renameLayer`
4. `moveLayer`
5. `mergeLayers`
6. `setLayerVisible`
7. `setLayerMute`
8. `setLayerSolo`
9. `setLayerTimeRange`
10. `createStroke`
11. `deleteStroke`
12. `renameStroke`
13. `moveStroke`
14. `mergeStrokes`
15. `splitStrokeBySelection`
16. `createSelectionSet`
17. `renameSelectionSet`
18. `deleteSelectionSet`
19. `storeCurrentSelection`
20. `paintStrokeCommit`
21. `eraseCommit`
22. `relaxSelection`
23. `reprojectSelection`
24. `freezeBakeSelection`
25. `exportAuthoredCache`
26. `exportEvaluatedPoints`
27. `exportInterchange`
28. `exportDiagnostics`
29. `validateCache`
30. `validateAttachments`
31. `migrateCache`
32. `relinkCache`
33. `upgradeCache`
34. `compactCache`

**4.6 Tablet behavior**

1. Pressure affects:
   - density
   - softness
2. Mapping selectable:
   - direct
   - curve
3. Tool uses node-backed defaults.
4. No-tablet fallback:
   - fixed defaults
   - optional modifier emulation

**4.7 Undo rules**

1. One undo per stroke paint commit.
2. Layer and stroke rename/reorder/visibility are one action each.
3. Merge and bake are one action each.
4. Split-by-delete is one action.

**5. Evaluation Contracts And Error Rules**

**5.1 Evaluation pipeline**

1. `PaintedPoints` reads authored storage.
2. Active layer/stroke set is resolved for current frame.
3. Layer visibility/mute/solo are evaluated using animated state or canonical state according to selected edit/eval rule.
4. Override precedence is applied:
   - manual order
   - last wins
5. Attachment solve chooses hybrid resolution path:
   - barycentric on matching topology target
   - fallback anchor path using stored object/world/UV data
   - reproject path when required by tool operation
6. Evaluated attrs are built:
   - `P`
   - `N`
   - `up`
   - `orient`
   - `width`
   - `scale`
   - ids and metadata
7. Output scene is generated at configured location.

**5.2 Surface solve contract**

1. Default surface basis is viewport mesh.
2. Selectable alternate basis is pre-subdiv cage.
3. Painting on instances is per-instance.
4. Instance identity uses both:
   - instance identity/path
   - transform plus source path
5. Default painting hits front-most surface only.
6. Through-paint allowed only via target filter.
7. Through-paint affects all filtered targets in brush volume.

**5.3 ID contract**

1. Point ids stable across cache export/import.
2. Reorder does not regenerate ids.
3. Only affected strokes regenerate ids after destructive edits.
4. Split strokes create new stroke ids.
5. Unaffected strokes keep their existing ids.

**5.4 Export contract**

1. Required exports:
   - authored cache
   - evaluated points geometry
   - layer/stroke interchange
   - diagnostics report
2. Geometry exports required:
   - Gaffer scene export first
   - USD
   - Alembic
3. Export presets:
   - minimal
   - full
   - custom

**5.5 Bake contract**

1. Freeze/bake outputs to separate static node.
2. Supports:
   - single frame
   - frame range
3. Bake result is detached from attachment solve.

**5.6 Missing / invalid cache rules**

1. Missing cache:
   - node exposes relink action
2. Relink failure:
   - node remains unresolved
   - diagnostics stay visible
3. Last valid output:
   - still exposed when available

**5.7 Validation contract**

1. Node must expose explicit validation actions.
2. Validation result categories include:
   - cache
   - lock
   - topology
   - attachment
   - export readiness
   - upgrade state
3. Validation status should not be only pass/fail.
4. Detailed category outputs required via plugs/metadata.

**5.8 Error rules**

1. Unresolved attachment beyond acceptable solve threshold:
   - hard node error
   - last valid output retained if available
   - diagnostics updated
2. Incompatible cache version:
   - no implicit read
   - explicit upgrade action only
3. Missing write lock or conflicting session lock:
   - write denied
   - diagnostics report lock owner metadata
4. Missing cache after relink attempt:
   - unresolved state
   - no silent fallback unless explicitly supported by future mode
5. Batch merge requiring prompt:
   - configurable policy
   - either defaults or reject, per runtime setting

**5.9 Diagnostics contract**

1. Required plugs/metadata include:
   - `invalidPointCount`
   - `invalidStrokeCount`
   - `failingFrame`
   - `failingTargetPaths`
   - `lastErrorMessage`
   - `cacheMode`
   - `cachePath`
   - `topologyMismatchCount`
2. Additional recommended evaluation diagnostics:
   - `resolvedPointCount`
   - `unresolvedPointCount`
   - `lastValidFrame`
   - `cacheVersion`
   - `lockOwner`
   - `lockHost`
   - `lockTimestamp`
   - `validationSummary`

**5.10 Non-functional requirements**

1. All hot paths in C++.
2. No per-point plug authoring.
3. Bulk stroke commit only.
4. Cache format chunked and versioned.
5. Compaction supported immediately after edit.
6. Writes protected by session-aware lock.
7. Large-stroke performance is first-class design target.

If you want, next I can turn this into:
1. a class-by-class header spec
2. a cache binary layout spec
3. a phased implementation milestone plan
