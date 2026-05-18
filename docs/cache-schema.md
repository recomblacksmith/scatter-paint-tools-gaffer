# Cache Schema

`GafferScatterPaint` uses a strict plugin-native cache schema shared by embedded and external storage modes.

## Versioning

- magic: `GSPAINT`
- schema version: `2`
- explicit plugin version fields in the cache header
- incompatible schema changes require an explicit upgrade action

## Serialized layout

The logical schema is represented by `CacheSchema`, but the current binary payload order is defined by `src/GafferScatterPaint/CacheFormat.cpp`.

1. `CacheHeader` prefix
2. persisted `schemaVersion`
3. `NextIdsRecord`
4. `NodeMetadata`
5. `LockMetadata`
6. scene path dictionaries (`scenePaths`, `instanceSourcePaths`)
7. `LayerRecord[]`
8. `StrokeRecord[]`
9. `PointChunkRecord[]`
10. `PointRecord[]`
11. `SelectionSetRecord[]`
12. `CurrentSelectionRecord`
13. `DiagnosticsSnapshot`
14. `UpgradeRecord[]`
15. `PointBackupRecord[]` keyed by `pointId`

The header prefix stores magic, endian marker, plugin version fields, `contentFlags`, and one persisted checksum field.

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

## Integrity and checksum scope

- `CacheHeader.checksum` is the only persisted cache-blob checksum.
- That checksum covers the entire payload after the header prefix, not an individual section.
- The checksum guarantee is therefore whole-payload integrity for load validation.
- The lock blob uses the same pattern: one checksum over its payload.
- Per-section checksum timings used in diagnostics are implementation details, not additional serialized integrity contracts.

## Content flags

- `contentFlags` is serialized in the cache header.
- The compiled packer currently sets flags for:
  - diagnostics content present
  - upgrade records present
  - external storage mode
- Runtime helpers should treat `contentFlags` as header metadata, not as a replacement for decoding the payload.

## Persisted next ids

- persisted `nextIds` are part of the serialized schema and track the next layer/stroke/point/selectionSet/chunk ids
- the typed schema now models them explicitly instead of reconstructing them from max existing ids
- Python and C++ compatibility paths both preserve them through roundtrip IO

## Authored color fields

- schema version `2` adds authored color persistence for scatter paint output parity
- `NodeMetadata` persists `defaultColor`
- `LayerRecord`, `StrokeRecord`, and `PointRecord` persist `colorEnabled` plus `color`
- authored color precedence is `point > stroke > layer > node default`
- `scatterColor` is always reconstructed from authored color during point expansion/output generation
- `debugColor` is a runtime display plug on `AttachedPoints` and `StaticPoints`; it is not stored in the cache schema
- when `debugColor` is off, `Cs` shows authored color
- when `debugColor` is on, `Cs` shows resolve-state debug color while `scatterColor` remains authored color

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
