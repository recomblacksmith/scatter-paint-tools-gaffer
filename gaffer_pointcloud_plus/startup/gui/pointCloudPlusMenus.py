"""GUI startup shim.

Gaffer scans `startup/gui` for GUI startup hooks, so this file delegates to the
shared PointCloud Plus menu registration in `startup/GafferPointCloudPlusUI/menus.py`.
"""

import importlib.util
import pathlib


def _load_gui_menus_module():
    menus_path = (
        pathlib.Path(__file__).resolve().parent.parent
        / "GafferPointCloudPlusUI"
        / "menus.py"
    )
    spec = importlib.util.spec_from_file_location("_pointCloudPlusMenus", menus_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load point cloud plus menus from {menus_path}")
    module = importlib.util.module_from_spec(spec)
    module.application = application  # noqa: F821
    spec.loader.exec_module(module)
    return module


_menus = _load_gui_menus_module()
