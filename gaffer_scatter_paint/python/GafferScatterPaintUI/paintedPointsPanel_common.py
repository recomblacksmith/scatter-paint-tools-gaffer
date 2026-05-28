# pyright: reportMissingImports=false, reportMissingModuleSource=false

import collections
import functools
import os
import time

import IECore
import Gaffer
import GafferUI

_CACHE_MODE_LABELS = {
    0: "Embedded",
    1: "External",
}

_FRAME_MODE_LABELS = {
    0: "Persistent",
    1: "Additive",
    2: "Override",
}

_POINT_FILTER_LABELS = collections.OrderedDict(
    [
        ("all", "All Points"),
        ("problematic", "Problematic"),
        ("invalid", "Invalid"),
        ("unresolved", "Unresolved"),
        ("fallback", "Approximated Anchors"),
    ]
)

_ANCHOR_MODE_LABELS = {
    0: "Barycentric",
    1: "HybridApproximated",
    2: "UVApproximated",
    3: "Unresolved",
}

_PANEL_DIAGNOSTICS_ENV = "GAFFER_SCATTER_PAINT_PANEL_DIAGNOSTICS"
_PANEL_LOG_CONTEXT = "GafferScatterPaintUI.Panel"


def _panelDiagnosticsEnabled():
    value = os.environ.get(_PANEL_DIAGNOSTICS_ENV, "")
    return value.strip().lower() in ("1", "true", "yes", "on")


def _panelFormatValue(value):
    if value is None:
        return "none"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, float):
        return "{:.3f}".format(value)
    if isinstance(value, (int, str)):
        text = str(value)
    elif isinstance(value, (list, tuple)):
        text = "[{}]".format(",".join(_panelFormatValue(item) for item in list(value)))
    else:
        text = str(value)
    text = text.replace("\n", "\\n")
    if not text:
        return '""'
    if any(character.isspace() for character in text) or "=" in text or '"' in text:
        return '"{}"'.format(text.replace('"', '\\"'))
    return text


def _panelFormatFields(fields):
    parts = []
    for key, value in fields.items():
        if value is None:
            continue
        if isinstance(value, (list, tuple)) and not list(value):
            continue
        if isinstance(value, str) and not value:
            continue
        parts.append("{}={}".format(key, _panelFormatValue(value)))
    return " ".join(parts)


def _panelPreviewMessage(message, limit=160):
    text = str(message or "").strip()
    if len(text) <= limit:
        return text
    return text[: limit - 3] + "..."


def _panelLogInfo(message):
    if _panelDiagnosticsEnabled():
        IECore.msg(IECore.Msg.Level.Info, _PANEL_LOG_CONTEXT, message)


def _panelLogWarning(message):
    IECore.msg(IECore.Msg.Level.Warning, _PANEL_LOG_CONTEXT, message)


def _panelIsDeletedRuntimeError(exception):
    return isinstance(exception, RuntimeError) and "already deleted" in str(exception)


def _panelWidgetAlive(widget):
    try:
        widget._qtWidget()
        return True
    except RuntimeError as exc:
        if _panelIsDeletedRuntimeError(exc):
            return False
        raise
