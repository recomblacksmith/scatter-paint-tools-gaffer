from . import _core_shared as _shared

globals().update(
    {name: value for name, value in vars(_shared).items() if not name.startswith("__")}
)

# ruff: noqa
from ._core_attached_points import AttachedPoints
from ._core_static_points import StaticPoints

_shared.AttachedPoints = AttachedPoints

from . import _core_painted_points_store_helpers as _store_helpers
from . import _core_painted_points_model_helpers as _model_helpers
from . import _core_painted_points_surface_helpers as _surface_helpers
from . import _core_painted_points_export_helpers as _export_helpers


class PaintedPoints(GafferScene.SceneProcessor):
    def __init__(self, name="PaintedPoints"):
        GafferScene.SceneProcessor.__init__(self, name)

        self.__syncing = False

        self["surfaceMode"] = Gaffer.IntPlug(defaultValue=SURFACE_MODE_VIEWPORT_MESH)
        self["paintThroughMode"] = Gaffer.IntPlug(
            defaultValue=PAINT_THROUGH_MODE_FRONT_MOST
        )
        self["relaxObjective"] = Gaffer.IntPlug(
            defaultValue=RELAX_OBJECTIVE_PRESERVE_SILHOUETTE,
            minValue=RELAX_OBJECTIVE_PRESERVE_SILHOUETTE,
            maxValue=RELAX_OBJECTIVE_EVEN_REDISTRIBUTION,
        )
        self["targetFilter"] = Gaffer.StringPlug(defaultValue="")
        self["targetSetFilter"] = Gaffer.StringPlug(defaultValue="")
        self["cachePath"] = Gaffer.StringPlug(defaultValue="")
        self["cacheMode"] = Gaffer.IntPlug(defaultValue=CACHE_MODE_EMBEDDED)
        self["cachePathMode"] = Gaffer.IntPlug(
            defaultValue=CACHE_PATH_MODE_RELATIVE_TO_SCRIPT
        )
        self["projectRoot"] = Gaffer.StringPlug(defaultValue="")
        self["lockMode"] = Gaffer.IntPlug(defaultValue=LOCK_MODE_SESSION_AWARE)
        self["backupEnabled"] = Gaffer.BoolPlug(defaultValue=False)
        self["backupPolicy"] = Gaffer.IntPlug(defaultValue=BACKUP_POLICY_OFF)
        self["compactionMode"] = Gaffer.IntPlug(
            defaultValue=COMPACTION_MODE_IMMEDIATE_BLOCKING
        )
        self["globalModePrecedence"] = Gaffer.IntPlug(
            defaultValue=MODE_PRECEDENCE_STROKE_WINS
        )
        self["defaultColor"] = Gaffer.Color3fPlug(defaultValue=DEFAULT_AUTHORED_COLOR)

        self["pressureDefaults"] = Gaffer.Plug()
        self["pressureDefaults"]["enabled"] = Gaffer.BoolPlug(defaultValue=True)
        self["pressureDefaults"]["mappingMode"] = Gaffer.IntPlug(
            defaultValue=PRESSURE_MAPPING_DIRECT
        )
        self["pressureDefaults"]["densityCurve"] = Gaffer.ObjectPlug(
            defaultValue=IECore.CompoundObject()
        )
        self["pressureDefaults"]["softnessCurve"] = Gaffer.ObjectPlug(
            defaultValue=IECore.CompoundObject()
        )

        self["brushDefaults"] = Gaffer.Plug()
        self["brushDefaults"]["size"] = Gaffer.FloatPlug(defaultValue=0.1, minValue=0.0)
        self["brushDefaults"]["points"] = Gaffer.IntPlug(
            defaultValue=1,
            minValue=1,
        )
        self["brushDefaults"]["density"] = Gaffer.FloatPlug(
            defaultValue=1.0,
            minValue=0.0,
        )
        self["brushDefaults"]["softness"] = Gaffer.FloatPlug(
            defaultValue=0.5,
            minValue=0.0,
            maxValue=1.0,
        )
        self["brushDefaults"]["spacing"] = Gaffer.FloatPlug(
            defaultValue=0.1,
            minValue=0.0,
        )
        self["brushDefaults"]["rotationMode"] = Gaffer.IntPlug(defaultValue=0)
        self["brushDefaults"]["scaleJitter"] = Gaffer.FloatPlug(
            defaultValue=0.0,
            minValue=0.0,
        )
        self["brushDefaults"]["widthJitter"] = Gaffer.FloatPlug(
            defaultValue=0.0,
            minValue=0.0,
        )

        self["layers"] = Gaffer.StringVectorDataPlug(
            defaultValue=IECore.StringVectorData()
        )
        self["selectionSets"] = Gaffer.StringVectorDataPlug(
            defaultValue=IECore.StringVectorData()
        )
        self["cacheBlob"] = Gaffer.ObjectPlug(defaultValue=IECore.UCharVectorData())

        self["cacheVersion"] = _out_int(SCHEMA_VERSION)
        self["invalidPointCount"] = _out_int(0)
        self["invalidStrokeCount"] = _out_int(0)
        self["failingFrame"] = _out_int(0)
        self["failingTargetPaths"] = _out_string_vector()
        self["lastErrorMessage"] = _out_string("")
        self["topologyMismatchCount"] = _out_int(0)
        self["validationSummary"] = _out_string(SCHEMA_SUMMARY)
        self["validationCategories"] = _out_string_vector(
            _validation_category_strings([6])
        )
        self["cacheResolvedPath"] = _out_string("")
        self["cacheLockedBy"] = _out_string("")
        self["cacheLockedHost"] = _out_string("")
        self["cacheLockedTime"] = _out_string("")
        self["cacheLockedScript"] = _out_string("")
        self["cacheDescription"] = _out_string(SCHEMA_DESCRIPTION)

        self["out"].setInput(self["in"])

        self.__plugSetConnection = self.plugSetSignal().connect(
            Gaffer.WeakMethod(self.__plugSet),
            scoped=True,
        )

        self.__ensureCacheStore()
        self.__syncStateFromStore()


