import importlib
import importlib.machinery
import importlib.util
import pathlib
import sys

import GafferScene  # noqa: F401


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


_GafferScatterPaint, _GAFFER_SCATTER_PAINT_IMPORT_ERROR = _load_extension(
    "_GafferScatterPaint", "lib_GafferScatterPaint"
)

from . import _core as _core

if _GafferScatterPaint is not None:
    AttachedPoints = _GafferScatterPaint.AttachedPoints
    PaintedPoints = _GafferScatterPaint.PaintedPoints
    StaticPoints = _GafferScatterPaint.StaticPoints

    _compiled_seed_benchmark_stroke = getattr(
        PaintedPoints, "seedBenchmarkStroke", None
    )
    _compiled_brush_paint_commit = getattr(PaintedPoints, "brushPaintCommit", None)
    _compiled_brush_erase_points = getattr(PaintedPoints, "brushErasePoints", None)
    _compiled_ensure_layer_and_stroke = getattr(
        PaintedPoints, "ensureLayerAndStroke", None
    )

    if hasattr(PaintedPoints, "cacheSnapshot") and not hasattr(
        PaintedPoints, "cacheStoreSnapshot"
    ):
        PaintedPoints.cacheStoreSnapshot = PaintedPoints.cacheSnapshot

    _painted_points_helpers = [
        "_PaintedPoints__nextId",
        "_PaintedPoints__pointChunkSlices",
        "_PaintedPoints__strokePoints",
        "_PaintedPoints__scenePathFromId",
        "_PaintedPoints__expandedPointRecord",
        "_PaintedPoints__surfaceRuntimeCache",
        "_PaintedPoints__surfacePointData",
        "_PaintedPoints__surfaceCandidates",
        "_PaintedPoints__pruneSelectionState",
        "_PaintedPoints__brushSampleWorldPosition",
        "_PaintedPoints__brushSampleSurfacePoints",
        "_PaintedPoints__authoredBrushPoint",
        "_PaintedPoints__refreshDerivedData",
        "brushPaintPoints",
        "totalAuthoredPointCount",
    ]
    for _name in _painted_points_helpers:
        setattr(PaintedPoints, _name, getattr(_core.PaintedPoints, _name))

    _painted_points_private_helpers = [
        _name
        for _name in dir(_core.PaintedPoints)
        if _name.startswith("_PaintedPoints__")
    ]
    for _name in _painted_points_private_helpers:
        setattr(PaintedPoints, _name, getattr(_core.PaintedPoints, _name))

    PaintedPoints.mutateCacheStore = _core.PaintedPoints.mutateCacheStore

    if _compiled_seed_benchmark_stroke is not None:
        PaintedPoints._PaintedPoints__compiledSeedBenchmarkStroke = (
            _compiled_seed_benchmark_stroke
        )

        def _seed_benchmark_stroke(self, point_count, layer_name="", stroke_name=""):
            return self._PaintedPoints__compiledSeedBenchmarkStroke(
                int(point_count), layer_name, stroke_name
            )

        PaintedPoints.seedBenchmarkStroke = _seed_benchmark_stroke
    else:
        PaintedPoints.seedBenchmarkStroke = _core.PaintedPoints.seedBenchmarkStroke

    if _compiled_brush_paint_commit is not None:
        PaintedPoints._PaintedPoints__compiledBrushPaintCommit = (
            _compiled_brush_paint_commit
        )

        def _brush_paint_commit(self, stroke_identifier, samples, append=True):
            return self._PaintedPoints__compiledBrushPaintCommit(
                stroke_identifier, samples, append
            )

        PaintedPoints.brushPaintCommit = _brush_paint_commit

    if _compiled_brush_erase_points is not None:
        PaintedPoints._PaintedPoints__compiledBrushErasePoints = (
            _compiled_brush_erase_points
        )

        def _brush_erase_points(self, samples, radius, erase_space=0):
            return self._PaintedPoints__compiledBrushErasePoints(
                samples, radius, erase_space
            )

        PaintedPoints.brushErasePoints = _brush_erase_points

    if _compiled_ensure_layer_and_stroke is not None:
        PaintedPoints._PaintedPoints__compiledEnsureLayerAndStroke = (
            _compiled_ensure_layer_and_stroke
        )

        def _ensure_layer_and_stroke(self, layer_name=None, stroke_name=None):
            return self._PaintedPoints__compiledEnsureLayerAndStroke(
                str(layer_name or ""), str(stroke_name or "")
            )

        PaintedPoints.ensureLayerAndStroke = _ensure_layer_and_stroke
    else:
        PaintedPoints.ensureLayerAndStroke = _core.PaintedPoints.ensureLayerAndStroke
else:
    from ._core import AttachedPoints
    from ._core import PaintedPoints
    from ._core import StaticPoints

    if hasattr(PaintedPoints, "cacheSnapshot") and not hasattr(
        PaintedPoints, "cacheStoreSnapshot"
    ):
        PaintedPoints.cacheStoreSnapshot = PaintedPoints.cacheSnapshot

__all__ = [
    "AttachedPoints",
    "PaintedPoints",
    "StaticPoints",
]
