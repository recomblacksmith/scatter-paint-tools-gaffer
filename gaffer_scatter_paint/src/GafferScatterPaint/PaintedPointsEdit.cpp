#include "PaintedPointsPrivate.h"

std::uint64_t PaintedPoints::createLayer( const std::string &name )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	std::vector<std::string> existingNames;
	bp::list layers = bp::extract<bp::list>( store["layers"] );
	for( bp::stl_input_iterator<bp::object> it( layers ), end; it != end; ++it )
	{
		existingNames.push_back( dictValue<std::string>( bp::extract<bp::dict>( *it ), "name", "" ) );
	}
	const std::string layerName = name.empty() ? uniqueName( existingNames, "Layer" ) : name;
	bp::dict layer;
	layer["layerId"] = nextId( store, "layer" );
	layer["name"] = layerName;
	layer["order"] = bp::len( layers );
	layer["enabled"] = true;
	layer["visible"] = true;
	layer["mute"] = false;
	layer["solo"] = false;
	layer["timeVarying"] = false;
	layer["mode"] = 0;
	layer["frameStart"] = 0;
	layer["frameEnd"] = 0;
	layer["holdOutsideRange"] = true;
	layer["firstStrokeId"] = 0;
	layer["lastStrokeId"] = 0;
	layers.append( layer );
	reindexLayers( store );
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return bp::extract<std::uint64_t>( layer["layerId"] );
}

std::uint64_t PaintedPoints::ensureLayer( const std::string &name )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	bp::list layers = bp::extract<bp::list>( store["layers"] );
	for( bp::stl_input_iterator<bp::object> it( layers ), end; it != end; ++it )
	{
		bp::dict layer = bp::extract<bp::dict>( *it );
		if( dictValue<std::string>( layer, "name", "" ) == name )
		{
			return dictValue<std::uint64_t>( layer, "layerId", 0 );
		}
	}
	return createLayer( name );
}

std::uint64_t PaintedPoints::deleteLayer( const bp::object &layerIdentifier )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	ResolvedLayer resolved = resolveLayer( store, layerIdentifier );
	bp::list layers = bp::extract<bp::list>( store["layers"] );
	bp::list strokes = bp::extract<bp::list>( store["strokes"] );
	bp::list points = bp::extract<bp::list>( store["points"] );
	bp::list chunks = bp::extract<bp::list>( store["chunks"] );
	const std::uint64_t layerId = dictValue<std::uint64_t>( resolved.layer, "layerId", 0 );

	std::set<std::uint64_t> removedStrokeIds;
	bp::list rewrittenStrokes;
	for( bp::stl_input_iterator<bp::object> it( strokes ), end; it != end; ++it )
	{
		bp::dict stroke = bp::extract<bp::dict>( *it );
		if( dictValue<std::uint64_t>( stroke, "layerId", 0 ) == layerId )
		{
			removedStrokeIds.insert( dictValue<std::uint64_t>( stroke, "strokeId", 0 ) );
		}
		else
		{
			rewrittenStrokes.append( stroke );
		}
	}
	store["strokes"] = rewrittenStrokes;

	bp::list rewrittenPoints;
	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		bp::dict point = bp::extract<bp::dict>( *it );
		if( dictValue<std::uint64_t>( point, "layerId", 0 ) != layerId )
		{
			rewrittenPoints.append( point );
		}
	}
	store["points"] = rewrittenPoints;

	bp::list rewrittenChunks;
	for( bp::stl_input_iterator<bp::object> it( chunks ), end; it != end; ++it )
	{
		bp::dict chunk = bp::extract<bp::dict>( *it );
		if( !removedStrokeIds.count( dictValue<std::uint64_t>( chunk, "strokeId", 0 ) ) )
		{
			rewrittenChunks.append( chunk );
		}
	}
	store["chunks"] = rewrittenChunks;

	bp::list rewrittenLayers;
	const ssize_t layerCount = bp::len( layers );
	for( ssize_t i = 0; i < layerCount; ++i )
	{
		if( i != resolved.index )
		{
			rewrittenLayers.append( layers[i] );
		}
	}
	store["layers"] = rewrittenLayers;

	reindexLayers( store );
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return layerId;
}

