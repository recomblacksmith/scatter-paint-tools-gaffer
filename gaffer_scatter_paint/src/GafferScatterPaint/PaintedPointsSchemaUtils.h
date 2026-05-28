#pragma once

namespace
{

const IECore::UCharVectorData *emptyBlobObject()
{
	static IECore::UCharVectorDataPtr g_emptyBlobObject = new IECore::UCharVectorData();
	return g_emptyBlobObject.get();
}

} // namespace

inline void populateSchemaNodeMetadata( const PaintedPoints *node, CacheSchema &schema )
{
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
}

inline CacheSchema defaultSchemaForNode( const PaintedPoints *node )
{
	CacheSchema schema = defaultCacheSchema();
	populateSchemaNodeMetadata( node, schema );
	return schema;
}

inline void setStorageTiming( bp::dict *timings, const char *key, double value )
{
	if( timings )
	{
		( *timings )[key] = value;
	}
}

inline CacheSchema loadCacheSchemaForNode( const PaintedPoints *node, std::string *error = nullptr, bp::dict *timings = nullptr )
{
	double resolvePathMs = 0.0;
	double readBytesMs = 0.0;
	double blobExtractMs = 0.0;
	double unpackMs = 0.0;
	double populateMetadataMs = 0.0;
	CacheUnpackTimings unpackTimings;
	auto recordTimings = [&]() {
		setStorageTiming( timings, "loadResolvePathMs", resolvePathMs );
		setStorageTiming( timings, "loadReadBytesMs", readBytesMs );
		setStorageTiming( timings, "loadBlobExtractMs", blobExtractMs );
		setStorageTiming( timings, "loadUnpackMs", unpackMs );
		setStorageTiming( timings, "loadPopulateMetadataMs", populateMetadataMs );
		setStorageTiming( timings, "unpackChecksumMs", unpackTimings.unpackChecksumMs );
		setStorageTiming( timings, "unpackHeaderMs", unpackTimings.unpackHeaderMs );
		setStorageTiming( timings, "unpackNodeMs", unpackTimings.unpackNodeMs );
		setStorageTiming( timings, "unpackLockMs", unpackTimings.unpackLockMs );
		setStorageTiming( timings, "unpackScenePathsMs", unpackTimings.unpackScenePathsMs );
		setStorageTiming( timings, "unpackLayersMs", unpackTimings.unpackLayersMs );
		setStorageTiming( timings, "unpackStrokesMs", unpackTimings.unpackStrokesMs );
		setStorageTiming( timings, "unpackChunksMs", unpackTimings.unpackChunksMs );
		setStorageTiming( timings, "unpackPointsMs", unpackTimings.unpackPointsMs );
		setStorageTiming( timings, "unpackSelectionSetsMs", unpackTimings.unpackSelectionSetsMs );
		setStorageTiming( timings, "unpackDiagnosticsMs", unpackTimings.unpackDiagnosticsMs );
		setStorageTiming( timings, "unpackUpgradesMs", unpackTimings.unpackUpgradesMs );
		setStorageTiming( timings, "unpackPointBackupsMs", unpackTimings.unpackPointBackupsMs );
	};

	if( node->cacheModePlug()->getValue() == static_cast<int>( g_cacheModeExternal ) )
	{
		const auto resolvePathStart = std::chrono::steady_clock::now();
		const std::string cachePath = resolvedCachePath( node );
		resolvePathMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - resolvePathStart ).count();
		if( cachePath.empty() )
		{
			if( error )
			{
				*error = "External cache mode requires a cache path";
			}
			recordTimings();
			return defaultSchemaForNode( node );
		}

		std::string blobBytes;
		try
		{
			const auto readBytesStart = std::chrono::steady_clock::now();
			blobBytes = readBytesFile( cachePath );
			readBytesMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - readBytesStart ).count();
		}
		catch( const std::runtime_error &e )
		{
			if( std::filesystem::exists( cachePath ) )
			{
				if( error )
				{
					*error = std::string( "Failed to read scatter paint blob: " ) + e.what();
				}
				return defaultSchemaForNode( node );
			}
			if( error )
			{
				*error = "External cache does not exist: " + cachePath;
			}
			recordTimings();
			return defaultSchemaForNode( node );
		}

		if( blobBytes.empty() )
		{
			if( error )
			{
				*error = "";
			}
			recordTimings();
			return defaultSchemaForNode( node );
		}

		std::string unpackError;
		const auto unpackStart = std::chrono::steady_clock::now();
		CacheSchema schema = unpackCacheSchema( blobBytes, unpackError, &unpackTimings, true );
		unpackMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - unpackStart ).count();
		if( !unpackError.empty() )
		{
			if( error )
			{
				*error = unpackError;
			}
			recordTimings();
			return defaultSchemaForNode( node );
		}
		const auto populateMetadataStart = std::chrono::steady_clock::now();
		populateSchemaNodeMetadata( node, schema );
		populateMetadataMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - populateMetadataStart ).count();
		if( error )
		{
			*error = "";
		}
		recordTimings();
		return schema;
	}

	const auto blobExtractStart = std::chrono::steady_clock::now();
	const std::string blobBytes = blobBytesFromObject( node->cacheBlobPlug()->getValue().get() );
	blobExtractMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - blobExtractStart ).count();
	if( blobBytes.empty() )
	{
		if( error )
		{
			*error = "";
		}
		recordTimings();
		return defaultSchemaForNode( node );
	}

	std::string unpackError;
	const auto unpackStart = std::chrono::steady_clock::now();
	CacheSchema schema = unpackCacheSchema( blobBytes, unpackError, &unpackTimings, false );
	unpackMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - unpackStart ).count();
	if( !unpackError.empty() )
	{
		if( error )
		{
			*error = unpackError;
		}
		recordTimings();
		return defaultSchemaForNode( node );
	}
	const auto populateMetadataStart = std::chrono::steady_clock::now();
	populateSchemaNodeMetadata( node, schema );
	populateMetadataMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - populateMetadataStart ).count();
	if( error )
	{
		*error = "";
	}
	recordTimings();
	return schema;
}

