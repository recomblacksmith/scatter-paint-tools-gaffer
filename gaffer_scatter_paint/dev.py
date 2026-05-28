#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import pathlib
import shutil
import subprocess
import sys


PLUGIN_ROOT = pathlib.Path(__file__).resolve().parent
REPO_ROOT = PLUGIN_ROOT.parent
DEFAULT_GAFFER_ROOT = None


def _existing_path(value: str | None) -> pathlib.Path | None:
    if not value:
        return None
    path = pathlib.Path(value).expanduser().resolve()
    return path if path.exists() else None


def _runtime_tree(path: pathlib.Path | None) -> bool:
    if path is None:
        return False
    return all(
        (path / entry).exists() for entry in ("include", "lib", "python", "bin/python")
    )

def _resolve_layout(
    gaffer_root_arg: str | None,
 ) -> tuple[pathlib.Path, pathlib.Path, pathlib.Path]:
    gaffer_root = _existing_path(gaffer_root_arg) or _existing_path(
        os.environ.get("GAFFER_ROOT")
    )
    if gaffer_root is None and DEFAULT_GAFFER_ROOT and DEFAULT_GAFFER_ROOT.exists():
        gaffer_root = DEFAULT_GAFFER_ROOT.resolve()

    if gaffer_root is None:
        raise RuntimeError(
            "Set --gaffer-root or GAFFER_ROOT to a packaged Gaffer runtime"
        )

    if _runtime_tree(gaffer_root):
        return gaffer_root, gaffer_root, gaffer_root

    raise RuntimeError(
        f"Unsupported GAFFER_ROOT layout: {gaffer_root}. Expected a packaged Gaffer runtime with include/, lib/, python/, and bin/python."
    )


def _prepend_env_path(env: dict[str, str], key: str, value: pathlib.Path | str) -> None:
    value = str(value)
    current = env.get(key, "")
    parts = [part for part in current.split(os.pathsep) if part]
    if value in parts:
        parts.remove(value)
    env[key] = os.pathsep.join([value] + parts) if parts else value


def _command_prefix(use_devbox: bool) -> list[str]:
    if use_devbox and shutil.which("devbox"):
        return ["devbox", "run", "--"]
    return []


def _run(
    command: list[str],
    *,
    cwd: pathlib.Path | None = None,
    env: dict[str, str] | None = None,
) -> None:
    print("+", " ".join(command))
    subprocess.run(command, cwd=str(cwd) if cwd else None, env=env, check=True)


def _runtime_env(
    gaffer_root: pathlib.Path,
    runtime_root: pathlib.Path,
    *,
    offscreen: bool,
) -> dict[str, str]:
    env = os.environ.copy()
    env["GAFFER_ROOT"] = str(gaffer_root)
    env["PYTHONNOUSERSITE"] = "1"
    _prepend_env_path(env, "LD_LIBRARY_PATH", runtime_root / "lib")
    _prepend_env_path(env, "PYTHONPATH", runtime_root / "python")
    _prepend_env_path(env, "PYTHONPATH", PLUGIN_ROOT / "python")
    _prepend_env_path(env, "GAFFER_STARTUP_PATHS", PLUGIN_ROOT / "startup")

    fonts_dir = runtime_root / "fonts"
    if fonts_dir.exists():
        env["IECORE_FONT_PATHS"] = str(fonts_dir)

    if offscreen:
        env.setdefault("QT_QPA_PLATFORM", "offscreen")

    return env


def _python_bin(runtime_root: pathlib.Path) -> pathlib.Path:
    python_bin = runtime_root / "bin" / "python"
    if not python_bin.exists():
        raise RuntimeError(f"Missing target Gaffer Python: {python_bin}")
    return python_bin


def _gaffer_bin(gaffer_root: pathlib.Path) -> pathlib.Path:
    gaffer_bin = gaffer_root / "bin" / "gaffer"
    if not gaffer_bin.exists():
        raise RuntimeError(f"Missing Gaffer launcher: {gaffer_bin}")
    return gaffer_bin