std::uint64_t PaintedPoints::renameLayer( const bp::object &layerIdentifier, const std::string &newName )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	ResolvedLayer resolved = resolveLayer( store, layerIdentifier );
	resolved.layer["name"] = newName;
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return dictValue<std::uint64_t>( resolved.layer, "layerId", 0 );
}

std::uint64_t PaintedPoints::moveLayer( const bp::object &layerIdentifier, int newIndex )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	bp::list layers = bp::extract<bp::list>( store["layers"] );
	ResolvedLayer resolved = resolveLayer( store, layerIdentifier );
	std::vector<bp::object> orderedLayers;
	for( bp::stl_input_iterator<bp::object> it( layers ), end; it != end; ++it )
	{
		orderedLayers.push_back( *it );
	}

	bp::object movedLayer = orderedLayers[resolved.index];
	orderedLayers.erase( orderedLayers.begin() + resolved.index );
	const int clampedIndex = std::max( 0, std::min<int>( newIndex, static_cast<int>( orderedLayers.size() ) ) );
	orderedLayers.insert( orderedLayers.begin() + clampedIndex, movedLayer );

	bp::list rewrittenLayers;
	for( const bp::object &layer : orderedLayers )
	{
		rewrittenLayers.append( layer );
	}
	store["layers"] = rewrittenLayers;

	reindexLayers( store );
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return dictValue<std::uint64_t>( resolved.layer, "layerId", 0 );
}

std::uint64_t PaintedPoints::mergeLayers( const bp::object &sourceLayerIdentifier, const bp::object &destinationLayerIdentifier )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	ResolvedLayer source = resolveLayer( store, sourceLayerIdentifier );
	ResolvedLayer destination = resolveLayer( store, destinationLayerIdentifier );
	if( source.index == destination.index )
	{
		return dictValue<std::uint64_t>( destination.layer, "layerId", 0 );
	}

	const std::uint64_t sourceLayerId = dictValue<std::uint64_t>( source.layer, "layerId", 0 );
	const std::uint64_t destinationLayerId = dictValue<std::uint64_t>( destination.layer, "layerId", 0 );
	bp::list strokes = bp::extract<bp::list>( store["strokes"] );
	bp::list points = bp::extract<bp::list>( store["points"] );
	bp::list layers = bp::extract<bp::list>( store["layers"] );

	for( bp::stl_input_iterator<bp::object> it( strokes ), end; it != end; ++it )
	{
		bp::dict stroke = bp::extract<bp::dict>( *it );
		if( dictValue<std::uint64_t>( stroke, "layerId", 0 ) == sourceLayerId )
		{
			stroke["layerId"] = destinationLayerId;
		}
	}

	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		bp::dict point = bp::extract<bp::dict>( *it );
		if( dictValue<std::uint64_t>( point, "layerId", 0 ) == sourceLayerId )
		{
			point["layerId"] = destinationLayerId;
		}
	}

	bp::list rewrittenLayers;
	const ssize_t layerCount = bp::len( layers );
	for( ssize_t i = 0; i < layerCount; ++i )
	{
		if( i != source.index )
		{
			rewrittenLayers.append( layers[i] );
		}
	}
	store["layers"] = rewrittenLayers;

	reindexLayers( store );
	reindexStrokes( store, destinationLayerId );
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return destinationLayerId;
}

std::uint64_t PaintedPoints::setLayerVisible( const bp::object &layerIdentifier, bool visible )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	ResolvedLayer resolved = resolveLayer( store, layerIdentifier );
	resolved.layer["visible"] = visible;
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return dictValue<std::uint64_t>( resolved.layer, "layerId", 0 );
}

std::uint64_t PaintedPoints::setLayerMute( const bp::object &layerIdentifier, bool mute )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	ResolvedLayer resolved = resolveLayer( store, layerIdentifier );
	resolved.layer["mute"] = mute;
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return dictValue<std::uint64_t>( resolved.layer, "layerId", 0 );
}

std::uint64_t PaintedPoints::setLayerSolo( const bp::object &layerIdentifier, bool solo )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	ResolvedLayer resolved = resolveLayer( store, layerIdentifier );
	resolved.layer["solo"] = solo;
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return dictValue<std::uint64_t>( resolved.layer, "layerId", 0 );
}

