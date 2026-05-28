#pragma once

inline bp::dict nodeMetadata( const PaintedPoints *node )
{
	bp::dict result;
	result["storageMode"] = node->cacheModePlug()->getValue();
	result["pathMode"] = node->cachePathModePlug()->getValue();
	result["projectRoot"] = node->projectRootPlug()->getValue();
	result["cachePath"] = node->cachePathPlug()->getValue();
	result["defaultColor"] = bp::list();
	result["defaultColor"].attr( "append" )( node->defaultColorPlug()->getValue()[0] );
	result["defaultColor"].attr( "append" )( node->defaultColorPlug()->getValue()[1] );
	result["defaultColor"].attr( "append" )( node->defaultColorPlug()->getValue()[2] );
	result["exportPreset"] = "";
	result["backupEnabled"] = node->backupEnabledPlug()->getValue();
	result["diagnosticsSnapshotEnabled"] = true;
	return result;
}

inline std::string scriptFilePath( const PaintedPoints *node )
{
	const ScriptNode *script = node->ancestor<ScriptNode>();
	if( !script || !script->fileNamePlug() )
	{
		return std::string();
	}

	return script->fileNamePlug()->getValue();
}

inline std::string resolvedCachePathForMode( const PaintedPoints *node, int cacheMode )
{
	if( cacheMode != static_cast<int>( g_cacheModeExternal ) )
	{
		return std::string();
	}

	const std::string cachePath = node->cachePathPlug()->getValue();
	if( cachePath.empty() )
	{
		return std::string();
	}

	namespace fs = std::filesystem;
	fs::path path( cachePath );
	if( path.is_absolute() )
	{
		return path.lexically_normal().string();
	}

	const int pathMode = node->cachePathModePlug()->getValue();
	if( pathMode == static_cast<int>( g_cachePathModeRelativeToProject ) )
	{
		const std::string projectRoot = node->projectRootPlug()->getValue();
		if( !projectRoot.empty() )
		{
			return ( fs::path( projectRoot ) / path ).lexically_normal().string();
		}
	}

	if( pathMode == static_cast<int>( g_cachePathModeRelativeToScript ) )
	{
		const std::string scriptPath = scriptFilePath( node );
		if( !scriptPath.empty() )
		{
			return ( fs::path( scriptPath ).parent_path() / path ).lexically_normal().string();
		}
	}

	return path.lexically_normal().string();
}

inline std::string resolvedCachePath( const PaintedPoints *node )
{
	return resolvedCachePathForMode( node, node->cacheModePlug()->getValue() );
}

inline std::string cacheTextPath( const std::string &basePath, const std::string &suffix )
{
	namespace fs = std::filesystem;
	const fs::path path( basePath );
	const std::string extension = path.extension().string();
	if( extension.empty() )
	{
		return basePath + suffix;
	}

	return ( path.parent_path() / ( path.stem().string() + suffix ) ).string();
}

inline std::string utcTimestamp()
{
	return utcTimestampNow();
}

inline std::string lockFilePath( const std::string &cachePath )
{
	return cachePath.empty() ? std::string() : cachePath + ".lock";
}

inline std::string readBytesFile( const std::string &path )
{
	std::ifstream stream( path, std::ios::binary );
	if( !stream )
	{
		throw std::runtime_error( "Unable to open file for reading: " + path );
	}

	return std::string(
		std::istreambuf_iterator<char>( stream ),
		std::istreambuf_iterator<char>()
	);
}

inline std::string diagnosticsExportPath( const PaintedPoints *node )
{
	const std::string cachePath = resolvedCachePath( node );
	if( !cachePath.empty() )
	{
		return cacheTextPath( cachePath, ".diagnostics.txt" );
	}

	namespace fs = std::filesystem;
	const std::string scriptPath = scriptFilePath( node );
	if( !scriptPath.empty() )
	{
		return ( fs::path( scriptPath ).parent_path() / ( node->getName().string() + "_diagnostics.txt" ) ).string();
	}

	return ( fs::current_path() / ( node->getName().string() + "_diagnostics.txt" ) ).string();
}

