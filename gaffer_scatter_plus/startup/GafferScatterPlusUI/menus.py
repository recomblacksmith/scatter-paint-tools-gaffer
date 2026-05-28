import importlib.util
from pathlib import Path

import IECore
import GafferUI

import GafferScatterPlus


def _load_actions_module():
    actions_path = Path(__file__).with_name("actions.py")
    spec = importlib.util.spec_from_file_location("_scatterPlusActions", actions_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load scatter plus actions from {actions_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


actions = _load_actions_module()


application = application  # noqa


nodeMenu = GafferUI.NodeMenu.acquire(application)
nodeMenu.append("/ScatterPlus/Nodes/Scatter Plus", GafferScatterPlus.ScatterPlus, searchText="ScatterPlus")
nodeMenu.append("/ScatterPlus/Demos/Image Scatter", actions.build_image_demo, searchText="ScatterPlus demo")


def _scatter_plus_menu_definition():
    definition = IECore.MenuDefinition()
    definition.append("/Create Scatter Plus", {"command": actions.create_scatter_plus})
    definition.append("/Demos/Image Scatter", {"command": actions.build_image_demo})
    return definition


for item_path, item in _scatter_plus_menu_definition().items():
    GafferUI.ScriptWindow.menuDefinition(application).append("/Tools/Scatter Plus" + item_path, item)