std::uint64_t PaintedPoints::setLayerTimeRange( const bp::object &layerIdentifier, int frameStart, int frameEnd )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	ResolvedLayer resolved = resolveLayer( store, layerIdentifier );
	resolved.layer["frameStart"] = frameStart;
	resolved.layer["frameEnd"] = frameEnd;
	resolved.layer["timeVarying"] = !( frameStart == 0 && frameEnd == 0 );
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return dictValue<std::uint64_t>( resolved.layer, "layerId", 0 );
}

std::uint64_t PaintedPoints::createStroke( const bp::object &layerIdentifier, const std::string &name )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	ResolvedLayer resolved = resolveLayer( store, layerIdentifier );
	const std::uint64_t layerId = dictValue<std::uint64_t>( resolved.layer, "layerId", 0 );
	std::vector<std::string> existingNames;
	bp::list strokes = bp::extract<bp::list>( store["strokes"] );
	for( bp::stl_input_iterator<bp::object> it( strokes ), end; it != end; ++it )
	{
		bp::dict stroke = bp::extract<bp::dict>( *it );
		if( dictValue<std::uint64_t>( stroke, "layerId", 0 ) == layerId )
		{
			existingNames.push_back( dictValue<std::string>( stroke, "name", "" ) );
		}
	}
	const std::string strokeName = name.empty() ? uniqueName( existingNames, "Stroke" ) : name;
	bp::dict stroke;
	stroke["strokeId"] = nextId( store, "stroke" );
	stroke["layerId"] = layerId;
	stroke["name"] = strokeName;
	stroke["order"] = static_cast<int>( existingNames.size() );
	stroke["mode"] = 0;
	stroke["frameStart"] = 0;
	stroke["frameEnd"] = 0;
	stroke["createdTimeUnixMicros"] = unixTimeMicros();
	stroke["firstChunkId"] = 0;
	stroke["lastChunkId"] = 0;
	stroke["pointCount"] = 0;
	stroke["targetCount"] = 0;
	stroke["selectionMaskId"] = 0;
	strokes.append( stroke );
	reindexStrokes( store, layerId );
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return bp::extract<std::uint64_t>( stroke["strokeId"] );
}

std::uint64_t PaintedPoints::ensureStroke( const bp::object &layerIdentifier, const std::string &name )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	ResolvedLayer resolved = resolveLayer( store, layerIdentifier );
	const std::uint64_t layerId = dictValue<std::uint64_t>( resolved.layer, "layerId", 0 );
	bp::list strokes = bp::extract<bp::list>( store["strokes"] );
	for( bp::stl_input_iterator<bp::object> it( strokes ), end; it != end; ++it )
	{
		bp::dict stroke = bp::extract<bp::dict>( *it );
		if( dictValue<std::uint64_t>( stroke, "layerId", 0 ) == layerId && dictValue<std::string>( stroke, "name", "" ) == name )
		{
			return dictValue<std::uint64_t>( stroke, "strokeId", 0 );
		}
	}
	return createStroke( resolved.layer["layerId"], name );
}

std::uint64_t PaintedPoints::deleteStroke( const bp::object &strokeIdentifier )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	ResolvedStroke resolved = resolveStroke( store, strokeIdentifier );
	bp::list strokes = bp::extract<bp::list>( store["strokes"] );
	bp::list points = bp::extract<bp::list>( store["points"] );
	bp::list chunks = bp::extract<bp::list>( store["chunks"] );
	const std::uint64_t strokeId = dictValue<std::uint64_t>( resolved.stroke, "strokeId", 0 );
	const std::uint64_t layerId = dictValue<std::uint64_t>( resolved.layer, "layerId", 0 );

	bp::list rewrittenStrokes;
	const ssize_t strokeCount = bp::len( strokes );
	for( ssize_t i = 0; i < strokeCount; ++i )
	{
		if( i != resolved.strokeIndex )
		{
			rewrittenStrokes.append( strokes[i] );
		}
	}
	store["strokes"] = rewrittenStrokes;

	bp::list rewrittenPoints;
	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		bp::dict point = bp::extract<bp::dict>( *it );
		if( dictValue<std::uint64_t>( point, "strokeId", 0 ) != strokeId )
		{
			rewrittenPoints.append( point );
		}
	}
	store["points"] = rewrittenPoints;

	bp::list rewrittenChunks;
	for( bp::stl_input_iterator<bp::object> it( chunks ), end; it != end; ++it )
	{
		bp::dict chunk = bp::extract<bp::dict>( *it );
		if( dictValue<std::uint64_t>( chunk, "strokeId", 0 ) != strokeId )
		{
			rewrittenChunks.append( chunk );
		}
	}
	store["chunks"] = rewrittenChunks;

	reindexStrokes( store, layerId );
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return strokeId;
}