inline std::uint32_t internSchemaPath( std::vector<std::string> &paths, const std::string &path )
{
	if( path.empty() )
	{
		return 0;
	}
	for( std::size_t i = 0; i < paths.size(); ++i )
	{
		if( paths[i] == path )
		{
			return static_cast<std::uint32_t>( i + 1 );
		}
	}
	paths.push_back( path );
	return static_cast<std::uint32_t>( paths.size() );
}

inline CacheSchema loadPendingPaintSchemaForNode( const PaintedPoints *node, std::string *error = nullptr )
{
	const std::string blobBytes = blobBytesFromObject( node->pendingPaintBlobPlug()->getValue().get() );
	if( blobBytes.empty() )
	{
		if( error )
		{
			*error = "";
		}
		return defaultCacheSchema();
	}

	std::string unpackError;
	CacheSchema schema = unpackCacheSchema( blobBytes, unpackError, nullptr, false );
	if( error )
	{
		*error = unpackError;
	}
	return unpackError.empty() ? schema : defaultCacheSchema();
}

inline void mergePendingPaintSchemaInto( CacheSchema &schema, const CacheSchema &pendingSchema )
{
	schema.nextIds.layer = std::max( schema.nextIds.layer, pendingSchema.nextIds.layer );
	schema.nextIds.stroke = std::max( schema.nextIds.stroke, pendingSchema.nextIds.stroke );
	schema.nextIds.point = std::max( schema.nextIds.point, pendingSchema.nextIds.point );
	schema.nextIds.selectionSet = std::max( schema.nextIds.selectionSet, pendingSchema.nextIds.selectionSet );
	schema.nextIds.chunk = std::max( schema.nextIds.chunk, pendingSchema.nextIds.chunk );

	for( const LayerRecord &pendingLayer : pendingSchema.layers )
	{
		auto it = std::find_if(
			schema.layers.begin(), schema.layers.end(),
			[&pendingLayer]( const LayerRecord &layer ) {
				return layer.layerId == pendingLayer.layerId;
			}
		);
		if( it == schema.layers.end() )
		{
			schema.layers.push_back( pendingLayer );
		}
	}

	for( const StrokeRecord &pendingStroke : pendingSchema.strokes )
	{
		auto it = std::find_if(
			schema.strokes.begin(), schema.strokes.end(),
			[&pendingStroke]( const StrokeRecord &stroke ) {
				return stroke.strokeId == pendingStroke.strokeId;
			}
		);
		if( it == schema.strokes.end() )
		{
			schema.strokes.push_back( pendingStroke );
		}
	}

	for( const PointRecord &pendingPoint : pendingSchema.points )
	{
		PointRecord point = pendingPoint;
		if( pendingPoint.targetPathId > 0 && static_cast<std::size_t>( pendingPoint.targetPathId ) <= pendingSchema.scenePaths.size() )
		{
			point.targetPathId = internSchemaPath( schema.scenePaths, pendingSchema.scenePaths[pendingPoint.targetPathId - 1] );
		}
		if( pendingPoint.instanceSourcePathId > 0 && static_cast<std::size_t>( pendingPoint.instanceSourcePathId ) <= pendingSchema.instanceSourcePaths.size() )
		{
			point.instanceSourcePathId = internSchemaPath( schema.instanceSourcePaths, pendingSchema.instanceSourcePaths[pendingPoint.instanceSourcePathId - 1] );
		}
		schema.points.push_back( point );
	}
}

