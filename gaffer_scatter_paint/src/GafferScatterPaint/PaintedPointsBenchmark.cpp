#include "PaintedPointsPrivate.h"

namespace
{

std::string benchmarkValidationSummary( std::uint64_t pointCount )
{
	return std::string(
		"1 layers, 1 strokes, " +
		std::to_string( pointCount ) +
		" points, 0 selection sets, " +
		std::to_string( pointCount ) +
		" resolved attachments, 0 fallback attachments" );
}

	void syncTrustedBenchmarkState( PaintedPoints *node, const CacheSchema &schema, const bp::dict &lock )
	{
	std::vector<std::string> layerNames;
	layerNames.reserve( schema.layers.size() );
	for( const LayerRecord &layer : schema.layers )
	{
		layerNames.push_back( layer.name );
	}

		node->layersPlug()->setValue( new StringVectorData( layerNames ) );
		node->selectionSetsPlug()->setValue( new StringVectorData() );
		node->authoredPointCountPlug()->setValue( static_cast<int>( schema.points.size() ) );
		node->cacheVersionPlug()->setValue( g_schemaVersion );
	node->invalidPointCountPlug()->setValue( 0 );
	node->invalidStrokeCountPlug()->setValue( 0 );
	node->failingFramePlug()->setValue( 0 );
	node->failingTargetPathsPlug()->setValue( new StringVectorData() );
	node->lastErrorMessagePlug()->setValue( "" );
	node->topologyMismatchCountPlug()->setValue( 0 );
	node->validationSummaryPlug()->setValue( benchmarkValidationSummary( schema.points.size() ) );
	std::vector<std::string> validationCategories = {"diagnostics"};
	if( !findAttachedPointsForPaintedNode( node ) )
	{
		validationCategories.push_back( "exportReadiness" );
	}
	node->validationCategoriesPlug()->setValue( new StringVectorData( validationCategories ) );
	node->cacheResolvedPathPlug()->setValue( resolvedCachePath( node ) );
	node->cacheLockedByPlug()->setValue( dictValue<std::string>( lock, "user", "" ) );
	node->cacheLockedHostPlug()->setValue( dictValue<std::string>( lock, "host", "" ) );
	node->cacheLockedTimePlug()->setValue( dictValue<std::string>( lock, "timestampUtc", "" ) );
	node->cacheLockedScriptPlug()->setValue( dictValue<std::string>( lock, "scriptPath", "" ) );

	bp::object nodeObject( bp::ptr( node ) );
	if( PyObject_SetAttrString( nodeObject.ptr(), "_PaintedPoints__authoredPointCount", bp::object( static_cast<int>( schema.points.size() ) ).ptr() ) != 0 )
	{
		PyErr_Clear();
	}
}

CacheSchema benchmarkSchema( PaintedPoints *node, std::uint64_t pointCount, const std::string &layerName, const std::string &strokeName )
{
	CacheSchema schema = defaultCacheSchema();
	schema.header.pluginVersionMajor = CacheHeader().pluginVersionMajor;
	schema.header.pluginVersionMinor = CacheHeader().pluginVersionMinor;
	schema.header.pluginVersionPatch = CacheHeader().pluginVersionPatch;
	schema.node.storageMode = static_cast<CacheStorageMode>( node->cacheModePlug()->getValue() );
	schema.node.pathMode = static_cast<CachePathMode>( node->cachePathModePlug()->getValue() );
	schema.node.projectRoot = node->projectRootPlug()->getValue();
	schema.node.cachePath = node->cachePathPlug()->getValue();
	schema.node.exportPreset = "";
	schema.node.backupEnabled = node->backupEnabledPlug()->getValue();
	schema.node.diagnosticsSnapshotEnabled = true;
	const Imath::Color3f defaultColor = node->defaultColorPlug()->getValue();
	schema.node.defaultColor = { defaultColor[0], defaultColor[1], defaultColor[2] };
	schema.scenePaths = { "/benchmarkScatter" };
	schema.instanceSourcePaths.clear();
	schema.selectionSets.clear();
	schema.currentSelection.pointIds.clear();
	schema.currentSelection.strokeIds.clear();
	schema.upgrades.clear();
	schema.pointBackups.clear();

	LayerRecord layer;
	layer.layerId = 1;
	layer.name = layerName;
	layer.order = 0;
	layer.enabled = true;
	layer.visible = true;
	layer.mute = false;
	layer.solo = false;
	layer.timeVarying = false;
	layer.mode = StrokeFrameMode::Persistent;
	layer.frameStart = 0;
	layer.frameEnd = 0;
	layer.holdOutsideRange = true;
	layer.firstStrokeId = pointCount ? 1 : 0;
	layer.lastStrokeId = pointCount ? 1 : 0;
	layer.colorEnabled = false;
	layer.color = { 1.0f, 1.0f, 1.0f };
	schema.layers.push_back( layer );

	StrokeRecord stroke;
	stroke.strokeId = 1;
	stroke.layerId = 1;
	stroke.name = strokeName;
	stroke.order = 0;
	stroke.mode = StrokeFrameMode::Persistent;
	stroke.frameStart = 0;
	stroke.frameEnd = 0;
	stroke.createdTimeUnixMicros = unixTimeMicros();
	stroke.firstChunkId = pointCount ? 1 : 0;
	stroke.lastChunkId = pointCount ? static_cast<std::uint64_t>( ( pointCount + g_chunkPointLimit - 1 ) / g_chunkPointLimit ) : 0;
	stroke.pointCount = static_cast<std::uint32_t>( pointCount );
	stroke.targetCount = pointCount ? 1 : 0;
	stroke.selectionMaskId = 0;
	stroke.colorEnabled = false;
	stroke.color = { 1.0f, 1.0f, 1.0f };
	schema.strokes.push_back( stroke );

	schema.points.reserve( static_cast<size_t>( pointCount ) );
	schema.chunks.reserve( static_cast<size_t>( ( pointCount + g_chunkPointLimit - 1 ) / g_chunkPointLimit ) );

	const std::uint64_t chunkCount = ( pointCount + g_chunkPointLimit - 1 ) / g_chunkPointLimit;
	for( std::uint64_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex )
	{
		PointChunkRecord chunk;
		chunk.chunkId = chunkIndex + 1;
		chunk.strokeId = 1;
		chunk.chunkIndex = static_cast<std::uint32_t>( chunkIndex );
		chunk.pointStart = static_cast<std::uint32_t>( chunkIndex * g_chunkPointLimit );
		chunk.pointCount = static_cast<std::uint32_t>( std::min<std::uint64_t>( g_chunkPointLimit, pointCount - ( chunkIndex * g_chunkPointLimit ) ) );
		chunk.generation = 1;
		chunk.deleted = false;
		schema.chunks.push_back( chunk );
	}

	const std::uint64_t columns = std::max<std::uint64_t>( 1, static_cast<std::uint64_t>( std::llround( std::sqrt( static_cast<long double>( pointCount ) ) ) ) );
	const std::uint64_t rows = pointCount ? ( pointCount + columns - 1 ) / columns : 1;
	const float usableColumns = static_cast<float>( std::max<std::uint64_t>( 1, columns - 1 ) );
	const float usableRows = static_cast<float>( std::max<std::uint64_t>( 1, rows - 1 ) );
	constexpr float margin = 0.04f;

	for( std::uint64_t index = 0; index < pointCount; ++index )
	{
		const std::uint64_t column = index % columns;
		const std::uint64_t row = index / columns;
		float u = usableColumns > 0.0f ? static_cast<float>( column ) / usableColumns : 0.5f;
		float v = usableRows > 0.0f ? static_cast<float>( row ) / usableRows : 0.5f;
		u = margin + ( 1.0f - margin * 2.0f ) * u;
		v = margin + ( 1.0f - margin * 2.0f ) * v;
		const float x = u - 0.5f;
		const float z = v - 0.5f;

		PointRecord point;
		point.pointId = index + 1;
		point.strokeId = 1;
		point.layerId = 1;
		point.targetPathId = 1;
		point.instanceId = 0;
		point.instanceSourcePathId = 0;
		if( u + v <= 1.0f )
		{
			point.triangleIndex = 0;
			point.barycentric = { 1.0f - u - v, u, v };
		}
		else
		{
			point.triangleIndex = 1;
			point.barycentric = { 1.0f - v, u + v - 1.0f, 1.0f - u };
		}
		point.restObjectP = { x, 0.01f, z };
		point.restWorldP = { x, 0.01f, z };
		point.restUV = { u, v };
		point.restNormal = { 0.0f, 1.0f, 0.0f };
		point.restUp = { 0.0f, 0.0f, 1.0f };
		point.width = 0.085f;
		point.uniformScale = 1.0f;
		point.seed = static_cast<std::uint32_t>( 5000 + index );
		point.normalSpin = 0.0f;
		point.tangentRotation = { 0.0f, 0.0f };
		point.pressureDensity = 1.0f;
		point.pressureSoftness = 1.0f;
		point.valid = true;
		point.lastValidFrame = 0;
		point.anchorModeUsed = AnchorMode::Barycentric;
		point.topologyGeneration = 0;
		point.colorEnabled = false;
		point.color = { 1.0f, 1.0f, 1.0f };
		schema.points.push_back( point );
	}

	schema.nextIds.layer = 2;
	schema.nextIds.stroke = 2;
	schema.nextIds.point = pointCount + 1;
	schema.nextIds.selectionSet = 1;
	schema.nextIds.chunk = chunkCount + 1;
	schema.diagnostics = defaultCacheSchema().diagnostics;
	schema.diagnostics.invalidPointCount = 0;
	schema.diagnostics.invalidStrokeCount = 0;
	schema.diagnostics.topologyMismatchCount = 0;
	schema.diagnostics.failingFrame = 0;
	schema.diagnostics.failingTargetPaths.clear();
	schema.diagnostics.lastErrorMessage.clear();
	schema.diagnostics.validationSummary = benchmarkValidationSummary( pointCount );
	schema.diagnostics.categories = { ValidationCategory::Diagnostics };
	if( !findAttachedPointsForPaintedNode( node ) )
	{
		schema.diagnostics.categories.push_back( ValidationCategory::ExportReadiness );
	}

	return schema;
}

}