inline std::string authoredExportPath( const PaintedPoints *node )
{
	const std::string cachePath = resolvedCachePath( node );
	if( !cachePath.empty() )
	{
		return cacheTextPath( cachePath, ".authored.bin" );
	}

	namespace fs = std::filesystem;
	const std::string scriptPath = scriptFilePath( node );
	if( !scriptPath.empty() )
	{
		return ( fs::path( scriptPath ).parent_path() / ( node->getName().string() + "_authored.bin" ) ).string();
	}

	return ( fs::current_path() / ( node->getName().string() + "_authored.bin" ) ).string();
}

inline std::string interchangeExportPath( const PaintedPoints *node )
{
	const std::string cachePath = resolvedCachePath( node );
	if( !cachePath.empty() )
	{
		return cacheTextPath( cachePath, ".interchange.bin" );
	}

	namespace fs = std::filesystem;
	const std::string scriptPath = scriptFilePath( node );
	if( !scriptPath.empty() )
	{
		return ( fs::path( scriptPath ).parent_path() / ( node->getName().string() + "_interchange.bin" ) ).string();
	}

	return ( fs::current_path() / ( node->getName().string() + "_interchange.bin" ) ).string();
}

inline std::string evaluatedExportPath( const PaintedPoints *node )
{
	const std::string cachePath = resolvedCachePath( node );
	if( !cachePath.empty() )
	{
		return cacheTextPath( cachePath, ".evaluated.cob" );
	}

	namespace fs = std::filesystem;
	const std::string scriptPath = scriptFilePath( node );
	if( !scriptPath.empty() )
	{
		return ( fs::path( scriptPath ).parent_path() / ( node->getName().string() + "_evaluated.cob" ) ).string();
	}

	return ( fs::current_path() / ( node->getName().string() + "_evaluated.cob" ) ).string();
}

inline std::string geometryExportPath( const PaintedPoints *node, const std::string &suffix )
{
	const std::string cachePath = resolvedCachePath( node );
	if( !cachePath.empty() )
	{
		return cacheTextPath( cachePath, suffix );
	}

	namespace fs = std::filesystem;
	const std::string scriptPath = scriptFilePath( node );
	if( !scriptPath.empty() )
	{
		return ( fs::path( scriptPath ).parent_path() / ( node->getName().string() + suffix ) ).string();
	}

	return ( fs::current_path() / ( node->getName().string() + suffix ) ).string();
}

inline std::string gafferSceneExportPath( const PaintedPoints *node )
{
	return geometryExportPath( node, "_scene.scc" );
}

inline std::string usdExportPath( const PaintedPoints *node )
{
	return geometryExportPath( node, "_scene.usda" );
}

inline std::string alembicExportPath( const PaintedPoints *node )
{
	return geometryExportPath( node, "_scene.abc" );
}

inline std::string upgradeExportPath( const PaintedPoints *node )
{
	const std::string cachePath = resolvedCachePath( node );
	if( cachePath.empty() )
	{
		return std::string();
	}

	return cacheTextPath( cachePath, ".v" + std::to_string( g_schemaVersion ) + ".upgrade" );
}

inline void writeTextFile( const std::string &path, const std::string &content )
{
	namespace fs = std::filesystem;
	const fs::path outputPath( path );
	if( !outputPath.parent_path().empty() )
	{
		fs::create_directories( outputPath.parent_path() );
	}

	std::ofstream stream( outputPath, std::ios::binary | std::ios::trunc );
	if( !stream )
	{
		throw std::runtime_error( "Unable to open diagnostics export path: " + path );
	}

	stream << content;
	if( !stream )
	{
		throw std::runtime_error( "Failed writing diagnostics export path: " + path );
	}
}

inline void writeBytesFile( const std::string &path, const std::string &content )
{
	namespace fs = std::filesystem;
	const fs::path outputPath( path );
	if( !outputPath.parent_path().empty() )
	{
		fs::create_directories( outputPath.parent_path() );
	}

	const fs::path tempPath = outputPath.string() + ".tmp";
	std::ofstream stream( tempPath, std::ios::binary | std::ios::trunc );
	if( !stream )
	{
		throw std::runtime_error( "Unable to open binary export path: " + path );
	}

	stream.write( content.data(), static_cast<std::streamsize>( content.size() ) );
	if( !stream )
	{
		throw std::runtime_error( "Failed writing binary export path: " + path );
	}
	stream.close();
	if( fs::exists( outputPath ) )
	{
		fs::remove( outputPath );
	}
	fs::rename( tempPath, outputPath );
}