inline void appendPendingPaintPoints(
	CacheSchema &pendingSchema,
	const CacheSchema &workingSchema,
	const std::vector<PointRecord> &authoredPoints
)
{
	pendingSchema.nextIds = workingSchema.nextIds;
	for( const PointRecord &workingPoint : authoredPoints )
	{
		PointRecord pendingPoint = workingPoint;
		if( workingPoint.targetPathId > 0 && static_cast<std::size_t>( workingPoint.targetPathId ) <= workingSchema.scenePaths.size() )
		{
			pendingPoint.targetPathId = internSchemaPath( pendingSchema.scenePaths, workingSchema.scenePaths[workingPoint.targetPathId - 1] );
		}
		if( workingPoint.instanceSourcePathId > 0 && static_cast<std::size_t>( workingPoint.instanceSourcePathId ) <= workingSchema.instanceSourcePaths.size() )
		{
			pendingPoint.instanceSourcePathId = internSchemaPath( pendingSchema.instanceSourcePaths, workingSchema.instanceSourcePaths[workingPoint.instanceSourcePathId - 1] );
		}
		pendingSchema.points.push_back( pendingPoint );
	}
}

inline void rebuildSchemaChunksAndCounts( CacheSchema &schema, const std::set<std::uint64_t> &changedStrokeIds )
{
	struct StrokeAggregate
	{
		std::uint32_t pointCount = 0;
		std::set<std::uint32_t> uniqueTargets;
	};

	std::map<std::uint64_t, std::uint32_t> existingGeneration;
	for( const PointChunkRecord &chunk : schema.chunks )
	{
		existingGeneration[chunk.strokeId] = std::max( existingGeneration[chunk.strokeId], chunk.generation );
	}

	std::sort(
		schema.layers.begin(), schema.layers.end(),
		[]( const LayerRecord &a, const LayerRecord &b ) {
			return a.order < b.order;
		}
	);
	std::sort(
		schema.strokes.begin(), schema.strokes.end(),
		[]( const StrokeRecord &a, const StrokeRecord &b ) {
			return std::make_pair( a.layerId, a.order ) < std::make_pair( b.layerId, b.order );
		}
	);

	std::map<std::uint64_t, StrokeAggregate> strokeAggregates;
	for( const PointRecord &point : schema.points )
	{
		StrokeAggregate &aggregate = strokeAggregates[point.strokeId];
		++aggregate.pointCount;
		if( point.targetPathId )
		{
			aggregate.uniqueTargets.insert( point.targetPathId );
		}
	}

	std::vector<PointChunkRecord> chunks;
	chunks.reserve( schema.points.empty() ? schema.strokes.size() : ( schema.points.size() + g_chunkPointLimit - 1 ) / g_chunkPointLimit + schema.strokes.size() );
	std::uint32_t pointStart = 0;
	std::uint64_t nextChunkId = 1;

	for( LayerRecord &layer : schema.layers )
	{
		layer.firstStrokeId = 0;
		layer.lastStrokeId = 0;
		bool firstStrokeSet = false;
		for( StrokeRecord &stroke : schema.strokes )
		{
			if( stroke.layerId != layer.layerId )
			{
				continue;
			}
			if( !firstStrokeSet )
			{
				layer.firstStrokeId = stroke.strokeId;
				firstStrokeSet = true;
			}
			layer.lastStrokeId = stroke.strokeId;

			const auto aggregateIt = strokeAggregates.find( stroke.strokeId );
			const std::uint32_t pointCount = aggregateIt != strokeAggregates.end() ? aggregateIt->second.pointCount : 0;
			stroke.pointCount = pointCount;
			stroke.targetCount = aggregateIt != strokeAggregates.end() ? static_cast<std::uint32_t>( aggregateIt->second.uniqueTargets.size() ) : 0;
			std::uint32_t generation = existingGeneration[stroke.strokeId];
			if( changedStrokeIds.count( stroke.strokeId ) )
			{
				generation += 1;
			}

			stroke.firstChunkId = 0;
			stroke.lastChunkId = 0;
			for( std::uint32_t chunkIndex = 0, chunkStart = 0; chunkStart < pointCount || ( pointCount == 0 && chunkIndex == 0 ); chunkStart += g_chunkPointLimit, ++chunkIndex )
			{
				PointChunkRecord chunk;
				chunk.chunkId = nextChunkId++;
				chunk.strokeId = stroke.strokeId;
				chunk.chunkIndex = chunkIndex;
				chunk.pointStart = pointStart;
				chunk.pointCount = pointCount == 0 ? 0 : std::min<std::uint32_t>( g_chunkPointLimit, pointCount - chunkStart );
				chunk.generation = generation;
				chunk.deleted = false;
				chunks.push_back( chunk );
				if( chunkIndex == 0 )
				{
					stroke.firstChunkId = chunk.chunkId;
				}
				stroke.lastChunkId = chunk.chunkId;
				pointStart += chunk.pointCount;
				if( pointCount == 0 )
				{
					break;
				}
			}
		}
	}

	schema.chunks.swap( chunks );
	schema.nextIds.chunk = nextChunkId;
}

