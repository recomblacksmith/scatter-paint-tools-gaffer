import os

import IECore
import GafferSceneUI
import GafferUI

import GafferScatterPaint
import GafferScatterPaintUI


_plugin_root = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
_graphics_dir = os.path.join(_plugin_root, "graphics")
_existing_image_paths = os.environ.get("GAFFERUI_IMAGE_PATHS", "")
if _existing_image_paths:
    image_paths = _graphics_dir + os.pathsep + _existing_image_paths
else:
    image_paths = _graphics_dir
os.environ["GAFFERUI_IMAGE_PATHS"] = image_paths


if GafferScatterPaintUI.PaintPointsTool is not None:
    GafferUI.Tool.registerTool(
        "PaintPointsTool",
        GafferSceneUI.SceneView,
        GafferScatterPaintUI.PaintPointsTool,
    )
else:
    _tool_error = getattr(GafferScatterPaintUI, "_PAINT_POINTS_TOOL_IMPORT_ERROR", None)
    IECore.msg(
        IECore.Msg.Level.Warning,
        "GafferScatterPaintUI",
        "PaintPointsTool UI extension is unavailable; skipping tool registration.{}".format(
            " {}: {}".format(type(_tool_error).__name__, _tool_error)
            if _tool_error is not None
            else ""
        ),
    )