inline void writeObjectFile( const std::string &path, const IECore::Object *object )
{
	if( !object )
	{
		throw std::runtime_error( "Unable to export evaluated points: no object was available" );
	}

	namespace fs = std::filesystem;
	const fs::path outputPath( path );
	if( !outputPath.parent_path().empty() )
	{
		fs::create_directories( outputPath.parent_path() );
	}

	IECore::WriterPtr writer = IECore::Writer::create( object->copy(), outputPath.string() );
	if( !writer )
	{
		throw std::runtime_error( "Unable to create evaluated points writer for: " + path );
	}

	writer->write();
}

inline std::string exportEvaluatedScene( const PaintedPoints *node, const std::string &path )
{
	AttachedPoints *attachedPoints = findAttachedPointsForPaintedNode( node );
	if( !attachedPoints )
	{
		throw std::runtime_error( "Unable to export evaluated scene: no AttachedPoints node is connected to this PaintedPoints output" );
	}

	const std::string outputLocation = attachedPoints->outputLocationPlug()->getValue().empty() ? "/scatter" : attachedPoints->outputLocationPlug()->getValue();
	const ScenePlug *scene = attachedPoints->outPlug();
	const ScenePlug::ScenePath scenePath = ScenePlug::stringToPath( outputLocation );
	if( !scene->exists( scenePath ) )
	{
		throw std::runtime_error( "Unable to export evaluated scene: AttachedPoints output location does not exist at " + outputLocation );
	}

	ConstObjectPtr sceneObject = scene->object( scenePath );
	IECoreScene::ConstPointsPrimitivePtr primitive = runTimeCast<const IECoreScene::PointsPrimitive>( sceneObject );
	if( !primitive )
	{
		throw std::runtime_error(
			"Unable to export evaluated scene: AttachedPoints output at " +
			outputLocation +
			" is not a PointsPrimitive"
		);
	}

	namespace fs = std::filesystem;
	const fs::path exportPath( path );
	if( !exportPath.parent_path().empty() )
	{
		fs::create_directories( exportPath.parent_path() );
	}

	const std::string extension = exportPath.extension().string();
	if(
		extension == ".usd" ||
		extension == ".usda" ||
		extension == ".usdc" ||
		extension == ".usdz"
	)
	{
		bp::import( "IECoreUSD" );
		bp::import( "GafferUSD" );
	}
	else if( extension == ".abc" )
	{
		bp::import( "IECoreAlembic" );
	}

	IECoreScene::PointsPrimitivePtr exportPrimitive = runTimeCast<IECoreScene::PointsPrimitive>( primitive->copy() );
	if( !exportPrimitive )
	{
		throw std::runtime_error( "Unable to export evaluated scene: failed to copy evaluated points" );
	}

	std::string pointType = "gl:point";
	IECoreScene::PrimitiveVariableMap::const_iterator typeIt = exportPrimitive->variables.find( "type" );
	if( typeIt != exportPrimitive->variables.end() )
	{
		if( const StringData *typeData = runTimeCast<const StringData>( typeIt->second.data.get() ) )
		{
			pointType = typeData->readable();
		}
	}
	exportPrimitive->variables["type"] = IECoreScene::PrimitiveVariable(
		IECoreScene::PrimitiveVariable::Constant,
		new StringData( pointType )
	);

	std::vector<std::string> pathComponents;
	pathComponents.reserve( scenePath.size() );
	for( const ScenePlug::ScenePath::value_type &component : scenePath )
	{
		pathComponents.push_back( component.string() );
	}
	if( pathComponents.empty() )
	{
		throw std::runtime_error( "Unable to export evaluated scene: output location must not be the scene root" );
	}

	ObjectToScenePtr objectToScene = new ObjectToScene( "ScatterPaintSceneObject" );
	objectToScene->namePlug()->setValue( pathComponents.back() );
	objectToScene->objectPlug()->setValue( exportPrimitive );

	ScenePlug *exportScene = objectToScene->outPlug();
	std::vector<GroupPtr> groups;
	groups.reserve( pathComponents.size() > 1 ? pathComponents.size() - 1 : 0 );
	for( std::vector<std::string>::const_reverse_iterator it = pathComponents.rbegin() + 1, eIt = pathComponents.rend(); it != eIt; ++it )
	{
		GroupPtr group = new Group( "ScatterPaintSceneGroup_" + *it );
		group->namePlug()->setValue( *it );
		group->inPlug()->setInput( exportScene );
		groups.push_back( group );
		exportScene = group->outPlug();
	}

	SceneWriterPtr writer = new SceneWriter( "ScatterPaintSceneWriter" );
	writer->inPlug()->setInput( exportScene );
	writer->fileNamePlug()->setValue( exportPath.string() );
	writer->taskPlug()->execute();
	return exportPath.string();
}