inline void pruneSchemaSelectionsToVisiblePointsAndStrokes( CacheSchema &schema )
{
	auto filterIds = []( std::vector<std::uint64_t> &ids, const std::set<std::uint64_t> &validIds ) {
		ids.erase(
			std::remove_if(
				ids.begin(), ids.end(),
				[&validIds]( std::uint64_t id ) {
					return !validIds.count( id );
				}
			),
			ids.end()
		);
	};

	std::set<std::uint64_t> validPointIds;
	std::set<std::uint64_t> validStrokeIds;
	for( const PointRecord &point : schema.points )
	{
		validPointIds.insert( point.pointId );
		validStrokeIds.insert( point.strokeId );
	}

	filterIds( schema.currentSelection.pointIds, validPointIds );
	filterIds( schema.currentSelection.strokeIds, validStrokeIds );
	for( SelectionSetRecord &selectionSet : schema.selectionSets )
	{
		filterIds( selectionSet.pointIds, validPointIds );
		filterIds( selectionSet.strokeIds, validStrokeIds );
	}
}

inline void writePendingPaintSchemaDirect( PaintedPoints *node, CacheSchema &schema, bp::dict *timings = nullptr )
{
	CachePackTimings packTimings;
	const auto packStart = std::chrono::steady_clock::now();
	IECore::UCharVectorDataPtr packedBlobObject = new IECore::UCharVectorData();
	auto &packedBlobWritable = packedBlobObject->writable();
	packCacheSchema( schema, packedBlobWritable, &packTimings );
	const double packMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - packStart ).count();
	const auto setBlobStart = std::chrono::steady_clock::now();
	node->pendingPaintBlobPlug()->setValue( packedBlobObject );
	const double blobPlugSetMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - setBlobStart ).count();

	setStorageTiming( timings, "writePopulateMetadataMs", 0.0 );
	setStorageTiming( timings, "writeLockMs", 0.0 );
	setStorageTiming( timings, "writePackMs", packMs );
	setStorageTiming( timings, "packHeaderMs", packTimings.packHeaderMs );
	setStorageTiming( timings, "packNodeMs", packTimings.packNodeMs );
	setStorageTiming( timings, "packLockMs", packTimings.packLockMs );
	setStorageTiming( timings, "packScenePathsMs", packTimings.packScenePathsMs );
	setStorageTiming( timings, "packLayersMs", packTimings.packLayersMs );
	setStorageTiming( timings, "packStrokesMs", packTimings.packStrokesMs );
	setStorageTiming( timings, "packChunksMs", packTimings.packChunksMs );
	setStorageTiming( timings, "packPointsMs", packTimings.packPointsMs );
	setStorageTiming( timings, "packPointsReserveMs", packTimings.packPointsReserveMs );
	setStorageTiming( timings, "packPointsReserveReallocated", packTimings.packPointsReserveReallocated );
	setStorageTiming( timings, "packPointsCapacityBeforeBytes", packTimings.packPointsCapacityBeforeBytes );
	setStorageTiming( timings, "packPointsCapacityAfterBytes", packTimings.packPointsCapacityAfterBytes );
	setStorageTiming( timings, "packPointsResizeMs", packTimings.packPointsResizeMs );
	setStorageTiming( timings, "packPointsFillMs", packTimings.packPointsFillMs );
	setStorageTiming( timings, "packPointsCopyMs", packTimings.packPointsCopyMs );
	setStorageTiming( timings, "packPointsChecksumInlineMs", packTimings.packPointsChecksumInlineMs );
	setStorageTiming( timings, "packPointsChecksumMs", packTimings.packPointsChecksumMs );
	setStorageTiming( timings, "packSelectionSetsMs", packTimings.packSelectionSetsMs );
	setStorageTiming( timings, "packDiagnosticsMs", packTimings.packDiagnosticsMs );
	setStorageTiming( timings, "packUpgradesMs", packTimings.packUpgradesMs );
	setStorageTiming( timings, "packPointBackupsMs", packTimings.packPointBackupsMs );
	setStorageTiming( timings, "packFinalChecksumMs", packTimings.packFinalChecksumMs );
	setStorageTiming( timings, "packFinalHeaderWriteMs", packTimings.packFinalHeaderWriteMs );
	setStorageTiming( timings, "packFinalBufferMs", packTimings.packFinalBufferMs );
	setStorageTiming( timings, "writeResolvePathMs", 0.0 );
	setStorageTiming( timings, "writeBackupMs", 0.0 );
	setStorageTiming( timings, "writeBytesMs", 0.0 );
	setStorageTiming( timings, "writeClearBlobMs", 0.0 );
	setStorageTiming( timings, "writeReleaseLockMs", 0.0 );
	setStorageTiming( timings, "writeBlobObjectMs", 0.0 );
	setStorageTiming( timings, "writeBlobObjectResizeMs", 0.0 );
	setStorageTiming( timings, "writeBlobObjectCopyMs", 0.0 );
	setStorageTiming( timings, "writeBlobPlugSetMs", blobPlugSetMs );
	setStorageTiming( timings, "writeSetBlobMs", blobPlugSetMs );
}

