# Changelog

## 0.1.3 - 2026-05-31

### Changed

- Refined the artist-facing README and plugin documentation copy.
- Replaced tool-context publish terminology with output, bake, export, and handoff wording to avoid confusion with pipeline publishing.
- Renamed the PointCloud Plus file demo menu copy from `File Republish` to `File Output`.

## 0.1.2 - 2026-05-30

### Added

- Added a native Windows build entrypoint, `build-plugins.ps1`, for building all three plugins against the packaged Gaffer Windows runtime.
- Added a self-hosted Windows GitHub Actions workflow that builds the toolset and uploads `dist/gaffer` as an artifact.
- Added Windows SIMD CPU feature detection for Scatter Paint brush kernels.

### Changed

- Updated SCons build logic to support Windows runtime layouts, MSVC flags, `.pyd` extension suffixes, and Windows import-library paths.
- Linked Windows builds against the required Gaffer, Cortex, Boost.Python, `fmt`, dispatch, UI, and OpenGL libraries.
- Documented local Windows build and runner requirements in the README.

### Fixed

- Fixed Windows portability issues in compiled Scatter Paint sources, including `gmtime_s` support and `ssize_t` availability.
- Updated ignore rules for generated MSVC and Python extension build outputs.