inline void copyFile( const std::string &sourcePath, const std::string &destinationPath )
{
	writeBytesFile( destinationPath, readBytesFile( sourcePath ) );
}

inline bp::dict activeLockMetadata( const PaintedPoints *node )
{
	const std::string scriptPath = scriptFilePath( node );
	const ScriptNode *script = node->ancestor<ScriptNode>();
	LockMetadata metadata;
	metadata.mode = static_cast<LockMode>( node->lockModePlug()->getValue() );
	metadata.user = extractOr<std::string>( bp::import( "getpass" ).attr( "getuser" )(), "" );
	metadata.host = extractOr<std::string>( bp::import( "socket" ).attr( "gethostname" )(), "" );
	metadata.timestampUtc = utcTimestamp();
	metadata.scriptPath = scriptPath;
	metadata.projectPath = scriptPath.empty() ? std::string() : std::filesystem::path( scriptPath ).parent_path().string();
	metadata.sessionId = std::to_string( static_cast<long long>( ::getpid() ) ) + ":" + std::to_string( reinterpret_cast<std::uintptr_t>( script ) );

	bp::dict lock;
	lock["mode"] = static_cast<std::uint32_t>( metadata.mode );
	lock["user"] = metadata.user;
	lock["host"] = metadata.host;
	lock["timestampUtc"] = metadata.timestampUtc;
	lock["scriptPath"] = metadata.scriptPath;
	lock["projectPath"] = metadata.projectPath;
	lock["sessionId"] = metadata.sessionId;
	return lock;
}

inline bp::dict readExternalLock( const PaintedPoints *node )
{
	const std::string path = lockFilePath( resolvedCachePath( node ) );
	if( path.empty() || !std::filesystem::exists( path ) )
	{
		return bp::dict();
	}

	std::string unpackError;
	const LockMetadata metadata = unpackLockMetadata( readBytesFile( path ), unpackError );
	if( !unpackError.empty() )
	{
		throw std::runtime_error( unpackError );
	}

	bp::dict lock;
	lock["mode"] = static_cast<std::uint32_t>( metadata.mode );
	lock["user"] = metadata.user;
	lock["host"] = metadata.host;
	lock["timestampUtc"] = metadata.timestampUtc;
	lock["scriptPath"] = metadata.scriptPath;
	lock["projectPath"] = metadata.projectPath;
	lock["sessionId"] = metadata.sessionId;
	return lock;
}

inline bool sameSessionLock( const bp::dict &existingLock, const bp::dict &activeLock )
{
	return
		dictValue<std::string>( existingLock, "sessionId", "" ) == dictValue<std::string>( activeLock, "sessionId", "" ) &&
		dictValue<std::string>( existingLock, "host", "" ) == dictValue<std::string>( activeLock, "host", "" ) &&
		dictValue<std::string>( existingLock, "user", "" ) == dictValue<std::string>( activeLock, "user", "" );
}