inline void applyTrustedDiagnostics( PaintedPoints *node, CacheSchema &schema )
{
	std::uint64_t resolvedCount = 0;
	for( const PointRecord &point : schema.points )
	{
		if( point.anchorModeUsed != AnchorMode::Reprojected )
		{
			++resolvedCount;
		}
	}
	const std::uint64_t fallbackCount = schema.points.size() - resolvedCount;
	schema.diagnostics.invalidPointCount = 0;
	schema.diagnostics.invalidStrokeCount = 0;
	schema.diagnostics.topologyMismatchCount = 0;
	schema.diagnostics.failingFrame = 0;
	schema.diagnostics.failingTargetPaths.clear();
	schema.diagnostics.lastErrorMessage.clear();
	schema.diagnostics.validationSummary =
		std::to_string( schema.layers.size() ) +
		" layers, " +
		std::to_string( schema.strokes.size() ) +
		" strokes, " +
		std::to_string( schema.points.size() ) +
		" points, " +
		std::to_string( schema.selectionSets.size() ) +
		" selection sets, " +
		std::to_string( resolvedCount ) +
		" resolved attachments, " +
		std::to_string( fallbackCount ) +
		" fallback attachments";
	schema.diagnostics.categories = { ValidationCategory::Diagnostics };
	if( !findAttachedPointsForPaintedNode( node ) )
	{
		schema.diagnostics.categories.push_back( ValidationCategory::ExportReadiness );
	}
}

