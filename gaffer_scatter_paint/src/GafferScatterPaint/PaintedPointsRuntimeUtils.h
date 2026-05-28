#pragma once

inline int currentFrame( const Context *context )
{
	return context ? static_cast<int>( std::lround( context->getFrame() ) ) : 0;
}

inline std::vector<int> bakeFrames( const bp::object &startFrameObject, const bp::object &endFrameObject )
{
	if( isNone( startFrameObject ) && isNone( endFrameObject ) )
	{
		return {};
	}

	const int defaultFrame = currentFrame( Context::current() );
	int startFrame = isNone( startFrameObject ) ? defaultFrame : extractOr<int>( startFrameObject, defaultFrame );
	int endFrame = isNone( endFrameObject ) ? startFrame : extractOr<int>( endFrameObject, startFrame );
	if( startFrame > endFrame )
	{
		std::swap( startFrame, endFrame );
	}

	std::vector<int> result;
	result.reserve( static_cast<size_t>( endFrame - startFrame + 1 ) );
	for( int frame = startFrame; frame <= endFrame; ++frame )
	{
		result.push_back( frame );
	}
	return result;
}

inline bool strokeFrameActive( const bp::dict &stroke, int frame )
{
	int frameStart = dictValue<int>( stroke, "frameStart", 0 );
	int frameEnd = dictValue<int>( stroke, "frameEnd", 0 );
	if( frameStart == 0 && frameEnd == 0 )
	{
		return true;
	}
	if( frameStart > frameEnd )
	{
		std::swap( frameStart, frameEnd );
	}
	return frameStart <= frame && frame <= frameEnd;
}

inline bool layerFrameActive( const bp::dict &layer, int frame )
{
	int frameStart = dictValue<int>( layer, "frameStart", 0 );
	int frameEnd = dictValue<int>( layer, "frameEnd", 0 );
	if( frameStart == 0 && frameEnd == 0 )
	{
		return true;
	}
	if( frameStart > frameEnd )
	{
		std::swap( frameStart, frameEnd );
	}
	if( frameStart <= frame && frame <= frameEnd )
	{
		return true;
	}
	return dictValue<bool>( layer, "holdOutsideRange", true );
}

inline std::set<std::uint64_t> activeStrokeIdsForFrame( const bp::dict &store, int frame )
{
	bp::list layers = bp::extract<bp::list>( store["layers"] );
	bp::list strokes = bp::extract<bp::list>( store["strokes"] );

	std::vector<bp::dict> visibleLayers;
	for( bp::stl_input_iterator<bp::object> it( layers ), end; it != end; ++it )
	{
		bp::dict layer = bp::extract<bp::dict>( *it );
		if(
			dictValue<bool>( layer, "enabled", true ) &&
			dictValue<bool>( layer, "visible", true ) &&
			!dictValue<bool>( layer, "mute", false ) &&
			layerFrameActive( layer, frame )
		)
		{
			visibleLayers.push_back( layer );
		}
	}

	std::sort(
		visibleLayers.begin(), visibleLayers.end(),
		[]( const bp::dict &a, const bp::dict &b ) {
			return dictValue<int>( a, "order", 0 ) < dictValue<int>( b, "order", 0 );
		}
	);

	const bool anySolo = std::any_of(
		visibleLayers.begin(), visibleLayers.end(),
		[]( const bp::dict &layer ) {
			return dictValue<bool>( layer, "solo", false );
		}
	);

	std::set<std::uint64_t> result;
	for( const bp::dict &layer : visibleLayers )
	{
		if( anySolo && !dictValue<bool>( layer, "solo", false ) )
		{
			continue;
		}

		const std::uint64_t layerId = dictValue<std::uint64_t>( layer, "layerId", 0 );
		std::vector<bp::dict> layerStrokes;
		for( bp::stl_input_iterator<bp::object> it( strokes ), end; it != end; ++it )
		{
			bp::dict stroke = bp::extract<bp::dict>( *it );
			if( dictValue<std::uint64_t>( stroke, "layerId", 0 ) == layerId && strokeFrameActive( stroke, frame ) )
			{
				layerStrokes.push_back( stroke );
			}
		}

		std::sort(
			layerStrokes.begin(), layerStrokes.end(),
			[]( const bp::dict &a, const bp::dict &b ) {
				return dictValue<int>( a, "order", 0 ) < dictValue<int>( b, "order", 0 );
			}
		);

		for( const bp::dict &stroke : layerStrokes )
		{
			result.insert( dictValue<std::uint64_t>( stroke, "strokeId", 0 ) );
		}
	}

	return result;
}

