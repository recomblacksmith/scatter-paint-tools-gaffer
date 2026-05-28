import Gaffer
import GafferScene
import GafferSceneUI
import GafferUI
import IECore
import imath

import GafferScatterPaint
import GafferScatterPaintUI


from pathlib import Path


def _load_actions_part(filename):
    path = Path(__file__).with_name(filename)
    code = compile(path.read_text(), str(path), "exec")
    exec(code, globals(), globals())


for _actions_part in (
    "actions_support.py",
    "actions_setup.py",
    "actions_demos.py",
    "actions_commands.py",
):
    _load_actions_part(_actions_part)


del _actions_part
del _load_actions_part

__all__ = [
    "build_box_instance_scatter_demo",
    "build_box_instance_scatter_demo_for_script",
    "build_benchmark_scene",
    "build_benchmark_scene_for_script",
    "build_layer_modes_timing_demo",
    "build_layer_modes_timing_demo_for_script",
    "build_layer_stroke_edit_demo",
    "build_layer_stroke_edit_demo_for_script",
    "break_attachment_paths",
    "break_attachment_triangles",
    "build_demo_graph_for_script",
    "build_paint_erase_basics_demo",
    "build_paint_erase_basics_demo_for_script",
    "build_paint_erase_cyclo_demo",
    "build_paint_erase_cyclo_demo_for_script",
    "build_paint_through_erase_space_demo",
    "build_paint_through_erase_space_demo_for_script",
    "build_relax_reproject_demo",
    "build_relax_reproject_demo_for_script",
    "commit_demo_stroke",
    "create_attached_points",
    "create_painted_points",
    "build_demo_graph",
    "ensure_attached_points",
    "ensure_painted_points",
    "erase_last_stroke",
    "frame_scatter_nodes",
    "inspect_scatter_positions",
    "repair_demo_attachments",
    "seed_large_benchmark_stroke",
    "seed_large_benchmark_stroke_for_script",
    "seed_medium_benchmark_stroke",
    "seed_medium_benchmark_stroke_for_script",
    "seed_small_benchmark_stroke",
    "seed_small_benchmark_stroke_for_script",
    "seed_stress_benchmark_stroke",
    "seed_stress_benchmark_stroke_for_script",
    "seed_xl_benchmark_stroke",
    "seed_xl_benchmark_stroke_for_script",
    "seed_xxl_benchmark_stroke",
    "seed_xxl_benchmark_stroke_for_script",
    "validate_attachments",
]