std::uint64_t PaintedPoints::renameStroke( const bp::object &strokeIdentifier, const std::string &newName )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	ResolvedStroke resolved = resolveStroke( store, strokeIdentifier );
	resolved.stroke["name"] = newName;
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return dictValue<std::uint64_t>( resolved.stroke, "strokeId", 0 );
}

std::uint64_t PaintedPoints::moveStroke( const bp::object &strokeIdentifier, int newIndex )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	ResolvedStroke resolved = resolveStroke( store, strokeIdentifier );
	bp::list strokes = bp::extract<bp::list>( store["strokes"] );
	const std::uint64_t layerId = dictValue<std::uint64_t>( resolved.layer, "layerId", 0 );

	std::vector<bp::dict> layerStrokes;
	for( bp::stl_input_iterator<bp::object> it( strokes ), end; it != end; ++it )
	{
		bp::dict stroke = bp::extract<bp::dict>( *it );
		if( dictValue<std::uint64_t>( stroke, "layerId", 0 ) == layerId )
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

	const auto existingIt = std::find_if(
		layerStrokes.begin(), layerStrokes.end(),
		[&]( const bp::dict &stroke ) {
			return dictValue<std::uint64_t>( stroke, "strokeId", 0 ) == dictValue<std::uint64_t>( resolved.stroke, "strokeId", 0 );
		}
	);
	if( existingIt == layerStrokes.end() )
	{
		throw std::runtime_error( "Unknown stroke" );
	}

	bp::dict movedStroke = *existingIt;
	layerStrokes.erase( existingIt );
	const int clampedIndex = std::max( 0, std::min<int>( newIndex, static_cast<int>( layerStrokes.size() ) ) );
	layerStrokes.insert( layerStrokes.begin() + clampedIndex, movedStroke );
	for( size_t i = 0; i < layerStrokes.size(); ++i )
	{
		layerStrokes[i]["order"] = static_cast<int>( i );
	}

	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return dictValue<std::uint64_t>( resolved.stroke, "strokeId", 0 );
}

std::uint64_t PaintedPoints::mergeStrokes( const bp::object &sourceStrokeIdentifier, const bp::object &destinationStrokeIdentifier )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	ResolvedStroke source = resolveStroke( store, sourceStrokeIdentifier );
	ResolvedStroke destination = resolveStroke( store, destinationStrokeIdentifier );
	if( source.strokeIndex == destination.strokeIndex )
	{
		return dictValue<std::uint64_t>( destination.stroke, "strokeId", 0 );
	}

	const std::uint64_t sourceStrokeId = dictValue<std::uint64_t>( source.stroke, "strokeId", 0 );
	const std::uint64_t sourceLayerId = dictValue<std::uint64_t>( source.layer, "layerId", 0 );
	const std::uint64_t destinationStrokeId = dictValue<std::uint64_t>( destination.stroke, "strokeId", 0 );
	const std::uint64_t destinationLayerId = dictValue<std::uint64_t>( destination.layer, "layerId", 0 );
	bp::list points = bp::extract<bp::list>( store["points"] );
	bp::list chunks = bp::extract<bp::list>( store["chunks"] );
	bp::list strokes = bp::extract<bp::list>( store["strokes"] );

	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		bp::dict point = bp::extract<bp::dict>( *it );
		if( dictValue<std::uint64_t>( point, "strokeId", 0 ) == sourceStrokeId )
		{
			point["strokeId"] = destinationStrokeId;
			point["layerId"] = destinationLayerId;
		}
	}

	bp::list rewrittenStrokes;
	const ssize_t strokeCount = bp::len( strokes );
	for( ssize_t i = 0; i < strokeCount; ++i )
	{
		if( i != source.strokeIndex )
		{
			rewrittenStrokes.append( strokes[i] );
		}
	}
	store["strokes"] = rewrittenStrokes;

	bp::list rewrittenChunks;
	for( bp::stl_input_iterator<bp::object> it( chunks ), end; it != end; ++it )
	{
		bp::dict chunk = bp::extract<bp::dict>( *it );
		if( dictValue<std::uint64_t>( chunk, "strokeId", 0 ) != sourceStrokeId )
		{
			rewrittenChunks.append( chunk );
		}
	}
	store["chunks"] = rewrittenChunks;

	reindexStrokes( store, sourceLayerId );
	if( sourceLayerId != destinationLayerId )
	{
		reindexStrokes( store, destinationLayerId );
	}
	refreshDerivedData( store, { destinationStrokeId } );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return destinationStrokeId;
}

