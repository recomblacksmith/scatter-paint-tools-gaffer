#include "AttachedPointsPrivate.h"

using namespace Gaffer;
using namespace GafferScatterPaint;
using namespace GafferScatterPaint::AttachedPointsPrivate;
using namespace GafferScene;
using namespace IECore;
using namespace IECoreScene;

GAFFER_NODE_DEFINE_TYPE( AttachedPoints );

size_t AttachedPoints::g_firstPlugIndex = 0;

AttachedPoints::AttachedPoints( const std::string &name )
	: SceneProcessor( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );

	addChild( new ScenePlug( "points", Plug::In ) );
	addChild( new StringPlug( "outputLocation", Plug::In, "/scatter" ) );
	addChild( new StringPlug( "pointType", Plug::In, "gl:point" ) );
	addChild( new StringPlug( "includeAttributes" ) );
	addChild( new IntPlug( "exportPreset", Plug::In, 1, 0, 2 ) );
	addChild( new IntPlug( "surfaceSolveMode", Plug::In, 0, 0, 0 ) );
	addChild( new BoolPlug( "allowCrossMeshReproject", Plug::In, false ) );
	addChild( new BoolPlug( "keepLastValidOutput", Plug::In, true ) );
	addChild( new BoolPlug( "strictUnresolved", Plug::In, true ) );
	addChild( new BoolPlug( "debugColor", Plug::In, false ) );
	addChild( new IntPlug( "cacheVersion", Plug::Out, g_schemaVersion ) );
	addChild( new IntPlug( "resolvedPointCount", Plug::Out, 0 ) );
	addChild( new IntPlug( "unresolvedPointCount", Plug::Out, 0 ) );
	addChild( new IntPlug( "lastValidFrame", Plug::Out, 0 ) );
	addChild( new IntPlug( "invalidPointCount", Plug::Out, 0 ) );
	addChild( new IntPlug( "invalidStrokeCount", Plug::Out, 0 ) );
	addChild( new IntPlug( "failingFrame", Plug::Out, 0 ) );
	addChild( new ObjectPlug( "failingTargetPaths", Plug::Out, new StringVectorData() ) );
	addChild( new ObjectPlug( "attachmentFailureReasons", Plug::Out, new StringVectorData() ) );
	addChild( new IntPlug( "topologyMismatchCount", Plug::Out, 0 ) );
	addChild( new StringPlug( "solveStatus", Plug::Out, "" ) );
	addChild( new ObjectPlug( "validationCategories", Plug::Out, new StringVectorData() ) );
}

AttachedPoints::~AttachedPoints()
{
	clearLastValidState( this );
}
