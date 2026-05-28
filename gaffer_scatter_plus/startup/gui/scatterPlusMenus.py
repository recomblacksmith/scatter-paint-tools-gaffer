"""GUI startup shim for Scatter Plus menus."""

import importlib.util
import pathlib


def _load_gui_menus_module():
    menus_path = pathlib.Path(__file__).resolve().parent.parent / "GafferScatterPlusUI" / "menus.py"
    spec = importlib.util.spec_from_file_location("_scatterPlusMenus", menus_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load scatter plus menus from {menus_path}")
    module = importlib.util.module_from_spec(spec)
    module.application = application  # noqa: F821
    spec.loader.exec_module(module)
    return module


_menus = _load_gui_menus_module()
