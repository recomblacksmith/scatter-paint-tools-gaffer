#pragma once

struct ResolvedLayer
{
	ssize_t index;
	bp::dict layer;
};

struct ResolvedStroke
{
	ssize_t layerIndex;
	ssize_t strokeIndex;
	bp::dict layer;
	bp::dict stroke;
};

struct ResolvedSelectionSet
{
	ssize_t index;
	bp::dict selectionSet;
};

inline std::string uniqueName( const std::vector<std::string> &existingNames, const std::string &prefix )
{
	std::set<std::string> existing( existingNames.begin(), existingNames.end() );
	for( size_t index = 1; ; ++index )
	{
		const std::string candidate = prefix + " " + std::to_string( index );
		if( !existing.count( candidate ) )
		{
			return candidate;
		}
	}
}

inline ResolvedSelectionSet resolveSelectionSet( const bp::dict &store, const bp::object &identifier )
{
	bp::list selectionSets = bp::extract<bp::list>( store["selectionSets"] );
	const bool hasInt = bp::extract<std::uint64_t>( identifier ).check();
	const std::uint64_t idValue = hasInt ? bp::extract<std::uint64_t>( identifier )() : 0;
	const bool hasString = bp::extract<std::string>( identifier ).check();
	const std::string nameValue = hasString ? bp::extract<std::string>( identifier )() : std::string();
	const ssize_t size = bp::len( selectionSets );
	for( ssize_t i = 0; i < size; ++i )
	{
		bp::dict selectionSet = bp::extract<bp::dict>( selectionSets[i] );
		if( ( hasInt && dictValue<std::uint64_t>( selectionSet, "selectionSetId", 0 ) == idValue ) || ( hasString && dictValue<std::string>( selectionSet, "name", "" ) == nameValue ) )
		{
			return { i, selectionSet };
		}
	}
	throw std::runtime_error( "Unknown selection set" );
}

inline void reindexLayers( bp::dict store )
{
	bp::list layers = bp::extract<bp::list>( store["layers"] );
	const ssize_t size = bp::len( layers );
	for( ssize_t i = 0; i < size; ++i )
	{
		bp::dict layer = bp::extract<bp::dict>( layers[i] );
		layer["order"] = static_cast<int>( i );
	}
}

inline ResolvedLayer resolveLayer( const bp::dict &store, const bp::object &identifier )
{
	bp::list layers = bp::extract<bp::list>( store["layers"] );
	const bool hasInt = bp::extract<std::uint64_t>( identifier ).check();
	const std::uint64_t idValue = hasInt ? bp::extract<std::uint64_t>( identifier )() : 0;
	const bool hasString = bp::extract<std::string>( identifier ).check();
	const std::string nameValue = hasString ? bp::extract<std::string>( identifier )() : std::string();
	const ssize_t size = bp::len( layers );
	for( ssize_t i = 0; i < size; ++i )
	{
		bp::dict layer = bp::extract<bp::dict>( layers[i] );
		if( ( hasInt && dictValue<std::uint64_t>( layer, "layerId", 0 ) == idValue ) || ( hasString && dictValue<std::string>( layer, "name", "" ) == nameValue ) )
		{
			return { i, layer };
		}
	}
	throw std::runtime_error( "Unknown layer" );
}

inline void reindexStrokes( bp::dict store, std::uint64_t layerId )
{
	bp::list strokes = bp::extract<bp::list>( store["strokes"] );
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
	for( size_t i = 0; i < layerStrokes.size(); ++i )
	{
		layerStrokes[i]["order"] = static_cast<int>( i );
	}
}