size_t PaintedPoints::paintStrokeCommit( const bp::object &strokeIdentifier, const bp::object &points, bool append )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	ResolvedStroke resolved = resolveStroke( store, strokeIdentifier );
	bp::list storePoints = bp::extract<bp::list>( store["points"] );
	bp::list authoredPoints;
	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		authoredPoints.append( defaultPointRecord( store, resolved.layer, resolved.stroke, bp::extract<bp::dict>( *it ) ) );
	}

	const std::uint64_t strokeId = dictValue<std::uint64_t>( resolved.stroke, "strokeId", 0 );
	if( !append )
	{
		bp::list remaining;
		for( bp::stl_input_iterator<bp::object> it( storePoints ), end; it != end; ++it )
		{
			bp::dict point = bp::extract<bp::dict>( *it );
			if( dictValue<std::uint64_t>( point, "strokeId", 0 ) != strokeId )
			{
				remaining.append( point );
			}
		}
		store["points"] = remaining;
		storePoints = remaining;
	}
	for( bp::stl_input_iterator<bp::object> it( authoredPoints ), end; it != end; ++it )
	{
		storePoints.append( *it );
	}
	refreshDerivedData( store, { strokeId } );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return static_cast<size_t>( bp::len( authoredPoints ) );
}

size_t PaintedPoints::eraseCommit( const bp::object &strokeIdentifier, const bp::object &pointIds, const bp::object &fraction )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	ResolvedStroke resolved = resolveStroke( store, strokeIdentifier );
	const std::uint64_t strokeId = dictValue<std::uint64_t>( resolved.stroke, "strokeId", 0 );
	bp::list existingPoints = strokePoints( store, strokeId );
	bp::list remainingPoints;

	if( !isNone( pointIds ) )
	{
		std::set<std::uint64_t> pointIdSet;
		for( bp::stl_input_iterator<bp::object> it( pointIds ), end; it != end; ++it )
		{
			pointIdSet.insert( extractOr<std::uint64_t>( *it, 0 ) );
		}
		for( bp::stl_input_iterator<bp::object> it( existingPoints ), end; it != end; ++it )
		{
			bp::dict point = bp::extract<bp::dict>( *it );
			if( !pointIdSet.count( dictValue<std::uint64_t>( point, "pointId", 0 ) ) )
			{
				remainingPoints.append( point );
			}
		}
	}
	else if( !isNone( fraction ) )
	{
		const ssize_t pointCount = bp::len( existingPoints );
		const float fractionValue = extractOr<float>( fraction, 0.0f );
		const ssize_t removeCount = std::max<ssize_t>( 0, std::min<ssize_t>( pointCount, static_cast<ssize_t>( std::llround( pointCount * fractionValue ) ) ) );
		for( ssize_t i = removeCount; i < pointCount; ++i )
		{
			remainingPoints.append( existingPoints[i] );
		}
	}

	bp::list allPoints = bp::extract<bp::list>( store["points"] );
	bp::list rewritten;
	for( bp::stl_input_iterator<bp::object> it( allPoints ), end; it != end; ++it )
	{
		bp::dict point = bp::extract<bp::dict>( *it );
		if( dictValue<std::uint64_t>( point, "strokeId", 0 ) != strokeId )
		{
			rewritten.append( point );
		}
	}
	for( bp::stl_input_iterator<bp::object> it( remainingPoints ), end; it != end; ++it )
	{
		rewritten.append( *it );
	}
	store["points"] = rewritten;
	std::set<std::uint64_t> validPointIds;
	for( bp::stl_input_iterator<bp::object> it( rewritten ), end; it != end; ++it )
	{
		validPointIds.insert( dictValue<std::uint64_t>( bp::extract<bp::dict>( *it ), "pointId", 0 ) );
	}
	std::set<std::uint64_t> validStrokeIds;
	bp::list strokes = bp::extract<bp::list>( store["strokes"] );
	for( bp::stl_input_iterator<bp::object> it( strokes ), end; it != end; ++it )
	{
		validStrokeIds.insert( dictValue<std::uint64_t>( bp::extract<bp::dict>( *it ), "strokeId", 0 ) );
	}
	pruneSelectionReferences( store, validPointIds, validStrokeIds );
	refreshDerivedData( store, { strokeId } );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return static_cast<size_t>( bp::len( existingPoints ) - bp::len( remainingPoints ) );
}

