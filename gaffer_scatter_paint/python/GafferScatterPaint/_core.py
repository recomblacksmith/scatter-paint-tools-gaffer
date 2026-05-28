# ruff: noqa
from . import _core_attached_points as _attached_points_module
from . import _core_painted_points as _painted_points_module
from . import _core_shared as _shared_module
from . import _core_static_points as _static_points_module


def _reexport(module):
    for name, value in vars(module).items():
        if name.startswith("__"):
            continue
        globals()[name] = value


for _module in (
    _shared_module,
    _attached_points_module,
    _static_points_module,
    _painted_points_module,
):
    _reexport(_module)

_AttachedPointsOutputGate.__module__ = __name__
PaintedPoints.__module__ = __name__
AttachedPoints.__module__ = __name__
StaticPoints.__module__ = __name__

if _should_register_public_fallback_types():
    IECore.registerRunTimeTyped(
        PaintedPoints,
        typeName="GafferScatterPaint::PaintedPoints",
    )

    IECore.registerRunTimeTyped(
        AttachedPoints,
        typeName="GafferScatterPaint::AttachedPoints",
    )

    IECore.registerRunTimeTyped(
        StaticPoints,
        typeName="GafferScatterPaint::StaticPoints",
    )