inline ResolvedStroke resolveStroke( const bp::dict &store, const bp::object &identifier )
{
	bp::list strokes = bp::extract<bp::list>( store["strokes"] );
	const bool hasInt = bp::extract<std::uint64_t>( identifier ).check();
	const std::uint64_t idValue = hasInt ? bp::extract<std::uint64_t>( identifier )() : 0;
	const bool hasString = bp::extract<std::string>( identifier ).check();
	const std::string nameValue = hasString ? bp::extract<std::string>( identifier )() : std::string();
	const ssize_t size = bp::len( strokes );
	for( ssize_t i = 0; i < size; ++i )
	{
		bp::dict stroke = bp::extract<bp::dict>( strokes[i] );
		if( ( hasInt && dictValue<std::uint64_t>( stroke, "strokeId", 0 ) == idValue ) || ( hasString && dictValue<std::string>( stroke, "name", "" ) == nameValue ) )
		{
			ResolvedLayer layer = resolveLayer( store, stroke["layerId"] );
			return { layer.index, i, layer.layer, stroke };
		}
	}
	throw std::runtime_error( "Unknown stroke" );
}

inline std::uint32_t internPath( bp::dict store, const char *key, const std::string &path )
{
	if( path.empty() )
	{
		return 0;
	}
	bp::list values = bp::extract<bp::list>( store[key] );
	const ssize_t size = bp::len( values );
	for( ssize_t i = 0; i < size; ++i )
	{
		if( extractOr<std::string>( values[i], "" ) == path )
		{
			return static_cast<std::uint32_t>( i + 1 );
		}
	}
	values.append( path );
	return static_cast<std::uint32_t>( bp::len( values ) );
}

inline std::string pathFromId( const bp::dict &store, const char *key, std::uint32_t pathId )
{
	if( !pathId )
	{
		return std::string();
	}
	bp::list values = bp::extract<bp::list>( store[key] );
	const ssize_t index = static_cast<ssize_t>( pathId ) - 1;
	if( index < 0 || index >= bp::len( values ) )
	{
		return std::string();
	}
	return extractOr<std::string>( values[index], "" );
}

inline bp::list strokePoints( const bp::dict &store, std::uint64_t strokeId )
{
	bp::list result;
	bp::list points = bp::extract<bp::list>( store["points"] );
	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		bp::dict point = bp::extract<bp::dict>( *it );
		if( dictValue<std::uint64_t>( point, "strokeId", 0 ) == strokeId )
		{
			result.append( point );
		}
	}
	return result;
}

inline bp::list colorList( const std::vector<float> &color )
{
	bp::list result;
	for( float value : color )
	{
		result.append( value );
	}
	return result;
}

inline std::vector<float> nodeDefaultColor( const bp::dict &store )
{
	if( !store.has_key( "node" ) )
	{
		return { 1.0f, 1.0f, 1.0f };
	}
	return floatVector( dictGet( bp::extract<bp::dict>( store["node"] ), "defaultColor" ), 3, { 1.0f, 1.0f, 1.0f } );
}

inline bool colorEqual( const std::vector<float> &a, const std::vector<float> &b, float tolerance = 1e-6f )
{
	if( a.size() != 3 || b.size() != 3 )
	{
		return false;
	}
	return
		std::abs( a[0] - b[0] ) <= tolerance &&
		std::abs( a[1] - b[1] ) <= tolerance &&
		std::abs( a[2] - b[2] ) <= tolerance;
}

inline std::vector<float> overrideColor( const bp::dict &record )
{
	if( !dictValue<bool>( record, "colorEnabled", false ) )
	{
		return {};
	}
	return floatVector( dictGet( record, "color" ), 3, { 1.0f, 1.0f, 1.0f } );
}

inline std::vector<float> resolvedAuthoredColor(
	const bp::dict &store,
	const bp::dict *layer,
	const bp::dict *stroke,
	const bp::dict *point
)
{
	if( point )
	{
		const std::vector<float> pointColor = overrideColor( *point );
		if( pointColor.size() == 3 )
		{
			return pointColor;
		}
	}
	if( stroke )
	{
		const std::vector<float> strokeColor = overrideColor( *stroke );
		if( strokeColor.size() == 3 )
		{
			return strokeColor;
		}
	}
	if( layer )
	{
		const std::vector<float> layerColor = overrideColor( *layer );
		if( layerColor.size() == 3 )
		{
			return layerColor;
		}
	}
	return nodeDefaultColor( store );
}

