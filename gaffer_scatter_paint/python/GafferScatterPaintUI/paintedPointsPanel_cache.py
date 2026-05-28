# pyright: reportMissingImports=false, reportMissingModuleSource=false

import collections
import functools
import os
import time

import IECore
import Gaffer
import GafferUI

from .paintedPointsPanel_common import (
    _CACHE_MODE_LABELS,
    _panelFormatFields,
    _panelIsDeletedRuntimeError,
    _panelLogInfo,
    _panelLogWarning,
    _panelPreviewMessage,
    _panelWidgetAlive,
)


class PaintedPointsCacheWidget(GafferUI.Widget):
    def __init__(self, node):
        self.__node = node
        self.__lastActionMessage = ""
        self.__lastActionFailed = False
        self.__actionTimingStack = []
        self.__lastMutationDurationMs = None
        self.__disposed = False

        self.__column = GafferUI.ListContainer(
            GafferUI.ListContainer.Orientation.Vertical,
            spacing=6,
        )

        GafferUI.Widget.__init__(self, self.__column)

        with self.__column:
            self.__cacheLabel = GafferUI.Label("")
            self.__statusLabel = GafferUI.Label("")
            self.__lockLabel = GafferUI.Label("")
            self.__actionLabel = GafferUI.Label("")

            with GafferUI.Collapsible(label="Cache Settings", collapsed=False):
                with GafferUI.ListContainer(
                    GafferUI.ListContainer.Orientation.Vertical,
                    spacing=4,
                ):
                    for plug_name in (
                        "cacheMode",
                        "cachePath",
                        "cachePathMode",
                        "projectRoot",
                        "lockMode",
                        "backupEnabled",
                        "backupPolicy",
                        "compactionMode",
                    ):
                        GafferUI.PlugWidget(node[plug_name])

            with GafferUI.Collapsible(label="Diagnostics", collapsed=False):
                with GafferUI.ListContainer(
                    GafferUI.ListContainer.Orientation.Vertical,
                    spacing=4,
                ):
                    for plug_name in (
                        "validationSummary",
                        "cacheResolvedPath",
                        "cacheVersion",
                        "invalidPointCount",
                        "invalidStrokeCount",
                        "failingFrame",
                        "failingTargetPaths",
                        "lastErrorMessage",
                        "topologyMismatchCount",
                        "validationCategories",
                        "cacheLockedBy",
                        "cacheLockedHost",
                        "cacheLockedTime",
                        "cacheLockedScript",
                        "cacheDescription",
                    ):
                        if plug_name in node:
                            GafferUI.PlugWidget(node[plug_name])

            with GafferUI.Collapsible(label="Cache Actions", collapsed=False):
                with GafferUI.ListContainer(
                    GafferUI.ListContainer.Orientation.Vertical,
                    spacing=4,
                ):
                    with GafferUI.ListContainer(
                        GafferUI.ListContainer.Orientation.Horizontal,
                        spacing=4,
                    ):
                        self.__validateCacheButton = GafferUI.Button("Validate Cache")
                        self.__validateAttachmentsButton = GafferUI.Button(
                            "Validate Attach"
                        )
                        self.__compactCacheButton = GafferUI.Button("Compact")

            with GafferUI.Collapsible(label="Exports", collapsed=False):
                with GafferUI.ListContainer(
                    GafferUI.ListContainer.Orientation.Vertical,
                    spacing=4,
                ):
                    with GafferUI.ListContainer(
                        GafferUI.ListContainer.Orientation.Horizontal,
                        spacing=4,
                    ):
                        self.__exportDiagnosticsButton = GafferUI.Button("Diagnostics")
                        self.__exportAuthoredButton = GafferUI.Button("Authored")
                    with GafferUI.ListContainer(
                        GafferUI.ListContainer.Orientation.Horizontal,
                        spacing=4,
                    ):
                        self.__exportInterchangeButton = GafferUI.Button("Interchange")
                        self.__exportEvaluatedButton = GafferUI.Button("Evaluated")

            with GafferUI.Collapsible(label="Bake", collapsed=False):
                with GafferUI.ListContainer(
                    GafferUI.ListContainer.Orientation.Vertical,
                    spacing=4,
                ):
                    with GafferUI.ListContainer(
                        GafferUI.ListContainer.Orientation.Horizontal,
                        spacing=4,
                    ):
                        self.__freezeBakeSelectionButton = GafferUI.Button(
                            "Bake Selection"
                        )
                        self.__freezeBakeAuthoredButton = GafferUI.Button(
                            "Bake Authored"
                        )
                    with GafferUI.ListContainer(
                        GafferUI.ListContainer.Orientation.Horizontal,
                        spacing=4,
                    ):
                        self.__freezeBakeEvaluatedButton = GafferUI.Button(
                            "Bake Evaluated"
                        )

        self._qtWidget().destroyed.connect(self.__qtDestroyed)

        self.__plugSetConnection = self.__node.plugSetSignal().connect(
            Gaffer.WeakMethod(self.__plugSet),
            scoped=True,
        )
        self.__validateCacheClickedConnection = self.__connectActionButton(
            self.__validateCacheButton,
            self.__validateCacheClicked,
            "cache.validate",
            "Cache",
        )
        self.__validateAttachmentsClickedConnection = self.__connectActionButton(
            self.__validateAttachmentsButton,
            self.__validateAttachmentsClicked,
            "cache.validate_attachments",
            "Cache",
        )
        self.__compactCacheClickedConnection = self.__connectActionButton(
            self.__compactCacheButton,
            self.__compactCacheClicked,
            "cache.compact",
            "Cache",
        )
        self.__exportDiagnosticsClickedConnection = self.__connectActionButton(
            self.__exportDiagnosticsButton,
            self.__exportDiagnosticsClicked,
            "export.diagnostics",
            "Exports",
        )
        self.__exportAuthoredClickedConnection = self.__connectActionButton(
            self.__exportAuthoredButton,
            self.__exportAuthoredClicked,
            "export.authored",
            "Exports",
        )
        self.__exportInterchangeClickedConnection = self.__connectActionButton(
            self.__exportInterchangeButton,
            self.__exportInterchangeClicked,
            "export.interchange",
            "Exports",
        )
        self.__exportEvaluatedClickedConnection = self.__connectActionButton(
            self.__exportEvaluatedButton,
            self.__exportEvaluatedClicked,
            "export.evaluated",
            "Exports",
        )
        self.__freezeBakeSelectionClickedConnection = self.__connectActionButton(
            self.__freezeBakeSelectionButton,
            self.__freezeBakeSelectionClicked,
            "bake.selection",
            "Bake",
        )
        self.__freezeBakeAuthoredClickedConnection = self.__connectActionButton(
            self.__freezeBakeAuthoredButton,
            self.__freezeBakeAuthoredClicked,
            "bake.authored",
            "Bake",
        )
        self.__freezeBakeEvaluatedClickedConnection = self.__connectActionButton(
            self.__freezeBakeEvaluatedButton,
            self.__freezeBakeEvaluatedClicked,
            "bake.evaluated",
            "Bake",
        )

        self.__update()

    def __plugSet(self, plug):
        if self.__disposed or not _panelWidgetAlive(self):
            return
        if plug.node().isSame(self.__node):
            self.__update()

    def __qtDestroyed(self, *unused_args):
        del unused_args
        self.__disposed = True
        self.__plugSetConnection = None

    def __actionLogContext(self, action_name=None, tab_name=None):
        context = collections.OrderedDict()
        context["action"] = action_name
        context["tab"] = tab_name
        context["node"] = self.__node.getName()
        cache_mode = int(self.__node["cacheMode"].getValue())
        context["cacheMode"] = _CACHE_MODE_LABELS.get(cache_mode, cache_mode)
        context["layerCount"] = len(self.__node["layers"].getValue())
        context["selectionSetCount"] = len(self.__node["selectionSets"].getValue())
        return context

    def __logActionStart(self, action_name, tab_name=None, extra_fields=None):
        fields = self.__actionLogContext(action_name, tab_name)
        if extra_fields:
            fields.update(extra_fields)
        _panelLogInfo("ACTION start {}".format(_panelFormatFields(fields)))

    def __logActionEnd(
        self,
        action_name,
        tab_name=None,
        extra_fields=None,
        result=None,
        mutation_ms=None,
        status="ok",
        elapsed_ms=None,
        error=None,
    ):
        fields = self.__actionLogContext(action_name, tab_name)
        fields["status"] = status
        if elapsed_ms is not None:
            fields["elapsedMs"] = elapsed_ms
        if mutation_ms is not None:
            fields["mutationMs"] = mutation_ms
        if result is not None:
            fields["result"] = _panelPreviewMessage(result)
        if extra_fields:
            fields.update(extra_fields)
        if error is not None:
            fields["error"] = _panelPreviewMessage(error)
            _panelLogWarning("ACTION end {}".format(_panelFormatFields(fields)))
            return
        _panelLogInfo("ACTION end {}".format(_panelFormatFields(fields)))

    def __timedButtonAction(
        self, action_name, callback, tab_name=None, extra_fields_fn=None
    ):
        def wrapper(button, *args):
            start_time = time.perf_counter()
            before_mutation_count = len(self.__actionTimingStack)
            self.__lastMutationDurationMs = None
            self.__lastActionFailed = False
            extra_fields = extra_fields_fn() if callable(extra_fields_fn) else None
            self.__logActionStart(action_name, tab_name, extra_fields)
            try:
                result = callback(button, *args)
            except Exception as exc:
                elapsed_ms = (time.perf_counter() - start_time) * 1000.0
                mutation_ms = None
                if len(self.__actionTimingStack) == before_mutation_count:
                    mutation_ms = self.__lastMutationDurationMs
                self.__logActionEnd(
                    action_name,
                    tab_name,
                    extra_fields=extra_fields,
                    mutation_ms=mutation_ms,
                    status="error",
                    elapsed_ms=elapsed_ms,
                    error=exc,
                )
                raise
            elapsed_ms = (time.perf_counter() - start_time) * 1000.0
            mutation_ms = None
            if len(self.__actionTimingStack) == before_mutation_count:
                mutation_ms = self.__lastMutationDurationMs
            result_text = self.__lastActionMessage or result
            if self.__lastActionFailed:
                self.__logActionEnd(
                    action_name,
                    tab_name,
                    extra_fields=extra_fields,
                    result=result_text,
                    mutation_ms=mutation_ms,
                    status="error",
                    elapsed_ms=elapsed_ms,
                    error=result_text or "action failed",
                )
                return result
            self.__logActionEnd(
                action_name,
                tab_name,
                extra_fields=extra_fields,
                result=result_text,
                mutation_ms=mutation_ms,
                status="ok",
                elapsed_ms=elapsed_ms,
            )
            return result

        return wrapper

    def __connectActionButton(
        self, button, callback, action_name, tab_name=None, extra_fields_fn=None
    ):
        return button.clickedSignal().connect(
            self.__timedButtonAction(
                action_name,
                callback,
                tab_name=tab_name,
                extra_fields_fn=extra_fields_fn,
            ),
            scoped=True,
        )

    def __scriptNode(self):
        return self.__node.ancestor(Gaffer.ScriptNode)

    def __performMutation(self, mutator):
        script_node = self.__scriptNode()
        if script_node is None:
            raise RuntimeError("PaintedPoints cache panel requires a ScriptNode")
        timing_entry = {"start": time.perf_counter(), "durationMs": None}
        self.__actionTimingStack.append(timing_entry)
        try:
            with Gaffer.UndoScope(script_node):
                return mutator()
        finally:
            timing_entry["durationMs"] = (
                time.perf_counter() - timing_entry["start"]
            ) * 1000.0
            self.__lastMutationDurationMs = timing_entry["durationMs"]
            self.__actionTimingStack.pop()

    def __setLastActionMessage(self, message):
        self.__lastActionMessage = str(message or "").strip()

    def __cacheActionEnabled(self):
        return bool(
            self.__node["cacheBlob"].getValue()
            or self.__node["cachePath"].getValue()
            or self.__node["layers"].getValue()
        )

    def __exportPathMessage(self, label, path):
        if not path:
            return label
        return "{}: {}".format(label, os.path.basename(str(path)))

    def __runNodeAction(self, action, success_prefix, use_undo=False):
        try:
            result = self.__performMutation(action) if use_undo else action()
        except Exception as exc:
            self.__lastActionFailed = True
            self.__setLastActionMessage("{} failed ({})".format(success_prefix, exc))
            self.__update()
            return None
        self.__lastActionFailed = False
        detail = result.strip() if isinstance(result, str) else result
        if detail in (None, ""):
            self.__setLastActionMessage(success_prefix)
        else:
            self.__setLastActionMessage("{}: {}".format(success_prefix, detail))
        self.__update()
        return result

    def __update(self):
        if self.__disposed or not _panelWidgetAlive(self):
            return
        cache_mode = _CACHE_MODE_LABELS.get(
            int(self.__node["cacheMode"].getValue()), "Unknown"
        )
        cache_path = (
            self.__node["cacheResolvedPath"].getValue()
            or self.__node["cachePath"].getValue()
        )
        try:
            self.__cacheLabel.setText(
                "Cache Status : {} | path={} | version={}".format(
                    cache_mode,
                    cache_path or "<embedded>",
                    self.__node["cacheVersion"].getValue(),
                )
            )
            self.__statusLabel.setText(
                "Diagnostics Summary : {} | invalid points={} | invalid strokes={} | topology mismatches={}".format(
                    self.__node["validationSummary"].getValue()
                    or "Unknown scatter paint state",
                    self.__node["invalidPointCount"].getValue(),
                    self.__node["invalidStrokeCount"].getValue(),
                    self.__node["topologyMismatchCount"].getValue(),
                )
            )
            self.__lockLabel.setText(
                "Lock Status : {}@{} {}".format(
                    self.__node["cacheLockedBy"].getValue() or "-",
                    self.__node["cacheLockedHost"].getValue() or "-",
                    self.__node["cacheLockedTime"].getValue() or "",
                ).strip()
            )
            self.__actionLabel.setText(
                "Last Action : {}".format(self.__lastActionMessage or "Ready")
            )
        except RuntimeError as exc:
            if _panelIsDeletedRuntimeError(exc):
                self.__disposed = True
                return
            raise

        cache_actions_enabled = self.__cacheActionEnabled()
        self.__validateCacheButton.setEnabled(cache_actions_enabled)
        self.__validateAttachmentsButton.setEnabled(
            bool(self.__node["layers"].getValue())
        )
        self.__compactCacheButton.setEnabled(cache_actions_enabled)
        self.__exportDiagnosticsButton.setEnabled(cache_actions_enabled)
        self.__exportAuthoredButton.setEnabled(cache_actions_enabled)
        self.__exportInterchangeButton.setEnabled(cache_actions_enabled)
        self.__exportEvaluatedButton.setEnabled(cache_actions_enabled)
        self.__freezeBakeSelectionButton.setEnabled(
            bool(
                self.__node["selectionSets"].getValue()
                or self.__node["layers"].getValue()
            )
        )
        self.__freezeBakeAuthoredButton.setEnabled(cache_actions_enabled)
        self.__freezeBakeEvaluatedButton.setEnabled(cache_actions_enabled)

    def __validateCacheClicked(self, button):
        del button
        self.__runNodeAction(self.__node.validateCache, "Validated cache")

    def __validateAttachmentsClicked(self, button):
        del button
        self.__runNodeAction(self.__node.validateAttachments, "Validated attachments")

    def __compactCacheClicked(self, button):
        del button
        self.__runNodeAction(self.__node.compactCache, "Compacted cache", use_undo=True)

    def __exportDiagnosticsClicked(self, button):
        del button
        result = self.__runNodeAction(
            self.__node.exportDiagnostics, "Exported diagnostics"
        )
        if result is not None:
            self.__setLastActionMessage(
                self.__exportPathMessage("Exported diagnostics", result)
            )
            self.__update()

    def __exportAuthoredClicked(self, button):
        del button
        result = self.__runNodeAction(
            self.__node.exportAuthoredCache, "Exported authored cache"
        )
        if result is not None:
            self.__setLastActionMessage(
                self.__exportPathMessage("Exported authored cache", result)
            )
            self.__update()

    def __exportInterchangeClicked(self, button):
        del button
        result = self.__runNodeAction(
            self.__node.exportInterchange, "Exported interchange"
        )
        if result is not None:
            self.__setLastActionMessage(
                self.__exportPathMessage("Exported interchange", result)
            )
            self.__update()

    def __exportEvaluatedClicked(self, button):
        del button
        result = self.__runNodeAction(
            self.__node.exportEvaluatedPoints, "Exported evaluated points"
        )
        if result is not None:
            self.__setLastActionMessage(
                self.__exportPathMessage("Exported evaluated points", result)
            )
            self.__update()

    def __freezeBakeSelectionClicked(self, button):
        del button
        self.__runNodeAction(
            self.__node.freezeBakeSelection, "Baked selection", use_undo=True
        )

    def __freezeBakeAuthoredClicked(self, button):
        del button
        self.__runNodeAction(
            self.__node.freezeBakeToStaticNode, "Baked authored", use_undo=True
        )

    def __freezeBakeEvaluatedClicked(self, button):
        del button
        self.__runNodeAction(
            self.__node.freezeBakeEvaluatedToStaticNode,
            "Baked evaluated",
            use_undo=True,
        )
