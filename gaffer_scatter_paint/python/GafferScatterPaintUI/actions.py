from importlib import util
from pathlib import Path


_startup_actions_path = (
    Path(__file__).resolve().parents[2]
    / "startup"
    / "GafferScatterPaintUI"
    / "actions.py"
)

_spec = util.spec_from_file_location(
    "GafferScatterPaintUI._startup_actions",
    _startup_actions_path,
)

if _spec is None or _spec.loader is None:
    raise ImportError(f"Unable to load startup actions from {_startup_actions_path}")

_module = util.module_from_spec(_spec)
_spec.loader.exec_module(_module)

for _name, _value in vars(_module).items():
    if _name.startswith("__"):
        continue
    globals()[_name] = _value

__all__ = list(getattr(_module, "__all__", []))