inline bp::object findLayerRecord( const bp::dict &store, std::uint64_t layerId )
{
	if( !store.has_key( "layers" ) )
	{
		return bp::object();
	}
	bp::list layers = bp::extract<bp::list>( store["layers"] );
	for( bp::stl_input_iterator<bp::object> it( layers ), end; it != end; ++it )
	{
		bp::dict layer = bp::extract<bp::dict>( *it );
		if( dictValue<std::uint64_t>( layer, "layerId", 0 ) == layerId )
		{
			return layer;
		}
	}
	return bp::object();
}

inline bp::object findStrokeRecord( const bp::dict &store, std::uint64_t strokeId )
{
	if( !store.has_key( "strokes" ) )
	{
		return bp::object();
	}
	bp::list strokes = bp::extract<bp::list>( store["strokes"] );
	for( bp::stl_input_iterator<bp::object> it( strokes ), end; it != end; ++it )
	{
		bp::dict stroke = bp::extract<bp::dict>( *it );
		if( dictValue<std::uint64_t>( stroke, "strokeId", 0 ) == strokeId )
		{
			return stroke;
		}
	}
	return bp::object();
}

inline bp::dict defaultPointRecord( bp::dict store, const bp::dict &layer, const bp::dict &stroke, const bp::dict &pointData )
{
	const std::vector<float> inheritedColor = resolvedAuthoredColor( store, &layer, &stroke, nullptr );
	const std::vector<float> pointColor = floatVector( dictGet( pointData, "color" ), 3, inheritedColor );
	const bool colorEnabled = dictValue<bool>( pointData, "colorEnabled", pointData.has_key( "color" ) );
	const std::vector<float> barycentric = floatVector( dictGet( pointData, "barycentric" ), 3, { 1.0f, 0.0f, 0.0f } );
	const std::vector<float> worldP = floatVector( pointData.has_key( "P" ) ? pointData["P"] : dictGet( pointData, "restWorldP" ), 3, { 0.0f, 0.0f, 0.0f } );
	const std::vector<float> objectP = floatVector( pointData.has_key( "restObjectP" ) ? pointData["restObjectP"] : bp::object( bp::list( bp::tuple( boost::python::make_tuple( worldP[0], worldP[1], worldP[2] ) ) ) ), 3, worldP );
	const std::vector<float> restUV = floatVector( dictGet( pointData, "restUV" ), 2, { 0.0f, 0.0f } );
	const std::vector<float> normal = floatVector( pointData.has_key( "N" ) ? pointData["N"] : dictGet( pointData, "restNormal" ), 3, { 0.0f, 1.0f, 0.0f } );
	const std::vector<float> up = floatVector( pointData.has_key( "up" ) ? pointData["up"] : dictGet( pointData, "restUp" ), 3, { 0.0f, 0.0f, 1.0f } );
	const std::vector<float> tangentRotation = floatVector( dictGet( pointData, "tangentRotation" ), 2, { 0.0f, 0.0f } );

	bp::list baryList;
	bp::list objectList;
	bp::list worldList;
	bp::list uvList;
	bp::list normalList;
	bp::list upList;
	bp::list tangentList;
	for( float value : barycentric ) baryList.append( value );
	for( float value : objectP ) objectList.append( value );
	for( float value : worldP ) worldList.append( value );
	for( float value : restUV ) uvList.append( value );
	for( float value : normal ) normalList.append( value );
	for( float value : up ) upList.append( value );
	for( float value : tangentRotation ) tangentList.append( value );

	bp::dict point;
	point["pointId"] = pointData.has_key( "pointId" ) ? dictGet( pointData, "pointId" ) : bp::object( nextId( store, "point" ) );
	point["strokeId"] = dictGet( stroke, "strokeId" );
	point["layerId"] = dictGet( layer, "layerId" );
	point["targetPathId"] = internPath( store, "scenePaths", dictValue<std::string>( pointData, "sourcePath", "" ) );
	point["instanceId"] = dictValue<std::uint32_t>( pointData, "instanceId", 0 );
	point["instanceSourcePathId"] = internPath( store, "instanceSourcePaths", dictValue<std::string>( pointData, "instanceSourcePath", "" ) );
	point["triangleIndex"] = dictValue<std::uint32_t>( pointData, "triangleIndex", 0 );
	point["barycentric"] = baryList;
	point["restObjectP"] = objectList;
	point["restWorldP"] = worldList;
	point["restUV"] = uvList;
	point["restNormal"] = normalList;
	point["restUp"] = upList;
	point["width"] = dictValue<float>( pointData, "width", 1.0f );
	point["uniformScale"] = pointData.has_key( "scale" ) ? dictGet( pointData, "scale" ) : bp::object( dictValue<float>( pointData, "uniformScale", 1.0f ) );
	point["seed"] = dictValue<std::uint32_t>( pointData, "seed", 0 );
	point["normalSpin"] = dictValue<float>( pointData, "normalSpin", 0.0f );
	point["tangentRotation"] = tangentList;
	point["pressureDensity"] = dictValue<float>( pointData, "pressureDensity", 1.0f );
	point["pressureSoftness"] = dictValue<float>( pointData, "pressureSoftness", 1.0f );
	point["valid"] = dictValue<bool>( pointData, "valid", true );
	point["lastValidFrame"] = dictValue<int>( pointData, "lastValidFrame", 0 );
	point["anchorModeUsed"] = dictValue<bool>( pointData, "attachmentResolved", false ) ? 0 : 3;
	point["topologyGeneration"] = dictValue<std::uint32_t>( pointData, "topologyGeneration", 0 );
	point["colorEnabled"] = colorEnabled;
	point["color"] = colorList( pointColor );
	return point;
}