inline bp::dict ensureWriteLock( const PaintedPoints *node )
{
	bp::dict activeLock = activeLockMetadata( node );
	if( node->cacheModePlug()->getValue() != static_cast<int>( g_cacheModeExternal ) )
	{
		return activeLock;
	}

	bp::dict existingLock;
	try
	{
		existingLock = readExternalLock( node );
	}
	catch( const std::exception &e )
	{
		throw std::runtime_error( std::string( "Unable to read scatter paint lock: " ) + e.what() );
	}

	if( bp::len( existingLock ) && !sameSessionLock( existingLock, activeLock ) )
	{
		LockMetadata existingMetadata;
		existingMetadata.mode = static_cast<LockMode>( dictValue<std::uint32_t>( existingLock, "mode", 0 ) );
		existingMetadata.user = dictValue<std::string>( existingLock, "user", "" );
		existingMetadata.host = dictValue<std::string>( existingLock, "host", "" );
		existingMetadata.timestampUtc = dictValue<std::string>( existingLock, "timestampUtc", "" );
		existingMetadata.scriptPath = dictValue<std::string>( existingLock, "scriptPath", "" );
		existingMetadata.projectPath = dictValue<std::string>( existingLock, "projectPath", "" );
		existingMetadata.sessionId = dictValue<std::string>( existingLock, "sessionId", "" );
		const bool stale = isStaleLock( existingMetadata, g_lockStaleSeconds );
		if( !stale )
		{
			throw std::runtime_error(
				"External cache is locked by " +
				dictValue<std::string>( existingLock, "user", "" ) + "@" +
				dictValue<std::string>( existingLock, "host", "" ) +
				" for " + dictValue<std::string>( existingLock, "scriptPath", "" )
			);
		}
	}

	const std::string path = lockFilePath( resolvedCachePath( node ) );
	if( path.empty() )
	{
		throw std::runtime_error( "External cache mode requires a cache path" );
	}

	LockMetadata activeMetadata;
	activeMetadata.mode = static_cast<LockMode>( dictValue<std::uint32_t>( activeLock, "mode", 0 ) );
	activeMetadata.user = dictValue<std::string>( activeLock, "user", "" );
	activeMetadata.host = dictValue<std::string>( activeLock, "host", "" );
	activeMetadata.timestampUtc = dictValue<std::string>( activeLock, "timestampUtc", "" );
	activeMetadata.scriptPath = dictValue<std::string>( activeLock, "scriptPath", "" );
	activeMetadata.projectPath = dictValue<std::string>( activeLock, "projectPath", "" );
	activeMetadata.sessionId = dictValue<std::string>( activeLock, "sessionId", "" );
	writeBytesFile( path, packLockMetadata( activeMetadata ) );
	return activeLock;
}

inline void releaseWriteLock( const PaintedPoints *node, const bp::dict *expectedLock = nullptr )
{
	if( node->cacheModePlug()->getValue() != static_cast<int>( g_cacheModeExternal ) )
	{
		return;
	}

	const std::string path = lockFilePath( resolvedCachePath( node ) );
	if( path.empty() || !std::filesystem::exists( path ) )
	{
		return;
	}

	if( expectedLock )
	{
		try
		{
			bp::dict existingLock = readExternalLock( node );
			if( bp::len( existingLock ) && !sameSessionLock( existingLock, *expectedLock ) )
			{
				return;
			}
		}
		catch( ... )
		{
		}
	}

	std::filesystem::remove( path );
}

inline std::string writeBackupIfEnabled( const PaintedPoints *node, const std::string &cachePath )
{
	if( !node->backupEnabledPlug()->getValue() || node->backupPolicyPlug()->getValue() != static_cast<int>( g_backupPolicyOn ) )
	{
		return std::string();
	}
	if( cachePath.empty() || !std::filesystem::exists( cachePath ) )
	{
		return std::string();
	}

	const std::string backupPath = cacheTextPath( cachePath, g_backupSuffix );
	copyFile( cachePath, backupPath );
	return backupPath;
}

