# Scatter Paint Stats

Latest normalized timings from recent real paint/erase runs.

Average point counts used for normalization:

- paint committed/resolved points: `36,696.6`
- erase removed points: `31,063.2`
- buffered/densified stroke input points: `110.2`

## Per 10k points

### Erase

- `totalMs`: `9.87ms / 10k`
- `brushEraseMs`: `7.06ms / 10k`
- `loadSchemaMs`: `2.22ms / 10k`
- `pointScanMs`: `1.44ms / 10k`
- `writeMs`: `2.35ms / 10k`
- `writePackMs`: `0.26ms / 10k`
- `packPointsMs`: `0.25ms / 10k`
- `packPointsChecksumMs`: `0.19ms / 10k`
- `writeBlobPlugSetMs`: `2.07ms / 10k`

Main read: erase work itself is reasonable; small-update writes are still dominated by embedded blob set overhead.

### Paint / Write

- `totalMs`: `18.52ms / 10k`
- `sampleBuildMs`: `0.70ms / 10k`
- `brushPaintMs`: `11.46ms / 10k`
- `surfaceResolveMs`: `5.47ms / 10k`
- `writeMs`: `3.62ms / 10k`
- `writePackMs`: `3.59ms / 10k`
- `packPointsMs`: `3.59ms / 10k`
- `packPointsChecksumMs`: `2.15ms / 10k`
- `packFinalChecksumMs`: effectively `0`
- `writeBlobPlugSetMs`: effectively `0`

Main read: large paint strokes are no longer bottlenecked by `writeBlobPlugSetMs`; write cost is mostly point packing, especially checksum inside `packPointsMs`.