inline void syncStateFromSchemaTrusted( PaintedPoints *node, const CacheSchema &schema )
{
	std::vector<LayerRecord> sortedLayers = schema.layers;
	std::sort(
		sortedLayers.begin(), sortedLayers.end(),
		[]( const LayerRecord &a, const LayerRecord &b ) {
			return a.order < b.order;
		}
	);

	std::vector<std::string> layerNames;
	layerNames.reserve( sortedLayers.size() );
	for( const LayerRecord &layer : sortedLayers )
	{
		layerNames.push_back( layer.name );
	}

	std::vector<std::string> selectionNames;
	selectionNames.reserve( schema.selectionSets.size() );
	for( const SelectionSetRecord &selectionSet : schema.selectionSets )
	{
		selectionNames.push_back( selectionSet.name );
	}

	std::vector<std::string> validationCategories;
	std::set<std::string> seenCategories;
	for( ValidationCategory category : schema.diagnostics.categories )
	{
		const std::string name = validationCategoryName( static_cast<int>( category ) );
		if( seenCategories.insert( name ).second )
		{
			validationCategories.push_back( name );
		}
	}
	if( validationCategories.empty() )
	{
		validationCategories.push_back( validationCategoryName( static_cast<int>( ValidationCategory::Diagnostics ) ) );
	}

	node->layersPlug()->setValue( new StringVectorData( layerNames ) );
	node->selectionSetsPlug()->setValue( new StringVectorData( selectionNames ) );
	node->authoredPointCountPlug()->setValue( static_cast<int>( schema.points.size() ) );
	node->cacheVersionPlug()->setValue( g_schemaVersion );
	node->invalidPointCountPlug()->setValue( static_cast<int>( schema.diagnostics.invalidPointCount ) );
	node->invalidStrokeCountPlug()->setValue( static_cast<int>( schema.diagnostics.invalidStrokeCount ) );
	node->failingFramePlug()->setValue( schema.diagnostics.failingFrame );
	node->failingTargetPathsPlug()->setValue( new StringVectorData( schema.diagnostics.failingTargetPaths ) );
	node->lastErrorMessagePlug()->setValue( schema.diagnostics.lastErrorMessage );
	node->topologyMismatchCountPlug()->setValue( static_cast<int>( schema.diagnostics.topologyMismatchCount ) );
	node->validationSummaryPlug()->setValue( schema.diagnostics.validationSummary.empty() ? g_schemaDescription : schema.diagnostics.validationSummary );
	node->validationCategoriesPlug()->setValue( new StringVectorData( validationCategories ) );
	node->cacheResolvedPathPlug()->setValue( resolvedCachePath( node ) );
	node->cacheLockedByPlug()->setValue( schema.lock.user );
	node->cacheLockedHostPlug()->setValue( schema.lock.host );
	node->cacheLockedTimePlug()->setValue( schema.lock.timestampUtc );
	node->cacheLockedScriptPlug()->setValue( schema.lock.scriptPath );

	bp::object nodeObject( bp::ptr( node ) );
	if( PyObject_SetAttrString( nodeObject.ptr(), "_PaintedPoints__authoredPointCount", bp::object( static_cast<int>( schema.points.size() ) ).ptr() ) != 0 )
	{
		PyErr_Clear();
	}
}

