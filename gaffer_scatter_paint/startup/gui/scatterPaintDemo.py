import os
import importlib.util
import pathlib

import Gaffer
import GafferScene
import GafferSceneUI
import GafferUI
import IECore

import GafferScatterPaint


def _load_actions_module():
    actions_path = (
        pathlib.Path(__file__).resolve().parent.parent
        / "GafferScatterPaintUI"
        / "actions.py"
    )
    spec = importlib.util.spec_from_file_location("_scatterPaintActions", actions_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load scatter paint actions from {actions_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


_actions = _load_actions_module()


if os.environ.get("GAFFER_SCATTER_PAINT_BUILD_DEMO") == "1":
    _built_scripts = set()

    def _script_id(script):
        application_root = script.ancestor(Gaffer.ApplicationRoot)
        if application_root is None:
            return script.getName()
        return script.relativeName(application_root)

    def _build_demo(script_window):
        script = script_window.scriptNode()
        graph_editor = None
        try:
            graph_editor = script_window.getLayout().editor(
                GafferUI.GraphEditor, focussedOnly=False
            )
        except Exception:
            graph_editor = None

        try:
            _actions.build_demo_graph_for_script(script, graph_editor=graph_editor)
        except Exception as exc:
            IECore.msg(
                IECore.Msg.Level.Error,
                "GafferScatterPaintUI",
                f"Failed to auto-build scatter paint demo graph: {exc}",
            )
        return False

    def _on_script_window_created(script_window):
        script = script_window.scriptNode()
        identifier = _script_id(script)
        if identifier in _built_scripts:
            return
        _built_scripts.add(identifier)
        GafferUI.EventLoop.addIdleCallback(lambda: _build_demo(script_window))

    GafferUI.ScriptWindow.instanceCreatedSignal().connect(_on_script_window_created)