inline IECore::ConstObjectPtr evaluatedPointsObject( const PaintedPoints *node, std::string *outputLocation = nullptr )
{
	AttachedPoints *attachedPoints = findAttachedPointsForPaintedNode( node );
	if( !attachedPoints )
	{
		throw std::runtime_error( "Unable to export evaluated points: no AttachedPoints node is connected to this PaintedPoints output" );
	}

	std::string resolvedOutputLocation = attachedPoints->outputLocationPlug()->getValue();
	if( resolvedOutputLocation.empty() )
	{
		resolvedOutputLocation = "/scatter";
	}

	if( outputLocation )
	{
		*outputLocation = resolvedOutputLocation;
	}

	const ScenePlug::ScenePath outputPath = ScenePlug::stringToPath( resolvedOutputLocation );
	IECore::ConstObjectPtr object = attachedPoints->outPlug()->object( outputPath );
	if( !object )
	{
		throw std::runtime_error( "Unable to export evaluated points: AttachedPoints produced no object at " + resolvedOutputLocation );
	}

	if( !IECore::runTimeCast<const IECoreScene::PointsPrimitive>( object.get() ) )
	{
		throw std::runtime_error( "Unable to export evaluated points: AttachedPoints output at " + resolvedOutputLocation + " is not a PointsPrimitive" );
	}

	return object;
}

inline IECoreScene::ConstPointsPrimitivePtr evaluatedPointsPrimitive( const PaintedPoints *node, AttachedPoints **attachedPointsResult = nullptr )
{
	AttachedPoints *attachedPoints = findAttachedPointsForPaintedNode( node );
	if( attachedPointsResult )
	{
		*attachedPointsResult = attachedPoints;
	}

	std::string outputLocation;
	ConstObjectPtr object = evaluatedPointsObject( node, &outputLocation );
	IECoreScene::ConstPointsPrimitivePtr points = IECore::runTimeCast<const IECoreScene::PointsPrimitive>( object );
	if( !points )
	{
		throw std::runtime_error( "Unable to freeze evaluated points: AttachedPoints output at " + outputLocation + " is not a PointsPrimitive" );
	}

	return points;
}

inline IECore::CompoundObjectPtr bakedPointDataFromPrimitive( const IECoreScene::PointsPrimitive *pointsPrimitive )
{
	if( !pointsPrimitive )
	{
		throw std::runtime_error( "Unable to bake evaluated points: no PointsPrimitive was available" );
	}

	IECore::CompoundObjectPtr pointData = new IECore::CompoundObject();
	pointData->members()["pointCount"] = new IECore::IntData( static_cast<int>( pointsPrimitive->getNumPoints() ) );
	pointData->members()["pointsPrimitive"] = pointsPrimitive->copy();
	return pointData;
}

inline IECore::CompoundObjectPtr bakedPointDataFromFramePrimitives( const std::vector<int> &frames, const std::vector<IECoreScene::ConstPointsPrimitivePtr> &primitives )
{
	if( frames.empty() || frames.size() != primitives.size() )
	{
		throw std::runtime_error( "Unable to bake evaluated points: invalid frame sample data" );
	}

	IECore::CompoundObjectPtr pointData = new IECore::CompoundObject();
	IECore::CompoundObjectPtr frameSamples = new IECore::CompoundObject();
	std::vector<int> frameNumbers;
	frameNumbers.reserve( frames.size() );

	for( size_t i = 0; i < frames.size(); ++i )
	{
		if( !primitives[i] )
		{
			throw std::runtime_error( "Unable to bake evaluated points: frame sample produced no PointsPrimitive" );
		}
		frameNumbers.push_back( frames[i] );
		frameSamples->members()[std::to_string( frames[i] )] = primitives[i]->copy();
	}

	pointData->members()["pointCount"] = new IECore::IntData( static_cast<int>( primitives.front()->getNumPoints() ) );
	pointData->members()["pointsPrimitive"] = primitives.front()->copy();
	pointData->members()["frameNumbers"] = new IECore::IntVectorData( frameNumbers );
	pointData->members()["frameSamples"] = frameSamples;
	return pointData;
}

inline bp::list expandedPointRecordsForFrame( const bp::dict &store, int frame, const std::set<std::uint64_t> *selectedIds = nullptr )
{
	bp::list result;
	const std::set<std::uint64_t> activeStrokeIds = activeStrokeIdsForFrame( store, frame );
	bp::list points = bp::extract<bp::list>( store["points"] );
	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		bp::dict point = bp::extract<bp::dict>( *it );
		const std::uint64_t pointId = dictValue<std::uint64_t>( point, "pointId", 0 );
		if( selectedIds && !selectedIds->count( pointId ) )
		{
			continue;
		}

		const std::uint64_t strokeId = dictValue<std::uint64_t>( point, "strokeId", 0 );
		if( !activeStrokeIds.count( strokeId ) )
		{
			continue;
		}

		result.append( deepCopyDict( expandedPointRecord( store, point ) ) );
	}
	return result;
}