def _build_plugin(
    gaffer_root: pathlib.Path,
    use_devbox: bool,
    jobs: int,
) -> None:
    prefix = _command_prefix(use_devbox)
    command = prefix + [
        "scons",
        f"GAFFER_ROOT={gaffer_root}",
        f"-j{jobs}",
    ]
    _run(command, cwd=PLUGIN_ROOT)


def _smoke_test(
    gaffer_root: pathlib.Path, runtime_root: pathlib.Path, ui: bool
) -> None:
    env = _runtime_env(gaffer_root, runtime_root, offscreen=ui)
    python_bin = _python_bin(runtime_root)
    if ui:
        code = (
            "import Gaffer, GafferUI, GafferScene, GafferSceneUI, GafferScatterPaint, GafferScatterPaintUI; "
            "print(GafferScatterPaint.PaintedPoints); "
            "print(GafferScatterPaintUI.PaintPointsTool)"
        )
    else:
        code = (
            "import Gaffer, GafferScene, GafferScatterPaint; "
            "node = GafferScatterPaint.PaintedPoints(); "
            "print(GafferScatterPaint.PaintedPoints); "
            "print(node.typeName())"
        )
    _run([str(python_bin), "-c", code], env=env)


def _launch(
    gaffer_root: pathlib.Path,
    runtime_root: pathlib.Path,
    extra_args: list[str],
    *,
    demo: bool,
) -> None:
    env = _runtime_env(gaffer_root, runtime_root, offscreen=False)
    if demo:
        env["GAFFER_SCATTER_PAINT_BUILD_DEMO"] = "1"
    command = [str(_gaffer_bin(gaffer_root))] + extra_args
    _run(command, env=env)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build and run the scatter-paint plugin against a target Gaffer runtime"
    )
    parser.add_argument("--gaffer-root", help="Packaged Gaffer runtime")
    parser.add_argument(
        "--jobs", type=int, default=8, help="Parallel jobs for SCons builds"
    )
    parser.add_argument(
        "--no-devbox",
        action="store_true",
        help="Run build/bootstrap commands directly instead of via devbox",
    )

    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser(
        "build", help="Build the plugin for the selected Gaffer runtime"
    )

    smoke_parser = subparsers.add_parser(
        "smoke-test", help="Run a headless import smoke test"
    )
    smoke_parser.add_argument(
        "--ui",
        action="store_true",
        help="Include GafferUI/GafferSceneUI/PaintPointsTool imports",
    )

    launch_parser = subparsers.add_parser(
        "launch", help="Launch Gaffer with the plugin side-loaded"
    )
    launch_parser.add_argument(
        "--build", action="store_true", help="Build the plugin before launching"
    )
    launch_parser.add_argument(
        "--demo",
        action="store_true",
        help="Auto-create the scatter paint demo graph on startup",
    )
    launch_parser.add_argument(
        "gaffer_args",
        nargs=argparse.REMAINDER,
        help="Arguments passed through to Gaffer",
    )

    args = parser.parse_args()

    gaffer_root, gaffer_header_root, gaffer_runtime_root = _resolve_layout(args.gaffer_root)
    del gaffer_header_root
    use_devbox = not args.no_devbox

    if args.command == "build":
        _build_plugin(gaffer_root, use_devbox, args.jobs)
        return 0

    if args.command == "smoke-test":
        _smoke_test(gaffer_root, gaffer_runtime_root, args.ui)
        return 0

    if args.command == "launch":
        if args.build:
            _build_plugin(gaffer_root, use_devbox, args.jobs)
        extra_args = list(args.gaffer_args)
        if extra_args and extra_args[0] == "--":
            extra_args = extra_args[1:]
        _launch(
            gaffer_root,
            gaffer_runtime_root,
            extra_args,
            demo=args.demo,
        )
        return 0

    raise AssertionError(f"Unhandled command: {args.command}")


if __name__ == "__main__":
    sys.exit(main())
