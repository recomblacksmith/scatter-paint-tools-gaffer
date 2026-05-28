# Current Status

## Current state

The plugin is now in active implementation mode.

- `PaintedPoints` owns authored scatter data, cache migration/relink/upgrade/compact actions, export actions, validation, and bake helpers.
- `AttachedPoints` evaluates authored points into output geometry with diagnostics and fallback behavior.
- `StaticPoints` is the bake destination node.
- `PaintPointsTool` has compiled viewport interaction plus node-backed action plumbing.

## Cache and runtime status

The cache/runtime cleanup is well underway.

Completed:

1. Native C++ cache blob pack/unpack is implemented in `src/GafferScatterPaint/CacheFormat.cpp`.
2. Typed schema definitions live in `include/GafferScatterPaint/CacheFormat.h`.
3. Dict/schema bridge code exists in `src/GafferScatterPaint/CacheFormatDict.cpp`.
4. Compiled `PaintedPoints` cache IO uses the native C++ serializer/deserializer.
5. Compiled validation moved into `src/GafferScatterPaint/CacheValidation.cpp`.
6. Persisted `nextIds` are now modeled explicitly in the typed schema.
7. Compiled lock/time helper dependencies on Python blob helpers were removed.
8. Regression coverage now pins down cache serialization compatibility for point backups, lock blobs, and embedded/external roundtrips.
9. Authored color support now persists node/layer/stroke/point overrides in schema v2 and keeps compiled/Python output behavior aligned.
10. Broader packaged-runtime regression coverage is green for `AttachmentRegressionTest`, `PaintPointsToolTest`, and `PaintedPointsCacheActionsTest`.
11. Test compatibility shims now cover `cacheStoreSnapshot`, startup actions import, and UI startup degradation when the compiled tool is unavailable during bootstrap.

## Current compatibility constraints

1. Keep the Python fallback compatibility path byte-aligned with the C++ blob format.
2. Startup and test compatibility shims remain in place around bootstrap and mixed compiled/Python launch paths.
3. Runtime/edit mutation still passes through dict-shaped helpers even though cache IO is now authoritative in C++.

## Current implementation boundaries

1. The Python compatibility path remains part of the shipped runtime surface and must stay byte-compatible with the compiled blob format.
2. Dict/schema conversion remains part of the current edit/runtime implementation.
3. Partial cache rewrite optimization is not part of the current delivered contract.
