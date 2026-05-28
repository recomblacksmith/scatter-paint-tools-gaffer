#include "boost/python.hpp"

#include "GafferScatterPaint/AttachedPoints.h"
#include "GafferScatterPaint/PaintedPoints.h"
#include "GafferScatterPaint/StaticPoints.h"

#include "GafferBindings/DependencyNodeBinding.h"

using namespace boost::python;
using namespace GafferScatterPaint;

BOOST_PYTHON_MODULE( _GafferScatterPaint )
{
	{
				scope paintedPointsScope =
					GafferBindings::DependencyNodeClass<PaintedPoints>()
					.def( "createLayer", &PaintedPoints::createLayer, ( arg( "name" ) = "" ) )
					.def( "ensureLayer", &PaintedPoints::ensureLayer, ( arg( "name" ) ) )
					.def( "deleteLayer", &PaintedPoints::deleteLayer, ( arg( "layerIdentifier" ) ) )
					.def( "renameLayer", &PaintedPoints::renameLayer, ( arg( "layerIdentifier" ), arg( "newName" ) ) )
					.def( "moveLayer", &PaintedPoints::moveLayer, ( arg( "layerIdentifier" ), arg( "newIndex" ) ) )
					.def( "mergeLayers", &PaintedPoints::mergeLayers, ( arg( "sourceLayerIdentifier" ), arg( "destinationLayerIdentifier" ) ) )
					.def( "setLayerVisible", &PaintedPoints::setLayerVisible, ( arg( "layerIdentifier" ), arg( "visible" ) ) )
					.def( "setLayerMute", &PaintedPoints::setLayerMute, ( arg( "layerIdentifier" ), arg( "mute" ) ) )
					.def( "setLayerSolo", &PaintedPoints::setLayerSolo, ( arg( "layerIdentifier" ), arg( "solo" ) ) )
					.def( "setLayerTimeRange", &PaintedPoints::setLayerTimeRange, ( arg( "layerIdentifier" ), arg( "frameStart" ), arg( "frameEnd" ) ) )
					.def( "createStroke", &PaintedPoints::createStroke, ( arg( "layerIdentifier" ), arg( "name" ) = "" ) )
					.def( "ensureStroke", &PaintedPoints::ensureStroke, ( arg( "layerIdentifier" ), arg( "name" ) ) )
					.def( "ensureLayerAndStroke", &PaintedPoints::ensureLayerAndStroke, ( arg( "layerName" ) = "", arg( "strokeName" ) = "" ) )
					.def( "deleteStroke", &PaintedPoints::deleteStroke, ( arg( "strokeIdentifier" ) ) )
					.def( "renameStroke", &PaintedPoints::renameStroke, ( arg( "strokeIdentifier" ), arg( "newName" ) ) )
					.def( "moveStroke", &PaintedPoints::moveStroke, ( arg( "strokeIdentifier" ), arg( "newIndex" ) ) )
					.def( "mergeStrokes", &PaintedPoints::mergeStrokes, ( arg( "sourceStrokeIdentifier" ), arg( "destinationStrokeIdentifier" ) ) )
					.def( "paintStrokeCommit", &PaintedPoints::paintStrokeCommit, ( arg( "strokeIdentifier" ), arg( "points" ), arg( "append" ) = true ) )
					.def( "brushPaintCommit", &PaintedPoints::brushPaintCommit, ( arg( "strokeIdentifier" ), arg( "samples" ), arg( "append" ) = true ) )
					.def( "brushPaintCommitNative", &PaintedPoints::brushPaintCommitNative, ( arg( "strokeIdentifier" ), arg( "samples" ), arg( "append" ) = true ) )
					.def( "eraseCommit", &PaintedPoints::eraseCommit, ( arg( "strokeIdentifier" ), arg( "pointIds" ) = object(), arg( "fraction" ) = object() ) )
					.def( "brushErasePoints", &PaintedPoints::brushErasePoints, ( arg( "samples" ), arg( "radius" ), arg( "eraseSpace" ) = 0 ) )
					.def( "lastStrokeId", &PaintedPoints::lastStrokeId )
					.def( "mutatePoints", &PaintedPoints::mutatePoints, ( arg( "mutator" ) ) )
					.def( "relaxSelection", &PaintedPoints::relaxSelection )
					.def( "reprojectSelection", &PaintedPoints::reprojectSelection )
					.def( "splitStrokeBySelection", &PaintedPoints::splitStrokeBySelection )
					.def( "createSelectionSet", &PaintedPoints::createSelectionSet, ( arg( "name" ) = "" ) )
					.def( "renameSelectionSet", &PaintedPoints::renameSelectionSet, ( arg( "selectionIdentifier" ), arg( "newName" ) ) )
					.def( "deleteSelectionSet", &PaintedPoints::deleteSelectionSet, ( arg( "selectionIdentifier" ) ) )
					.def( "storeCurrentSelection", &PaintedPoints::storeCurrentSelection, ( arg( "selectionIdentifier" ) = object(), arg( "name" ) = object() ) )
					.def( "setCurrentSelection", &PaintedPoints::setCurrentSelection, ( arg( "pointIds" ) = object(), arg( "strokeIds" ) = object() ) )
					.def( "cacheSnapshot", &PaintedPoints::cacheSnapshot )
					.def( "layerRecords", &PaintedPoints::layerRecords )
					.def( "strokeRecords", &PaintedPoints::strokeRecords )
					.def( "pointRecords", &PaintedPoints::pointRecords )
					.def( "mutateCacheStore", &PaintedPoints::mutateCacheStore, ( arg( "mutator" ) ) )
					.def( "seedBenchmarkStroke", &PaintedPoints::seedBenchmarkStroke, ( arg( "pointCount" ), arg( "layerName" ) = "", arg( "strokeName" ) = "" ) )
					.def( "validateCache", &PaintedPoints::validateCache )
					.def( "validateAttachments", &PaintedPoints::validateAttachments )
					.def( "migrateCacheMode", &PaintedPoints::migrateCacheMode )
					.def( "relinkCache", &PaintedPoints::relinkCache )
					.def( "upgradeCache", &PaintedPoints::upgradeCache )
					.def( "exportAuthoredCache", &PaintedPoints::exportAuthoredCache )
					.def( "exportEvaluatedPoints", &PaintedPoints::exportEvaluatedPoints )
					.def( "exportGafferScene", &PaintedPoints::exportGafferScene )
					.def( "exportUSD", &PaintedPoints::exportUSD )
					.def( "exportAlembic", &PaintedPoints::exportAlembic )
					.def( "exportInterchange", &PaintedPoints::exportInterchange )
					.def( "exportDiagnostics", &PaintedPoints::exportDiagnostics )
					.def( "freezeBakeSelection", &PaintedPoints::freezeBakeSelection, ( arg( "startFrame" ) = object(), arg( "endFrame" ) = object() ) )
					.def( "freezeBakeToStaticNode", &PaintedPoints::freezeBakeToStaticNode, ( arg( "startFrame" ) = object(), arg( "endFrame" ) = object() ) )
					.def( "freezeBakeEvaluatedToStaticNode", &PaintedPoints::freezeBakeEvaluatedToStaticNode, ( arg( "startFrame" ) = object(), arg( "endFrame" ) = object() ) )
					.def( "compactCache", &PaintedPoints::compactCache )
		;
	}
	GafferBindings::DependencyNodeClass<AttachedPoints>();
	GafferBindings::DependencyNodeClass<StaticPoints>()
		.def( "setPointRecords", &StaticPoints::setPointRecords, ( arg( "records" ) ) )
		.def( "setFramePointRecords", &StaticPoints::setFramePointRecords, ( arg( "frameRecords" ) ) )
		.def( "setFramePointData", &StaticPoints::setFramePointData, ( arg( "frameData" ) ) );
}
