import importlib.util
from pathlib import Path

import IECore
import GafferUI

import GafferPointCloudPlus


def _load_actions_module():
    actions_path = Path(__file__).with_name("actions.py")
    spec = importlib.util.spec_from_file_location("_pointCloudPlusActions", actions_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load point cloud plus actions from {actions_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


actions = _load_actions_module()


application = application  # noqa


def _register_pointcloud_plus_node_menu(node_menu):
    node_menu.append(
        "/PointCloud/PointCloud Plus",
        GafferPointCloudPlus.PointCloudPlus,
        searchText="PointCloudPlus",
    )
    node_menu.append(
        "/PointCloudPlus/Nodes/PointCloud Plus",
        GafferPointCloudPlus.PointCloudPlus,
        searchText="PointCloudPlus node",
    )
    node_menu.append(
        "/PointCloudPlus/Demos/Geometry Basic",
        actions.build_geometry_demo,
        searchText="PointCloudPlus geometry demo",
    )
    node_menu.append(
        "/PointCloudPlus/Demos/Primitive Center",
        actions.build_primitive_center_demo,
        searchText="PointCloudPlus primitive center demo",
    )
    node_menu.append(
        "/PointCloudPlus/Demos/File Republish",
        actions.build_file_demo,
        searchText="PointCloudPlus file demo",
    )


nodeMenu = GafferUI.NodeMenu.acquire(application)
_register_pointcloud_plus_node_menu(nodeMenu)


def _pointcloud_menu_definition(menu):
    definition = IECore.MenuDefinition()
    definition.append(
        "/Create PointCloud Plus",
        {"command": actions.create_pointcloud_plus},
    )
    definition.append(
        "/Demos/Geometry Basic",
        {"command": actions.build_geometry_demo},
    )
    definition.append(
        "/Demos/Primitive Center",
        {"command": actions.build_primitive_center_demo},
    )
    definition.append(
        "/Demos/File Republish",
        {"command": actions.build_file_demo},
    )
    return definition


def _append_script_window_menu(menu_definition, prefix):
    for item_path, item in _pointcloud_menu_definition(None).items():
        menu_definition.append(prefix + item_path, item)


scriptWindowMenu = GafferUI.ScriptWindow.menuDefinition(application)
_append_script_window_menu(scriptWindowMenu, "/Tools/PointCloud Plus")