bp::object PaintedPoints::lastStrokeId() const
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	bp::list strokes = bp::extract<bp::list>( store["strokes"] );
	if( bp::len( strokes ) == 0 )
	{
		return bp::object();
	}
	std::vector<bp::dict> ordered;
	for( bp::stl_input_iterator<bp::object> it( strokes ), end; it != end; ++it )
	{
		ordered.push_back( bp::extract<bp::dict>( *it ) );
	}
	std::sort(
		ordered.begin(), ordered.end(),
		[]( const bp::dict &a, const bp::dict &b ) {
			const auto keyA = std::make_pair( dictValue<std::uint64_t>( a, "layerId", 0 ), dictValue<int>( a, "order", 0 ) );
			const auto keyB = std::make_pair( dictValue<std::uint64_t>( b, "layerId", 0 ), dictValue<int>( b, "order", 0 ) );
			return keyA < keyB;
		}
	);
	return ordered.back()["strokeId"];
}

size_t PaintedPoints::mutatePoints( const bp::object &mutator )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	bp::list points = bp::extract<bp::list>( store["points"] );
	std::set<std::uint64_t> changedStrokeIds;
	size_t updatedCount = 0;
	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		bp::dict point = bp::extract<bp::dict>( *it );
		bp::dict expanded = expandedPointRecord( store, point );
		if( bp::extract<bool>( mutator( expanded ) ) )
		{
			applyExpandedPointEdit( store, point, expanded );
			changedStrokeIds.insert( dictValue<std::uint64_t>( point, "strokeId", 0 ) );
			updatedCount += 1;
		}
	}
	refreshDerivedData( store, changedStrokeIds );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return updatedCount;
}

