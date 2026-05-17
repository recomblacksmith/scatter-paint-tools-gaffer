# Cache Schema

`GafferScatterPaint` uses a strict plugin-native cache schema shared by embedded and external storage modes.

## Versioning

- magic: `GSPAINT`
- schema version: `1`
- explicit plugin version fields in the cache header
- incompatible schema changes require an explicit upgrade action

## Top-level sections

1. `CacheHeader`
2. `LockMetadata`
3. `NodeMetadata`
4. path dictionaries
5. `LayerRecord[]`
6. `StrokeRecord[]`
7. `PointChunkRecord[]`
8. `PointRecord[]`
9. `SelectionSetRecord[]`
10. `CurrentSelectionRecord`
11. `DiagnosticsSnapshot`
12. `UpgradeRecord[]`
13. `PointBackupRecord[]` keyed by `pointId`

## Storage rules

- embedded mode stores the same logical schema inside node-owned custom blob data
- external mode stores the same schema in a plugin-native custom blob file format
- switching storage mode is explicit and never automatic

The native C++ cache format implementation in `src/GafferScatterPaint/CacheFormat.cpp` is now the authoritative serialized-layout implementation for compiled cache IO.

- `include/GafferScatterPaint/CacheFormat.h` is the typed schema contract.
- `src/GafferScatterPaint/CacheFormat.cpp` is the canonical blob pack/unpack implementation.
- `src/GafferScatterPaint/CacheFormatDict.cpp` maps between the typed schema and the existing dict-shaped runtime/edit helpers.
- Python fallback helpers in `python/GafferScatterPaint/_core_shared.py` are compatibility-path implementations and must remain byte-compatible with the C++ format.
- `python/GafferScatterPaint/_storebridge.py` is now only a compatibility shim over the shared Python path.

The bigger runtime migration away from dict-based mutation is still future work. For now, authoritative cache IO is C++ while Python fallback keeps the same on-disk/on-blob schema for parity.

## Persisted next ids

- persisted `nextIds` are part of the serialized schema and track the next layer/stroke/point/selectionSet/chunk ids
- the typed schema now models them explicitly instead of reconstructing them from max existing ids
- Python and C++ compatibility paths both preserve them through roundtrip IO

## Identity rules

- `pointId` is stable across cache IO
- reordering layers and strokes does not regenerate `pointId`
- destructive edits only regenerate ids for affected strokes
- selection sets reference stable point and stroke ids

## Chunking

- the logical cache schema supports chunk metadata per stroke
- both the Python fallback and the compiled C++ path now chunk large strokes by `CHUNK_POINT_LIMIT = 8192`
- chunk records are contiguous per stroke and use zero-based `chunkIndex`
- `firstChunkId` and `lastChunkId` bracket the generated chunk range for each stroke
- partial rewrite optimization remains future work, but multi-chunk generation and validation are now part of the delivered contract

## Selection state and backups

- named selection sets are serialized in `SelectionSetRecord[]`
- the transient current selection is also serialized as `CurrentSelectionRecord` with `pointIds` and `strokeIds`
- authored point backup data used by current demo attachment editing flows is serialized as `PointBackupRecord[]` keyed by `pointId`
- compiled `PaintedPoints.expandedPointRecord()` currently exposes those backups as `_demoAttachmentBackup`

## Locking

- write access is protected by session-aware lock metadata
- lock ownership tracks user, host, timestamp, script path, project path, and session id
- stale or conflicting locks are represented as validation failures, not silent overrides

## Point anchor model

Each `PointRecord` stores hybrid attachment data:

- triangle index
- barycentric coordinates
- rest object-space position
- rest world-space position
- rest UV fallback
- rest normal and up vectors

This keeps barycentric attachment primary while preserving fallback data for diagnostics, reprojection, and future recovery paths.

## Diagnostics and upgrade history

- diagnostics store counts, failing targets, summary text, and validation categories
- upgrades append history records instead of mutating prior upgrade information in place

## Current code mapping

- The typed schema structs live in `include/GafferScatterPaint/CacheFormat.h`.
- The canonical compiled serializer/deserializer lives in `src/GafferScatterPaint/CacheFormat.cpp`.
- Dict conversion glue for the current runtime/edit workflow lives in `src/GafferScatterPaint/CacheFormatDict.cpp`.
- Python fallback compatibility pack/unpack lives in `python/GafferScatterPaint/_core_shared.py`.
- Any schema change must keep the C++ cache format, Python compatibility path, tests, and this document in sync.