inline bp::dict expandedPointRecord( const bp::dict &store, const bp::dict &point )
{
	bp::object copyModule = bp::import( "copy" );
	bp::dict record = bp::extract<bp::dict>( copyModule.attr( "deepcopy" )( point ) );
	bp::object layerObject = findLayerRecord( store, dictValue<std::uint64_t>( point, "layerId", 0 ) );
	bp::object strokeObject = findStrokeRecord( store, dictValue<std::uint64_t>( point, "strokeId", 0 ) );
	bp::dict layer = bp::extract<bp::dict>( layerObject ) .check() ? bp::extract<bp::dict>( layerObject )() : bp::dict();
	bp::dict stroke = bp::extract<bp::dict>( strokeObject ) .check() ? bp::extract<bp::dict>( strokeObject )() : bp::dict();
	const std::uint64_t pointId = dictValue<std::uint64_t>( point, "pointId", 0 );
	if( pointId && store.has_key( "pointBackups" ) )
	{
		bp::dict pointBackups = bp::extract<bp::dict>( store["pointBackups"] );
		bp::object backupObject;
		if( pointBackups.has_key( pointId ) )
		{
			backupObject = pointBackups[pointId];
		}
		else
		{
			const std::string pointIdString = std::to_string( pointId );
			if( pointBackups.has_key( pointIdString ) )
			{
				backupObject = pointBackups[pointIdString];
			}
		}

		if( !isNone( backupObject ) && backupObject.ptr() )
		{
			bp::dict backup = bp::extract<bp::dict>( backupObject );
			bp::dict expandedBackup;
			expandedBackup["sourcePath"] = pathFromId( store, "scenePaths", dictValue<std::uint32_t>( backup, "targetPathId", 0 ) );
			expandedBackup["triangleIndex"] = dictValue<std::uint32_t>( backup, "triangleIndex", 0 );
			expandedBackup["barycentric"] = dictGet( backup, "barycentric" );
			expandedBackup["attachmentResolved"] = dictValue<bool>( backup, "attachmentResolved", false );
			record["_demoAttachmentBackup"] = expandedBackup;
		}
	}
	record["sourcePath"] = pathFromId( store, "scenePaths", dictValue<std::uint32_t>( point, "targetPathId", 0 ) );
	record["instanceSourcePath"] = pathFromId( store, "instanceSourcePaths", dictValue<std::uint32_t>( point, "instanceSourcePathId", 0 ) );
	record["P"] = dictGet( point, "restWorldP" );
	record["N"] = dictGet( point, "restNormal" );
	record["up"] = dictGet( point, "restUp" );
	record["scale"] = dictGet( point, "uniformScale" );
	record["attachmentResolved"] = dictValue<int>( point, "anchorModeUsed", 3 ) != 3;
	const std::vector<float> authoredColor = resolvedAuthoredColor(
		store,
		layerObject.ptr() ? &layer : nullptr,
		strokeObject.ptr() ? &stroke : nullptr,
		&point
	);
	record["colorEnabled"] = dictValue<bool>( point, "colorEnabled", false );
	record["color"] = point.has_key( "color" ) ? dictGet( point, "color" ) : bp::object( colorList( authoredColor ) );
	record["authoredColor"] = colorList( authoredColor );
	record["scatterColor"] = colorList( authoredColor );
	return record;
}