std::string PaintedPoints::splitStrokeBySelection()
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	const std::set<std::uint64_t> selectedIds = selectedPointIds( store );
	if( selectedIds.empty() )
	{
		return std::string();
	}

	bp::list points = bp::extract<bp::list>( store["points"] );
	bp::list strokes = bp::extract<bp::list>( store["strokes"] );

	std::map<std::uint64_t, std::vector<bp::dict>> pointsByStroke;
	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		bp::dict point = bp::extract<bp::dict>( *it );
		pointsByStroke[dictValue<std::uint64_t>( point, "strokeId", 0 )].push_back( point );
	}

	std::set<std::uint64_t> affectedStrokeIds;
	for( const auto &entry : pointsByStroke )
	{
		for( const bp::dict &point : entry.second )
		{
			if( selectedIds.count( dictValue<std::uint64_t>( point, "pointId", 0 ) ) )
			{
				affectedStrokeIds.insert( entry.first );
				break;
			}
		}
	}

	if( affectedStrokeIds.empty() )
	{
		return std::string();
	}

	bp::list rewrittenStrokes;
	bp::list rewrittenPoints;
	std::set<std::uint64_t> affectedLayerIds;
	std::set<std::uint64_t> changedStrokeIds;
	std::set<std::uint64_t> removedPointIds;
	std::size_t removedPointCount = 0;
	std::size_t createdStrokeCount = 0;

	for( bp::stl_input_iterator<bp::object> it( strokes ), end; it != end; ++it )
	{
		bp::dict stroke = bp::extract<bp::dict>( *it );
		const std::uint64_t strokeId = dictValue<std::uint64_t>( stroke, "strokeId", 0 );
		const std::uint64_t layerId = dictValue<std::uint64_t>( stroke, "layerId", 0 );
		const auto pointsIt = pointsByStroke.find( strokeId );
		const std::vector<bp::dict> strokePointVector = pointsIt != pointsByStroke.end() ? pointsIt->second : std::vector<bp::dict>();

		if( !affectedStrokeIds.count( strokeId ) )
		{
			rewrittenStrokes.append( stroke );
			for( const bp::dict &point : strokePointVector )
			{
				rewrittenPoints.append( point );
			}
			continue;
		}

		affectedLayerIds.insert( layerId );

		std::vector<std::vector<bp::dict>> survivingSegments;
		std::vector<bp::dict> currentSegment;
		for( const bp::dict &point : strokePointVector )
		{
			const std::uint64_t pointId = dictValue<std::uint64_t>( point, "pointId", 0 );
			if( selectedIds.count( pointId ) )
			{
				removedPointIds.insert( pointId );
				removedPointCount += 1;
				if( !currentSegment.empty() )
				{
					survivingSegments.push_back( currentSegment );
					currentSegment.clear();
				}
				continue;
			}

			currentSegment.push_back( point );
		}
		if( !currentSegment.empty() )
		{
			survivingSegments.push_back( currentSegment );
		}

		std::vector<std::string> existingNames;
		for( bp::stl_input_iterator<bp::object> rewrittenIt( rewrittenStrokes ), rewrittenEnd; rewrittenIt != rewrittenEnd; ++rewrittenIt )
		{
			bp::dict existingStroke = bp::extract<bp::dict>( *rewrittenIt );
			if( dictValue<std::uint64_t>( existingStroke, "layerId", 0 ) == layerId )
			{
				existingNames.push_back( dictValue<std::string>( existingStroke, "name", "" ) );
			}
		}

		for( std::size_t segmentIndex = 0; segmentIndex < survivingSegments.size(); ++segmentIndex )
		{
			bp::dict newStroke = deepCopyDict( stroke );
			const std::uint64_t newStrokeId = nextId( store, "stroke" );
			newStroke["strokeId"] = newStrokeId;
			newStroke["name"] = uniqueName( existingNames, dictValue<std::string>( stroke, "name", "Stroke" ) );
			newStroke["order"] = dictValue<int>( stroke, "order", 0 ) + static_cast<int>( segmentIndex );
			newStroke["firstChunkId"] = 0;
			newStroke["lastChunkId"] = 0;
			newStroke["pointCount"] = 0;
			newStroke["targetCount"] = 0;
			rewrittenStrokes.append( newStroke );
			existingNames.push_back( dictValue<std::string>( newStroke, "name", "" ) );
			changedStrokeIds.insert( newStrokeId );
			createdStrokeCount += 1;

			for( const bp::dict &segmentPoint : survivingSegments[segmentIndex] )
			{
				bp::dict newPoint = deepCopyDict( segmentPoint );
				newPoint["strokeId"] = newStrokeId;
				newPoint["layerId"] = layerId;
				rewrittenPoints.append( newPoint );
			}
		}
	}

	store["strokes"] = rewrittenStrokes;
	store["points"] = rewrittenPoints;
	for( const std::uint64_t layerId : affectedLayerIds )
	{
		reindexStrokes( store, layerId );
	}

	std::set<std::uint64_t> validPointIds;
	for( bp::stl_input_iterator<bp::object> it( rewrittenPoints ), end; it != end; ++it )
	{
		validPointIds.insert( dictValue<std::uint64_t>( bp::extract<bp::dict>( *it ), "pointId", 0 ) );
	}
	std::set<std::uint64_t> validStrokeIds;
	for( bp::stl_input_iterator<bp::object> it( rewrittenStrokes ), end; it != end; ++it )
	{
		validStrokeIds.insert( dictValue<std::uint64_t>( bp::extract<bp::dict>( *it ), "strokeId", 0 ) );
	}
	pruneSelectionReferences( store, validPointIds, validStrokeIds );

	refreshDerivedData( store, changedStrokeIds );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );

	return
		"Removed " + std::to_string( removedPointCount ) + " selected points from " +
		std::to_string( affectedStrokeIds.size() ) + " stroke(s), created " +
		std::to_string( createdStrokeCount ) + " surviving split stroke(s)";
}

