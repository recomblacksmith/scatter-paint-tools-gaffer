#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import shutil
import subprocess
import sys


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_BUILD_DIR = REPO_ROOT / "temp" / "gaffer-build"


def _run(*args: str) -> str:
    result = subprocess.run(
        args,
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def _ensure_symlink(source: pathlib.Path, destination: pathlib.Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.is_symlink() or destination.exists():
        if destination.is_symlink() and destination.resolve() == source.resolve():
            return
        destination.unlink()
    destination.symlink_to(source)


def _pkg_config_include_dir(package: str) -> pathlib.Path:
    return pathlib.Path(_run("pkg-config", "--variable=includedir", package))


def _pkg_config_lib_dir(package: str) -> pathlib.Path | None:
    output = _run("pkg-config", "--variable=libdir", package)
    if not output:
        return None
    path = pathlib.Path(output)
    return path if path.exists() else None


def _pkg_config_lib_dirs(*packages: str) -> list[pathlib.Path]:
    output = _run("pkg-config", "--libs-only-L", *packages)
    result = []
    seen = set()
    for token in output.split():
        if not token.startswith("-L"):
            continue
        path = pathlib.Path(token[2:])
        if path not in seen:
            seen.add(path)
            result.append(path)
    return result


def _first_existing(paths: list[pathlib.Path]) -> pathlib.Path:
    for path in paths:
        if path.exists():
            return path
    raise FileNotFoundError(", ".join(str(path) for path in paths))


def patch_murmurhash(build_dir: pathlib.Path) -> None:
    header_path = build_dir / "include" / "IECore" / "MurmurHash.h"
    text = header_path.read_text()
    include_line = "#include <cstdint>\n"
    if include_line in text:
        print(f"ok  MurmurHash header already patched: {header_path}")
        return
    marker = "#include <iostream>\n"
    if marker not in text:
        raise RuntimeError(f"Unable to patch {header_path}: missing {marker.strip()}")
    header_path.write_text(text.replace(marker, include_line + marker, 1))
    print(f"fix MurmurHash header: {header_path}")


def link_gl_headers(build_dir: pathlib.Path) -> None:
    gl_include = _pkg_config_include_dir("gl") / "GL"
    glu_include = _pkg_config_include_dir("glu") / "GL"
    build_gl_include = build_dir / "include" / "GL"

    header_sources = {
        "gl.h": gl_include / "gl.h",
        "glx.h": gl_include / "glx.h",
        "glext.h": gl_include / "glext.h",
        "glxext.h": gl_include / "glxext.h",
        "glcorearb.h": gl_include / "glcorearb.h",
        "glu.h": glu_include / "glu.h",
    }

    for name, source in header_sources.items():
        _ensure_symlink(source, build_gl_include / name)
        print(f"link {build_gl_include / name} -> {source}")


def link_gl_libs(build_dir: pathlib.Path) -> None:
    lib_dirs = []
    seen = set()
    for package in ("gl", "glu"):
        lib_dir = _pkg_config_lib_dir(package)
        if lib_dir is not None and lib_dir not in seen:
            seen.add(lib_dir)
            lib_dirs.append(lib_dir)
    for lib_dir in _pkg_config_lib_dirs("gl", "glu"):
        if lib_dir not in seen:
            seen.add(lib_dir)
            lib_dirs.append(lib_dir)

    if not lib_dirs:
        raise RuntimeError(
            "Unable to locate GL/GLU library directories via pkg-config. "
            "Run this script inside the configured devbox environment."
        )

    build_lib_dir = build_dir / "lib"
    lib_names = ["libGL.so", "libGLX.so", "libOpenGL.so", "libGLU.so"]

    for name in lib_names:
        source = _first_existing([directory / name for directory in lib_dirs])
        _ensure_symlink(source, build_lib_dir / name)
        print(f"link {build_lib_dir / name} -> {source}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--gaffer-build-dir",
        default=str(DEFAULT_BUILD_DIR),
        help="Path to the dependency-backed Gaffer build tree.",
    )
    args = parser.parse_args()

    if shutil.which("pkg-config") is None:
        raise RuntimeError("pkg-config is required on PATH")

    build_dir = pathlib.Path(args.gaffer_build_dir).expanduser().resolve()
    if not build_dir.exists():
        raise RuntimeError(f"Build dir does not exist: {build_dir}")

    patch_murmurhash(build_dir)
    link_gl_headers(build_dir)
    link_gl_libs(build_dir)
    print(f"done bootstrap fixes for {build_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