inline bp::dict writeCacheSchemaDirect( PaintedPoints *node, CacheSchema &schema, bp::dict *timings = nullptr )
{
	double populateMetadataMs = 0.0;
	double lockMs = 0.0;
	double packMs = 0.0;
	double resolvePathMs = 0.0;
	double backupMs = 0.0;
	double writeBytesMs = 0.0;
	double clearBlobMs = 0.0;
	double releaseLockMs = 0.0;
	double blobObjectMs = 0.0;
	double blobObjectResizeMs = 0.0;
	double blobObjectCopyMs = 0.0;
	double blobPlugSetMs = 0.0;
	double setBlobMs = 0.0;
	CachePackTimings packTimings;
	auto recordTimings = [&]() {
		setStorageTiming( timings, "writePopulateMetadataMs", populateMetadataMs );
		setStorageTiming( timings, "writeLockMs", lockMs );
		setStorageTiming( timings, "writePackMs", packMs );
		setStorageTiming( timings, "packHeaderMs", packTimings.packHeaderMs );
		setStorageTiming( timings, "packNodeMs", packTimings.packNodeMs );
		setStorageTiming( timings, "packLockMs", packTimings.packLockMs );
		setStorageTiming( timings, "packScenePathsMs", packTimings.packScenePathsMs );
		setStorageTiming( timings, "packLayersMs", packTimings.packLayersMs );
		setStorageTiming( timings, "packStrokesMs", packTimings.packStrokesMs );
		setStorageTiming( timings, "packChunksMs", packTimings.packChunksMs );
		setStorageTiming( timings, "packPointsMs", packTimings.packPointsMs );
		setStorageTiming( timings, "packPointsReserveMs", packTimings.packPointsReserveMs );
		setStorageTiming( timings, "packPointsReserveReallocated", packTimings.packPointsReserveReallocated );
		setStorageTiming( timings, "packPointsCapacityBeforeBytes", packTimings.packPointsCapacityBeforeBytes );
		setStorageTiming( timings, "packPointsCapacityAfterBytes", packTimings.packPointsCapacityAfterBytes );
		setStorageTiming( timings, "packPointsResizeMs", packTimings.packPointsResizeMs );
		setStorageTiming( timings, "packPointsFillMs", packTimings.packPointsFillMs );
		setStorageTiming( timings, "packPointsCopyMs", packTimings.packPointsCopyMs );
		setStorageTiming( timings, "packPointsChecksumInlineMs", packTimings.packPointsChecksumInlineMs );
		setStorageTiming( timings, "packPointsChecksumMs", packTimings.packPointsChecksumMs );
		setStorageTiming( timings, "packSelectionSetsMs", packTimings.packSelectionSetsMs );
		setStorageTiming( timings, "packDiagnosticsMs", packTimings.packDiagnosticsMs );
		setStorageTiming( timings, "packUpgradesMs", packTimings.packUpgradesMs );
		setStorageTiming( timings, "packPointBackupsMs", packTimings.packPointBackupsMs );
		setStorageTiming( timings, "packFinalChecksumMs", packTimings.packFinalChecksumMs );
		setStorageTiming( timings, "packFinalHeaderWriteMs", packTimings.packFinalHeaderWriteMs );
		setStorageTiming( timings, "packFinalBufferMs", packTimings.packFinalBufferMs );
		setStorageTiming( timings, "writeResolvePathMs", resolvePathMs );
		setStorageTiming( timings, "writeBackupMs", backupMs );
		setStorageTiming( timings, "writeBytesMs", writeBytesMs );
		setStorageTiming( timings, "writeClearBlobMs", clearBlobMs );
		setStorageTiming( timings, "writeReleaseLockMs", releaseLockMs );
		setStorageTiming( timings, "writeBlobObjectMs", blobObjectMs );
		setStorageTiming( timings, "writeBlobObjectResizeMs", blobObjectResizeMs );
		setStorageTiming( timings, "writeBlobObjectCopyMs", blobObjectCopyMs );
		setStorageTiming( timings, "writeBlobPlugSetMs", blobPlugSetMs );
		setStorageTiming( timings, "writeSetBlobMs", setBlobMs );
	};
	const auto populateMetadataStart = std::chrono::steady_clock::now();
	populateSchemaNodeMetadata( node, schema );
	populateMetadataMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - populateMetadataStart ).count();
	const auto lockStart = std::chrono::steady_clock::now();
	bp::dict lock = ensureWriteLock( node );
	lockMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - lockStart ).count();
	schema.lock.mode = static_cast<LockMode>( dictValue<std::uint32_t>( lock, "mode", 0 ) );
	schema.lock.user = dictValue<std::string>( lock, "user", "" );
	schema.lock.host = dictValue<std::string>( lock, "host", "" );
	schema.lock.timestampUtc = dictValue<std::string>( lock, "timestampUtc", "" );
	schema.lock.scriptPath = dictValue<std::string>( lock, "scriptPath", "" );
	schema.lock.projectPath = dictValue<std::string>( lock, "projectPath", "" );
	schema.lock.sessionId = dictValue<std::string>( lock, "sessionId", "" );

	const auto packStart = std::chrono::steady_clock::now();
	std::string packedBytes;
	IECore::UCharVectorDataPtr packedBlobObject;
	if( node->cacheModePlug()->getValue() == static_cast<int>( g_cacheModeExternal ) )
	{
		packedBytes = packCacheSchema( schema, &packTimings );
	}
	else
	{
		packedBlobObject = new IECore::UCharVectorData();
		auto &packedBlobWritable = packedBlobObject->writable();
		packCacheSchema( schema, packedBlobWritable, &packTimings );
	}
	packMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - packStart ).count();
	if( node->cacheModePlug()->getValue() == static_cast<int>( g_cacheModeExternal ) )
	{
		const auto resolvePathStart = std::chrono::steady_clock::now();
		const std::string cachePath = resolvedCachePath( node );
		resolvePathMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - resolvePathStart ).count();
		if( cachePath.empty() )
		{
			recordTimings();
			throw std::runtime_error( "External cache mode requires a cache path" );
		}

		try
		{
			const auto backupStart = std::chrono::steady_clock::now();
			writeBackupIfEnabled( node, cachePath );
			backupMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - backupStart ).count();
			const auto writeBytesStart = std::chrono::steady_clock::now();
			writeBytesFile( cachePath, packedBytes );
			writeBytesMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - writeBytesStart ).count();
			const auto clearBlobStart = std::chrono::steady_clock::now();
			node->cacheBlobPlug()->setValue( emptyBlobObject() );
			clearBlobMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - clearBlobStart ).count();
		}
		catch( ... )
		{
			recordTimings();
			releaseWriteLock( node, &lock );
			throw;
		}

		const auto releaseLockStart = std::chrono::steady_clock::now();
		releaseWriteLock( node, &lock );
		releaseLockMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - releaseLockStart ).count();
		recordTimings();
		return lock;
	}

	blobObjectMs = 0.0;
	blobObjectResizeMs = 0.0;
	blobObjectCopyMs = 0.0;
	const auto setBlobStart = std::chrono::steady_clock::now();
	node->cacheBlobPlug()->setValue( packedBlobObject );
	blobPlugSetMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - setBlobStart ).count();
	setBlobMs = blobObjectMs + blobPlugSetMs;
	recordTimings();
	return lock;
}
