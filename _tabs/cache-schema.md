---
title: Cache Schema
icon: fas fa-database
order: 7
---

# Cache Schema

`GafferScatterPaint` uses a strict plugin-native cache schema shared by embedded and external storage modes.

## Versioning

- magic: `GSPAINT`
- schema version: `2`
- explicit plugin version fields in the cache header
- incompatible schema changes require an explicit upgrade action

## Serialized Layout

The current binary payload order is defined by `src/GafferScatterPaint/CacheFormat.cpp`.

1. `CacheHeader`
2. persisted `schemaVersion`
3. `NextIdsRecord`
4. `NodeMetadata`
5. `LockMetadata`
6. scene path dictionaries
7. `LayerRecord[]`
8. `StrokeRecord[]`
9. `PointChunkRecord[]`
10. `PointRecord[]`
11. `SelectionSetRecord[]`
12. `CurrentSelectionRecord`
13. `DiagnosticsSnapshot`
14. `UpgradeRecord[]`
15. `PointBackupRecord[]`

## Current Rules

- C++ cache IO is authoritative
- Python fallback remains byte-compatible with the same format
- dict-shaped runtime/edit helpers are still part of the current implementation
- partial cache rewrite optimization is not part of the current delivered contract
- authored color persists in schema version `2`
- `debugColor` is runtime-only and is not serialized

## Identity And Chunking

- `pointId` is stable across cache IO
- selection sets reference stable point and stroke ids
- both Python and C++ paths chunk large strokes by `CHUNK_POINT_LIMIT = 8192`
- chunk records are contiguous per stroke with zero-based `chunkIndex`

## Locking And Diagnostics

- writes use session-aware lock metadata
- diagnostics persist counts, failing targets, summary text, and validation categories
- upgrades append history records instead of overwriting prior upgrade information
