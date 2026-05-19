# Roadmap

## Current state

The plugin is no longer in placeholder/scaffold mode.

- `PaintedPoints` owns authored scatter data, cache migration/relink/upgrade/compact actions, export actions, validation, and bake helpers.
- `AttachedPoints` evaluates authored points into output geometry with diagnostics and fallback behavior.
- `StaticPoints` is the bake destination node.
- `PaintPointsTool` has compiled viewport interaction plus node-backed action plumbing.

## Cache milestone status

The cache-authority milestone is partially complete.

Completed:

1. Native C++ cache blob pack/unpack is implemented in `src/GafferScatterPaint/CacheFormat.cpp`.
2. Typed schema definitions live in `include/GafferScatterPaint/CacheFormat.h`.
3. Dict/schema bridge code exists in `src/GafferScatterPaint/CacheFormatDict.cpp`.
4. Compiled `PaintedPoints` cache IO uses the native C++ serializer/deserializer.
5. Compiled validation moved into `src/GafferScatterPaint/CacheValidation.cpp`.
6. Persisted `nextIds` are now modeled explicitly in the typed schema.
7. Compiled lock/time helper dependencies on Python blob helpers were removed.
8. Regression coverage now pins down cache serialization parity for point backups, lock blobs, and embedded/external roundtrips.
9. Authored color support now persists node/layer/stroke/point overrides in schema v2 and keeps compiled/Python output behavior aligned.
10. Broader packaged-runtime regression coverage is green for `AttachmentRegressionTest`, `PaintPointsToolTest`, and `PaintedPointsCacheActionsTest`.
11. Test compatibility shims now cover `cacheStoreSnapshot`, startup actions import, and UI startup degradation when the compiled tool is unavailable during bootstrap.

Still in progress:

1. Keep the Python fallback compatibility path byte-aligned with the C++ blob format.
2. Continue replacing older scaffold/spec wording with current shipped behavior in remaining docs and demo notes.
3. Reduce the remaining startup/test compatibility shims once compiled UI bootstrap is stable in all launch paths.

Deferred until after parity is locked down:

1. Replace the current dict-based runtime/edit mutation flow with a fully typed runtime model.
2. Reduce or remove dict-schema roundtripping inside runtime/edit helpers.
3. Revisit partial cache rewrites and deeper cache-performance work once format authority/parity is stable.

## Near-term priorities

1. Cache parity and authority cleanup
2. Cache-format and lock/validation regression coverage
3. Attachment and tool UX follow-up work
4. Remaining export/helper cleanup around compatibility paths
