---
title: Status
icon: fas fa-list-check
order: 8
---

# Scatter Paint Status

## Current State

- `PaintedPoints` owns authored scatter data, cache migration/relink/upgrade/compact actions, export actions, validation, and bake helpers
- `AttachedPoints` evaluates authored points into output geometry with diagnostics and fallback behavior
- `StaticPoints` is the bake destination node
- `PaintPointsTool` has compiled viewport interaction plus node-backed action plumbing

## Completed

1. Native C++ cache blob pack/unpack is implemented.
2. Typed schema definitions live in `include/GafferScatterPaint/CacheFormat.h`.
3. Dict/schema bridge code exists in compiled code.
4. Compiled `PaintedPoints` cache IO uses the native serializer/deserializer.
5. Compiled validation moved into dedicated cache validation code.
6. Persisted `nextIds` are modeled explicitly in the typed schema.
7. Compiled lock/time helper dependencies on Python blob helpers were removed.
8. Regression coverage pins down cache serialization compatibility for point backups, lock blobs, and embedded/external roundtrips.
9. Authored color support persists node/layer/stroke/point overrides in schema v2.
10. Broader packaged-runtime regression coverage is green for the main scatter-paint test suites.

## Current Compatibility Constraints

1. Keep the Python fallback compatibility path byte-aligned with the C++ blob format.
2. Startup and test compatibility shims remain in place around bootstrap and mixed compiled/Python launch paths.
3. Runtime/edit mutation still passes through dict-shaped helpers even though cache IO is authoritative in C++.

## Current Implementation Boundaries

1. The Python compatibility path remains part of the shipped runtime surface.
2. Dict/schema conversion remains part of the current edit/runtime implementation.
3. Partial cache rewrite optimization is not part of the current delivered contract.
