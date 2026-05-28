#include "PaintedPointsPrivate.h"

CacheSchema PaintedPoints::visibleSchema() const
{
	std::string loadError;
	CacheSchema schema = loadCacheSchemaForNode( this, &loadError );
	if( !loadError.empty() )
	{
		throw std::runtime_error( loadError );
	}

	std::string pendingError;
	const CacheSchema pendingSchema = loadPendingPaintSchemaForNode( this, &pendingError );
	if( !pendingError.empty() )
	{
		throw std::runtime_error( pendingError );
	}
	mergePendingPaintSchemaInto( schema, pendingSchema );

	applyInteractiveOverlayToSchema( this, schema );
	rebuildSchemaChunksAndCounts( schema, {} );
	pruneSchemaSelectionsToVisiblePointsAndStrokes( schema );
	applyTrustedDiagnostics( const_cast<PaintedPoints *>( this ), schema );
	return schema;
}

bp::object PaintedPoints::cacheSnapshot() const
{
	return cacheSchemaToDict( visibleSchema() );
}

bp::object PaintedPoints::layerRecords() const
{
	bp::dict store = bp::extract<bp::dict>( cacheSnapshot() );
	return deepCopyList( bp::extract<bp::list>( store["layers"] ) );
}

bp::object PaintedPoints::strokeRecords() const
{
	bp::dict store = bp::extract<bp::dict>( cacheSnapshot() );
	return deepCopyList( bp::extract<bp::list>( store["strokes"] ) );
}

bp::object PaintedPoints::pointRecords() const
{
	bp::dict store = bp::extract<bp::dict>( cacheSnapshot() );
	bp::list points = bp::extract<bp::list>( store["points"] );
	bp::list result;
	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		result.append( deepCopyDict( expandedPointRecord( store, bp::extract<bp::dict>( *it ) ) ) );
	}
	return result;
}

bp::object PaintedPoints::mutateCacheStore( const bp::object &mutator )
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	bp::object result = mutator( store );
	refreshDerivedData( store );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return result;
}

std::uint64_t PaintedPoints::interactiveRevision() const
{
	return interactivePaintRevision( this );
}

std::string PaintedPoints::validateCache()
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	syncStateFromStore( this, store, loadError );
	return validationSummaryPlug()->getValue();
}

std::string PaintedPoints::validateAttachments()
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	if( !loadError.empty() )
	{
		throw std::runtime_error( loadError );
	}

	const bp::list points = bp::extract<bp::list>( store["points"] );
	int unresolvedCount = 0;
	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		const bp::dict point = bp::extract<bp::dict>( *it );
		if( dictValue<int>( point, "anchorModeUsed", 3 ) == 3 )
		{
			++unresolvedCount;
		}
	}

	const int pointCount = bp::len( points );
	return std::to_string( pointCount ) + " authored points, " + std::to_string( unresolvedCount ) + " unresolved attachment fallbacks";
}

std::uint32_t PaintedPoints::migrateCacheMode()
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	const int currentMode = cacheModePlug()->getValue();
	if( currentMode == static_cast<int>( g_cacheModeExternal ) )
	{
		cacheModePlug()->setValue( 0 );
		writeStore( this, store );
	}
	else
	{
		const std::string cachePath = resolvedCachePathForMode( this, static_cast<int>( g_cacheModeExternal ) );
		if( cachePath.empty() )
		{
			throw std::runtime_error( "External cache mode requires a cache path" );
		}
		cacheModePlug()->setValue( static_cast<int>( g_cacheModeExternal ) );
		writeStore( this, store );
	}

	syncStateFromStore( this, loadStore( this, &loadError ), loadError );
	return g_schemaVersion;
}

std::string PaintedPoints::relinkCache()
{
	if( cacheModePlug()->getValue() != static_cast<int>( g_cacheModeExternal ) )
	{
		throw std::runtime_error( "Relink is only valid in external cache mode" );
	}

	const std::string cachePath = resolvedCachePath( this );
	if( cachePath.empty() || !std::filesystem::exists( cachePath ) )
	{
		throw std::runtime_error( "External cache does not exist: " + cachePath );
	}

	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	if( !loadError.empty() )
	{
		throw std::runtime_error( loadError );
	}

	syncStateFromStore( this, store, loadError );
	return cachePath;
}

bp::object PaintedPoints::upgradeCache()
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	if( !loadError.empty() )
	{
		throw std::runtime_error( loadError );
	}

	const std::uint32_t sourceVersion = dictValue<std::uint32_t>( store, "schemaVersion", g_schemaVersion );
	if( sourceVersion == g_schemaVersion )
	{
		syncStateFromStore( this, store, loadError );
		return bp::object( g_schemaVersion );
	}

	bp::dict upgradedStore = deepCopyDict( store );
	upgradedStore["schemaVersion"] = g_schemaVersion;
	bp::list upgrades;
	if( upgradedStore.has_key( "upgrades" ) )
	{
		upgrades = bp::extract<bp::list>( upgradedStore["upgrades"] );
	}
	bp::dict upgrade;
	upgrade["sourceSchemaVersion"] = sourceVersion;
	upgrade["upgradedSchemaVersion"] = g_schemaVersion;
	upgrade["timestampUtc"] = utcTimestamp();
	upgrade["report"] = "Upgraded scatter paint blob from schema v" + std::to_string( sourceVersion ) + " to v" + std::to_string( g_schemaVersion ) + ".";
	upgrades.append( upgrade );
	upgradedStore["upgrades"] = upgrades;

	if( cacheModePlug()->getValue() == static_cast<int>( g_cacheModeExternal ) )
	{
		const std::string cachePath = resolvedCachePath( this );
		if( cachePath.empty() )
		{
			throw std::runtime_error( "External cache mode requires a cache path" );
		}

		const std::string upgradedPath = upgradeExportPath( this );
		const std::string originalCachePath = cachePathPlug()->getValue();
		cachePathPlug()->setValue( upgradedPath );
		try
		{
			writeStore( this, upgradedStore );
		}
		catch( ... )
		{
			cachePathPlug()->setValue( originalCachePath );
			throw;
		}
		cachePathPlug()->setValue( originalCachePath );
		syncStateFromStore( this, loadStore( this, &loadError ), loadError );
		return bp::object( upgradedPath );
	}

	writeStore( this, upgradedStore );
	syncStateFromStore( this, loadStore( this, &loadError ), loadError );
	return bp::object( g_schemaVersion );
}


size_t PaintedPoints::compactCache()
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	bp::list strokes = bp::extract<bp::list>( store["strokes"] );
	std::set<std::uint64_t> changedStrokeIds;
	for( bp::stl_input_iterator<bp::object> it( strokes ), end; it != end; ++it )
	{
		changedStrokeIds.insert( dictValue<std::uint64_t>( bp::extract<bp::dict>( *it ), "strokeId", 0 ) );
	}
	refreshDerivedData( store, changedStrokeIds );
	const size_t chunkCount = static_cast<size_t>( bp::len( bp::extract<bp::list>( store["chunks"] ) ) );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return chunkCount;
}
