#include "PaintedPointsPrivate.h"

GAFFER_NODE_DEFINE_TYPE( PaintedPoints );

size_t PaintedPoints::g_firstPlugIndex = 0;

PaintedPoints::PaintedPoints( const std::string &name )
	: SceneProcessor( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );

	addChild( new StringPlug( "targetFilter" ) );
	addChild( new StringPlug( "targetSetFilter" ) );
	addChild( new IntPlug( "surfaceMode", Plug::In, 0, 0, 1 ) );
	addChild( new IntPlug( "paintThroughMode", Plug::In, 0, 0, 1 ) );
	addChild( new IntPlug( "relaxObjective", Plug::In, g_relaxObjectivePreserveSilhouette, g_relaxObjectivePreserveSilhouette, g_relaxObjectiveEvenRedistribution ) );
	addChild( new IntPlug( "cacheMode", Plug::In, 0, 0, 1 ) );
	addChild( new StringPlug( "cachePath" ) );
	addChild( new IntPlug( "cachePathMode", Plug::In, 1, 0, 2 ) );
	addChild( new StringPlug( "projectRoot" ) );
	addChild( new IntPlug( "lockMode", Plug::In, 0, 0, 0 ) );
	addChild( new BoolPlug( "backupEnabled", Plug::In, false ) );
	addChild( new IntPlug( "backupPolicy", Plug::In, 0, 0, 1 ) );
	addChild( new IntPlug( "compactionMode", Plug::In, 0, 0, 0 ) );
	addChild( new IntPlug( "globalModePrecedence", Plug::In, 0, 0, 1 ) );
	addChild( new Color3fPlug( "defaultColor", Plug::In, Imath::Color3f( 0.0f, 0.0f, 120.0f / 255.0f ) ) );

	PlugPtr pressureDefaults = new Plug( "pressureDefaults" );
	pressureDefaults->addChild( new BoolPlug( "enabled", Plug::In, false ) );
	pressureDefaults->addChild( new IntPlug( "mappingMode", Plug::In, 0, 0, 1 ) );
	pressureDefaults->addChild( new ObjectPlug( "densityCurve", Plug::In, new CompoundObject() ) );
	pressureDefaults->addChild( new ObjectPlug( "softnessCurve", Plug::In, new CompoundObject() ) );
	addChild( pressureDefaults );

	PlugPtr brushDefaults = new Plug( "brushDefaults" );
	brushDefaults->addChild( new FloatPlug( "size", Plug::In, 0.1f, 0.0f ) );
	brushDefaults->addChild( new FloatPlug( "density", Plug::In, 1.0f, 0.0f ) );
	brushDefaults->addChild( new FloatPlug( "softness", Plug::In, 1.0f, 0.0f ) );
	brushDefaults->addChild( new FloatPlug( "spacing", Plug::In, 0.25f, 0.0f ) );
	brushDefaults->addChild( new IntPlug( "points", Plug::In, 1, 1 ) );
	brushDefaults->addChild( new IntPlug( "rotationMode", Plug::In, 0 ) );
	brushDefaults->addChild( new FloatPlug( "scaleJitter", Plug::In, 0.0f, 0.0f ) );
	brushDefaults->addChild( new FloatPlug( "widthJitter", Plug::In, 0.0f, 0.0f ) );
	addChild( brushDefaults );

	addChild( new ObjectPlug( "layers", Plug::In, new CompoundObject() ) );
	addChild( new ObjectPlug( "selectionSets", Plug::In, new CompoundObject() ) );
	addChild( new ObjectPlug( "cacheBlob", Plug::In, new UCharVectorData() ) );
	addChild( new IntPlug( "authoredPointCount", Plug::In, 0 ) );

	addChild( new IntPlug( "invalidPointCount", Plug::In, 0 ) );
	addChild( new IntPlug( "invalidStrokeCount", Plug::In, 0 ) );
	addChild( new IntPlug( "failingFrame", Plug::In, 0 ) );
	addChild( new ObjectPlug( "failingTargetPaths", Plug::In, new StringVectorData() ) );
	addChild( new StringPlug( "lastErrorMessage", Plug::In, "" ) );
	addChild( new IntPlug( "topologyMismatchCount", Plug::In, 0 ) );
	addChild( new StringPlug( "validationSummary", Plug::In, "" ) );
	addChild( new ObjectPlug( "validationCategories", Plug::In, new StringVectorData() ) );
	addChild( new StringPlug( "cacheResolvedPath", Plug::In, "" ) );
	addChild( new IntPlug( "cacheVersion", Plug::In, 1 ) );
	addChild( new StringPlug( "cacheLockedBy", Plug::In, "" ) );
	addChild( new StringPlug( "cacheLockedHost", Plug::In, "" ) );
	addChild( new StringPlug( "cacheLockedTime", Plug::In, "" ) );
	addChild( new StringPlug( "cacheLockedScript", Plug::In, "" ) );
	addChild( new ObjectPlug( "pendingPaintBlob", Plug::In, new UCharVectorData() ) );
	addChild( new IntPlug( "interactiveRevision", Plug::In, 0, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), Plug::Default & ~Plug::Serialisable ) );
}

PaintedPoints::~PaintedPoints()
{
	eraseInteractiveOverlayForNode( this );
}
