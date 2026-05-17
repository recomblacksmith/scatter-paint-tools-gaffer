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

Still in progress:

1. Finish Python fallback parity so the compatibility path matches the C++ blob format exactly.
2. Align Python fallback validation with native multi-chunk validation behavior.
3. Add serialization-specific parity coverage for backups, lock blobs, and mixed-path roundtrips.
4. Update docs/tests to reflect C++ cache authority instead of older Python-source-of-truth wording.

Deferred until after parity is locked down:

1. Replace the current dict-based runtime/edit mutation flow with a fully typed runtime model.
2. Reduce or remove dict-schema roundtripping inside runtime/edit helpers.
3. Revisit partial cache rewrites and deeper cache-performance work once format authority/parity is stable.

## Near-term priorities

1. Cache parity and authority cleanup
2. Cache-format and lock/validation regression coverage
3. Remaining export/helper cleanup around compatibility paths
4. Attachment and tool UX/performance follow-up work