inline std::string uniqueBakeNodeName( const GraphComponent *parentComponent, const std::string &baseName )
{
	std::string nodeName = baseName;
	int suffix = 1;
	while( parentComponent->getChild( nodeName ) )
	{
		++suffix;
		nodeName = baseName + std::to_string( suffix );
	}
	return nodeName;
}

inline bp::dict exportPayload( PaintedPoints *node, const bp::dict &store, bool includeResolved = false )
{
	bp::dict payload = deepCopyDict( store );
	payload["schemaVersion"] = dictValue<int>( store, "schemaVersion", g_schemaVersion );
	payload["validationSummary"] = node->validationSummaryPlug()->getValue();
	bp::dict diagnostics = deepCopyDict( validateStore( node, store, "" ) );
	diagnostics["validationSummary"] = node->validationSummaryPlug()->getValue();
	payload["diagnostics"] = diagnostics;

	if( includeResolved )
	{
		payload["points"] = node->pointRecords();
	}

	return payload;
}

inline void syncStateFromStore( PaintedPoints *node, const bp::dict &store, const std::string &loadError = std::string() )
{
	bp::dict diagnostics = validateStore( node, store, loadError );
	bp::list layerNames;
	std::vector<bp::dict> sortedLayers;
	bp::list layers = bp::extract<bp::list>( store["layers"] );
	for( bp::stl_input_iterator<bp::object> it( layers ), end; it != end; ++it )
	{
		sortedLayers.push_back( bp::extract<bp::dict>( *it ) );
	}
	std::sort(
		sortedLayers.begin(), sortedLayers.end(),
		[]( const bp::dict &a, const bp::dict &b ) {
			return dictValue<int>( a, "order", 0 ) < dictValue<int>( b, "order", 0 );
		}
	);
	for( const bp::dict &layer : sortedLayers )
	{
		layerNames.append( dictValue<std::string>( layer, "name", "" ) );
	}
	bp::list selectionNames;
	bp::list selectionSets = bp::extract<bp::list>( store["selectionSets"] );
	for( bp::stl_input_iterator<bp::object> it( selectionSets ), end; it != end; ++it )
	{
		bp::dict selectionSet = bp::extract<bp::dict>( *it );
		selectionNames.append( dictValue<std::string>( selectionSet, "name", "" ) );
	}

	std::vector<std::string> layerNameStrings = stringVector( layerNames );
	std::vector<std::string> selectionNameStrings = stringVector( selectionNames );
	std::vector<std::string> failingPathStrings = stringVector( diagnostics["failingTargetPaths"] );
	std::vector<std::string> validationCategoryNameStrings = validationCategoryStrings( diagnostics["categories"] );

	node->layersPlug()->setValue( new StringVectorData( layerNameStrings ) );
	node->selectionSetsPlug()->setValue( new StringVectorData( selectionNameStrings ) );
	node->authoredPointCountPlug()->setValue( static_cast<int>( bp::len( bp::extract<bp::list>( store["points"] ) ) ) );
	node->cacheVersionPlug()->setValue( g_schemaVersion );
	node->invalidPointCountPlug()->setValue( dictValue<int>( diagnostics, "invalidPointCount", 0 ) );
	node->invalidStrokeCountPlug()->setValue( dictValue<int>( diagnostics, "invalidStrokeCount", 0 ) );
	node->failingFramePlug()->setValue( 0 );
	node->failingTargetPathsPlug()->setValue( new StringVectorData( failingPathStrings ) );
	node->lastErrorMessagePlug()->setValue( dictValue<std::string>( diagnostics, "lastErrorMessage", "" ) );
	node->topologyMismatchCountPlug()->setValue( dictValue<int>( diagnostics, "topologyMismatchCount", 0 ) );
	node->validationSummaryPlug()->setValue( dictValue<std::string>( diagnostics, "validationSummary", g_schemaDescription ) );
	node->validationCategoriesPlug()->setValue( new StringVectorData( validationCategoryNameStrings ) );
	const bp::dict lock = bp::extract<bp::dict>( dictGet( store, "lock" ) );
	node->cacheResolvedPathPlug()->setValue( resolvedCachePath( node ) );
	node->cacheLockedByPlug()->setValue( dictValue<std::string>( lock, "user", "" ) );
	node->cacheLockedHostPlug()->setValue( dictValue<std::string>( lock, "host", "" ) );
	node->cacheLockedTimePlug()->setValue( dictValue<std::string>( lock, "timestampUtc", "" ) );
	node->cacheLockedScriptPlug()->setValue( dictValue<std::string>( lock, "scriptPath", "" ) );
}
