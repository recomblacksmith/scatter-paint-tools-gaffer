# pyright: reportMissingImports=false, reportMissingModuleSource=false

import collections
import functools
import os
import time

import IECore
import Gaffer
import GafferUI
from Qt import QtCore

from .paintedPointsPanel_common import (
    _ANCHOR_MODE_LABELS,
    _CACHE_MODE_LABELS,
    _FRAME_MODE_LABELS,
    _POINT_FILTER_LABELS,
    _panelFormatFields,
    _panelIsDeletedRuntimeError,
    _panelLogInfo,
    _panelLogWarning,
    _panelPreviewMessage,
    _panelWidgetAlive,
)


class PaintedPointsAuthoringWidget(GafferUI.Widget):
    def __init__(self, node):
        self.__node = node
        self.__selectedLayerId = None
        self.__selectedStrokeId = None
        self.__selectedPointIds = []
        self.__selectedSelectionSetId = None
        self.__currentSelectionPointCount = 0
        self.__currentSelectionStrokeCount = 0
        self.__currentSelectionPointIds = []
        self.__currentSelectionStrokeIds = []
        self.__pointFilterMode = "all"
        self.__pointFilterQuery = ""
        self.__pointFilterLayerId = None
        self.__pointFilterStrokeId = None
        self.__filteredPointIds = []
        self.__filteredPointSummary = {
            "invalid": 0,
            "unresolved": 0,
            "fallback": 0,
        }
        self.__lastActionMessage = ""
        self.__lastActionFailed = False
        self.__updatingSelections = False
        self.__layersById = {}
        self.__strokesById = {}
        self.__pointsById = {}
        self.__selectionSetsById = {}
        self.__layerIdByPath = {}
        self.__strokeIdByPath = {}
        self.__pointIdByPath = {}
        self.__selectionSetIdByPath = {}
        self.__layerPathById = {}
        self.__strokePathById = {}
        self.__pointPathById = {}
        self.__selectionSetPathById = {}
        self.__layersData = collections.OrderedDict()
        self.__strokesData = collections.OrderedDict()
        self.__pointsData = collections.OrderedDict()
        self.__selectionsData = collections.OrderedDict()
        self.__authoredPointCount = 0
        self.__pointsLoaded = False
        self.__updateQueued = False
        self.__forcePointReload = False
        self.__disposed = False

        self.__column = GafferUI.ListContainer(
            GafferUI.ListContainer.Orientation.Vertical,
            spacing=6,
        )
        self.__actionTimingStack = []
        self.__lastMutationDurationMs = None

        GafferUI.Widget.__init__(self, self.__column)

        with self.__column:
            self.__cacheLabel = GafferUI.Label("")
            self.__countsLabel = GafferUI.Label("")
            self.__selectionLabel = GafferUI.Label("")
            self.__focusLabel = GafferUI.Label("")
            self.__statusLabel = GafferUI.Label("")

            with GafferUI.Collapsible(label="Layer Actions", collapsed=False):
                with GafferUI.ListContainer(
                    GafferUI.ListContainer.Orientation.Horizontal,
                    spacing=4,
                ):
                    self.__layerAddButton = GafferUI.Button("Add")
                    self.__layerRenameButton = GafferUI.Button("Rename")
                    self.__layerDeleteButton = GafferUI.Button("Delete")
                    self.__layerMergeButton = GafferUI.MenuButton(
                        "Merge Into",
                        menu=GafferUI.Menu(
                            Gaffer.WeakMethod(self.__layerMergeMenuDefinition)
                        ),
                    )
                    self.__layerUpButton = GafferUI.Button("Up")
                    self.__layerDownButton = GafferUI.Button("Down")
                    self.__layerVisibleButton = GafferUI.Button("Visible")
                    self.__layerMuteButton = GafferUI.Button("Mute")
                    self.__layerSoloButton = GafferUI.Button("Solo")

            with GafferUI.Collapsible(label="Layer Timing", collapsed=False):
                with GafferUI.ListContainer(
                    GafferUI.ListContainer.Orientation.Horizontal,
                    spacing=4,
                ):
                    GafferUI.Label("Mode")
                    self.__layerModeButton = GafferUI.MenuButton(
                        "Persistent",
                        menu=GafferUI.Menu(
                            Gaffer.WeakMethod(self.__layerModeMenuDefinition)
                        ),
                    )
                    GafferUI.Label("Start")
                    self.__layerFrameStartWidget = GafferUI.NumericWidget(0)
                    self.__layerFrameStartWidget.setFixedCharacterWidth(5)
                    GafferUI.Label("End")
                    self.__layerFrameEndWidget = GafferUI.NumericWidget(0)
                    self.__layerFrameEndWidget.setFixedCharacterWidth(5)
                    self.__layerFrameApplyButton = GafferUI.Button("Set Range")

            with GafferUI.Collapsible(label="Stroke Actions", collapsed=False):
                with GafferUI.ListContainer(
                    GafferUI.ListContainer.Orientation.Horizontal,
                    spacing=4,
                ):
                    self.__strokeAddButton = GafferUI.Button("Add")
                    self.__strokeRenameButton = GafferUI.Button("Rename")
                    self.__strokeDeleteButton = GafferUI.Button("Delete")
                    self.__strokeMergeButton = GafferUI.MenuButton(
                        "Merge Into",
                        menu=GafferUI.Menu(
                            Gaffer.WeakMethod(self.__strokeMergeMenuDefinition)
                        ),
                    )
                    self.__strokeUpButton = GafferUI.Button("Up")
                    self.__strokeDownButton = GafferUI.Button("Down")
                    self.__splitSelectionButton = GafferUI.Button("Split Selection")

            with GafferUI.Collapsible(label="Stroke Timing", collapsed=False):
                with GafferUI.ListContainer(
                    GafferUI.ListContainer.Orientation.Horizontal,
                    spacing=4,
                ):
                    GafferUI.Label("Mode")
                    self.__strokeModeButton = GafferUI.MenuButton(
                        "Persistent",
                        menu=GafferUI.Menu(
                            Gaffer.WeakMethod(self.__strokeModeMenuDefinition)
                        ),
                    )
                    GafferUI.Label("Start")
                    self.__strokeFrameStartWidget = GafferUI.NumericWidget(0)
                    self.__strokeFrameStartWidget.setFixedCharacterWidth(5)
                    GafferUI.Label("End")
                    self.__strokeFrameEndWidget = GafferUI.NumericWidget(0)
                    self.__strokeFrameEndWidget.setFixedCharacterWidth(5)
                    self.__strokeFrameApplyButton = GafferUI.Button("Set Range")

            with GafferUI.Collapsible(label="Selection Actions", collapsed=False):
                with GafferUI.ListContainer(
                    GafferUI.ListContainer.Orientation.Horizontal,
                    spacing=4,
                ):
                    self.__selectionStoreButton = GafferUI.Button("Store")
                    self.__selectionRecallButton = GafferUI.Button("Recall")
                    self.__selectionRenameButton = GafferUI.Button("Rename")
                    self.__selectionDeleteButton = GafferUI.Button("Delete")
                    self.__selectionClearButton = GafferUI.Button("Clear Current")

            with GafferUI.Collapsible(label="Point Repair", collapsed=False):
                with GafferUI.ListContainer(
                    GafferUI.ListContainer.Orientation.Horizontal,
                    spacing=4,
                ):
                    self.__relaxSelectionButton = GafferUI.Button("Relax Selected")
                    self.__reprojectSelectionButton = GafferUI.Button(
                        "Reproject Selected"
                    )
                    self.__removeSelectionButton = GafferUI.Button("Remove Selected")

            with GafferUI.Collapsible(label="Point Filters", collapsed=False):
                with GafferUI.ListContainer(
                    GafferUI.ListContainer.Orientation.Vertical,
                    spacing=4,
                ):
                    with GafferUI.ListContainer(
                        GafferUI.ListContainer.Orientation.Horizontal,
                        spacing=4,
                    ):
                        GafferUI.Label("Show")
                        self.__pointFilterModeButton = GafferUI.MenuButton(
                            _POINT_FILTER_LABELS["all"],
                            menu=GafferUI.Menu(
                                Gaffer.WeakMethod(self.__pointFilterMenuDefinition)
                            ),
                        )
                        GafferUI.Label("Search")
                        self.__pointFilterTextWidget = GafferUI.TextWidget(
                            "",
                            placeholderText="point id, layer, stroke, path",
                            parenting={"expand": True},
                        )
                        self.__pointFilterClearButton = GafferUI.Button("Clear")
                        self.__pointFilterSelectButton = GafferUI.Button(
                            "Select Filtered"
                        )
                    with GafferUI.ListContainer(
                        GafferUI.ListContainer.Orientation.Horizontal,
                        spacing=4,
                    ):
                        GafferUI.Label("Layer")
                        self.__pointFilterLayerButton = GafferUI.MenuButton(
                            "All Layers",
                            menu=GafferUI.Menu(
                                Gaffer.WeakMethod(self.__pointFilterLayerMenuDefinition)
                            ),
                        )
                        GafferUI.Label("Stroke")
                        self.__pointFilterStrokeButton = GafferUI.MenuButton(
                            "All Strokes",
                            menu=GafferUI.Menu(
                                Gaffer.WeakMethod(
                                    self.__pointFilterStrokeMenuDefinition
                                )
                            ),
                        )
                    with GafferUI.ListContainer(
                        GafferUI.ListContainer.Orientation.Horizontal,
                        spacing=4,
                    ):
                        GafferUI.Label("Quick Select")
                        self.__pointSelectProblematicButton = GafferUI.Button(
                            "Problematic"
                        )
                        self.__pointSelectInvalidButton = GafferUI.Button("Invalid")
                        self.__pointSelectUnresolvedButton = GafferUI.Button(
                            "Unresolved"
                        )
                        self.__pointSelectFallbackButton = GafferUI.Button(
                            "Approximated"
                        )
                    self.__pointFilterSummaryLabel = GafferUI.Label("")

            self.__tabbedContainer = GafferUI.TabbedContainer()
            with self.__tabbedContainer:
                self.__overviewListing = self.__pathListing("Overview")
                self.__layersListing = self.__pathListing("Layers")
                self.__strokesListing = self.__pathListing("Strokes")
                self.__pointsListing = self.__pathListing(
                    "Points",
                    selection_mode=GafferUI.PathListingWidget.SelectionMode.Rows,
                )
                self.__selectionsListing = self.__pathListing("Selections")

        for listing in (
            self.__overviewListing,
            self.__layersListing,
            self.__strokesListing,
            self.__pointsListing,
            self.__selectionsListing,
        ):
            listing._qtWidget().setMinimumHeight(160)

        self.__pointsListing._qtWidget().setMinimumHeight(220)

        self._qtWidget().destroyed.connect(self.__qtDestroyed)
        self.__tabChangedConnection = (
            self.__tabbedContainer._qtWidget().currentChanged.connect(self.__tabChanged)
        )

        self.__plugSetConnection = self.__node.plugSetSignal().connect(
            Gaffer.WeakMethod(self.__plugSet),
            scoped=True,
        )

        self.__layersSelectionChangedConnection = (
            self.__layersListing.selectionChangedSignal().connect(
                Gaffer.WeakMethod(self.__layersSelectionChanged),
                scoped=True,
            )
        )
        self.__strokesSelectionChangedConnection = (
            self.__strokesListing.selectionChangedSignal().connect(
                Gaffer.WeakMethod(self.__strokesSelectionChanged),
                scoped=True,
            )
        )
        self.__pointsSelectionChangedConnection = (
            self.__pointsListing.selectionChangedSignal().connect(
                Gaffer.WeakMethod(self.__pointsSelectionChanged),
                scoped=True,
            )
        )
        self.__selectionsSelectionChangedConnection = (
            self.__selectionsListing.selectionChangedSignal().connect(
                Gaffer.WeakMethod(self.__selectionsSelectionChanged),
                scoped=True,
            )
        )

        self.__layerAddClickedConnection = self.__connectActionButton(
            self.__layerAddButton,
            self.__layerAddClicked,
            "layer.add",
            "Layers",
        )
        self.__layerRenameClickedConnection = self.__connectActionButton(
            self.__layerRenameButton,
            self.__layerRenameClicked,
            "layer.rename",
            "Layers",
        )
        self.__layerDeleteClickedConnection = self.__connectActionButton(
            self.__layerDeleteButton,
            self.__layerDeleteClicked,
            "layer.delete",
            "Layers",
        )
        self.__layerUpClickedConnection = self.__connectActionButton(
            self.__layerUpButton,
            self.__layerUpClicked,
            "layer.move_up",
            "Layers",
        )
        self.__layerDownClickedConnection = self.__connectActionButton(
            self.__layerDownButton,
            self.__layerDownClicked,
            "layer.move_down",
            "Layers",
        )
        self.__layerVisibleClickedConnection = self.__connectActionButton(
            self.__layerVisibleButton,
            self.__layerVisibleClicked,
            "layer.toggle_visible",
            "Layers",
        )
        self.__layerMuteClickedConnection = self.__connectActionButton(
            self.__layerMuteButton,
            self.__layerMuteClicked,
            "layer.toggle_mute",
            "Layers",
        )
        self.__layerSoloClickedConnection = self.__connectActionButton(
            self.__layerSoloButton,
            self.__layerSoloClicked,
            "layer.toggle_solo",
            "Layers",
        )
        self.__layerFrameApplyClickedConnection = self.__connectActionButton(
            self.__layerFrameApplyButton,
            self.__layerFrameApplyClicked,
            "layer.set_range",
            "Layers",
        )
        self.__strokeAddClickedConnection = self.__connectActionButton(
            self.__strokeAddButton,
            self.__strokeAddClicked,
            "stroke.add",
            "Strokes",
        )
        self.__strokeRenameClickedConnection = self.__connectActionButton(
            self.__strokeRenameButton,
            self.__strokeRenameClicked,
            "stroke.rename",
            "Strokes",
        )
        self.__strokeDeleteClickedConnection = self.__connectActionButton(
            self.__strokeDeleteButton,
            self.__strokeDeleteClicked,
            "stroke.delete",
            "Strokes",
        )
        self.__strokeUpClickedConnection = self.__connectActionButton(
            self.__strokeUpButton,
            self.__strokeUpClicked,
            "stroke.move_up",
            "Strokes",
        )
        self.__strokeDownClickedConnection = self.__connectActionButton(
            self.__strokeDownButton,
            self.__strokeDownClicked,
            "stroke.move_down",
            "Strokes",
        )
        self.__splitSelectionClickedConnection = self.__connectActionButton(
            self.__splitSelectionButton,
            self.__splitSelectionClicked,
            "stroke.split_selection",
            "Strokes",
        )
        self.__strokeFrameApplyClickedConnection = self.__connectActionButton(
            self.__strokeFrameApplyButton,
            self.__strokeFrameApplyClicked,
            "stroke.set_range",
            "Strokes",
        )
        self.__selectionStoreClickedConnection = self.__connectActionButton(
            self.__selectionStoreButton,
            self.__selectionStoreClicked,
            "selection.store",
            "Selections",
        )
        self.__selectionRecallClickedConnection = self.__connectActionButton(
            self.__selectionRecallButton,
            self.__selectionRecallClicked,
            "selection.recall",
            "Selections",
        )
        self.__selectionRenameClickedConnection = self.__connectActionButton(
            self.__selectionRenameButton,
            self.__selectionRenameClicked,
            "selection.rename",
            "Selections",
        )
        self.__selectionDeleteClickedConnection = self.__connectActionButton(
            self.__selectionDeleteButton,
            self.__selectionDeleteClicked,
            "selection.delete",
            "Selections",
        )
        self.__selectionClearClickedConnection = self.__connectActionButton(
            self.__selectionClearButton,
            self.__selectionClearClicked,
            "selection.clear_current",
            "Selections",
        )
        self.__relaxSelectionClickedConnection = self.__connectActionButton(
            self.__relaxSelectionButton,
            self.__relaxSelectionClicked,
            "repair.relax_selected",
            "Points",
        )
        self.__reprojectSelectionClickedConnection = self.__connectActionButton(
            self.__reprojectSelectionButton,
            self.__reprojectSelectionClicked,
            "repair.reproject_selected",
            "Points",
        )
        self.__removeSelectionClickedConnection = self.__connectActionButton(
            self.__removeSelectionButton,
            self.__removeSelectionClicked,
            "repair.remove_selected",
            "Points",
        )
        self.__pointFilterEditedConnection = (
            self.__pointFilterTextWidget.editingFinishedSignal().connect(
                Gaffer.WeakMethod(self.__pointFilterEdited),
                scoped=True,
            )
        )
        self.__pointFilterClearClickedConnection = self.__connectActionButton(
            self.__pointFilterClearButton,
            self.__pointFilterClearClicked,
            "filter.clear",
            "Points",
        )
        self.__pointFilterSelectClickedConnection = self.__connectActionButton(
            self.__pointFilterSelectButton,
            self.__pointFilterSelectClicked,
            "filter.select_filtered",
            "Points",
        )
        self.__pointSelectProblematicClickedConnection = self.__connectActionButton(
            self.__pointSelectProblematicButton,
            self.__pointSelectProblematicClicked,
            "filter.select_problematic",
            "Points",
        )
        self.__pointSelectInvalidClickedConnection = self.__connectActionButton(
            self.__pointSelectInvalidButton,
            self.__pointSelectInvalidClicked,
            "filter.select_invalid",
            "Points",
        )
        self.__pointSelectUnresolvedClickedConnection = self.__connectActionButton(
            self.__pointSelectUnresolvedButton,
            self.__pointSelectUnresolvedClicked,
            "filter.select_unresolved",
            "Points",
        )
        self.__pointSelectFallbackClickedConnection = self.__connectActionButton(
            self.__pointSelectFallbackButton,
            self.__pointSelectFallbackClicked,
            "filter.select_approximated",
            "Points",
        )

        self.__update()

    def __pathListing(
        self, label, selection_mode=GafferUI.PathListingWidget.SelectionMode.Row
    ):
        return GafferUI.PathListingWidget(
            Gaffer.DictPath(collections.OrderedDict(), "/"),
            columns=[
                GafferUI.PathListingWidget.defaultNameColumn,
                GafferUI.StandardPathColumn(
                    "Value",
                    "dict:value",
                    sizeMode=GafferUI.PathColumn.SizeMode.Stretch,
                ),
            ],
            displayMode=GafferUI.PathListingWidget.DisplayMode.Tree,
            selectionMode=selection_mode,
            horizontalScrollMode=GafferUI.ScrollMode.Automatic,
            sortable=False,
            parenting={"label": label},
        )

    def __plugSet(self, plug):
        if self.__disposed or not _panelWidgetAlive(self):
            return
        if plug.node().isSame(self.__node):
            self.__scheduleUpdate(force_points=self.__pointsTabActive())

    def __tabChanged(self, *unused_args):
        self.__scheduleUpdate(force_points=self.__pointsTabActive())

    def __scheduleUpdate(self, force_points=False):
        if self.__disposed or not _panelWidgetAlive(self):
            return
        self.__forcePointReload = self.__forcePointReload or bool(force_points)
        if self.__updateQueued:
            return
        self.__updateQueued = True
        QtCore.QTimer.singleShot(0, Gaffer.WeakMethod(self.__flushScheduledUpdate))

    def __flushScheduledUpdate(self):
        self.__updateQueued = False
        force_points = self.__forcePointReload
        self.__forcePointReload = False
        self.__update(force_points=force_points)

    def __qtDestroyed(self, *unused_args):
        del unused_args
        self.__disposed = True
        self.__plugSetConnection = None
        self.__tabChangedConnection = None

    def __layersSelectionChanged(self, listing):
        del listing
        if self.__updatingSelections:
            return
        selected_paths = self.__selectedPathStrings(self.__layersListing)
        selected_layer_ids = [
            self.__layerIdByPath.get(path)
            for path in selected_paths
            if self.__layerIdByPath.get(path) is not None
        ]
        self.__selectedLayerId = selected_layer_ids[0] if selected_layer_ids else None
        if self.__selectedStrokeId is not None:
            stroke = self.__strokesById.get(self.__selectedStrokeId)
            if stroke is None or int(stroke.get("layerId", 0)) != int(
                self.__selectedLayerId or 0
            ):
                self.__selectedStrokeId = None
        self.__selectedPointIds = []
        self.__restoreSelections()
        self.__updateFocusLabel()
        self.__updateActionState()

    def __strokesSelectionChanged(self, listing):
        del listing
        if self.__updatingSelections:
            return
        selected_paths = self.__selectedPathStrings(self.__strokesListing)
        selected_stroke_ids = [
            self.__strokeIdByPath.get(path)
            for path in selected_paths
            if self.__strokeIdByPath.get(path) is not None
        ]
        self.__selectedStrokeId = (
            selected_stroke_ids[0] if selected_stroke_ids else None
        )
        if self.__selectedStrokeId is not None:
            stroke = self.__strokesById.get(self.__selectedStrokeId)
            if stroke is not None:
                self.__selectedLayerId = int(stroke.get("layerId", 0))
        self.__selectedPointIds = []
        self.__syncCurrentSelectionFromPanel(stroke_ids=selected_stroke_ids)
        self.__restoreSelections()
        self.__updateFocusLabel()
        self.__updateActionState()

    def __pointsSelectionChanged(self, listing):
        del listing
        if self.__updatingSelections:
            return
        selected_paths = self.__selectedPathStrings(self.__pointsListing)
        selected_point_ids = []
        for path in selected_paths:
            point_id = self.__pointIdByPath.get(path)
            if point_id is None:
                continue
            selected_point_ids.append(int(point_id))
        self.__selectedPointIds = selected_point_ids
        selected_stroke_ids = []
        if self.__selectedPointIds:
            first_point_id = self.__selectedPointIds[0]
            point_record = self.__pointsById.get(first_point_id)
            if point_record is not None:
                self.__selectedStrokeId = int(point_record.get("strokeId", 0))
                self.__selectedLayerId = int(point_record.get("layerId", 0))
            selected_stroke_ids_map = collections.OrderedDict()
            for point_id in self.__selectedPointIds:
                point = self.__pointsById.get(point_id)
                if point is None:
                    continue
                selected_stroke_ids_map[int(point.get("strokeId", 0))] = None
            selected_stroke_ids = list(selected_stroke_ids_map.keys())
        self.__syncCurrentSelectionFromPanel(
            point_ids=self.__selectedPointIds,
            stroke_ids=selected_stroke_ids,
        )
        self.__restoreSelections()
        self.__updateFocusLabel()
        self.__updateActionState()

    def __selectionsSelectionChanged(self, listing):
        del listing
        if self.__updatingSelections:
            return
        selected_paths = self.__selectedPathStrings(self.__selectionsListing)
        selected_ids = [
            self.__selectionSetIdByPath.get(path)
            for path in selected_paths
            if self.__selectionSetIdByPath.get(path) is not None
        ]
        self.__selectedSelectionSetId = selected_ids[0] if selected_ids else None
        self.__restoreSelections()
        self.__updateFocusLabel()
        self.__updateActionState()

    def __update(self, force_points=False):
        if self.__disposed or not _panelWidgetAlive(self):
            return
        previous_layer_id = self.__selectedLayerId
        previous_stroke_id = self.__selectedStrokeId
        previous_point_ids = list(self.__selectedPointIds)
        previous_selection_set_id = self.__selectedSelectionSetId

        point_records = []
        point_records_error = ""
        load_points = bool(force_points or self.__pointsTabActive())

        try:
            snapshot = self.__node.cacheSnapshot()
            load_error = ""
        except Exception as exc:
            snapshot = None
            load_error = str(exc)

        if snapshot is not None:
            try:
                self.__authoredPointCount = int(
                    self.__node["authoredPointCount"].getValue()
                )
            except Exception:
                self.__authoredPointCount = 0

            if load_points:
                try:
                    point_records = list(self.__node.pointRecords())
                except Exception as exc:
                    point_records = []
                    point_records_error = str(exc)

        self.__pointsLoaded = bool(load_points and not point_records_error)

        if snapshot is None:
            self.__authoredPointCount = 0
            self.__pointsLoaded = False
            self.__layersById = {}
            self.__strokesById = {}
            self.__pointsById = {}
            self.__selectionSetsById = {}
            self.__layerIdByPath = {}
            self.__strokeIdByPath = {}
            self.__pointIdByPath = {}
            self.__selectionSetIdByPath = {}
            self.__layerPathById = {}
            self.__strokePathById = {}
            self.__pointPathById = {}
            self.__selectionSetPathById = {}
            self.__layersData = collections.OrderedDict()
            self.__strokesData = collections.OrderedDict()
            self.__pointsData = collections.OrderedDict()
            self.__selectionsData = collections.OrderedDict()
            self.__selectedLayerId = None
            self.__selectedStrokeId = None
            self.__selectedPointIds = []
            self.__selectedSelectionSetId = None
            self.__currentSelectionPointCount = 0
            self.__currentSelectionStrokeCount = 0
            self.__currentSelectionPointIds = []
            self.__currentSelectionStrokeIds = []
            self.__filteredPointIds = []
            self.__filteredPointSummary = {
                "invalid": 0,
                "unresolved": 0,
                "fallback": 0,
            }
            try:
                self.__cacheLabel.setText("Cache : unavailable")
                self.__countsLabel.setText("Authoring : unavailable")
                self.__selectionLabel.setText("Selection : unavailable")
                self.__focusLabel.setText("Focus : unavailable")
                self.__statusLabel.setText(
                    "Status : {}".format(load_error or "Unknown error")
                )
            except RuntimeError as exc:
                if _panelIsDeletedRuntimeError(exc):
                    self.__disposed = True
                    return
                raise
            empty = collections.OrderedDict({"Error": load_error or "Unknown error"})
            self.__setListingData(self.__overviewListing, empty, expand_all=True)
            self.__setListingData(self.__layersListing, collections.OrderedDict())
            self.__setListingData(self.__strokesListing, collections.OrderedDict())
            self.__setListingData(self.__pointsListing, collections.OrderedDict())
            self.__setListingData(self.__selectionsListing, collections.OrderedDict())
            self.__updateActionState()
            return

        layers = list(snapshot.get("layers", []))
        strokes = list(snapshot.get("strokes", []))
        selection_sets = list(snapshot.get("selectionSets", []))
        current_selection = dict(snapshot.get("currentSelection", {}))
        diagnostics = dict(snapshot.get("diagnostics", {}))
        node_metadata = dict(snapshot.get("node", {}))

        self.__layersById = {
            int(layer.get("layerId", 0)): dict(layer) for layer in layers
        }
        self.__strokesById = {
            int(stroke.get("strokeId", 0)): dict(stroke) for stroke in strokes
        }
        self.__pointsById = {
            int(point.get("pointId", 0)): dict(point) for point in point_records
        }
        self.__selectionSetsById = {
            int(selection_set.get("selectionSetId", 0)): dict(selection_set)
            for selection_set in selection_sets
        }

        ordered_layers = sorted(
            layers,
            key=lambda item: (item.get("order", 0), item.get("layerId", 0)),
        )
        ordered_strokes = sorted(
            strokes,
            key=lambda item: (
                self.__layersById.get(int(item.get("layerId", 0)), {}).get("order", 0),
                item.get("order", 0),
                item.get("strokeId", 0),
            ),
        )
        ordered_points = []
        if self.__pointsLoaded:
            ordered_points = sorted(
                point_records,
                key=lambda item: (
                    self.__layersById.get(int(item.get("layerId", 0)), {}).get(
                        "order", 0
                    ),
                    self.__strokesById.get(int(item.get("strokeId", 0)), {}).get(
                        "order", 0
                    ),
                    item.get("pointId", 0),
                ),
            )

        strokes_by_layer = collections.OrderedDict()
        for layer in ordered_layers:
            strokes_by_layer[int(layer.get("layerId", 0))] = []
        for stroke in ordered_strokes:
            strokes_by_layer.setdefault(int(stroke.get("layerId", 0)), []).append(
                stroke
            )

        points_by_stroke = collections.OrderedDict()
        for stroke in ordered_strokes:
            points_by_stroke[int(stroke.get("strokeId", 0))] = []
        for point in ordered_points:
            points_by_stroke.setdefault(int(point.get("strokeId", 0)), []).append(point)

        total_points = self.__authoredPointCount
        if self.__pointsLoaded:
            total_points = len(ordered_points)
        cache_mode = _CACHE_MODE_LABELS.get(
            int(node_metadata.get("storageMode", self.__node["cacheMode"].getValue())),
            str(node_metadata.get("storageMode", "Unknown")),
        )
        resolved_path = self.__node["cacheResolvedPath"].getValue().strip()
        invalid_points = int(diagnostics.get("invalidPointCount", 0))
        invalid_strokes = int(diagnostics.get("invalidStrokeCount", 0))
        topology_mismatch = int(diagnostics.get("topologyMismatchCount", 0))
        unresolved_points = None
        if self.__pointsLoaded:
            unresolved_points = len(
                [
                    point
                    for point in ordered_points
                    if not bool(point.get("attachmentResolved", False))
                ]
            )
        self.__currentSelectionPointCount = len(current_selection.get("pointIds", []))
        self.__currentSelectionStrokeCount = len(current_selection.get("strokeIds", []))
        self.__currentSelectionPointIds = [
            int(point_id) for point_id in current_selection.get("pointIds", [])
        ]
        self.__currentSelectionStrokeIds = [
            int(stroke_id) for stroke_id in current_selection.get("strokeIds", [])
        ]

        try:
            unresolved_label = (
                str(unresolved_points)
                if unresolved_points is not None
                else "deferred until Points tab is opened"
            )
            status_suffix = (
                " | point rows unavailable ({})".format(point_records_error)
                if point_records_error
                else ""
            )
            self.__cacheLabel.setText(
                "Cache : {}{}".format(
                    cache_mode,
                    " | {}".format(resolved_path) if resolved_path else "",
                )
            )
            self.__countsLabel.setText(
                "Authoring : {} layers | {} strokes | {} points | {} selection sets".format(
                    len(ordered_layers),
                    len(ordered_strokes),
                    total_points,
                    len(selection_sets),
                )
            )
            self.__selectionLabel.setText(
                "Selection : {} points | {} strokes".format(
                    self.__currentSelectionPointCount,
                    self.__currentSelectionStrokeCount,
                )
            )
            self.__statusLabel.setText(
                "Status : {} | invalid points={} | unresolved points={} | invalid strokes={} | topology mismatches={}{}".format(
                    diagnostics.get("validationSummary", "Unknown scatter paint state"),
                    invalid_points,
                    unresolved_label,
                    invalid_strokes,
                    topology_mismatch,
                    status_suffix,
                )
            )
        except RuntimeError as exc:
            if _panelIsDeletedRuntimeError(exc):
                self.__disposed = True
                return
            raise

        overview = collections.OrderedDict(
            {
                "Cache": collections.OrderedDict(
                    {
                        "schemaVersion": int(snapshot.get("schemaVersion", 0)),
                        "storageMode": cache_mode,
                        "resolvedPath": resolved_path or "",
                        "projectRoot": node_metadata.get("projectRoot", ""),
                        "cachePath": node_metadata.get("cachePath", ""),
                        "backupEnabled": bool(
                            node_metadata.get("backupEnabled", False)
                        ),
                    }
                ),
                "Counts": collections.OrderedDict(
                    {
                        "layers": len(ordered_layers),
                        "strokes": len(ordered_strokes),
                        "points": total_points,
                        "selectionSets": len(selection_sets),
                        "chunks": len(snapshot.get("chunks", [])),
                    }
                ),
                "Current Selection": collections.OrderedDict(
                    {
                        "pointCount": self.__currentSelectionPointCount,
                        "pointIds": self.__previewIds(
                            current_selection.get("pointIds", [])
                        ),
                        "strokeCount": self.__currentSelectionStrokeCount,
                        "strokeIds": self.__previewIds(
                            current_selection.get("strokeIds", [])
                        ),
                    }
                ),
                "Diagnostics": collections.OrderedDict(
                    {
                        "validationSummary": diagnostics.get("validationSummary", ""),
                        "invalidPointCount": invalid_points,
                        "unresolvedPointCount": unresolved_points
                        if unresolved_points is not None
                        else "Deferred",
                        "invalidStrokeCount": invalid_strokes,
                        "topologyMismatchCount": topology_mismatch,
                        "failingFrame": int(diagnostics.get("failingFrame", 0)),
                        "failingTargetPaths": self.__previewStrings(
                            diagnostics.get("failingTargetPaths", [])
                        ),
                        "lastErrorMessage": diagnostics.get("lastErrorMessage", ""),
                    }
                ),
            }
        )

        layers_data = collections.OrderedDict()
        self.__layerIdByPath = {}
        self.__layerPathById = {}
        for layer in ordered_layers:
            layer_id = int(layer.get("layerId", 0))
            layer_strokes = strokes_by_layer.get(layer_id, [])
            layer_points = sum(
                int(stroke.get("pointCount", 0)) for stroke in layer_strokes
            )
            layer_targets = sum(
                int(stroke.get("targetCount", 0)) for stroke in layer_strokes
            )
            label = self.__layerLabel(layer)
            layers_data[label] = collections.OrderedDict(
                {
                    "name": layer.get("name", ""),
                    "layerId": layer_id,
                    "order": int(layer.get("order", 0)),
                    "visible": bool(layer.get("visible", True)),
                    "mute": bool(layer.get("mute", False)),
                    "solo": bool(layer.get("solo", False)),
                    "mode": _FRAME_MODE_LABELS.get(
                        int(layer.get("mode", 0)),
                        int(layer.get("mode", 0)),
                    ),
                    "timeVarying": bool(layer.get("timeVarying", False)),
                    "frameStart": int(layer.get("frameStart", 0)),
                    "frameEnd": int(layer.get("frameEnd", 0)),
                    "strokeCount": len(layer_strokes),
                    "pointCount": layer_points,
                    "targetCount": layer_targets,
                    "firstStrokeId": int(layer.get("firstStrokeId", 0)),
                    "lastStrokeId": int(layer.get("lastStrokeId", 0)),
                }
            )
            path = "/{}".format(label)
            self.__layerIdByPath[path] = layer_id
            self.__layerPathById[layer_id] = path

        strokes_data = collections.OrderedDict()
        self.__strokeIdByPath = {}
        self.__strokePathById = {}
        for stroke in ordered_strokes:
            layer = self.__layersById.get(int(stroke.get("layerId", 0)), {})
            label = self.__strokeLabel(stroke, layer)
            strokes_data[label] = collections.OrderedDict(
                {
                    "name": stroke.get("name", ""),
                    "strokeId": int(stroke.get("strokeId", 0)),
                    "layer": layer.get("name", "<missing layer>"),
                    "layerId": int(stroke.get("layerId", 0)),
                    "order": int(stroke.get("order", 0)),
                    "pointCount": int(stroke.get("pointCount", 0)),
                    "targetCount": int(stroke.get("targetCount", 0)),
                    "mode": _FRAME_MODE_LABELS.get(
                        int(stroke.get("mode", 0)),
                        int(stroke.get("mode", 0)),
                    ),
                    "frameStart": int(stroke.get("frameStart", 0)),
                    "frameEnd": int(stroke.get("frameEnd", 0)),
                    "firstChunkId": int(stroke.get("firstChunkId", 0)),
                    "lastChunkId": int(stroke.get("lastChunkId", 0)),
                }
            )
            path = "/{}".format(label)
            stroke_id = int(stroke.get("strokeId", 0))
            self.__strokeIdByPath[path] = stroke_id
            self.__strokePathById[stroke_id] = path

        points_data = collections.OrderedDict()
        self.__pointIdByPath = {}
        self.__pointPathById = {}
        filtered_point_ids = []
        filtered_invalid_count = 0
        filtered_unresolved_count = 0
        filtered_fallback_count = 0
        if self.__pointsLoaded:
            for layer in ordered_layers:
                layer_id = int(layer.get("layerId", 0))
                layer_key = self.__layerLabel(layer)
                stroke_entries = collections.OrderedDict()
                for stroke in strokes_by_layer.get(layer_id, []):
                    stroke_id = int(stroke.get("strokeId", 0))
                    stroke_key = self.__strokeLabel(stroke, layer)
                    point_entries = collections.OrderedDict()
                    filtered_index = 1
                    for point in points_by_stroke.get(stroke_id, []):
                        if not self.__pointMatchesFilters(point, layer, stroke):
                            continue
                        point_id = int(point.get("pointId", 0))
                        filtered_point_ids.append(point_id)
                        if not bool(point.get("valid", True)):
                            filtered_invalid_count += 1
                        if not bool(point.get("attachmentResolved", False)):
                            filtered_unresolved_count += 1
                        if self.__pointUsesFallback(point):
                            filtered_fallback_count += 1
                        point_key = self.__pointLabel(filtered_index, point)
                        filtered_index += 1
                        point_entries[point_key] = collections.OrderedDict(
                            {
                                "pointId": point_id,
                                "status": self.__pointStatus(point),
                                "layer": layer.get("name", ""),
                                "stroke": stroke.get("name", ""),
                                "sourcePath": point.get("sourcePath", ""),
                                "instanceSourcePath": point.get(
                                    "instanceSourcePath", ""
                                ),
                                "valid": bool(point.get("valid", True)),
                                "attachmentResolved": bool(
                                    point.get("attachmentResolved", False)
                                ),
                                "anchorMode": _ANCHOR_MODE_LABELS.get(
                                    int(point.get("anchorModeUsed", 0)),
                                    int(point.get("anchorModeUsed", 0)),
                                ),
                                "P": self.__formatVector(
                                    point.get("P", [0.0, 0.0, 0.0])
                                ),
                                "N": self.__formatVector(
                                    point.get("N", [0.0, 1.0, 0.0])
                                ),
                                "up": self.__formatVector(
                                    point.get("up", [0.0, 0.0, 1.0])
                                ),
                                "width": self.__formatNumber(point.get("width", 0.0)),
                                "scale": self.__formatNumber(point.get("scale", 1.0)),
                                "seed": int(point.get("seed", 0)),
                                "triangleIndex": int(point.get("triangleIndex", -1)),
                                "barycentric": self.__formatList(
                                    point.get("barycentric", [])
                                ),
                                "lastValidFrame": int(point.get("lastValidFrame", 0)),
                                "topologyGeneration": int(
                                    point.get("topologyGeneration", 0)
                                ),
                            }
                        )
                        point_path = "/{}/{}/{}".format(
                            layer_key, stroke_key, point_key
                        )
                        self.__pointIdByPath[point_path] = point_id
                        self.__pointPathById[point_id] = point_path
                    if point_entries:
                        stroke_entries[stroke_key] = point_entries
                if stroke_entries:
                    points_data[layer_key] = stroke_entries
        elif point_records_error:
            points_data["Point Records"] = collections.OrderedDict(
                {
                    "status": "Unavailable",
                    "error": point_records_error,
                }
            )
        elif total_points:
            points_data["Point Records"] = collections.OrderedDict(
                {
                    "status": "Deferred",
                    "pointCount": total_points,
                    "hint": "Open the Points tab to load point rows.",
                }
            )

        self.__filteredPointIds = filtered_point_ids
        self.__filteredPointSummary = {
            "invalid": filtered_invalid_count,
            "unresolved": filtered_unresolved_count,
            "fallback": filtered_fallback_count,
        }

        selections_data = collections.OrderedDict()
        selections_data["Current Selection"] = collections.OrderedDict(
            {
                "name": "Current Selection",
                "selectionSetId": 0,
                "pointCount": len(current_selection.get("pointIds", [])),
                "pointIds": self.__previewIds(
                    current_selection.get("pointIds", []), limit=24
                ),
                "strokeCount": len(current_selection.get("strokeIds", [])),
                "strokeIds": self.__previewIds(
                    current_selection.get("strokeIds", []), limit=24
                ),
            }
        )
        self.__selectionSetIdByPath = {}
        self.__selectionSetPathById = {}
        for selection_set in sorted(
            selection_sets,
            key=lambda item: (item.get("name", ""), item.get("selectionSetId", 0)),
        ):
            label = self.__selectionLabelForSet(selection_set)
            selection_id = int(selection_set.get("selectionSetId", 0))
            selections_data[label] = collections.OrderedDict(
                {
                    "name": selection_set.get("name", ""),
                    "selectionSetId": selection_id,
                    "pointCount": len(selection_set.get("pointIds", [])),
                    "pointIds": self.__previewIds(
                        selection_set.get("pointIds", []), limit=24
                    ),
                    "strokeCount": len(selection_set.get("strokeIds", [])),
                    "strokeIds": self.__previewIds(
                        selection_set.get("strokeIds", []), limit=24
                    ),
                }
            )
            path = "/{}".format(label)
            self.__selectionSetIdByPath[path] = selection_id
            self.__selectionSetPathById[selection_id] = path

        self.__layersData = layers_data
        self.__strokesData = strokes_data
        self.__pointsData = points_data
        self.__selectionsData = selections_data

        self.__setListingData(self.__overviewListing, overview, expand_all=True)
        self.__setListingData(self.__layersListing, layers_data, expand_all=True)
        self.__setListingData(self.__strokesListing, strokes_data, expand_all=True)
        self.__setListingData(self.__pointsListing, points_data, expand_all=False)
        self.__setListingData(
            self.__selectionsListing, selections_data, expand_all=True
        )

        self.__selectedLayerId = (
            previous_layer_id if previous_layer_id in self.__layersById else None
        )
        self.__selectedStrokeId = (
            previous_stroke_id if previous_stroke_id in self.__strokesById else None
        )
        current_selection_point_ids = [
            int(point_id) for point_id in current_selection.get("pointIds", [])
        ]
        if self.__pointsLoaded:
            restored_point_ids = [
                point_id
                for point_id in previous_point_ids
                if point_id in self.__pointsById
            ]
            if restored_point_ids:
                self.__selectedPointIds = restored_point_ids
            else:
                self.__selectedPointIds = [
                    point_id
                    for point_id in current_selection_point_ids
                    if point_id in self.__pointsById
                ]
        else:
            self.__selectedPointIds = (
                [int(point_id) for point_id in previous_point_ids]
                if previous_point_ids
                else current_selection_point_ids
            )
        self.__selectedSelectionSetId = (
            previous_selection_set_id
            if previous_selection_set_id in self.__selectionSetsById
            else None
        )

        self.__restoreSelections()
        self.__updateFocusLabel()
        self.__updateActionState()

    def __restoreSelections(self):
        self.__updatingSelections = True
        try:
            self.__layersListing.setSelection(
                self.__selectionMatcherForIds(
                    self.__layerPathById, [self.__selectedLayerId]
                )
            )
            self.__strokesListing.setSelection(
                self.__selectionMatcherForIds(
                    self.__strokePathById, [self.__selectedStrokeId]
                )
            )
            self.__pointsListing.setSelection(
                self.__selectionMatcherForIds(
                    self.__pointPathById, self.__selectedPointIds
                ),
                scrollToFirst=False,
            )
            self.__selectionsListing.setSelection(
                self.__selectionMatcherForIds(
                    self.__selectionSetPathById, [self.__selectedSelectionSetId]
                )
            )
        finally:
            self.__updatingSelections = False

    def __selectionMatcherForIds(self, path_by_id, identifiers):
        paths = []
        for identifier in identifiers:
            if identifier is None:
                continue
            path_string = path_by_id.get(identifier)
            if not path_string:
                continue
            paths.append(path_string)
        return IECore.PathMatcher(paths)

    def __selectedPathStrings(self, listing):
        return list(listing.getSelection().paths())

    def __activeTabName(self):
        tabbed_container = self.__overviewListing.parent()
        if tabbed_container is None:
            return None
        try:
            current = tabbed_container.getCurrent()
        except Exception:
            return None
        labels = {
            self.__overviewListing: "Overview",
            self.__layersListing: "Layers",
            self.__strokesListing: "Strokes",
            self.__pointsListing: "Points",
            self.__selectionsListing: "Selections",
        }
        return labels.get(current)

    def __actionLogContext(self, action_name=None, tab_name=None):
        context = collections.OrderedDict()
        context["action"] = action_name
        context["tab"] = tab_name or self.__activeTabName()
        context["node"] = self.__node.getName()
        context["layerId"] = self.__selectedLayerId
        context["strokeId"] = self.__selectedStrokeId
        context["selectedPointCount"] = len(self.__selectedPointIds)
        context["currentSelectionPointCount"] = self.__currentSelectionPointCount
        context["currentSelectionStrokeCount"] = self.__currentSelectionStrokeCount
        context["selectionSetId"] = self.__selectedSelectionSetId
        if self.__pointFilterMode != "all":
            context["filterMode"] = self.__pointFilterMode
        if self.__pointFilterQuery:
            context["filterQuery"] = self.__pointFilterQuery
        if self.__pointFilterLayerId is not None:
            context["filterLayerId"] = self.__pointFilterLayerId
        if self.__pointFilterStrokeId is not None:
            context["filterStrokeId"] = self.__pointFilterStrokeId
        if self.__filteredPointIds:
            context["filteredPointCount"] = len(self.__filteredPointIds)
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
            result_text = None
            if self.__lastActionMessage:
                result_text = self.__lastActionMessage
            elif result not in (None, ""):
                result_text = result
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

    def __setListingData(self, listing, data, expand_all=False):
        path = Gaffer.DictPath(data, "/")
        listing.setPath(path)
        if expand_all:
            self.__expandChildren(listing, data, data, "/")

    def __expandChildren(self, listing, root_data, data, root):
        for key, value in data.items():
            path = root.rstrip("/") + "/" + key if root != "/" else "/" + key
            if isinstance(value, dict) and value:
                listing.setPathExpanded(Gaffer.DictPath(root_data, path), True)
                self.__expandChildren(listing, root_data, value, path)

    def __syncCurrentSelectionFromPanel(self, point_ids=None, stroke_ids=None):
        point_ids = [
            int(point_id) for point_id in list(point_ids or []) if point_id is not None
        ]
        stroke_ids = [
            int(stroke_id)
            for stroke_id in list(stroke_ids or [])
            if stroke_id is not None
        ]
        if (
            point_ids == self.__currentSelectionPointIds
            and stroke_ids == self.__currentSelectionStrokeIds
        ):
            return
        try:
            self.__performMutation(
                lambda: self.__node.setCurrentSelection(point_ids, stroke_ids)
            )
        except Exception:
            return

    def __updateFocusLabel(self):
        layer_record = self.__selectedLayerRecord()
        stroke_record = self.__selectedStrokeRecord()
        focus_parts = []
        if layer_record is not None:
            focus_parts.append(
                "Layer={} [L{}]".format(
                    layer_record.get("name", "Layer"),
                    int(layer_record.get("layerId", 0)),
                )
            )
        if stroke_record is not None:
            focus_parts.append(
                "Stroke={} [S{}]".format(
                    stroke_record.get("name", "Stroke"),
                    int(stroke_record.get("strokeId", 0)),
                )
            )
        if self.__selectedPointIds:
            focus_parts.append("Point rows={}".format(len(self.__selectedPointIds)))
        if self.__selectedSelectionSetId is not None:
            selection_set = self.__selectionSetsById.get(
                self.__selectedSelectionSetId, {}
            )
            focus_parts.append(
                "SelectionSet={} [Sel{}]".format(
                    selection_set.get("name", "Selection"),
                    self.__selectedSelectionSetId,
                )
            )
        if (
            self.__pointFilterMode != "all"
            or self.__pointFilterLayerId is not None
            or self.__pointFilterStrokeId is not None
            or self.__pointFilterQuery
        ):
            focus_parts.append(
                "Filter={} [{} shown]".format(
                    self.__pointFilterModeLabel(),
                    len(self.__filteredPointIds),
                )
            )
        if self.__lastActionMessage:
            focus_parts.append(self.__lastActionMessage)
        self.__focusLabel.setText(
            "Focus : {}".format(" | ".join(focus_parts) if focus_parts else "None")
        )

    def __updateActionState(self):
        layer_record = self.__selectedLayerRecord()
        stroke_record = self.__selectedStrokeRecord()
        selection_set_record = self.__selectedSelectionSetRecord()
        can_create_stroke = self.__targetLayerIdForStrokeCreate() is not None

        self.__layerRenameButton.setEnabled(layer_record is not None)
        self.__layerDeleteButton.setEnabled(layer_record is not None)
        self.__layerMergeButton.setEnabled(
            layer_record is not None and len(self.__mergeableLayerRecords()) > 0
        )
        self.__layerUpButton.setEnabled(
            layer_record is not None and int(layer_record.get("order", 0)) > 0
        )
        self.__layerDownButton.setEnabled(
            layer_record is not None
            and int(layer_record.get("order", 0)) < max(0, len(self.__layersById) - 1)
        )
        self.__layerVisibleButton.setEnabled(layer_record is not None)
        self.__layerMuteButton.setEnabled(layer_record is not None)
        self.__layerSoloButton.setEnabled(layer_record is not None)
        self.__layerModeButton.setEnabled(layer_record is not None)
        self.__layerFrameStartWidget.setEnabled(layer_record is not None)
        self.__layerFrameEndWidget.setEnabled(layer_record is not None)
        self.__layerFrameApplyButton.setEnabled(layer_record is not None)
        self.__strokeAddButton.setEnabled(can_create_stroke)
        self.__strokeRenameButton.setEnabled(stroke_record is not None)
        self.__strokeDeleteButton.setEnabled(stroke_record is not None)
        self.__strokeMergeButton.setEnabled(
            stroke_record is not None and len(self.__mergeableStrokeRecords()) > 0
        )
        self.__strokeUpButton.setEnabled(
            stroke_record is not None and int(stroke_record.get("order", 0)) > 0
        )

        same_layer_strokes = []
        if stroke_record is not None:
            layer_id = int(stroke_record.get("layerId", 0))
            same_layer_strokes = [
                item
                for item in self.__strokesById.values()
                if int(item.get("layerId", 0)) == layer_id
            ]

        self.__strokeDownButton.setEnabled(
            stroke_record is not None
            and int(stroke_record.get("order", 0)) < max(0, len(same_layer_strokes) - 1)
        )
        self.__strokeModeButton.setEnabled(stroke_record is not None)
        self.__strokeFrameStartWidget.setEnabled(stroke_record is not None)
        self.__strokeFrameEndWidget.setEnabled(stroke_record is not None)
        self.__strokeFrameApplyButton.setEnabled(stroke_record is not None)
        has_current_selection = bool(
            self.__currentSelectionPointCount > 0
            or self.__currentSelectionStrokeCount > 0
        )
        self.__splitSelectionButton.setEnabled(has_current_selection)
        self.__selectionStoreButton.setEnabled(has_current_selection)
        self.__selectionRecallButton.setEnabled(
            self.__selectedSelectionSetId is not None
        )
        self.__selectionRenameButton.setEnabled(
            self.__selectedSelectionSetId is not None
        )
        self.__selectionDeleteButton.setEnabled(
            self.__selectedSelectionSetId is not None
        )
        self.__selectionClearButton.setEnabled(has_current_selection)
        self.__relaxSelectionButton.setEnabled(bool(self.__selectedPointIds))
        self.__reprojectSelectionButton.setEnabled(bool(self.__selectedPointIds))
        self.__removeSelectionButton.setEnabled(bool(self.__selectedPointIds))
        filter_mode_label = self.__pointFilterModeLabel()
        self.__pointFilterModeButton.setText(filter_mode_label)
        self.__pointFilterLayerButton.setText(self.__pointFilterLayerLabel())
        self.__pointFilterStrokeButton.setText(self.__pointFilterStrokeLabel())
        if self.__pointFilterTextWidget.getText() != self.__pointFilterQuery:
            self.__pointFilterTextWidget.setText(self.__pointFilterQuery)
        self.__pointFilterClearButton.setEnabled(
            bool(self.__pointFilterQuery)
            or self.__pointFilterMode != "all"
            or self.__pointFilterLayerId is not None
            or self.__pointFilterStrokeId is not None
        )
        self.__pointFilterSelectButton.setEnabled(
            self.__pointsLoaded and bool(self.__filteredPointIds)
        )
        self.__pointSelectProblematicButton.setEnabled(bool(self.__pointsById))
        self.__pointSelectInvalidButton.setEnabled(bool(self.__pointsById))
        self.__pointSelectUnresolvedButton.setEnabled(bool(self.__pointsById))
        self.__pointSelectFallbackButton.setEnabled(bool(self.__pointsById))
        if self.__pointsLoaded:
            self.__pointFilterSummaryLabel.setText(
                "Point View : {} of {} shown ({}) | invalid={} unresolved={} approx={}".format(
                    len(self.__filteredPointIds),
                    len(self.__pointsById),
                    filter_mode_label,
                    int(self.__filteredPointSummary.get("invalid", 0)),
                    int(self.__filteredPointSummary.get("unresolved", 0)),
                    int(self.__filteredPointSummary.get("fallback", 0)),
                )
            )
        else:
            self.__pointFilterSummaryLabel.setText(
                "Point View : {} authored points deferred. Open the Points tab to load rows.".format(
                    int(self.__authoredPointCount)
                )
            )

        if selection_set_record is not None:
            self.__selectionStoreButton.setText("Update Set")
            self.__selectionRecallButton.setText("Recall Set")
            self.__selectionRenameButton.setText("Rename Set")
            self.__selectionDeleteButton.setText("Delete Set")
        else:
            self.__selectionStoreButton.setText("Store")
            self.__selectionRecallButton.setText("Recall")
            self.__selectionRenameButton.setText("Rename")
            self.__selectionDeleteButton.setText("Delete")

        if layer_record is not None:
            self.__layerVisibleButton.setText(
                "Visible: {}".format(
                    "On" if layer_record.get("visible", True) else "Off"
                )
            )
            self.__layerMuteButton.setText(
                "Mute: {}".format("On" if layer_record.get("mute", False) else "Off")
            )
            self.__layerSoloButton.setText(
                "Solo: {}".format("On" if layer_record.get("solo", False) else "Off")
            )
        else:
            self.__layerVisibleButton.setText("Visible")
            self.__layerMuteButton.setText("Mute")
            self.__layerSoloButton.setText("Solo")

        if layer_record is not None and self.__mergeableLayerRecords():
            self.__layerMergeButton.setText("Merge Into")
        else:
            self.__layerMergeButton.setText("Merge Into")

        if layer_record is not None:
            self.__layerModeButton.setText(
                _FRAME_MODE_LABELS.get(
                    int(layer_record.get("mode", 0)),
                    str(layer_record.get("mode", 0)),
                )
            )
            self.__layerFrameStartWidget.setValue(
                int(layer_record.get("frameStart", 0))
            )
            self.__layerFrameEndWidget.setValue(int(layer_record.get("frameEnd", 0)))
        else:
            self.__layerModeButton.setText("Mode")
            self.__layerFrameStartWidget.setValue(0)
            self.__layerFrameEndWidget.setValue(0)

        if stroke_record is not None:
            self.__strokeModeButton.setText(
                _FRAME_MODE_LABELS.get(
                    int(stroke_record.get("mode", 0)),
                    str(stroke_record.get("mode", 0)),
                )
            )
            self.__strokeFrameStartWidget.setValue(
                int(stroke_record.get("frameStart", 0))
            )
            self.__strokeFrameEndWidget.setValue(int(stroke_record.get("frameEnd", 0)))
        else:
            self.__strokeModeButton.setText("Mode")
            self.__strokeFrameStartWidget.setValue(0)
            self.__strokeFrameEndWidget.setValue(0)

        if stroke_record is not None and self.__mergeableStrokeRecords():
            self.__strokeMergeButton.setText("Merge Into")
        else:
            self.__strokeMergeButton.setText("Merge Into")

    def __selectedLayerRecord(self):
        if self.__selectedLayerId is None:
            return None
        return self.__layersById.get(self.__selectedLayerId)

    def __selectedStrokeRecord(self):
        if self.__selectedStrokeId is None:
            return None
        return self.__strokesById.get(self.__selectedStrokeId)

    def __selectedSelectionSetRecord(self):
        if self.__selectedSelectionSetId is None:
            return None
        return self.__selectionSetsById.get(int(self.__selectedSelectionSetId))

    def __selectionStoreTarget(self):
        selection_set = self.__selectedSelectionSetRecord()
        if selection_set is None:
            return None, None
        return int(selection_set.get("selectionSetId", 0)), selection_set

    def __pointFilterMenuDefinition(self):
        menu_definition = IECore.MenuDefinition()
        for mode, label in _POINT_FILTER_LABELS.items():
            menu_definition.append(
                "/{}".format(label),
                {
                    "command": functools.partial(
                        Gaffer.WeakMethod(self.__pointFilterModeSelected),
                        mode,
                    ),
                    "checkBox": self.__pointFilterMode == mode,
                },
            )
        return menu_definition

    def __pointFilterLayerMenuDefinition(self):
        menu_definition = IECore.MenuDefinition()
        menu_definition.append(
            "/All Layers",
            {
                "command": Gaffer.WeakMethod(self.__pointFilterAllLayersSelected),
                "checkBox": self.__pointFilterLayerId is None,
            },
        )
        for layer in sorted(
            self.__layersById.values(),
            key=lambda item: (int(item.get("order", 0)), int(item.get("layerId", 0))),
        ):
            layer_id = int(layer.get("layerId", 0))
            menu_definition.append(
                "/{}".format(self.__layerLabel(layer)),
                {
                    "command": functools.partial(
                        Gaffer.WeakMethod(self.__pointFilterLayerSelected),
                        layer_id,
                    ),
                    "checkBox": self.__pointFilterLayerId == layer_id,
                },
            )
        return menu_definition

    def __pointFilterStrokeMenuDefinition(self):
        menu_definition = IECore.MenuDefinition()
        menu_definition.append(
            "/All Strokes",
            {
                "command": Gaffer.WeakMethod(self.__pointFilterAllStrokesSelected),
                "checkBox": self.__pointFilterStrokeId is None,
            },
        )
        for stroke in self.__filterableStrokeRecords():
            stroke_id = int(stroke.get("strokeId", 0))
            layer = self.__layersById.get(int(stroke.get("layerId", 0)), {})
            menu_definition.append(
                "/{}".format(self.__strokeLabel(stroke, layer)),
                {
                    "command": functools.partial(
                        Gaffer.WeakMethod(self.__pointFilterStrokeSelected),
                        stroke_id,
                    ),
                    "checkBox": self.__pointFilterStrokeId == stroke_id,
                },
            )
        return menu_definition

    def __pointFilterModeSelected(self, mode, *unused):
        self.__pointFilterMode = str(mode)
        self.__update(force_points=True)

    def __pointFilterModeLabel(self):
        if self.__pointFilterMode == "fallback":
            return _POINT_FILTER_LABELS["fallback"]
        if self.__pointFilterMode == "problematic":
            return _POINT_FILTER_LABELS["problematic"]
        if self.__pointFilterMode == "invalid":
            return _POINT_FILTER_LABELS["invalid"]
        if self.__pointFilterMode == "unresolved":
            return _POINT_FILTER_LABELS["unresolved"]
        return _POINT_FILTER_LABELS["all"]

    def __pointFilterLayerLabel(self):
        if self.__pointFilterLayerId is None:
            return "All Layers"
        layer = self.__layersById.get(int(self.__pointFilterLayerId))
        if layer is None:
            return "All Layers"
        return self.__layerLabel(layer)

    def __pointFilterStrokeLabel(self):
        if self.__pointFilterStrokeId is None:
            return "All Strokes"
        stroke = self.__strokesById.get(int(self.__pointFilterStrokeId))
        if stroke is None:
            return "All Strokes"
        layer = self.__layersById.get(int(stroke.get("layerId", 0)), {})
        return self.__strokeLabel(stroke, layer)

    def __pointFilterAllLayersSelected(self, *unused):
        self.__pointFilterLayerId = None
        self.__pointFilterStrokeId = None
        self.__update(force_points=True)

    def __pointFilterLayerSelected(self, layer_id, *unused):
        self.__pointFilterLayerId = int(layer_id)
        if self.__pointFilterStrokeId is not None:
            stroke = self.__strokesById.get(int(self.__pointFilterStrokeId))
            if (
                stroke is None
                or int(stroke.get("layerId", 0)) != self.__pointFilterLayerId
            ):
                self.__pointFilterStrokeId = None
        self.__update(force_points=True)

    def __pointFilterAllStrokesSelected(self, *unused):
        self.__pointFilterStrokeId = None
        self.__update(force_points=True)

    def __pointFilterStrokeSelected(self, stroke_id, *unused):
        stroke = self.__strokesById.get(int(stroke_id))
        if stroke is None:
            self.__pointFilterStrokeId = None
            self.__update(force_points=True)
            return
        self.__pointFilterStrokeId = int(stroke_id)
        self.__pointFilterLayerId = int(stroke.get("layerId", 0))
        self.__update(force_points=True)

    def __pointFilterEdited(self, widget):
        del widget
        self.__pointFilterQuery = self.__pointFilterTextWidget.getText().strip()
        self.__update(force_points=True)

    def __pointFilterClearClicked(self, button):
        del button
        self.__pointFilterMode = "all"
        self.__pointFilterQuery = ""
        self.__pointFilterLayerId = None
        self.__pointFilterStrokeId = None
        self.__pointFilterTextWidget.setText("")
        self.__update(force_points=True)

    def __pointFilterSelectClicked(self, button):
        del button
        self.__selectPointIds(self.__filteredPointIds)

    def __pointSelectProblematicClicked(self, button):
        del button
        self.__selectPointIds(self.__diagnosticPointIds("problematic"))

    def __pointSelectInvalidClicked(self, button):
        del button
        self.__selectPointIds(self.__diagnosticPointIds("invalid"))

    def __pointSelectUnresolvedClicked(self, button):
        del button
        self.__selectPointIds(self.__diagnosticPointIds("unresolved"))

    def __pointSelectFallbackClicked(self, button):
        del button
        self.__selectPointIds(self.__diagnosticPointIds("fallback"))

    def __selectPointIds(self, point_ids):
        point_ids = [
            int(point_id) for point_id in list(point_ids or []) if point_id is not None
        ]
        if not point_ids:
            return
        stroke_ids_map = collections.OrderedDict()
        for point_id in point_ids:
            point = self.__pointsById.get(int(point_id))
            if point is None:
                continue
            stroke_ids_map[int(point.get("strokeId", 0))] = None
        stroke_ids = list(stroke_ids_map.keys())
        try:
            self.__performMutation(
                lambda: self.__node.setCurrentSelection(point_ids, stroke_ids)
            )
        except Exception as exc:
            self.__focusLabel.setText(
                "Focus : Failed to select filtered points ({})".format(exc)
            )
            return
        self.__selectedPointIds = list(point_ids)
        if stroke_ids:
            self.__selectedStrokeId = int(stroke_ids[0])
            stroke_record = self.__strokesById.get(self.__selectedStrokeId)
            if stroke_record is not None:
                self.__selectedLayerId = int(stroke_record.get("layerId", 0))
        self.__update()

    def __diagnosticPointIds(self, mode):
        point_ids = []
        for point_id, point in sorted(self.__pointsById.items()):
            if mode == "invalid" and bool(point.get("valid", True)):
                continue
            if mode == "unresolved" and bool(point.get("attachmentResolved", False)):
                continue
            if mode == "fallback" and not self.__pointUsesFallback(point):
                continue
            if mode == "problematic":
                if (
                    bool(point.get("valid", True))
                    and bool(point.get("attachmentResolved", False))
                    and not self.__pointUsesFallback(point)
                ):
                    continue
            point_ids.append(int(point_id))
        return point_ids

    def __filterableStrokeRecords(self):
        strokes = sorted(
            self.__strokesById.values(),
            key=lambda item: (
                int(item.get("layerId", 0)),
                int(item.get("order", 0)),
                int(item.get("strokeId", 0)),
            ),
        )
        if self.__pointFilterLayerId is None:
            return strokes
        return [
            stroke
            for stroke in strokes
            if int(stroke.get("layerId", 0)) == int(self.__pointFilterLayerId)
        ]

    def __targetLayerIdForStrokeCreate(self):
        if (
            self.__selectedLayerId is not None
            and self.__selectedLayerId in self.__layersById
        ):
            return self.__selectedLayerId
        stroke = self.__selectedStrokeRecord()
        if stroke is None:
            return None
        return int(stroke.get("layerId", 0))

    def __mergeableLayerRecords(self):
        selected_layer = self.__selectedLayerRecord()
        if selected_layer is None:
            return []
        selected_layer_id = int(selected_layer.get("layerId", 0))
        return [
            layer
            for layer in sorted(
                self.__layersById.values(),
                key=lambda item: (
                    int(item.get("order", 0)),
                    int(item.get("layerId", 0)),
                ),
            )
            if int(layer.get("layerId", 0)) != selected_layer_id
        ]

    def __mergeableStrokeRecords(self):
        selected_stroke = self.__selectedStrokeRecord()
        if selected_stroke is None:
            return []
        selected_stroke_id = int(selected_stroke.get("strokeId", 0))
        return [
            stroke
            for stroke in sorted(
                self.__strokesById.values(),
                key=lambda item: (
                    int(item.get("layerId", 0)),
                    int(item.get("order", 0)),
                    int(item.get("strokeId", 0)),
                ),
            )
            if int(stroke.get("strokeId", 0)) != selected_stroke_id
        ]

    def __pointMatchesFilters(self, point, layer, stroke):
        if self.__pointFilterLayerId is not None and int(
            layer.get("layerId", 0)
        ) != int(self.__pointFilterLayerId):
            return False
        if self.__pointFilterStrokeId is not None and int(
            stroke.get("strokeId", 0)
        ) != int(self.__pointFilterStrokeId):
            return False

        if self.__pointFilterMode == "fallback":
            if not self.__pointUsesFallback(point):
                return False
        if self.__pointFilterMode == "problematic":
            if (
                bool(point.get("valid", True))
                and bool(point.get("attachmentResolved", False))
                and not self.__pointUsesFallback(point)
            ):
                return False
        elif self.__pointFilterMode == "invalid":
            if bool(point.get("valid", True)):
                return False
        elif self.__pointFilterMode == "unresolved":
            if bool(point.get("attachmentResolved", False)):
                return False

        query = self.__pointFilterQuery.strip().lower()
        if not query:
            return True

        haystack = [
            str(point.get("pointId", "")),
            str(layer.get("name", "")),
            str(stroke.get("name", "")),
            str(point.get("sourcePath", "")),
            str(point.get("instanceSourcePath", "")),
            self.__pointStatus(point),
        ]
        haystack_text = " ".join(value.lower() for value in haystack if value)
        return query in haystack_text

    def __pointStatus(self, point):
        valid = bool(point.get("valid", True))
        resolved = bool(point.get("attachmentResolved", False))
        if not valid and not resolved:
            return "Invalid+Unresolved"
        if not valid:
            return "Invalid"
        if not resolved:
            return "Unresolved"
        if self.__pointUsesFallback(point):
            return "Approximated"
        return "OK"

    def __pointUsesFallback(self, point):
        anchor_mode = int(point.get("anchorModeUsed", 0))
        return anchor_mode in (1, 2)

    def __scriptNode(self):
        return self.__node.ancestor(Gaffer.ScriptNode)

    def __textInput(self, title, confirm_label, initial_text=""):
        dialogue = GafferUI.TextInputDialogue(
            initialText=initial_text,
            title=title,
            confirmLabel=confirm_label,
        )
        result = dialogue.waitForText()
        if result is None:
            return None
        return result.strip()

    def __confirm(self, title, message, confirm_label):
        dialogue = GafferUI.ConfirmationDialogue(
            title=title,
            message=message,
            confirmLabel=confirm_label,
        )
        return bool(dialogue.waitForConfirmation())

    def __performMutation(self, mutator):
        script_node = self.__scriptNode()
        if script_node is None:
            raise RuntimeError("PaintedPoints panel requires a ScriptNode")
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

    def __runNodeAction(self, action, success_prefix, use_undo=False, refresh=True):
        try:
            result = self.__performMutation(action) if use_undo else action()
        except Exception as exc:
            self.__lastActionFailed = True
            self.__setLastActionMessage("{} failed ({})".format(success_prefix, exc))
            self.__updateFocusLabel()
            return None
        self.__lastActionFailed = False
        detail = result
        if isinstance(detail, str):
            detail = detail.strip()
        if detail in (None, ""):
            self.__setLastActionMessage(success_prefix)
        else:
            self.__setLastActionMessage("{}: {}".format(success_prefix, detail))
        if refresh:
            self.__update()
        else:
            self.__updateFocusLabel()
            self.__updateActionState()
        return result

    def __repairSelectedPoints(self, mutator, success_prefix):
        if not self.__selectedPointIds:
            self.__setLastActionMessage(
                "{} skipped (no selected points)".format(success_prefix)
            )
            self.__updateFocusLabel()
            return None
        return self.__runNodeAction(
            mutator, success_prefix, use_undo=False, refresh=True
        )

    def __removeSelectedPoints(self):
        if not self.__selectedPointIds:
            return "Removed 0 selected points. Stroke selection was unchanged."
        return self.__performMutation(self.__node.splitStrokeBySelection)

    def __setLayerMode(self, layer_id, mode):
        def mutator(store):
            for layer in store.get("layers", []):
                if int(layer.get("layerId", 0)) != int(layer_id):
                    continue
                layer["mode"] = int(mode)
                return int(layer_id)
            raise RuntimeError("Layer {} no longer exists".format(layer_id))

        return self.__node.mutateCacheStore(mutator)

    def __setStrokeModeAndRange(
        self, stroke_id, mode=None, frame_start=None, frame_end=None
    ):
        def mutator(store):
            for stroke in store.get("strokes", []):
                if int(stroke.get("strokeId", 0)) != int(stroke_id):
                    continue
                if mode is not None:
                    stroke["mode"] = int(mode)
                if frame_start is not None:
                    stroke["frameStart"] = int(frame_start)
                if frame_end is not None:
                    stroke["frameEnd"] = int(frame_end)
                return int(stroke_id)
            raise RuntimeError("Stroke {} no longer exists".format(stroke_id))

        return self.__node.mutateCacheStore(mutator)

    def __layerModeMenuDefinition(self):
        menu_definition = IECore.MenuDefinition()
        layer = self.__selectedLayerRecord()
        current_mode = int(layer.get("mode", 0)) if layer is not None else 0
        for mode, label in _FRAME_MODE_LABELS.items():
            menu_definition.append(
                "/{}".format(label),
                {
                    "command": functools.partial(
                        Gaffer.WeakMethod(self.__layerModeSelected),
                        int(mode),
                    ),
                    "checkBox": current_mode == int(mode),
                    "active": layer is not None,
                },
            )
        return menu_definition

    def __strokeModeMenuDefinition(self):
        menu_definition = IECore.MenuDefinition()
        stroke = self.__selectedStrokeRecord()
        current_mode = int(stroke.get("mode", 0)) if stroke is not None else 0
        for mode, label in _FRAME_MODE_LABELS.items():
            menu_definition.append(
                "/{}".format(label),
                {
                    "command": functools.partial(
                        Gaffer.WeakMethod(self.__strokeModeSelected),
                        int(mode),
                    ),
                    "checkBox": current_mode == int(mode),
                    "active": stroke is not None,
                },
            )
        return menu_definition

    def __layerModeSelected(self, mode, *unused):
        layer = self.__selectedLayerRecord()
        if layer is None:
            return
        try:
            self.__performMutation(
                lambda: self.__setLayerMode(int(layer.get("layerId", 0)), int(mode))
            )
        except Exception as exc:
            self.__focusLabel.setText(
                "Focus : Failed to set layer mode ({})".format(exc)
            )
            return
        self.__update()

    def __strokeModeSelected(self, mode, *unused):
        stroke = self.__selectedStrokeRecord()
        if stroke is None:
            return
        try:
            self.__performMutation(
                lambda: self.__setStrokeModeAndRange(
                    int(stroke.get("strokeId", 0)),
                    mode=int(mode),
                )
            )
        except Exception as exc:
            self.__focusLabel.setText(
                "Focus : Failed to set stroke mode ({})".format(exc)
            )
            return
        self.__update()

    def __layerFrameApplyClicked(self, button):
        del button
        layer = self.__selectedLayerRecord()
        if layer is None:
            return
        frame_start = int(self.__layerFrameStartWidget.getValue())
        frame_end = int(self.__layerFrameEndWidget.getValue())
        try:
            self.__performMutation(
                lambda: self.__node.setLayerTimeRange(
                    int(layer.get("layerId", 0)),
                    frame_start,
                    frame_end,
                )
            )
        except Exception as exc:
            self.__focusLabel.setText(
                "Focus : Failed to set layer range ({})".format(exc)
            )
            return
        self.__update()

    def __strokeFrameApplyClicked(self, button):
        del button
        stroke = self.__selectedStrokeRecord()
        if stroke is None:
            return
        frame_start = int(self.__strokeFrameStartWidget.getValue())
        frame_end = int(self.__strokeFrameEndWidget.getValue())
        try:
            self.__performMutation(
                lambda: self.__setStrokeModeAndRange(
                    int(stroke.get("strokeId", 0)),
                    frame_start=frame_start,
                    frame_end=frame_end,
                )
            )
        except Exception as exc:
            self.__focusLabel.setText(
                "Focus : Failed to set stroke range ({})".format(exc)
            )
            return
        self.__update()

    def __layerMergeMenuDefinition(self):
        menu_definition = IECore.MenuDefinition()
        layer = self.__selectedLayerRecord()
        candidates = self.__mergeableLayerRecords()
        if layer is None or not candidates:
            menu_definition.append("/No Merge Targets", {"active": False})
            return menu_definition
        for candidate in candidates:
            destination_id = int(candidate.get("layerId", 0))
            menu_definition.append(
                "/{}".format(self.__layerLabel(candidate)),
                {
                    "command": functools.partial(
                        Gaffer.WeakMethod(self.__mergeLayerInto),
                        destination_id,
                    )
                },
            )
        return menu_definition

    def __strokeMergeMenuDefinition(self):
        menu_definition = IECore.MenuDefinition()
        stroke = self.__selectedStrokeRecord()
        candidates = self.__mergeableStrokeRecords()
        if stroke is None or not candidates:
            menu_definition.append("/No Merge Targets", {"active": False})
            return menu_definition
        for candidate in candidates:
            destination_id = int(candidate.get("strokeId", 0))
            destination_layer = self.__layersById.get(
                int(candidate.get("layerId", 0)), {}
            )
            menu_definition.append(
                "/{}".format(self.__strokeLabel(candidate, destination_layer)),
                {
                    "command": functools.partial(
                        Gaffer.WeakMethod(self.__mergeStrokeInto),
                        destination_id,
                    )
                },
            )
        return menu_definition

    def __mergeLayerInto(self, destination_layer_id, *unused):
        layer = self.__selectedLayerRecord()
        destination = self.__layersById.get(int(destination_layer_id))
        if layer is None or destination is None:
            return
        if not self.__confirm(
            "Merge Layer",
            "Merge layer '{}' into '{}' ?".format(
                layer.get("name", "Layer"),
                destination.get("name", "Layer"),
            ),
            "Merge",
        ):
            return
        try:
            merged_layer_id = self.__performMutation(
                lambda: self.__node.mergeLayers(
                    int(layer.get("layerId", 0)),
                    int(destination_layer_id),
                )
            )
        except Exception as exc:
            self.__focusLabel.setText("Focus : Failed to merge layer ({})".format(exc))
            return
        self.__selectedLayerId = int(merged_layer_id)
        self.__selectedStrokeId = None
        self.__selectedPointIds = []
        self.__update()

    def __mergeStrokeInto(self, destination_stroke_id, *unused):
        stroke = self.__selectedStrokeRecord()
        destination = self.__strokesById.get(int(destination_stroke_id))
        if stroke is None or destination is None:
            return
        destination_layer = self.__layersById.get(
            int(destination.get("layerId", 0)), {}
        )
        if not self.__confirm(
            "Merge Stroke",
            "Merge stroke '{}' into '{}' ?".format(
                stroke.get("name", "Stroke"),
                self.__strokeLabel(destination, destination_layer),
            ),
            "Merge",
        ):
            return
        try:
            merged_stroke_id = self.__performMutation(
                lambda: self.__node.mergeStrokes(
                    int(stroke.get("strokeId", 0)),
                    int(destination_stroke_id),
                )
            )
        except Exception as exc:
            self.__focusLabel.setText("Focus : Failed to merge stroke ({})".format(exc))
            return
        self.__selectedStrokeId = int(merged_stroke_id)
        destination_record = self.__strokesById.get(
            int(destination_stroke_id), destination
        )
        self.__selectedLayerId = int(destination_record.get("layerId", 0))
        self.__selectedPointIds = []
        self.__update()

    def __layerAddClicked(self, button):
        del button
        name = self.__textInput("Add Layer", "Add", "")
        if name is None:
            return
        try:
            layer_id = self.__performMutation(
                lambda: self.__node.createLayer(name or None)
            )
        except Exception as exc:
            self.__focusLabel.setText("Focus : Failed to add layer ({})".format(exc))
            return
        self.__selectedLayerId = int(layer_id)
        self.__selectedStrokeId = None
        self.__selectedPointIds = []
        self.__update()

    def __layerRenameClicked(self, button):
        del button
        layer = self.__selectedLayerRecord()
        if layer is None:
            return
        new_name = self.__textInput(
            "Rename Layer",
            "Rename",
            layer.get("name", ""),
        )
        if new_name is None or not new_name:
            return
        try:
            self.__performMutation(
                lambda: self.__node.renameLayer(int(layer.get("layerId", 0)), new_name)
            )
        except Exception as exc:
            self.__focusLabel.setText("Focus : Failed to rename layer ({})".format(exc))
            return
        self.__update()

    def __layerDeleteClicked(self, button):
        del button
        layer = self.__selectedLayerRecord()
        if layer is None:
            return
        if not self.__confirm(
            "Delete Layer",
            "Delete layer '{}' and all of its strokes and points?".format(
                layer.get("name", "Layer")
            ),
            "Delete",
        ):
            return
        layer_id = int(layer.get("layerId", 0))
        try:
            self.__performMutation(lambda: self.__node.deleteLayer(layer_id))
        except Exception as exc:
            self.__focusLabel.setText("Focus : Failed to delete layer ({})".format(exc))
            return
        self.__selectedLayerId = None
        self.__selectedStrokeId = None
        self.__selectedPointIds = []
        self.__update()

    def __layerUpClicked(self, button):
        del button
        layer = self.__selectedLayerRecord()
        if layer is None:
            return
        try:
            self.__performMutation(
                lambda: self.__node.moveLayer(
                    int(layer.get("layerId", 0)), int(layer.get("order", 0)) - 1
                )
            )
        except Exception as exc:
            self.__focusLabel.setText("Focus : Failed to move layer ({})".format(exc))
            return
        self.__update()

    def __layerDownClicked(self, button):
        del button
        layer = self.__selectedLayerRecord()
        if layer is None:
            return
        try:
            self.__performMutation(
                lambda: self.__node.moveLayer(
                    int(layer.get("layerId", 0)), int(layer.get("order", 0)) + 1
                )
            )
        except Exception as exc:
            self.__focusLabel.setText("Focus : Failed to move layer ({})".format(exc))
            return
        self.__update()

    def __layerVisibleClicked(self, button):
        del button
        layer = self.__selectedLayerRecord()
        if layer is None:
            return
        try:
            self.__performMutation(
                lambda: self.__node.setLayerVisible(
                    int(layer.get("layerId", 0)), not bool(layer.get("visible", True))
                )
            )
        except Exception as exc:
            self.__focusLabel.setText(
                "Focus : Failed to toggle visibility ({})".format(exc)
            )
            return
        self.__update()

    def __layerMuteClicked(self, button):
        del button
        layer = self.__selectedLayerRecord()
        if layer is None:
            return
        try:
            self.__performMutation(
                lambda: self.__node.setLayerMute(
                    int(layer.get("layerId", 0)), not bool(layer.get("mute", False))
                )
            )
        except Exception as exc:
            self.__focusLabel.setText("Focus : Failed to toggle mute ({})".format(exc))
            return
        self.__update()

    def __layerSoloClicked(self, button):
        del button
        layer = self.__selectedLayerRecord()
        if layer is None:
            return
        try:
            self.__performMutation(
                lambda: self.__node.setLayerSolo(
                    int(layer.get("layerId", 0)), not bool(layer.get("solo", False))
                )
            )
        except Exception as exc:
            self.__focusLabel.setText("Focus : Failed to toggle solo ({})".format(exc))
            return
        self.__update()

    def __strokeAddClicked(self, button):
        del button
        layer_id = self.__targetLayerIdForStrokeCreate()
        if layer_id is None:
            return
        name = self.__textInput("Add Stroke", "Add", "")
        if name is None:
            return
        try:
            stroke_id = self.__performMutation(
                lambda: self.__node.createStroke(int(layer_id), name or None)
            )
        except Exception as exc:
            self.__focusLabel.setText("Focus : Failed to add stroke ({})".format(exc))
            return
        self.__selectedLayerId = int(layer_id)
        self.__selectedStrokeId = int(stroke_id)
        self.__selectedPointIds = []
        self.__update()

    def __strokeRenameClicked(self, button):
        del button
        stroke = self.__selectedStrokeRecord()
        if stroke is None:
            return
        new_name = self.__textInput(
            "Rename Stroke",
            "Rename",
            stroke.get("name", ""),
        )
        if new_name is None or not new_name:
            return
        try:
            self.__performMutation(
                lambda: self.__node.renameStroke(
                    int(stroke.get("strokeId", 0)), new_name
                )
            )
        except Exception as exc:
            self.__focusLabel.setText(
                "Focus : Failed to rename stroke ({})".format(exc)
            )
            return
        self.__update()

    def __strokeDeleteClicked(self, button):
        del button
        stroke = self.__selectedStrokeRecord()
        if stroke is None:
            return
        if not self.__confirm(
            "Delete Stroke",
            "Delete stroke '{}' and all of its points?".format(
                stroke.get("name", "Stroke")
            ),
            "Delete",
        ):
            return
        try:
            self.__performMutation(
                lambda: self.__node.deleteStroke(int(stroke.get("strokeId", 0)))
            )
        except Exception as exc:
            self.__focusLabel.setText(
                "Focus : Failed to delete stroke ({})".format(exc)
            )
            return
        self.__selectedStrokeId = None
        self.__selectedPointIds = []
        self.__update()

    def __strokeUpClicked(self, button):
        del button
        stroke = self.__selectedStrokeRecord()
        if stroke is None:
            return
        try:
            self.__performMutation(
                lambda: self.__node.moveStroke(
                    int(stroke.get("strokeId", 0)), int(stroke.get("order", 0)) - 1
                )
            )
        except Exception as exc:
            self.__focusLabel.setText("Focus : Failed to move stroke ({})".format(exc))
            return
        self.__update()

    def __strokeDownClicked(self, button):
        del button
        stroke = self.__selectedStrokeRecord()
        if stroke is None:
            return
        try:
            self.__performMutation(
                lambda: self.__node.moveStroke(
                    int(stroke.get("strokeId", 0)), int(stroke.get("order", 0)) + 1
                )
            )
        except Exception as exc:
            self.__focusLabel.setText("Focus : Failed to move stroke ({})".format(exc))
            return
        self.__update()

    def __splitSelectionClicked(self, button):
        del button
        if not self.__confirm(
            "Split Stroke By Selection",
            "Split the currently selected points or strokes into replacement strokes?",
            "Split",
        ):
            return
        try:
            self.__performMutation(self.__node.splitStrokeBySelection)
        except Exception as exc:
            self.__focusLabel.setText(
                "Focus : Failed to split selection ({})".format(exc)
            )
            return
        self.__selectedStrokeId = None
        self.__selectedPointIds = []
        self.__update()

    def __relaxSelectionClicked(self, button):
        del button
        self.__repairSelectedPoints(self.__node.relaxSelection, "Relaxed selection")

    def __reprojectSelectionClicked(self, button):
        del button
        self.__repairSelectedPoints(
            self.__node.reprojectSelection,
            "Reprojected selection",
        )

    def __removeSelectionClicked(self, button):
        del button
        if not self.__confirm(
            "Remove Selected Points",
            "Remove the currently selected authored points from their strokes?",
            "Remove",
        ):
            return
        self.__repairSelectedPoints(
            self.__removeSelectedPoints,
            "Removed selection",
        )

    def __selectionStoreClicked(self, button):
        del button
        selection_set_id, selection_set = self.__selectionStoreTarget()
        if selection_set_id is None:
            name = self.__textInput("Store Selection Set", "Store", "")
            if name is None:
                return
        else:
            if selection_set is None:
                return
            name = selection_set.get("name", "")
        try:
            selection_result = self.__performMutation(
                lambda: self.__node.storeCurrentSelection(
                    selection_set_id,
                    name or None,
                )
            )
        except Exception as exc:
            self.__focusLabel.setText(
                "Focus : Failed to store selection ({})".format(exc)
            )
            return
        if isinstance(selection_result, dict):
            selection_set_id = selection_result.get("selectionSetId")
        else:
            selection_set_id = selection_result
        if selection_set_id is None:
            self.__focusLabel.setText("Focus : Failed to store selection (missing id)")
            return
        self.__selectedSelectionSetId = int(selection_set_id)
        self.__update()

    def __selectionRecallClicked(self, button):
        del button
        selection_set = self.__selectedSelectionSetRecord()
        if selection_set is None:
            return
        point_ids = [int(point_id) for point_id in selection_set.get("pointIds", [])]
        stroke_ids = [
            int(stroke_id)
            for stroke_id in selection_set.get("strokeIds", [])
            if int(stroke_id) in self.__strokesById
        ]
        try:
            self.__performMutation(
                lambda: self.__node.setCurrentSelection(point_ids, stroke_ids)
            )
        except Exception as exc:
            self.__focusLabel.setText(
                "Focus : Failed to recall selection ({})".format(exc)
            )
            return
        self.__selectedPointIds = point_ids
        self.__selectedStrokeId = int(stroke_ids[0]) if stroke_ids else None
        if self.__selectedStrokeId is None and self.__selectedPointIds:
            point_record = self.__pointsById.get(self.__selectedPointIds[0])
            if point_record is not None:
                self.__selectedStrokeId = int(point_record.get("strokeId", 0))
        if self.__selectedStrokeId is not None:
            stroke_record = self.__strokesById.get(self.__selectedStrokeId)
            if stroke_record is not None:
                self.__selectedLayerId = int(stroke_record.get("layerId", 0))
        elif self.__selectedPointIds:
            point_record = self.__pointsById.get(self.__selectedPointIds[0])
            if point_record is not None:
                self.__selectedLayerId = int(point_record.get("layerId", 0))
        self.__update()

    def __selectionRenameClicked(self, button):
        del button
        selection_set = self.__selectedSelectionSetRecord()
        if selection_set is None:
            return
        new_name = self.__textInput(
            "Rename Selection Set",
            "Rename",
            selection_set.get("name", ""),
        )
        if new_name is None or not new_name:
            return
        try:
            self.__performMutation(
                lambda: self.__node.renameSelectionSet(
                    int(selection_set.get("selectionSetId", 0)), new_name
                )
            )
        except Exception as exc:
            self.__focusLabel.setText(
                "Focus : Failed to rename selection ({})".format(exc)
            )
            return
        self.__update()

    def __selectionDeleteClicked(self, button):
        del button
        selection_set = self.__selectedSelectionSetRecord()
        if selection_set is None:
            return
        if not self.__confirm(
            "Delete Selection Set",
            "Delete saved selection '{}' ?".format(
                selection_set.get("name", "Selection")
            ),
            "Delete",
        ):
            return
        try:
            self.__performMutation(
                lambda: self.__node.deleteSelectionSet(
                    int(selection_set.get("selectionSetId", 0))
                )
            )
        except Exception as exc:
            self.__focusLabel.setText(
                "Focus : Failed to delete selection ({})".format(exc)
            )
            return
        self.__selectedSelectionSetId = None
        self.__update()

    def __selectionClearClicked(self, button):
        del button
        try:
            self.__performMutation(lambda: self.__node.setCurrentSelection([], []))
        except Exception as exc:
            self.__focusLabel.setText(
                "Focus : Failed to clear selection ({})".format(exc)
            )
            return
        self.__selectedStrokeId = None
        self.__selectedPointIds = []
        self.__update()

    def __layerLabel(self, layer):
        return "{order:02d} {name} [L{layer_id}]".format(
            order=int(layer.get("order", 0)),
            name=self.__safeName(layer.get("name", "Layer")),
            layer_id=int(layer.get("layerId", 0)),
        )

    def __strokeLabel(self, stroke, layer):
        return "{layer}.{order:02d} {name} [S{stroke_id}]".format(
            layer=self.__safeName(layer.get("name", "Layer")),
            order=int(stroke.get("order", 0)),
            name=self.__safeName(stroke.get("name", "Stroke")),
            stroke_id=int(stroke.get("strokeId", 0)),
        )

    def __pointLabel(self, index, point):
        return "{index:04d} {status} [P{point_id}]".format(
            index=int(index),
            status=self.__pointStatus(point),
            point_id=int(point.get("pointId", 0)),
        )

    def __selectionLabelForSet(self, selection_set):
        return "{name} [Sel{selection_id}]".format(
            name=self.__safeName(selection_set.get("name", "Selection")),
            selection_id=int(selection_set.get("selectionSetId", 0)),
        )

    def __safeName(self, value):
        return str(value or "").replace("/", "_")

    def __formatNumber(self, value):
        try:
            return ("{:.4f}".format(float(value))).rstrip("0").rstrip(".")
        except Exception:
            return str(value)

    def __formatVector(self, value):
        values = list(value or [])
        if len(values) < 3:
            values = values + [0.0] * (3 - len(values))
        return "({}, {}, {})".format(
            self.__formatNumber(values[0]),
            self.__formatNumber(values[1]),
            self.__formatNumber(values[2]),
        )

    def __formatList(self, values):
        return [self.__formatNumber(value) for value in list(values or [])]

    def __previewIds(self, values, limit=12):
        values = [int(value) for value in list(values or [])]
        if len(values) <= limit:
            return values
        return values[:limit] + ["..."]

    def __previewStrings(self, values, limit=8):
        values = [str(value) for value in list(values or []) if str(value)]
        if len(values) <= limit:
            return values
        return values[:limit] + ["..."]

    def __pointsTabActive(self):
        return self.__activeTabName() == "Points"