inline bp::list deepCopyList( const bp::list &values )
{
	bp::object copyModule = bp::import( "copy" );
	bp::list result;
	for( bp::stl_input_iterator<bp::object> it( values ), end; it != end; ++it )
	{
		result.append( copyModule.attr( "deepcopy" )( *it ) );
	}
	return result;
}

inline bp::dict deepCopyDict( const bp::dict &value )
{
	bp::object copyModule = bp::import( "copy" );
	return bp::extract<bp::dict>( copyModule.attr( "deepcopy" )( value ) );
}

inline bp::list integerList( const bp::object &values )
{
	bp::list result;
	if( isNone( values ) )
	{
		return result;
	}
	for( bp::stl_input_iterator<bp::object> it( values ), end; it != end; ++it )
	{
		result.append( extractOr<std::uint64_t>( *it, 0 ) );
	}
	return result;
}

inline bp::list filteredIntegerList( const bp::object &values, const std::set<std::uint64_t> &validIds )
{
	bp::list result;
	if( isNone( values ) )
	{
		return result;
	}
	for( bp::stl_input_iterator<bp::object> it( values ), end; it != end; ++it )
	{
		const std::uint64_t id = extractOr<std::uint64_t>( *it, 0 );
		if( validIds.count( id ) )
		{
			result.append( id );
		}
	}
	return result;
}

inline std::set<std::uint64_t> integerSet( const bp::object &values )
{
	std::set<std::uint64_t> result;
	if( isNone( values ) )
	{
		return result;
	}
	for( bp::stl_input_iterator<bp::object> it( values ), end; it != end; ++it )
	{
		result.insert( extractOr<std::uint64_t>( *it, 0 ) );
	}
	return result;
}

inline std::set<std::uint64_t> selectedPointIds( const bp::dict &store )
{
	std::set<std::uint64_t> result;
	const bp::dict currentSelection = bp::extract<bp::dict>( store["currentSelection"] );
	const std::set<std::uint64_t> selectedStrokeIds = integerSet( dictGet( currentSelection, "strokeIds" ) );
	const std::set<std::uint64_t> explicitlySelectedPointIds = integerSet( dictGet( currentSelection, "pointIds" ) );
	result.insert( explicitlySelectedPointIds.begin(), explicitlySelectedPointIds.end() );

	if( selectedStrokeIds.empty() )
	{
		return result;
	}

	const bp::list points = bp::extract<bp::list>( store["points"] );
	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		const bp::dict point = bp::extract<bp::dict>( *it );
		if( selectedStrokeIds.count( dictValue<std::uint64_t>( point, "strokeId", 0 ) ) )
		{
			result.insert( dictValue<std::uint64_t>( point, "pointId", 0 ) );
		}
	}

	return result;
}