bp::object PaintedPoints::seedBenchmarkStroke( std::uint64_t pointCount, const std::string &layerName, const std::string &strokeName )
{
	const std::string resolvedLayerName = layerName.empty() ? "Benchmark Layer" : layerName;
	const std::string resolvedStrokeName = strokeName.empty() ? ( "Benchmark Stroke " + std::to_string( pointCount ) ) : strokeName;
	const CacheSchema schema = benchmarkSchema( this, pointCount, resolvedLayerName, resolvedStrokeName );
	bp::dict lock = ensureWriteLock( this );
	std::string packedBytes;
	CachePackTimings packTimings;
	try
	{
		CacheSchema writableSchema = schema;
		writableSchema.lock.mode = static_cast<LockMode>( dictValue<std::uint32_t>( lock, "mode", 0 ) );
		writableSchema.lock.user = dictValue<std::string>( lock, "user", "" );
		writableSchema.lock.host = dictValue<std::string>( lock, "host", "" );
		writableSchema.lock.timestampUtc = dictValue<std::string>( lock, "timestampUtc", "" );
		writableSchema.lock.scriptPath = dictValue<std::string>( lock, "scriptPath", "" );
		writableSchema.lock.projectPath = dictValue<std::string>( lock, "projectPath", "" );
		writableSchema.lock.sessionId = dictValue<std::string>( lock, "sessionId", "" );
		if( cacheModePlug()->getValue() == static_cast<int>( g_cacheModeExternal ) )
		{
			packedBytes = packCacheSchema( writableSchema, &packTimings );
			const std::string cachePath = resolvedCachePath( this );
			if( cachePath.empty() )
			{
				throw std::runtime_error( "External cache mode requires a cache path" );
			}
			writeBackupIfEnabled( this, cachePath );
			writeBytesFile( cachePath, packedBytes );
			cacheBlobPlug()->setValue( new UCharVectorData() );
			releaseWriteLock( this, &lock );
		}
		else
		{
			IECore::UCharVectorDataPtr packedBlob = new IECore::UCharVectorData();
			packCacheSchema( writableSchema, packedBlob->writable(), &packTimings );
			cacheBlobPlug()->setValue( packedBlob );
		}
	}
	catch( ... )
	{
		releaseWriteLock( this, &lock );
		throw;
	}

	syncTrustedBenchmarkState( this, schema, lock );
	bp::dict result;
	result["layerId"] = 1;
	result["strokeId"] = 1;
	result["pointCount"] = pointCount;
	result["chunkCount"] = static_cast<std::uint64_t>( schema.chunks.size() );
	result["packPointsReserveMs"] = packTimings.packPointsReserveMs;
	result["packPointsReserveReallocated"] = packTimings.packPointsReserveReallocated;
	result["packPointsCapacityBeforeBytes"] = packTimings.packPointsCapacityBeforeBytes;
	result["packPointsCapacityAfterBytes"] = packTimings.packPointsCapacityAfterBytes;
	result["packPointsResizeMs"] = packTimings.packPointsResizeMs;
	result["packPointsFillMs"] = packTimings.packPointsFillMs;
	result["packPointsCopyMs"] = packTimings.packPointsCopyMs;
	result["packPointsChecksumInlineMs"] = packTimings.packPointsChecksumInlineMs;
	result["packPointsChecksumMs"] = packTimings.packPointsChecksumMs;
	result["packFinalChecksumMs"] = packTimings.packFinalChecksumMs;
	result["packFinalHeaderWriteMs"] = packTimings.packFinalHeaderWriteMs;
	result["packFinalBufferMs"] = packTimings.packFinalBufferMs;
	result["packPointsMs"] = packTimings.packPointsMs;
	return result;
}
