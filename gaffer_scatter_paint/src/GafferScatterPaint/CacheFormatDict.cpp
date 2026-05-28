#include "CacheFormatDict.h"
#include <string>

namespace bp = boost::python;

namespace GafferScatterPaint
{

namespace
{

template<typename T>
T extractOr( const bp::object &value, const T &defaultValue )
{
	bp::extract<T> extractor( value );
	return extractor.check() ? extractor() : defaultValue;
}

bp::object dictGet( const bp::dict &dictionary, const char *key )
{
	return dictionary.has_key( key ) ? dictionary[key] : bp::object();
}

template<typename T>
T dictValue( const bp::dict &dictionary, const char *key, const T &defaultValue )
{
	return dictionary.has_key( key ) ? extractOr<T>( dictionary[key], defaultValue ) : defaultValue;
}

std::string pyString( const bp::object &value )
{
	return bp::extract<std::string>( bp::str( value ) );
}

std::vector<std::uint64_t> readU64List( const bp::object &value )
{
	std::vector<std::uint64_t> result;
	if( value.ptr() == Py_None )
	{
		return result;
	}
	for( bp::stl_input_iterator<bp::object> it( value ), end; it != end; ++it )
	{
		result.push_back( extractOr<std::uint64_t>( *it, 0 ) );
	}
	return result;
}

std::vector<std::string> readStringList( const bp::object &value )
{
	std::vector<std::string> result;
	if( value.ptr() == Py_None )
	{
		return result;
	}
	for( bp::stl_input_iterator<bp::object> it( value ), end; it != end; ++it )
	{
		result.push_back( pyString( *it ) );
	}
	return result;
}

void readF32List( const bp::object &value, float *out, size_t count, float defaultValue = 0.0f )
{
	for( size_t i = 0; i < count; ++i ) out[i] = defaultValue;
	if( value.ptr() == Py_None ) return;
	
	size_t i = 0;
	for( bp::stl_input_iterator<bp::object> it( value ), end; it != end; ++it )
	{
		if( i >= count ) break;
		out[i++] = extractOr<float>( *it, defaultValue );
	}
}

bp::list writeU64List( const std::vector<std::uint64_t> &values )
{
	bp::list result;
	for( std::uint64_t value : values )
	{
		result.append( value );
	}
	return result;
}

bp::list writeStringList( const std::vector<std::string> &values )
{
	bp::list result;
	for( const std::string &value : values )
	{
		result.append( value );
	}
	return result;
}

bp::list writeF32List( const float *values, size_t count )
{
	bp::list result;
	for( size_t i = 0; i < count; ++i )
	{
		result.append( values[i] );
	}
	return result;
}

} // namespace

bp::dict cacheSchemaToDict( const CacheSchema &schema )
{
	bp::dict result;
	result["schemaVersion"] = CacheHeader::schemaVersion;

	bp::dict nextIds;
	nextIds["layer"] = schema.nextIds.layer;
	nextIds["stroke"] = schema.nextIds.stroke;
	nextIds["point"] = schema.nextIds.point;
	nextIds["selectionSet"] = schema.nextIds.selectionSet;
	nextIds["chunk"] = schema.nextIds.chunk;
	result["nextIds"] = nextIds;

	bp::dict node;
	node["storageMode"] = static_cast<std::uint32_t>( schema.node.storageMode );
	node["pathMode"] = static_cast<std::uint32_t>( schema.node.pathMode );
	node["projectRoot"] = schema.node.projectRoot;
	node["cachePath"] = schema.node.cachePath;
	node["exportPreset"] = schema.node.exportPreset;
	node["backupEnabled"] = schema.node.backupEnabled;
	node["diagnosticsSnapshotEnabled"] = schema.node.diagnosticsSnapshotEnabled;
	node["defaultColor"] = writeF32List( schema.node.defaultColor.data(), 3 );
	node["contentFlags"] = schema.header.contentFlags;
	node["pluginVersionMajor"] = schema.header.pluginVersionMajor;
	node["pluginVersionMinor"] = schema.header.pluginVersionMinor;
	node["pluginVersionPatch"] = schema.header.pluginVersionPatch;
	result["node"] = node;

	bp::dict lock;
	lock["mode"] = static_cast<std::uint32_t>( schema.lock.mode );
	lock["user"] = schema.lock.user;
	lock["host"] = schema.lock.host;
	lock["timestampUtc"] = schema.lock.timestampUtc;
	lock["scriptPath"] = schema.lock.scriptPath;
	lock["projectPath"] = schema.lock.projectPath;
	lock["sessionId"] = schema.lock.sessionId;
	result["lock"] = lock;

	result["scenePaths"] = writeStringList( schema.scenePaths );
	result["instanceSourcePaths"] = writeStringList( schema.instanceSourcePaths );

	bp::list layers;
	for( const auto &layerData : schema.layers )
	{
		bp::dict layer;
		layer["layerId"] = layerData.layerId;
		layer["name"] = layerData.name;
		layer["order"] = layerData.order;
		layer["enabled"] = layerData.enabled;
		layer["visible"] = layerData.visible;
		layer["mute"] = layerData.mute;
		layer["solo"] = layerData.solo;
		layer["timeVarying"] = layerData.timeVarying;
		layer["mode"] = static_cast<std::uint32_t>( layerData.mode );
		layer["frameStart"] = layerData.frameStart;
		layer["frameEnd"] = layerData.frameEnd;
		layer["holdOutsideRange"] = layerData.holdOutsideRange;
		layer["firstStrokeId"] = layerData.firstStrokeId;
		layer["lastStrokeId"] = layerData.lastStrokeId;
		layer["colorEnabled"] = layerData.colorEnabled;
		layer["color"] = writeF32List( layerData.color.data(), 3 );
		layers.append( layer );
	}
	result["layers"] = layers;

	bp::list strokes;
	for( const auto &strokeData : schema.strokes )
	{
		bp::dict stroke;
		stroke["strokeId"] = strokeData.strokeId;
		stroke["layerId"] = strokeData.layerId;
		stroke["name"] = strokeData.name;
		stroke["order"] = strokeData.order;
		stroke["mode"] = static_cast<std::uint32_t>( strokeData.mode );
		stroke["frameStart"] = strokeData.frameStart;
		stroke["frameEnd"] = strokeData.frameEnd;
		stroke["createdTimeUnixMicros"] = strokeData.createdTimeUnixMicros;
		stroke["firstChunkId"] = strokeData.firstChunkId;
		stroke["lastChunkId"] = strokeData.lastChunkId;
		stroke["pointCount"] = strokeData.pointCount;
		stroke["targetCount"] = strokeData.targetCount;
		stroke["selectionMaskId"] = strokeData.selectionMaskId;
		stroke["colorEnabled"] = strokeData.colorEnabled;
		stroke["color"] = writeF32List( strokeData.color.data(), 3 );
		strokes.append( stroke );
	}
	result["strokes"] = strokes;

	bp::list chunks;
	for( const auto &chunkData : schema.chunks )
	{
		bp::dict chunk;
		chunk["chunkId"] = chunkData.chunkId;
		chunk["strokeId"] = chunkData.strokeId;
		chunk["chunkIndex"] = chunkData.chunkIndex;
		chunk["pointStart"] = chunkData.pointStart;
		chunk["pointCount"] = chunkData.pointCount;
		chunk["generation"] = chunkData.generation;
		chunk["deleted"] = chunkData.deleted;
		chunks.append( chunk );
	}
	result["chunks"] = chunks;

	bp::list points;
	for( const auto &pointData : schema.points )
	{
		bp::dict point;
		point["pointId"] = pointData.pointId;
		point["strokeId"] = pointData.strokeId;
		point["layerId"] = pointData.layerId;
		point["targetPathId"] = pointData.targetPathId;
		point["instanceId"] = pointData.instanceId;
		point["instanceSourcePathId"] = pointData.instanceSourcePathId;
		point["triangleIndex"] = pointData.triangleIndex;
		point["barycentric"] = writeF32List( pointData.barycentric.data(), 3 );
		point["restObjectP"] = writeF32List( pointData.restObjectP.data(), 3 );
		point["restWorldP"] = writeF32List( pointData.restWorldP.data(), 3 );
		point["restUV"] = writeF32List( pointData.restUV.data(), 2 );
		point["restNormal"] = writeF32List( pointData.restNormal.data(), 3 );
		point["restUp"] = writeF32List( pointData.restUp.data(), 3 );
		point["width"] = pointData.width;
		point["uniformScale"] = pointData.uniformScale;
		point["seed"] = pointData.seed;
		point["normalSpin"] = pointData.normalSpin;
		point["tangentRotation"] = writeF32List( pointData.tangentRotation.data(), 2 );
		point["pressureDensity"] = pointData.pressureDensity;
		point["pressureSoftness"] = pointData.pressureSoftness;
		point["valid"] = pointData.valid;
		point["lastValidFrame"] = pointData.lastValidFrame;
		point["anchorModeUsed"] = static_cast<std::uint32_t>( pointData.anchorModeUsed );
		point["topologyGeneration"] = pointData.topologyGeneration;
		point["colorEnabled"] = pointData.colorEnabled;
		point["color"] = writeF32List( pointData.color.data(), 3 );
		points.append( point );
	}
	result["points"] = points;

	bp::list selectionSets;
	for( const auto &setData : schema.selectionSets )
	{
		bp::dict selectionSet;
		selectionSet["selectionSetId"] = setData.selectionSetId;
		selectionSet["name"] = setData.name;
		selectionSet["pointIds"] = writeU64List( setData.pointIds );
		selectionSet["strokeIds"] = writeU64List( setData.strokeIds );
		selectionSets.append( selectionSet );
	}
	result["selectionSets"] = selectionSets;

	bp::dict currentSelection;
	currentSelection["pointIds"] = writeU64List( schema.currentSelection.pointIds );
	currentSelection["strokeIds"] = writeU64List( schema.currentSelection.strokeIds );
	result["currentSelection"] = currentSelection;

	bp::dict diagnostics;
	diagnostics["invalidPointCount"] = schema.diagnostics.invalidPointCount;
	diagnostics["invalidStrokeCount"] = schema.diagnostics.invalidStrokeCount;
	diagnostics["topologyMismatchCount"] = schema.diagnostics.topologyMismatchCount;
	diagnostics["failingFrame"] = schema.diagnostics.failingFrame;
	diagnostics["failingTargetPaths"] = writeStringList( schema.diagnostics.failingTargetPaths );
	diagnostics["lastErrorMessage"] = schema.diagnostics.lastErrorMessage;
	diagnostics["validationSummary"] = schema.diagnostics.validationSummary;
	
	bp::list categories;
	for( ValidationCategory cat : schema.diagnostics.categories )
	{
		categories.append( static_cast<std::uint32_t>( cat ) );
	}
	diagnostics["categories"] = categories;
	result["diagnostics"] = diagnostics;

	bp::list upgrades;
	for( const auto &upgradeData : schema.upgrades )
	{
		bp::dict upgrade;
		upgrade["sourceSchemaVersion"] = upgradeData.sourceSchemaVersion;
		upgrade["upgradedSchemaVersion"] = upgradeData.upgradedSchemaVersion;
		upgrade["timestampUtc"] = upgradeData.timestampUtc;
		upgrade["report"] = upgradeData.report;
		upgrades.append( upgrade );
	}
	result["upgrades"] = upgrades;

	bp::dict pointBackups;
	for( const auto &backupPair : schema.pointBackups )
	{
		bp::dict backup;
		backup["targetPathId"] = backupPair.second.targetPathId;
		backup["triangleIndex"] = backupPair.second.triangleIndex;
		backup["barycentric"] = writeF32List( backupPair.second.barycentric.data(), 3 );
		backup["attachmentResolved"] = backupPair.second.attachmentResolved;
		pointBackups[backupPair.first] = backup;
	}
	result["pointBackups"] = pointBackups;

	return result;
}

CacheSchema dictToCacheSchema( const bp::dict &dict )
{
	CacheSchema schema = defaultCacheSchema();
	bp::dict nextIds = dictValue<bp::dict>( dict, "nextIds", bp::dict() );
	schema.nextIds.layer = dictValue<std::uint64_t>( nextIds, "layer", 1 );
	schema.nextIds.stroke = dictValue<std::uint64_t>( nextIds, "stroke", 1 );
	schema.nextIds.point = dictValue<std::uint64_t>( nextIds, "point", 1 );
	schema.nextIds.selectionSet = dictValue<std::uint64_t>( nextIds, "selectionSet", 1 );
	schema.nextIds.chunk = dictValue<std::uint64_t>( nextIds, "chunk", 1 );
	
	bp::dict node = dictValue<bp::dict>( dict, "node", bp::dict() );
	schema.node.storageMode = static_cast<CacheStorageMode>( dictValue<std::uint32_t>( node, "storageMode", 0 ) );
	schema.node.pathMode = static_cast<CachePathMode>( dictValue<std::uint32_t>( node, "pathMode", 1 ) );
	schema.node.projectRoot = dictValue<std::string>( node, "projectRoot", "" );
	schema.node.cachePath = dictValue<std::string>( node, "cachePath", "" );
	schema.node.exportPreset = dictValue<std::string>( node, "exportPreset", "" );
	schema.node.backupEnabled = dictValue<bool>( node, "backupEnabled", false );
	schema.node.diagnosticsSnapshotEnabled = dictValue<bool>( node, "diagnosticsSnapshotEnabled", true );
	readF32List( dictGet( node, "defaultColor" ), schema.node.defaultColor.data(), 3, 1.0f );

	bp::dict lock = dictValue<bp::dict>( dict, "lock", bp::dict() );
	schema.lock.mode = static_cast<LockMode>( dictValue<std::uint32_t>( lock, "mode", 0 ) );
	schema.lock.user = dictValue<std::string>( lock, "user", "" );
	schema.lock.host = dictValue<std::string>( lock, "host", "" );
	schema.lock.timestampUtc = dictValue<std::string>( lock, "timestampUtc", "" );
	schema.lock.scriptPath = dictValue<std::string>( lock, "scriptPath", "" );
	schema.lock.projectPath = dictValue<std::string>( lock, "projectPath", "" );
	schema.lock.sessionId = dictValue<std::string>( lock, "sessionId", "" );

	schema.scenePaths = readStringList( dictGet( dict, "scenePaths" ) );
	schema.instanceSourcePaths = readStringList( dictGet( dict, "instanceSourcePaths" ) );

	bp::list layers = extractOr<bp::list>( dictGet( dict, "layers" ), bp::list() );
	for( bp::stl_input_iterator<bp::object> it( layers ), end; it != end; ++it )
	{
		bp::dict l = bp::extract<bp::dict>( *it );
		LayerRecord layer;
		layer.layerId = dictValue<std::uint64_t>( l, "layerId", 0 );
		layer.name = dictValue<std::string>( l, "name", "" );
		layer.order = dictValue<std::int32_t>( l, "order", 0 );
		layer.enabled = dictValue<bool>( l, "enabled", true );
		layer.visible = dictValue<bool>( l, "visible", true );
		layer.mute = dictValue<bool>( l, "mute", false );
		layer.solo = dictValue<bool>( l, "solo", false );
		layer.timeVarying = dictValue<bool>( l, "timeVarying", false );
		layer.mode = static_cast<StrokeFrameMode>( dictValue<std::uint32_t>( l, "mode", 0 ) );
		layer.frameStart = dictValue<std::int32_t>( l, "frameStart", 0 );
		layer.frameEnd = dictValue<std::int32_t>( l, "frameEnd", 0 );
		layer.holdOutsideRange = dictValue<bool>( l, "holdOutsideRange", true );
		layer.firstStrokeId = dictValue<std::uint64_t>( l, "firstStrokeId", 0 );
		layer.lastStrokeId = dictValue<std::uint64_t>( l, "lastStrokeId", 0 );
		layer.colorEnabled = dictValue<bool>( l, "colorEnabled", false );
		readF32List( dictGet( l, "color" ), layer.color.data(), 3, 1.0f );
		schema.layers.push_back( layer );
	}

	bp::list strokes = extractOr<bp::list>( dictGet( dict, "strokes" ), bp::list() );
	for( bp::stl_input_iterator<bp::object> it( strokes ), end; it != end; ++it )
	{
		bp::dict s = bp::extract<bp::dict>( *it );
		StrokeRecord stroke;
		stroke.strokeId = dictValue<std::uint64_t>( s, "strokeId", 0 );
		stroke.layerId = dictValue<std::uint64_t>( s, "layerId", 0 );
		stroke.name = dictValue<std::string>( s, "name", "" );
		stroke.order = dictValue<std::int32_t>( s, "order", 0 );
		stroke.mode = static_cast<StrokeFrameMode>( dictValue<std::uint32_t>( s, "mode", 0 ) );
		stroke.frameStart = dictValue<std::int32_t>( s, "frameStart", 0 );
		stroke.frameEnd = dictValue<std::int32_t>( s, "frameEnd", 0 );
		stroke.createdTimeUnixMicros = dictValue<std::uint64_t>( s, "createdTimeUnixMicros", 0 );
		stroke.firstChunkId = dictValue<std::uint64_t>( s, "firstChunkId", 0 );
		stroke.lastChunkId = dictValue<std::uint64_t>( s, "lastChunkId", 0 );
		stroke.pointCount = dictValue<std::uint32_t>( s, "pointCount", 0 );
		stroke.targetCount = dictValue<std::uint32_t>( s, "targetCount", 0 );
		stroke.selectionMaskId = dictValue<std::uint64_t>( s, "selectionMaskId", 0 );
		stroke.colorEnabled = dictValue<bool>( s, "colorEnabled", false );
		readF32List( dictGet( s, "color" ), stroke.color.data(), 3, 1.0f );
		schema.strokes.push_back( stroke );
	}

	bp::list chunks = extractOr<bp::list>( dictGet( dict, "chunks" ), bp::list() );
	for( bp::stl_input_iterator<bp::object> it( chunks ), end; it != end; ++it )
	{
		bp::dict c = bp::extract<bp::dict>( *it );
		PointChunkRecord chunk;
		chunk.chunkId = dictValue<std::uint64_t>( c, "chunkId", 0 );
		chunk.strokeId = dictValue<std::uint64_t>( c, "strokeId", 0 );
		chunk.chunkIndex = dictValue<std::uint32_t>( c, "chunkIndex", 0 );
		chunk.pointStart = dictValue<std::uint32_t>( c, "pointStart", 0 );
		chunk.pointCount = dictValue<std::uint32_t>( c, "pointCount", 0 );
		chunk.generation = dictValue<std::uint32_t>( c, "generation", 0 );
		chunk.deleted = dictValue<bool>( c, "deleted", false );
		schema.chunks.push_back( chunk );
	}

	bp::list points = extractOr<bp::list>( dictGet( dict, "points" ), bp::list() );
	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		bp::dict p = bp::extract<bp::dict>( *it );
		PointRecord point;
		point.pointId = dictValue<std::uint64_t>( p, "pointId", 0 );
		point.strokeId = dictValue<std::uint64_t>( p, "strokeId", 0 );
		point.layerId = dictValue<std::uint64_t>( p, "layerId", 0 );
		point.targetPathId = dictValue<std::uint32_t>( p, "targetPathId", 0 );
		point.instanceId = dictValue<std::uint32_t>( p, "instanceId", 0 );
		point.instanceSourcePathId = dictValue<std::uint32_t>( p, "instanceSourcePathId", 0 );
		point.triangleIndex = dictValue<std::uint32_t>( p, "triangleIndex", 0 );
		readF32List( dictGet( p, "barycentric" ), point.barycentric.data(), 3, 0.0f );
		readF32List( dictGet( p, "restObjectP" ), point.restObjectP.data(), 3, 0.0f );
		readF32List( dictGet( p, "restWorldP" ), point.restWorldP.data(), 3, 0.0f );
		readF32List( dictGet( p, "restUV" ), point.restUV.data(), 2, 0.0f );
		readF32List( dictGet( p, "restNormal" ), point.restNormal.data(), 3, 0.0f );
		readF32List( dictGet( p, "restUp" ), point.restUp.data(), 3, 0.0f );
		point.width = dictValue<float>( p, "width", 1.0f );
		point.uniformScale = dictValue<float>( p, "uniformScale", 1.0f );
		point.seed = dictValue<std::uint32_t>( p, "seed", 0 );
		point.normalSpin = dictValue<float>( p, "normalSpin", 0.0f );
		readF32List( dictGet( p, "tangentRotation" ), point.tangentRotation.data(), 2, 0.0f );
		point.pressureDensity = dictValue<float>( p, "pressureDensity", 1.0f );
		point.pressureSoftness = dictValue<float>( p, "pressureSoftness", 1.0f );
		point.valid = dictValue<bool>( p, "valid", true );
		point.lastValidFrame = dictValue<std::int32_t>( p, "lastValidFrame", 0 );
		point.anchorModeUsed = static_cast<AnchorMode>( dictValue<std::uint32_t>( p, "anchorModeUsed", 0 ) );
		point.topologyGeneration = dictValue<std::uint32_t>( p, "topologyGeneration", 0 );
		point.colorEnabled = dictValue<bool>( p, "colorEnabled", false );
		readF32List( dictGet( p, "color" ), point.color.data(), 3, 1.0f );
		schema.points.push_back( point );
	}

