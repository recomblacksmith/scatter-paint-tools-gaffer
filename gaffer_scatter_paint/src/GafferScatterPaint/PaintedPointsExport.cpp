#include "PaintedPointsPrivate.h"
#include "CacheFormatDict.h"

std::string PaintedPoints::exportAuthoredCache()
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	if( !loadError.empty() )
	{
		throw std::runtime_error( loadError );
	}

	const std::string packedBytes = packCacheSchema( dictToCacheSchema( store ) );
	const std::string exportPath = authoredExportPath( this );
	writeBytesFile( exportPath, packedBytes );
	return exportPath;
}

std::string PaintedPoints::exportEvaluatedPoints()
{
	std::string outputLocation;
	ConstObjectPtr evaluatedObject = evaluatedPointsObject( this, &outputLocation );
	const std::string exportPath = evaluatedExportPath( this );
	writeObjectFile( exportPath, evaluatedObject.get() );
	return exportPath;
}

std::string PaintedPoints::exportGafferScene()
{
	return exportEvaluatedScene( this, gafferSceneExportPath( this ) );
}

std::string PaintedPoints::exportUSD()
{
	return exportEvaluatedScene( this, usdExportPath( this ) );
}

std::string PaintedPoints::exportAlembic()
{
	return exportEvaluatedScene( this, alembicExportPath( this ) );
}

std::string PaintedPoints::exportInterchange()
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	if( !loadError.empty() )
	{
		throw std::runtime_error( loadError );
	}

	syncStateFromStore( this, store, loadError );
	bp::dict payload = exportPayload( this, store, false );
	const std::string packedBytes = packCacheSchema( dictToCacheSchema( payload ) );
	const std::string exportPath = interchangeExportPath( this );
	writeBytesFile( exportPath, packedBytes );
	return exportPath;
}

std::string PaintedPoints::exportDiagnostics()
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	syncStateFromStore( this, store, loadError );

	ConstStringVectorDataPtr failingPathsData = runTimeCast<const StringVectorData>( failingTargetPathsPlug()->getValue() );
	std::string failingPaths;
	if( failingPathsData )
	{
		const auto &paths = failingPathsData->readable();
		for( size_t i = 0; i < paths.size(); ++i )
		{
			if( i )
			{
				failingPaths += ", ";
			}
			failingPaths += paths[i];
		}
	}

	ConstStringVectorDataPtr validationCategoriesData = runTimeCast<const StringVectorData>( validationCategoriesPlug()->getValue() );
	std::string validationCategories;
	if( validationCategoriesData )
	{
		const auto &categories = validationCategoriesData->readable();
		for( size_t i = 0; i < categories.size(); ++i )
		{
			if( i )
			{
				validationCategories += ", ";
			}
			validationCategories += categories[i];
		}
	}

	const std::string diagnosticsText =
		"validationSummary: " + validationSummaryPlug()->getValue() + "\n" +
		"validationCategories: " + validationCategories + "\n" +
		"invalidPointCount: " + std::to_string( invalidPointCountPlug()->getValue() ) + "\n" +
		"invalidStrokeCount: " + std::to_string( invalidStrokeCountPlug()->getValue() ) + "\n" +
		"failingFrame: " + std::to_string( failingFramePlug()->getValue() ) + "\n" +
		"topologyMismatchCount: " + std::to_string( topologyMismatchCountPlug()->getValue() ) + "\n" +
		"failingTargetPaths: " + failingPaths + "\n" +
		"lastErrorMessage: " + lastErrorMessagePlug()->getValue() + "\n" +
		"cacheResolvedPath: " + cacheResolvedPathPlug()->getValue() + "\n" +
		"cacheVersion: " + std::to_string( cacheVersionPlug()->getValue() ) + "\n" +
		"cacheLockedBy: " + cacheLockedByPlug()->getValue() + "\n" +
		"cacheLockedHost: " + cacheLockedHostPlug()->getValue() + "\n" +
		"cacheLockedTime: " + cacheLockedTimePlug()->getValue() + "\n" +
		"cacheLockedScript: " + cacheLockedScriptPlug()->getValue();

	const std::string exportPath = diagnosticsExportPath( this );
	writeTextFile( exportPath, diagnosticsText );
	return exportPath;
}

