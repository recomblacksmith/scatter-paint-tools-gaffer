import ctypes
import importlib
import importlib.machinery
import importlib.util
import os
import pathlib
import sys


def _log_warning(message):
    try:
        import IECore

        IECore.msg(IECore.Msg.Level.Warning, "GafferScatterPaintUI", message)
    except Exception:
        pass


_PAINT_POINTS_TOOL_IMPORT_ERROR = None
_PAINTED_POINTS_PANEL_IMPORT_ERROR = None
_GafferScatterPaintUI = None


def _preload_core_extension_symbols():
    try:
        import GafferScatterPaint
    except Exception:
        return

    core_module = getattr(GafferScatterPaint, "_GafferScatterPaint", None)
    core_path = getattr(core_module, "__file__", None)
    if not core_path:
        return

    mode = getattr(os, "RTLD_GLOBAL", 0) | getattr(os, "RTLD_NOW", 0)
    try:
        ctypes.CDLL(core_path, mode=mode)
    except Exception:
        pass


def _load_extension(module_basename, legacy_basename=None):
    module_name = "{}.{}".format(__name__, module_basename)
    try:
        return importlib.import_module(module_name), None
    except ModuleNotFoundError as exc:
        if exc.name != module_name or not legacy_basename:
            return None, exc
    except Exception as exc:
        return None, exc

    package_dir = pathlib.Path(__file__).resolve().parent
    for suffix in importlib.machinery.EXTENSION_SUFFIXES:
        legacy_path = package_dir / "{}{}".format(legacy_basename, suffix)
        if not legacy_path.exists():
            continue

        try:
            loader = importlib.machinery.ExtensionFileLoader(
                module_name, str(legacy_path)
            )
            spec = importlib.util.spec_from_loader(module_name, loader)
            if spec is None:
                continue
            module = importlib.util.module_from_spec(spec)
            sys.modules[module_name] = module
            loader.exec_module(module)
        except Exception as exc:
            sys.modules.pop(module_name, None)
            return None, exc
        return module, None

    return None, ModuleNotFoundError(module_name)


def _ensure_paint_points_tool_loaded():
    global _GafferScatterPaintUI
    global _PAINT_POINTS_TOOL_IMPORT_ERROR

    if _GafferScatterPaintUI is not None:
        paint_points_tool = _GafferScatterPaintUI.PaintPointsTool
        globals()["PaintPointsTool"] = paint_points_tool
        return paint_points_tool

    _preload_core_extension_symbols()
    module, error = _load_extension("_GafferScatterPaintUI", "lib_GafferScatterPaintUI")
    if module is None:
        _PAINT_POINTS_TOOL_IMPORT_ERROR = error
        globals().pop("PaintPointsTool", None)
        return None

    _GafferScatterPaintUI = module
    _PAINT_POINTS_TOOL_IMPORT_ERROR = None
    globals()["PaintPointsTool"] = module.PaintPointsTool
    return module.PaintPointsTool


def __getattr__(name):
    if name == "PaintPointsTool":
        return _ensure_paint_points_tool_loaded()
    raise AttributeError(name)


_ensure_paint_points_tool_loaded()

try:
    from .paintedPointsPanel import (
        PaintedPointsAuthoringWidget,
        PaintedPointsCacheWidget,
    )
except Exception as exc:
    PaintedPointsAuthoringWidget = None
    PaintedPointsCacheWidget = None
    _PAINTED_POINTS_PANEL_IMPORT_ERROR = exc
    _log_warning(
        "PaintedPoints panel import failed; tool remains available if the UI extension loads: {}: {}".format(
            type(exc).__name__, exc
        )
    )

__all__ = [
    "PaintPointsTool",
    "PaintedPointsAuthoringWidget",
    "PaintedPointsCacheWidget",
]