inline void pruneSelectionReferences( bp::dict store, const std::set<std::uint64_t> &validPointIds, const std::set<std::uint64_t> &validStrokeIds )
{
	bp::dict currentSelection = bp::extract<bp::dict>( store["currentSelection"] );
	currentSelection["pointIds"] = filteredIntegerList( dictGet( currentSelection, "pointIds" ), validPointIds );
	currentSelection["strokeIds"] = filteredIntegerList( dictGet( currentSelection, "strokeIds" ), validStrokeIds );
	store["currentSelection"] = currentSelection;

	bp::list selectionSets = bp::extract<bp::list>( store["selectionSets"] );
	for( bp::stl_input_iterator<bp::object> it( selectionSets ), end; it != end; ++it )
	{
		bp::dict selectionSet = bp::extract<bp::dict>( *it );
		selectionSet["pointIds"] = filteredIntegerList( dictGet( selectionSet, "pointIds" ), validPointIds );
		selectionSet["strokeIds"] = filteredIntegerList( dictGet( selectionSet, "strokeIds" ), validStrokeIds );
	}
}

inline void applyExpandedPointEdit( bp::dict store, bp::dict point, const bp::dict &editedPoint )
{
	bp::object layerObject = findLayerRecord( store, dictValue<std::uint64_t>( point, "layerId", 0 ) );
	bp::object strokeObject = findStrokeRecord( store, dictValue<std::uint64_t>( point, "strokeId", 0 ) );
	bp::dict layer = bp::extract<bp::dict>( layerObject ) .check() ? bp::extract<bp::dict>( layerObject )() : bp::dict();
	bp::dict stroke = bp::extract<bp::dict>( strokeObject ) .check() ? bp::extract<bp::dict>( strokeObject )() : bp::dict();
	const std::vector<float> inheritedColor = resolvedAuthoredColor(
		store,
		layerObject.ptr() ? &layer : nullptr,
		strokeObject.ptr() ? &stroke : nullptr,
		nullptr
	);
	bp::dict pointBackups;
	if( store.has_key( "pointBackups" ) )
	{
		pointBackups = bp::extract<bp::dict>( store["pointBackups"] );
	}
	store["pointBackups"] = pointBackups;
	const std::uint64_t pointId = dictValue<std::uint64_t>( point, "pointId", 0 );
	const std::string pointIdString = std::to_string( pointId );
	if( pointId )
	{
		if( editedPoint.has_key( "_demoAttachmentBackup" ) )
		{
			bp::dict backup = bp::extract<bp::dict>( editedPoint["_demoAttachmentBackup"] );
			bp::dict storedBackup;
			storedBackup["targetPathId"] = internPath( store, "scenePaths", dictValue<std::string>( backup, "sourcePath", "" ) );
			storedBackup["triangleIndex"] = dictValue<std::uint32_t>( backup, "triangleIndex", 0 );
			storedBackup["barycentric"] = backup.has_key( "barycentric" ) ? dictGet( backup, "barycentric" ) : bp::object( bp::list() );
			storedBackup["attachmentResolved"] = dictValue<bool>( backup, "attachmentResolved", false );
			pointBackups[pointId] = storedBackup;
			pointBackups[pointIdString] = storedBackup;
		}
		else
		{
			if( pointBackups.has_key( pointId ) )
			{
				pointBackups.attr( "pop" )( pointId );
			}
			if( pointBackups.has_key( pointIdString ) )
			{
				pointBackups.attr( "pop" )( pointIdString );
			}
		}
	}

	bp::list existingKeys = bp::extract<bp::list>( point.keys() );
	for( bp::stl_input_iterator<bp::object> it( existingKeys ), end; it != end; ++it )
	{
		const std::string key = pyString( *it );
		if( !key.empty() && key[0] == '_' && !editedPoint.has_key( key ) )
		{
			point.attr( "pop" )( key, bp::object() );
		}
	}

	bp::list editedKeys = bp::extract<bp::list>( editedPoint.keys() );
	for( bp::stl_input_iterator<bp::object> it( editedKeys ), end; it != end; ++it )
	{
		const std::string key = pyString( *it );
		if( !key.empty() && key[0] == '_' )
		{
			point[key] = editedPoint[key];
		}
	}

	point["targetPathId"] = internPath( store, "scenePaths", dictValue<std::string>( editedPoint, "sourcePath", "" ) );
	point["instanceSourcePathId"] = internPath( store, "instanceSourcePaths", dictValue<std::string>( editedPoint, "instanceSourcePath", "" ) );
	point["triangleIndex"] = dictValue<std::uint32_t>( editedPoint, "triangleIndex", dictValue<std::uint32_t>( point, "triangleIndex", 0 ) );
	point["barycentric"] = editedPoint.has_key( "barycentric" ) ? dictGet( editedPoint, "barycentric" ) : dictGet( point, "barycentric" );
	point["restWorldP"] = editedPoint.has_key( "P" ) ? dictGet( editedPoint, "P" ) : dictGet( point, "restWorldP" );
	point["restObjectP"] = editedPoint.has_key( "restObjectP" ) ? dictGet( editedPoint, "restObjectP" ) : dictGet( point, "restWorldP" );
	point["restUV"] = editedPoint.has_key( "restUV" ) ? dictGet( editedPoint, "restUV" ) : dictGet( point, "restUV" );
	point["restNormal"] = editedPoint.has_key( "N" ) ? dictGet( editedPoint, "N" ) : dictGet( point, "restNormal" );
	point["restUp"] = editedPoint.has_key( "up" ) ? dictGet( editedPoint, "up" ) : dictGet( point, "restUp" );
	point["width"] = dictValue<float>( editedPoint, "width", dictValue<float>( point, "width", 1.0f ) );
	point["uniformScale"] = editedPoint.has_key( "scale" ) ? dictGet( editedPoint, "scale" ) : bp::object( dictValue<float>( editedPoint, "uniformScale", dictValue<float>( point, "uniformScale", 1.0f ) ) );
	point["seed"] = dictValue<std::uint32_t>( editedPoint, "seed", dictValue<std::uint32_t>( point, "seed", 0 ) );
	point["normalSpin"] = dictValue<float>( editedPoint, "normalSpin", dictValue<float>( point, "normalSpin", 0.0f ) );
	point["tangentRotation"] = editedPoint.has_key( "tangentRotation" ) ? dictGet( editedPoint, "tangentRotation" ) : dictGet( point, "tangentRotation" );
	point["pressureDensity"] = dictValue<float>( editedPoint, "pressureDensity", dictValue<float>( point, "pressureDensity", 1.0f ) );
	point["pressureSoftness"] = dictValue<float>( editedPoint, "pressureSoftness", dictValue<float>( point, "pressureSoftness", 1.0f ) );
	point["valid"] = dictValue<bool>( editedPoint, "valid", dictValue<bool>( point, "valid", true ) );
	point["lastValidFrame"] = dictValue<int>( editedPoint, "lastValidFrame", dictValue<int>( point, "lastValidFrame", 0 ) );
	point["anchorModeUsed"] = dictValue<bool>( editedPoint, "attachmentResolved", true ) ? 0 : 3;
	point["topologyGeneration"] = dictValue<std::uint32_t>( editedPoint, "topologyGeneration", dictValue<std::uint32_t>( point, "topologyGeneration", 0 ) );
	const std::vector<float> editedColor = floatVector(
		editedPoint.has_key( "color" ) ? editedPoint["color"] : dictGet( point, "color" ),
		3,
		inheritedColor
	);
	bool colorEnabled = dictValue<bool>( editedPoint, "colorEnabled", dictValue<bool>( point, "colorEnabled", false ) );
	if( editedPoint.has_key( "color" ) && !editedPoint.has_key( "colorEnabled" ) )
	{
		colorEnabled = !colorEqual( editedColor, inheritedColor );
	}
	point["colorEnabled"] = colorEnabled;
	point["color"] = colorList( editedColor );
}