std::string PaintedPoints::freezeBakeSelection( const bp::object &startFrame, const bp::object &endFrame )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	if( !loadError.empty() )
	{
		throw std::runtime_error( loadError );
	}

	const std::vector<int> frames = bakeFrames( startFrame, endFrame );
	const std::set<std::uint64_t> pointIds = selectedPointIds( store );
	if( pointIds.empty() )
	{
		throw std::runtime_error( "Unable to bake selection: no points are currently selected" );
	}

	GraphComponent *parentComponent = parent<GraphComponent>();
	if( !parentComponent )
	{
		throw std::runtime_error( "PaintedPoints must have a parent to create a StaticPoints bake node" );
	}

	const std::string nodeName = uniqueBakeNodeName( parentComponent, getName().string() + "SelectionStaticBake" );
	StaticPointsPtr staticPoints = new StaticPoints( nodeName );
	parentComponent->addChild( staticPoints );
	staticPoints->outputLocationPlug()->setValue( "/scatterBakeSelection" );
	if( frames.empty() )
	{
		staticPoints->setPointRecords( expandedPointRecordsForFrame( store, currentFrame( Context::current() ), &pointIds ) );
	}
	else
	{
		bp::list frameRecords;
		for( const int frame : frames )
		{
			bp::dict frameRecord;
			frameRecord["frame"] = frame;
			frameRecord["records"] = expandedPointRecordsForFrame( store, frame, &pointIds );
			frameRecords.append( frameRecord );
		}
		staticPoints->setFramePointRecords( frameRecords );
	}
	return staticPoints->getName().string();
}

std::string PaintedPoints::freezeBakeToStaticNode( const bp::object &startFrame, const bp::object &endFrame )
{
	GraphComponent *parentComponent = parent<GraphComponent>();
	if( !parentComponent )
	{
		throw std::runtime_error( "PaintedPoints must have a parent to create a StaticPoints bake node" );
	}

	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	if( !loadError.empty() )
	{
		throw std::runtime_error( loadError );
	}

	const std::vector<int> frames = bakeFrames( startFrame, endFrame );
	const std::string nodeName = uniqueBakeNodeName( parentComponent, getName().string() + "StaticBake" );
	StaticPointsPtr staticPoints = new StaticPoints( nodeName );
	parentComponent->addChild( staticPoints );
	staticPoints->outputLocationPlug()->setValue( "/scatterBake" );
	if( frames.empty() )
	{
		staticPoints->setPointRecords( expandedPointRecordsForFrame( store, currentFrame( Context::current() ) ) );
	}
	else
	{
		bp::list frameRecords;
		for( const int frame : frames )
		{
			bp::dict frameRecord;
			frameRecord["frame"] = frame;
			frameRecord["records"] = expandedPointRecordsForFrame( store, frame );
			frameRecords.append( frameRecord );
		}
		staticPoints->setFramePointRecords( frameRecords );
	}
	return staticPoints->getName().string();
}

std::string PaintedPoints::freezeBakeEvaluatedToStaticNode( const bp::object &startFrame, const bp::object &endFrame )
{
	GraphComponent *parentComponent = parent<GraphComponent>();
	if( !parentComponent )
	{
		throw std::runtime_error( "PaintedPoints must have a parent to create a StaticPoints bake node" );
	}

	const std::vector<int> frames = bakeFrames( startFrame, endFrame );
	AttachedPoints *attachedPoints = nullptr;
	IECoreScene::ConstPointsPrimitivePtr evaluatedPoints;
	std::vector<IECoreScene::ConstPointsPrimitivePtr> evaluatedSamples;
	if( frames.empty() )
	{
		evaluatedPoints = evaluatedPointsPrimitive( this, &attachedPoints );
	}
	else
	{
		evaluatedSamples.reserve( frames.size() );
		for( const int frame : frames )
		{
			Context::EditableScope scope( Context::current() );
			scope.setFrame( static_cast<float>( frame ) );
			evaluatedSamples.push_back( evaluatedPointsPrimitive( this, &attachedPoints ) );
		}
		evaluatedPoints = evaluatedSamples.front();
	}
	if( !attachedPoints )
	{
		throw std::runtime_error( "Unable to freeze evaluated points: no AttachedPoints node is connected to this PaintedPoints output" );
	}

	const std::string nodeName = uniqueBakeNodeName( parentComponent, getName().string() + "EvaluatedStaticBake" );
	StaticPointsPtr staticPoints = new StaticPoints( nodeName );
	parentComponent->addChild( staticPoints );
	staticPoints->outputLocationPlug()->setValue( "/scatterBake" );
	staticPoints->pointTypePlug()->setValue( attachedPoints->pointTypePlug()->getValue() );
	if( frames.empty() )
	{
		staticPoints->pointDataPlug()->setValue( bakedPointDataFromPrimitive( evaluatedPoints.get() ) );
	}
	else
	{
		staticPoints->pointDataPlug()->setValue( bakedPointDataFromFramePrimitives( frames, evaluatedSamples ) );
	}
	return staticPoints->getName().string();
}