inline bp::dict defaultStore( const PaintedPoints *node )
{
	bp::dict nextIds;
	nextIds["layer"] = 1;
	nextIds["stroke"] = 1;
	nextIds["point"] = 1;
	nextIds["selectionSet"] = 1;
	nextIds["chunk"] = 1;

	bp::dict currentSelection;
	currentSelection["pointIds"] = bp::list();
	currentSelection["strokeIds"] = bp::list();

	bp::dict result;
	result["schemaVersion"] = g_schemaVersion;
	result["nextIds"] = nextIds;
	result["node"] = nodeMetadata( node );
	result["lock"] = bp::dict();
	result["scenePaths"] = bp::list();
	result["instanceSourcePaths"] = bp::list();
	result["layers"] = bp::list();
	result["strokes"] = bp::list();
	result["chunks"] = bp::list();
	result["points"] = bp::list();
	result["selectionSets"] = bp::list();
	result["currentSelection"] = currentSelection;
	result["diagnostics"] = emptyDiagnostics();
	result["upgrades"] = bp::list();
	result["pointBackups"] = bp::dict();
	return result;
}

inline std::string readLoadError( const std::string &loadError )
{
	return loadError.empty() ? std::string() : loadError;
}

inline bp::dict loadStore( const PaintedPoints *node, std::string *error = nullptr )
{
	if( node->cacheModePlug()->getValue() == static_cast<int>( g_cacheModeExternal ) )
	{
		const std::string cachePath = resolvedCachePath( node );
		if( cachePath.empty() )
		{
			if( error )
			{
				*error = "External cache mode requires a cache path";
			}
			return defaultStore( node );
		}

		std::string blobBytes;
		try
		{
			blobBytes = readBytesFile( cachePath );
		}
		catch( const std::runtime_error &e )
		{
			if( std::filesystem::exists( cachePath ) )
			{
				if( error )
				{
					*error = std::string( "Failed to read scatter paint blob: " ) + e.what();
				}
				return defaultStore( node );
			}
			if( error )
			{
				*error = "External cache does not exist: " + cachePath;
			}
			return defaultStore( node );
		}

		if( blobBytes.empty() )
		{
			if( error )
			{
				*error = "";
			}
			return defaultStore( node );
		}

		std::string unpackError;
		const CacheSchema cacheSchema = unpackCacheSchema( blobBytes, unpackError );
		if( !unpackError.empty() )
		{
			if( error )
			{
				*error = unpackError;
			}
			return defaultStore( node );
		}

		bp::dict store = cacheSchemaToDict( cacheSchema );
		store["node"] = nodeMetadata( node );
		if( error )
		{
			*error = "";
		}
		return store;
	}

	const std::string blobBytes = blobBytesFromObject( node->cacheBlobPlug()->getValue().get() );
	if( blobBytes.empty() )
	{
		if( error )
		{
			*error = "";
		}
		return defaultStore( node );
	}

	std::string unpackError;
	const CacheSchema cacheSchema = unpackCacheSchema( blobBytes, unpackError );
	if( !unpackError.empty() )
	{
		if( error )
		{
			*error = unpackError;
		}
		return defaultStore( node );
	}

	bp::dict store = cacheSchemaToDict( cacheSchema );
	store["node"] = nodeMetadata( node );
	if( error )
	{
		*error = "";
	}
	return store;
}

inline void writeStore( PaintedPoints *node, bp::dict store )
{
	store["schemaVersion"] = g_schemaVersion;
	store["node"] = nodeMetadata( node );
	bp::dict lock = ensureWriteLock( node );
	store["lock"] = lock;

	const CacheSchema cacheSchema = dictToCacheSchema( store );
	if( node->cacheModePlug()->getValue() == static_cast<int>( g_cacheModeExternal ) )
	{
		const std::string packedBytes = packCacheSchema( cacheSchema );
		const std::string cachePath = resolvedCachePath( node );
		if( cachePath.empty() )
		{
			throw std::runtime_error( "External cache mode requires a cache path" );
		}

		try
		{
			writeBackupIfEnabled( node, cachePath );
			writeBytesFile( cachePath, packedBytes );
			node->cacheBlobPlug()->setValue( new UCharVectorData() );
		}
		catch( ... )
		{
			releaseWriteLock( node, &lock );
			throw;
		}

		releaseWriteLock( node, &lock );
		return;
	}

	IECore::UCharVectorDataPtr packedBlob = new IECore::UCharVectorData();
	packCacheSchema( cacheSchema, packedBlob->writable() );
	node->cacheBlobPlug()->setValue( packedBlob );
}