std::uint64_t PaintedPoints::createSelectionSet( const std::string &name )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	std::vector<std::string> existingNames;
	bp::list selectionSets = bp::extract<bp::list>( store["selectionSets"] );
	for( bp::stl_input_iterator<bp::object> it( selectionSets ), end; it != end; ++it )
	{
		existingNames.push_back( dictValue<std::string>( bp::extract<bp::dict>( *it ), "name", "" ) );
	}
	const std::string selectionName = name.empty() ? uniqueName( existingNames, "Selection" ) : name;
	bp::dict selectionSet;
	selectionSet["selectionSetId"] = nextId( store, "selectionSet" );
	selectionSet["name"] = selectionName;
	selectionSet["pointIds"] = bp::list();
	selectionSet["strokeIds"] = bp::list();
	selectionSets.append( selectionSet );
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return bp::extract<std::uint64_t>( selectionSet["selectionSetId"] );
}

std::uint64_t PaintedPoints::renameSelectionSet( const bp::object &selectionIdentifier, const std::string &newName )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	ResolvedSelectionSet resolved = resolveSelectionSet( store, selectionIdentifier );
	resolved.selectionSet["name"] = newName;
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return dictValue<std::uint64_t>( resolved.selectionSet, "selectionSetId", 0 );
}

std::uint64_t PaintedPoints::deleteSelectionSet( const bp::object &selectionIdentifier )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	bp::list selectionSets = bp::extract<bp::list>( store["selectionSets"] );
	ResolvedSelectionSet resolved = resolveSelectionSet( store, selectionIdentifier );
	const std::uint64_t selectionSetId = dictValue<std::uint64_t>( resolved.selectionSet, "selectionSetId", 0 );
	const ssize_t size = bp::len( selectionSets );
	bp::list rewritten;
	for( ssize_t i = 0; i < size; ++i )
	{
		if( i != resolved.index )
		{
			rewritten.append( selectionSets[i] );
		}
	}
	store["selectionSets"] = rewritten;
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return selectionSetId;
}

bp::object PaintedPoints::storeCurrentSelection( const bp::object &selectionIdentifier, const bp::object &name )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	bp::dict currentSelection = bp::extract<bp::dict>( store["currentSelection"] );
	bp::list selectionSets = bp::extract<bp::list>( store["selectionSets"] );

	bp::dict selectionSet;
	if( isNone( selectionIdentifier ) )
	{
		std::vector<std::string> existingNames;
		for( bp::stl_input_iterator<bp::object> it( selectionSets ), end; it != end; ++it )
		{
			existingNames.push_back( dictValue<std::string>( bp::extract<bp::dict>( *it ), "name", "" ) );
		}
		const std::string selectionName = isNone( name ) ? uniqueName( existingNames, "Selection" ) : extractOr<std::string>( name, "" );
		selectionSet["selectionSetId"] = nextId( store, "selectionSet" );
		selectionSet["name"] = selectionName;
		selectionSet["pointIds"] = integerList( dictGet( currentSelection, "pointIds" ) );
		selectionSet["strokeIds"] = integerList( dictGet( currentSelection, "strokeIds" ) );
		selectionSets.append( selectionSet );
	}
	else
	{
		ResolvedSelectionSet resolved = resolveSelectionSet( store, selectionIdentifier );
		selectionSet = resolved.selectionSet;
		selectionSet["pointIds"] = integerList( dictGet( currentSelection, "pointIds" ) );
		selectionSet["strokeIds"] = integerList( dictGet( currentSelection, "strokeIds" ) );
		if( !isNone( name ) )
		{
			selectionSet["name"] = extractOr<std::string>( name, dictValue<std::string>( selectionSet, "name", "" ) );
		}
	}

	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return deepCopyDict( selectionSet );
}

bp::object PaintedPoints::setCurrentSelection( const bp::object &pointIds, const bp::object &strokeIds )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	bp::dict currentSelection;
	currentSelection["pointIds"] = integerList( pointIds );
	currentSelection["strokeIds"] = integerList( strokeIds );
	store["currentSelection"] = currentSelection;
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return deepCopyDict( currentSelection );
}