def _bind_painted_points_methods():
    setattr(
        PaintedPoints, "_PaintedPoints__defaultStore", _store_helpers.__defaultStore
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__emptyDiagnostics",
        _store_helpers.__emptyDiagnostics,
    )
    setattr(
        PaintedPoints, "_PaintedPoints__nodeMetadata", _store_helpers.__nodeMetadata
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__activeLockMetadata",
        _store_helpers.__activeLockMetadata,
    )
    setattr(
        PaintedPoints, "_PaintedPoints__lockFilePath", _store_helpers.__lockFilePath
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__readExternalLock",
        _store_helpers.__readExternalLock,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__sameSessionLock",
        _store_helpers.__sameSessionLock,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__ensureWriteLock",
        _store_helpers.__ensureWriteLock,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__releaseWriteLock",
        _store_helpers.__releaseWriteLock,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__writeBackupIfEnabled",
        _store_helpers.__writeBackupIfEnabled,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__diagnosticsExportPath",
        _store_helpers.__diagnosticsExportPath,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__interchangeExportPath",
        _store_helpers.__interchangeExportPath,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__authoredExportPath",
        _store_helpers.__authoredExportPath,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__resolvedCachePath",
        _store_helpers.__resolvedCachePath,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__ensureCacheStore",
        _store_helpers.__ensureCacheStore,
    )
    setattr(PaintedPoints, "_PaintedPoints__loadStore", _store_helpers.__loadStore)
    setattr(PaintedPoints, "_PaintedPoints__writeStore", _store_helpers.__writeStore)
    setattr(
        PaintedPoints, "_PaintedPoints__validateStore", _store_helpers.__validateStore
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__trustedValidation",
        _store_helpers.__trustedValidation,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__applySyncedState",
        _store_helpers.__applySyncedState,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__syncStateFromStore",
        _store_helpers.__syncStateFromStore,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__finalizeStoreMutation",
        _store_helpers.__finalizeStoreMutation,
    )
    setattr(PaintedPoints, "_PaintedPoints__plugSet", _store_helpers.__plugSet)
    setattr(PaintedPoints, "_PaintedPoints__mutateStore", _store_helpers.__mutateStore)
    setattr(PaintedPoints, "migrateCacheMode", _store_helpers.migrateCacheMode)
    setattr(PaintedPoints, "relinkCache", _store_helpers.relinkCache)
    setattr(PaintedPoints, "upgradeCache", _store_helpers.upgradeCache)
    setattr(PaintedPoints, "validateCache", _store_helpers.validateCache)
    setattr(PaintedPoints, "validateAttachments", _store_helpers.validateAttachments)
    setattr(PaintedPoints, "compactCache", _store_helpers.compactCache)
    setattr(PaintedPoints, "cacheSnapshot", _store_helpers.cacheSnapshot)
    setattr(PaintedPoints, "cacheStoreSnapshot", _store_helpers.cacheSnapshot)
    setattr(PaintedPoints, "mutateCacheStore", _store_helpers.mutateCacheStore)
    setattr(
        PaintedPoints,
        "_PaintedPoints__pointChunkSlices",
        _model_helpers.__pointChunkSlices,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__strokeFrameActive",
        _model_helpers.__strokeFrameActive,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__layerFrameActive",
        _model_helpers.__layerFrameActive,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__activeStrokeIdsForFrame",
        _model_helpers.__activeStrokeIdsForFrame,
    )
    setattr(PaintedPoints, "_PaintedPoints__nextId", _model_helpers.__nextId)
    setattr(PaintedPoints, "_PaintedPoints__layerNames", _model_helpers.__layerNames)
    setattr(
        PaintedPoints,
        "_PaintedPoints__selectionSetNames",
        _model_helpers.__selectionSetNames,
    )
    setattr(PaintedPoints, "_PaintedPoints__iterStrokes", _model_helpers.__iterStrokes)
    setattr(PaintedPoints, "_PaintedPoints__iterPoints", _model_helpers.__iterPoints)
    setattr(
        PaintedPoints,
        "_PaintedPoints__selectedPointIds",
        _model_helpers.__selectedPointIds,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__pruneSelectionState",
        _model_helpers.__pruneSelectionState,
    )
    setattr(
        PaintedPoints, "_PaintedPoints__strokePoints", _model_helpers.__strokePoints
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__scenePathFromId",
        _model_helpers.__scenePathFromId,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__internScenePath",
        _model_helpers.__internScenePath,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__internInstanceSourcePath",
        _model_helpers.__internInstanceSourcePath,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__applyExpandedPointEdit",
        _model_helpers.__applyExpandedPointEdit,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__defaultPointRecord",
        _model_helpers.__defaultPointRecord,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__refreshDerivedData",
        _model_helpers.__refreshDerivedData,
    )
    setattr(
        PaintedPoints, "_PaintedPoints__resolveLayer", _model_helpers.__resolveLayer
    )
    setattr(
        PaintedPoints, "_PaintedPoints__resolveStroke", _model_helpers.__resolveStroke
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__resolveSelectionSet",
        _model_helpers.__resolveSelectionSet,
    )
    setattr(
        PaintedPoints, "_PaintedPoints__reindexLayers", _model_helpers.__reindexLayers
    )
    setattr(
        PaintedPoints, "_PaintedPoints__reindexStrokes", _model_helpers.__reindexStrokes
    )
    setattr(PaintedPoints, "_PaintedPoints__uniqueName", _model_helpers.__uniqueName)
    setattr(PaintedPoints, "createLayer", _model_helpers.createLayer)
    setattr(PaintedPoints, "deleteLayer", _model_helpers.deleteLayer)
    setattr(PaintedPoints, "renameLayer", _model_helpers.renameLayer)
    setattr(PaintedPoints, "moveLayer", _model_helpers.moveLayer)
    setattr(PaintedPoints, "mergeLayers", _model_helpers.mergeLayers)
    setattr(PaintedPoints, "setLayerVisible", _model_helpers.setLayerVisible)
    setattr(PaintedPoints, "setLayerMute", _model_helpers.setLayerMute)
    setattr(PaintedPoints, "setLayerSolo", _model_helpers.setLayerSolo)
    setattr(PaintedPoints, "setLayerTimeRange", _model_helpers.setLayerTimeRange)
    setattr(PaintedPoints, "createStroke", _model_helpers.createStroke)
    setattr(PaintedPoints, "deleteStroke", _model_helpers.deleteStroke)
    setattr(PaintedPoints, "renameStroke", _model_helpers.renameStroke)
    setattr(PaintedPoints, "moveStroke", _model_helpers.moveStroke)
    setattr(PaintedPoints, "mergeStrokes", _model_helpers.mergeStrokes)
    setattr(PaintedPoints, "paintStrokeCommit", _model_helpers.paintStrokeCommit)
    setattr(PaintedPoints, "eraseCommit", _model_helpers.eraseCommit)
    setattr(
        PaintedPoints, "splitStrokeBySelection", _model_helpers.splitStrokeBySelection
    )
    setattr(PaintedPoints, "createSelectionSet", _model_helpers.createSelectionSet)
    setattr(PaintedPoints, "renameSelectionSet", _model_helpers.renameSelectionSet)
    setattr(PaintedPoints, "deleteSelectionSet", _model_helpers.deleteSelectionSet)
    setattr(
        PaintedPoints, "storeCurrentSelection", _model_helpers.storeCurrentSelection
    )
    setattr(PaintedPoints, "setCurrentSelection", _model_helpers.setCurrentSelection)
    setattr(PaintedPoints, "layerRecords", _model_helpers.layerRecords)
    setattr(PaintedPoints, "strokeRecords", _model_helpers.strokeRecords)
    setattr(PaintedPoints, "pointRecords", _model_helpers.pointRecords)
    setattr(PaintedPoints, "ensureLayer", _model_helpers.ensureLayer)
    setattr(PaintedPoints, "ensureStroke", _model_helpers.ensureStroke)
    setattr(PaintedPoints, "ensureLayerAndStroke", _model_helpers.ensureLayerAndStroke)
    setattr(PaintedPoints, "seedBenchmarkStroke", _model_helpers.seedBenchmarkStroke)
    setattr(PaintedPoints, "lastStrokeId", _model_helpers.lastStrokeId)
    setattr(PaintedPoints, "mutatePoints", _model_helpers.mutatePoints)
    setattr(
        PaintedPoints, "totalAuthoredPointCount", _model_helpers.totalAuthoredPointCount
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__expandedPointRecord",
        _surface_helpers.__expandedPointRecord,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__surfaceRuntimeCache",
        _surface_helpers.__surfaceRuntimeCache,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__surfacePointData",
        _surface_helpers.__surfacePointData,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__surfaceCandidates",
        _surface_helpers.__surfaceCandidates,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__surfaceAllowedPaths",
        _surface_helpers.__surfaceAllowedPaths,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__surfaceCandidateForPath",
        _surface_helpers.__surfaceCandidateForPath,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__surfaceCandidatesForSamples",
        _surface_helpers.__surfaceCandidatesForSamples,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__surfaceCandidatesWithAllowedPaths",
        _surface_helpers.__surfaceCandidatesWithAllowedPaths,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__brushSampleWorldPosition",
        _surface_helpers.__brushSampleWorldPosition,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__brushSampleSurfacePoints",
        _surface_helpers.__brushSampleSurfacePoints,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__authoredBrushPoint",
        _surface_helpers.__authoredBrushPoint,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__authoredBrushPoints",
        _surface_helpers.__authoredBrushPoints,
    )
    setattr(PaintedPoints, "brushPaintPoints", _surface_helpers.brushPaintPoints)
    setattr(PaintedPoints, "brushPaintCommit", _surface_helpers.brushPaintCommit)
    setattr(PaintedPoints, "brushErasePoints", _surface_helpers.brushErasePoints)
    setattr(PaintedPoints, "relaxSelection", _surface_helpers.relaxSelection)
    setattr(PaintedPoints, "reprojectSelection", _surface_helpers.reprojectSelection)
    setattr(
        PaintedPoints, "_PaintedPoints__exportPayload", _export_helpers.__exportPayload
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__expandedPointRecordsForFrame",
        _export_helpers.__expandedPointRecordsForFrame,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__frameRecordPayload",
        _export_helpers.__frameRecordPayload,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__createStaticBakeNode",
        _export_helpers.__createStaticBakeNode,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__attachedPointsForOutput",
        _export_helpers.__attachedPointsForOutput,
    )
    setattr(
        PaintedPoints,
        "_PaintedPoints__evaluatedPointsPrimitiveForFrame",
        _export_helpers.__evaluatedPointsPrimitiveForFrame,
    )
    setattr(PaintedPoints, "exportAuthoredCache", _export_helpers.exportAuthoredCache)
    setattr(
        PaintedPoints, "exportEvaluatedPoints", _export_helpers.exportEvaluatedPoints
    )
    setattr(PaintedPoints, "exportGafferScene", _export_helpers.exportGafferScene)
    setattr(PaintedPoints, "exportUSD", _export_helpers.exportUSD)
    setattr(PaintedPoints, "exportAlembic", _export_helpers.exportAlembic)
    setattr(PaintedPoints, "exportInterchange", _export_helpers.exportInterchange)
    setattr(PaintedPoints, "exportDiagnostics", _export_helpers.exportDiagnostics)
    setattr(PaintedPoints, "freezeBakeSelection", _export_helpers.freezeBakeSelection)
    setattr(
        PaintedPoints, "freezeBakeToStaticNode", _export_helpers.freezeBakeToStaticNode
    )
    setattr(
        PaintedPoints,
        "freezeBakeEvaluatedToStaticNode",
        _export_helpers.freezeBakeEvaluatedToStaticNode,
    )


_bind_painted_points_methods()

del _bind_painted_points_methods
