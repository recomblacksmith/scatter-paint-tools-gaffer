import importlib.util
from pathlib import Path

import IECore
import GafferUI

import GafferScatterPaint


def _load_actions_module():
    actions_path = Path(__file__).with_name("actions.py")
    spec = importlib.util.spec_from_file_location("_scatterPaintActions", actions_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load scatter paint actions from {actions_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


actions = _load_actions_module()


application = application  # noqa


def _documentation_url(page_name="PaintPointsTool.html"):
    path = Path(__file__).resolve().parents[2] / "docs" / "html" / page_name
    return path.as_uri() if path.exists() else None


def _show_paint_tool_documentation(menu=None):
    url = _documentation_url()
    if url:
        GafferUI.showURL(url)


def _register_scatter_paint_node_menu(node_menu):
    node_menu.append(
        "/Scatter/Painted Points",
        GafferScatterPaint.PaintedPoints,
        searchText="PaintedPoints",
    )
    node_menu.append(
        "/Scatter/Attached Points",
        GafferScatterPaint.AttachedPoints,
        searchText="AttachedPoints",
    )
    node_menu.append(
        "/Scatter/Static Points",
        GafferScatterPaint.StaticPoints,
        searchText="StaticPoints",
    )

    node_menu.append(
        "/ScatterPaint/Nodes/Painted Points",
        GafferScatterPaint.PaintedPoints,
        searchText="ScatterPaint PaintedPoints",
    )
    node_menu.append(
        "/ScatterPaint/Nodes/Attached Points",
        GafferScatterPaint.AttachedPoints,
        searchText="ScatterPaint AttachedPoints",
    )
    node_menu.append(
        "/ScatterPaint/Nodes/Static Points",
        GafferScatterPaint.StaticPoints,
        searchText="ScatterPaint StaticPoints",
    )

    node_menu.append(
        "/ScatterPaint/Demos/Build Demo Graph",
        actions.build_demo_graph,
        searchText="ScatterPaint demo graph",
    )
    node_menu.append(
        "/ScatterPaint/Demos/Paint + Erase Basics",
        actions.build_paint_erase_basics_demo,
        searchText="ScatterPaint paint erase demo",
    )
    node_menu.append(
        "/ScatterPaint/Demos/Paint + Erase Cyclo",
        actions.build_paint_erase_cyclo_demo,
        searchText="ScatterPaint cyclo demo",
    )
    node_menu.append(
        "/ScatterPaint/Demos/Paint Through + Erase Space",
        actions.build_paint_through_erase_space_demo,
        searchText="ScatterPaint paint through demo",
    )
    node_menu.append(
        "/ScatterPaint/Demos/Relax + Reproject",
        actions.build_relax_reproject_demo,
        searchText="ScatterPaint relax reproject demo",
    )
    node_menu.append(
        "/ScatterPaint/Demos/Layer Modes + Timing",
        actions.build_layer_modes_timing_demo,
        searchText="ScatterPaint layer timing demo",
    )
    node_menu.append(
        "/ScatterPaint/Demos/Layer + Stroke Editing",
        actions.build_layer_stroke_edit_demo,
        searchText="ScatterPaint stroke edit demo",
    )
    node_menu.append(
        "/ScatterPaint/Demos/Box Instance Scatter",
        actions.build_box_instance_scatter_demo,
        searchText="ScatterPaint box instance demo",
    )
    node_menu.append(
        "/ScatterPaint/Benchmark/Build Benchmark Scene",
        actions.build_benchmark_scene,
        searchText="ScatterPaint benchmark scene",
    )
    node_menu.append(
        "/ScatterPaint/Benchmark/Seed Small Stroke",
        actions.seed_small_benchmark_stroke,
        searchText="ScatterPaint benchmark small",
    )
    node_menu.append(
        "/ScatterPaint/Benchmark/Seed Medium Stroke",
        actions.seed_medium_benchmark_stroke,
        searchText="ScatterPaint benchmark medium",
    )
    node_menu.append(
        "/ScatterPaint/Benchmark/Seed Large Stroke",
        actions.seed_large_benchmark_stroke,
        searchText="ScatterPaint benchmark large",
    )
    node_menu.append(
        "/ScatterPaint/Benchmark/Seed XL Stroke",
        actions.seed_xl_benchmark_stroke,
        searchText="ScatterPaint benchmark 25000",
    )
    node_menu.append(
        "/ScatterPaint/Benchmark/Seed XXL Stroke",
        actions.seed_xxl_benchmark_stroke,
        searchText="ScatterPaint benchmark 100000",
    )
    node_menu.append(
        "/ScatterPaint/Benchmark/Seed Stress Stroke",
        actions.seed_stress_benchmark_stroke,
        searchText="ScatterPaint benchmark 1000000",
    )


nodeMenu = GafferUI.NodeMenu.acquire(application)
_register_scatter_paint_node_menu(nodeMenu)


def _scatter_menu_definition(menu):
    definition = IECore.MenuDefinition()
    definition.append(
        "/Create Painted Points",
        {"command": actions.create_painted_points},
    )
    definition.append(
        "/Create Attached Points",
        {"command": actions.create_attached_points},
    )
    definition.append(
        "/Build Demo Graph",
        {"command": actions.build_demo_graph},
    )
    definition.append(
        "/Documentation...",
        {"command": _show_paint_tool_documentation, "active": bool(_documentation_url())},
    )
    definition.append(
        "/Demos/Paint + Erase Basics",
        {"command": actions.build_paint_erase_basics_demo},
    )
    definition.append(
        "/Demos/Paint + Erase Cyclo",
        {"command": actions.build_paint_erase_cyclo_demo},
    )
    definition.append(
        "/Demos/Paint Through + Erase Space",
        {"command": actions.build_paint_through_erase_space_demo},
    )
    definition.append(
        "/Demos/Relax + Reproject",
        {"command": actions.build_relax_reproject_demo},
    )
    definition.append(
        "/Demos/Layer Modes + Timing",
        {"command": actions.build_layer_modes_timing_demo},
    )
    definition.append(
        "/Demos/Layer + Stroke Editing",
        {"command": actions.build_layer_stroke_edit_demo},
    )
    definition.append(
        "/Demos/Box Instance Scatter",
        {"command": actions.build_box_instance_scatter_demo},
    )
    definition.append(
        "/Benchmark/Build Benchmark Scene",
        {"command": actions.build_benchmark_scene},
    )
    definition.append(
        "/Benchmark/Seed Small Stroke",
        {"command": actions.seed_small_benchmark_stroke},
    )
    definition.append(
        "/Benchmark/Seed Medium Stroke",
        {"command": actions.seed_medium_benchmark_stroke},
    )
    definition.append(
        "/Benchmark/Seed Large Stroke",
        {"command": actions.seed_large_benchmark_stroke},
    )
    definition.append(
        "/Benchmark/Seed XL Stroke",
        {"command": actions.seed_xl_benchmark_stroke},
    )
    definition.append(
        "/Benchmark/Seed XXL Stroke",
        {"command": actions.seed_xxl_benchmark_stroke},
    )
    definition.append(
        "/Benchmark/Seed Stress Stroke",
        {"command": actions.seed_stress_benchmark_stroke},
    )
    definition.append(
        "/NodesDivider",
        {"divider": True},
    )
    definition.append(
        "/Commit Demo Stroke",
        {"command": actions.commit_demo_stroke},
    )
    definition.append(
        "/Erase Last Stroke",
        {"command": actions.erase_last_stroke},
    )
    definition.append(
        "/Frame Scatter Nodes",
        {"command": actions.frame_scatter_nodes},
    )
    definition.append(
        "/Inspect Scatter Positions",
        {"command": actions.inspect_scatter_positions},
    )
    definition.append(
        "/Validate Attachments",
        {"command": actions.validate_attachments},
    )
    definition.append(
        "/Break Attachment Paths",
        {"command": actions.break_attachment_paths},
    )
    definition.append(
        "/Break Attachment Triangles",
        {"command": actions.break_attachment_triangles},
    )
    definition.append(
        "/Repair Demo Attachments",
        {"command": actions.repair_demo_attachments},
    )
    return definition


script_window_definition = GafferUI.ScriptWindow.menuDefinition(application)
script_window_definition.append(
    "/Tools/Scatter Paint",
    {"subMenu": _scatter_menu_definition},
)