	bp::list selectionSets = extractOr<bp::list>( dictGet( dict, "selectionSets" ), bp::list() );
	for( bp::stl_input_iterator<bp::object> it( selectionSets ), end; it != end; ++it )
	{
		bp::dict ss = bp::extract<bp::dict>( *it );
		SelectionSetRecord selectionSet;
		selectionSet.selectionSetId = dictValue<std::uint64_t>( ss, "selectionSetId", 0 );
		selectionSet.name = dictValue<std::string>( ss, "name", "" );
		selectionSet.pointIds = readU64List( dictGet( ss, "pointIds" ) );
		selectionSet.strokeIds = readU64List( dictGet( ss, "strokeIds" ) );
		schema.selectionSets.push_back( selectionSet );
	}

	bp::dict currentSelection = dictValue<bp::dict>( dict, "currentSelection", bp::dict() );
	schema.currentSelection.pointIds = readU64List( dictGet( currentSelection, "pointIds" ) );
	schema.currentSelection.strokeIds = readU64List( dictGet( currentSelection, "strokeIds" ) );

	bp::dict diagnostics = dictValue<bp::dict>( dict, "diagnostics", bp::dict() );
	schema.diagnostics.invalidPointCount = dictValue<std::uint64_t>( diagnostics, "invalidPointCount", 0 );
	schema.diagnostics.invalidStrokeCount = dictValue<std::uint64_t>( diagnostics, "invalidStrokeCount", 0 );
	schema.diagnostics.topologyMismatchCount = dictValue<std::uint64_t>( diagnostics, "topologyMismatchCount", 0 );
	schema.diagnostics.failingFrame = dictValue<std::int32_t>( diagnostics, "failingFrame", 0 );
	schema.diagnostics.failingTargetPaths = readStringList( dictGet( diagnostics, "failingTargetPaths" ) );
	schema.diagnostics.lastErrorMessage = dictValue<std::string>( diagnostics, "lastErrorMessage", "" );
	schema.diagnostics.validationSummary = dictValue<std::string>( diagnostics, "validationSummary", "" );