inline void refreshDerivedData( bp::dict store, const std::set<std::uint64_t> &changedStrokeIds = {} )
{
	bp::list layers = bp::extract<bp::list>( store["layers"] );
	bp::list strokes = bp::extract<bp::list>( store["strokes"] );
	bp::list oldChunks = bp::extract<bp::list>( store["chunks"] );
	bp::list newChunks;
	std::uint32_t pointStart = 0;

	std::map<std::uint64_t, std::uint32_t> existingGeneration;
	for( bp::stl_input_iterator<bp::object> it( oldChunks ), end; it != end; ++it )
	{
		bp::dict chunk = bp::extract<bp::dict>( *it );
		const std::uint64_t strokeId = dictValue<std::uint64_t>( chunk, "strokeId", 0 );
		existingGeneration[strokeId] = std::max( existingGeneration[strokeId], dictValue<std::uint32_t>( chunk, "generation", 0 ) );
	}

	std::vector<bp::dict> sortedLayers;
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

	for( bp::dict &layer : sortedLayers )
	{
		const std::uint64_t layerId = dictValue<std::uint64_t>( layer, "layerId", 0 );
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
		layer["firstStrokeId"] = layerStrokes.empty() ? bp::object( 0 ) : dictGet( layerStrokes.front(), "strokeId" );
		layer["lastStrokeId"] = layerStrokes.empty() ? bp::object( 0 ) : dictGet( layerStrokes.back(), "strokeId" );

		for( bp::dict &stroke : layerStrokes )
		{
			const std::uint64_t strokeId = dictValue<std::uint64_t>( stroke, "strokeId", 0 );
			bp::list points = strokePoints( store, strokeId );
			const std::uint32_t strokePointCount = static_cast<std::uint32_t>( bp::len( points ) );
			stroke["pointCount"] = strokePointCount;
			std::set<std::uint32_t> uniqueTargets;
			for( bp::stl_input_iterator<bp::object> pointIt( points ), pointEnd; pointIt != pointEnd; ++pointIt )
			{
				bp::dict point = bp::extract<bp::dict>( *pointIt );
				const std::uint32_t targetPathId = dictValue<std::uint32_t>( point, "targetPathId", 0 );
				if( targetPathId )
				{
					uniqueTargets.insert( targetPathId );
				}
			}
			stroke["targetCount"] = static_cast<std::uint32_t>( uniqueTargets.size() );
			std::uint32_t generation = existingGeneration[strokeId];
			if( changedStrokeIds.count( strokeId ) )
			{
				generation += 1;
			}

			std::uint32_t chunkIndex = 0;
			bp::object firstChunkId( 0 );
			bp::object lastChunkId( 0 );
			for( std::uint32_t chunkStart = 0; chunkStart < strokePointCount || ( strokePointCount == 0 && chunkIndex == 0 ); chunkStart += g_chunkPointLimit, ++chunkIndex )
			{
				const std::uint32_t chunkPointCount = strokePointCount == 0 ? 0 : std::min<std::uint32_t>( g_chunkPointLimit, strokePointCount - chunkStart );
				bp::dict chunk;
				chunk["chunkId"] = nextId( store, "chunk" );
				chunk["strokeId"] = strokeId;
				chunk["chunkIndex"] = chunkIndex;
				chunk["pointStart"] = pointStart;
				chunk["pointCount"] = chunkPointCount;
				chunk["generation"] = generation;
				chunk["deleted"] = false;
				newChunks.append( chunk );
				pointStart += chunkPointCount;
				bp::object chunkId = dictGet( chunk, "chunkId" );
				if( chunkIndex == 0 )
				{
					firstChunkId = chunkId;
				}
				lastChunkId = chunkId;
				if( strokePointCount == 0 )
				{
					break;
				}
			}
			stroke["firstChunkId"] = firstChunkId;
			stroke["lastChunkId"] = lastChunkId;
		}
	}

	store["chunks"] = newChunks;
}