	bp::list categories = extractOr<bp::list>( dictGet( diagnostics, "categories" ), bp::list() );
	for( bp::stl_input_iterator<bp::object> it( categories ), end; it != end; ++it )
	{
		schema.diagnostics.categories.push_back( static_cast<ValidationCategory>( extractOr<std::uint32_t>( *it, 6 ) ) );
	}

	bp::list upgrades = extractOr<bp::list>( dictGet( dict, "upgrades" ), bp::list() );
	for( bp::stl_input_iterator<bp::object> it( upgrades ), end; it != end; ++it )
	{
		bp::dict u = bp::extract<bp::dict>( *it );
		UpgradeRecord upgrade;
		upgrade.sourceSchemaVersion = dictValue<std::uint32_t>( u, "sourceSchemaVersion", 0 );
		upgrade.upgradedSchemaVersion = dictValue<std::uint32_t>( u, "upgradedSchemaVersion", CacheHeader::schemaVersion );
		upgrade.timestampUtc = dictValue<std::string>( u, "timestampUtc", "" );
		upgrade.report = dictValue<std::string>( u, "report", "" );
		schema.upgrades.push_back( upgrade );
	}

	bp::dict pointBackups = dictValue<bp::dict>( dict, "pointBackups", bp::dict() );
	bp::list backupKeys = pointBackups.keys();
	for( bp::stl_input_iterator<bp::object> it( backupKeys ), end; it != end; ++it )
	{
		std::uint64_t pointId = 0;
		if( bp::extract<std::uint64_t>( *it ).check() )
		{
			pointId = bp::extract<std::uint64_t>( *it )();
		}
		else if( bp::extract<std::string>( *it ).check() )
		{
			try { pointId = std::stoull( bp::extract<std::string>( *it )() ); } catch(...) { continue; }
		}
		else
		{
			continue;
		}

		bp::dict b = bp::extract<bp::dict>( pointBackups[*it] );
		PointBackupRecord backup;
		backup.targetPathId = dictValue<std::uint32_t>( b, "targetPathId", 0 );
		backup.triangleIndex = dictValue<std::uint32_t>( b, "triangleIndex", 0 );
		readF32List( dictGet( b, "barycentric" ), backup.barycentric.data(), 3, 0.0f );
		backup.attachmentResolved = dictValue<bool>( b, "attachmentResolved", false );
		schema.pointBackups.emplace_back( pointId, backup );
	}

	// Sort pointBackups by pointId as _storebridge.py did
	std::sort( schema.pointBackups.begin(), schema.pointBackups.end(), []( const auto &a, const auto &b ) {
		return a.first < b.first;
	});

	return schema;
}

} // namespace GafferScatterPaint
