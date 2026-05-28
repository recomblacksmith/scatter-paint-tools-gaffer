#pragma once

Imath::V3f vectorFromObject( const bp::object &value, const Imath::V3f &fallback )
{
	try
	{
		bp::stl_input_iterator<float> it( value ), end;
		std::vector<float> values( it, end );
		if( values.size() >= 3 )
		{
			return Imath::V3f( values[0], values[1], values[2] );
		}
	}
	catch( const bp::error_already_set & )
	{
		PyErr_Clear();
	}

	return fallback;
}

std::uint64_t uint64FromObject( const bp::object &value, const std::uint64_t fallback )
{
	try
	{
		return bp::extract<std::uint64_t>( value );
	}
	catch( const bp::error_already_set & )
	{
		PyErr_Clear();
	}

	return fallback;
}

struct LayerStrokeCacheEntry
{
	const Gaffer::Node *node = nullptr;
	std::string layerName;
	std::string strokeName;
	std::pair<std::uint64_t, std::uint64_t> ids = { 0, 0 };
};

std::unordered_map<const PaintPointsTool *, LayerStrokeCacheEntry> &layerStrokeCacheByTool()
{
	static std::unordered_map<const PaintPointsTool *, LayerStrokeCacheEntry> g_cache;
	return g_cache;
}

bool isPaintedPointsNode( const IECore::RunTimeTyped *component )
{
	return component && std::string( component->typeName() ) == "GafferScatterPaint::PaintedPoints";
}

bool isAttachedPointsNode( const IECore::RunTimeTyped *component )
{
	return component && std::string( component->typeName() ) == "GafferScatterPaint::AttachedPoints";
}

Node *findPaintedPointsRecursive( GraphComponent *parent )
{
	if( !parent )
	{
		return nullptr;
	}

	for( const auto &child : parent->children() )
	{
		GraphComponent *childComponent = child.get();
		if( isPaintedPointsNode( childComponent ) )
		{
			return static_cast<Node *>( childComponent );
		}

		if( Node *paintedPoints = findPaintedPointsRecursive( childComponent ) )
		{
			return paintedPoints;
		}
	}

	return nullptr;
}

void appendAttachedPointsNodesRecursive( GraphComponent *parent, std::vector<Node *> &result )
{
	if( !parent )
	{
		return;
	}

	for( const auto &child : parent->children() )
	{
		GraphComponent *childComponent = child.get();
		if( isAttachedPointsNode( childComponent ) )
		{
			if( Node *attachedPoints = IECore::runTimeCast<Node>( childComponent ) )
			{
				result.push_back( attachedPoints );
			}
		}

		appendAttachedPointsNodesRecursive( childComponent, result );
	}
}

std::string nodeNameOrFallback( const GraphComponent *component )
{
	return component ? component->getName().string() : std::string( "<none>" );
}

std::string objectSummary( IECore::ConstObjectPtr object )
{
	if( !object )
	{
		return "object=<null>";
	}

	std::string summary = std::string( "objectType=" ) + object->typeName();
	if( const IECoreScene::PointsPrimitive *points = IECore::runTimeCast<const IECoreScene::PointsPrimitive>( object.get() ) )
	{
		summary += " numPoints=" + std::to_string( points->getNumPoints() );
	}
	return summary;
}

std::string sceneGadgetStateName( const SceneGadget::State state )
{
	switch( state )
	{
		case SceneGadget::Paused :
			return "Paused";
		case SceneGadget::Running :
			return "Running";
		case SceneGadget::Complete :
			return "Complete";
	}

	return "Unknown";
}

IECore::PathMatcher affectedOutputLocations( ScriptNode *script )
{
	IECore::PathMatcher result;
	if( !script )
	{
		return result;
	}

	std::vector<Node *> attachedPointsNodes;
	appendAttachedPointsNodesRecursive( script, attachedPointsNodes );
	for( Node *attachedPoints : attachedPointsNodes )
	{
		if( !attachedPoints )
		{
			continue;
		}

		if( const StringPlug *outputLocationPlug = attachedPoints->getChild<StringPlug>( "outputLocation" ) )
		{
			std::string outputLocation = trimmed( outputLocationPlug->getValue() );
			if( outputLocation.empty() )
			{
				outputLocation = "/scatter";
			}
			result.addPath( outputLocation );
		}
	}

	return result;
}

void logScenePlugState( const std::string &prefix, const GafferScene::ScenePlug *scenePlug, const std::string &pathString )
{
	if( !scenePlug )
	{
		logWarning( prefix + " scenePlug=<null>" );
		return;
	}

	try
	{
		const GafferScene::ScenePlug::ScenePath scenePath = GafferScene::ScenePlug::stringToPath( pathString );
		const bool exists = scenePlug->exists( scenePath );
		const IECore::MurmurHash objectHash = scenePlug->objectHash( scenePath );
		std::string childNamesSummary = "childNames=<n/a>";

		const std::string parentPathString = [&]() {
			const size_t slash = pathString.find_last_of( '/' );
			if( slash == std::string::npos || slash == 0 )
			{
				return std::string( "/" );
			}
			return pathString.substr( 0, slash );
		}();

		try
		{
			IECore::ConstInternedStringVectorDataPtr childNamesData = scenePlug->childNames( GafferScene::ScenePlug::stringToPath( parentPathString ) );
			childNamesSummary = "parentChildCount=" + std::to_string( childNamesData ? childNamesData->readable().size() : 0 );
		}
		catch( const std::exception &e )
		{
			childNamesSummary = std::string( "parentChildNamesError=" ) + e.what();
		}

		IECore::ConstObjectPtr object;
		std::string objectDetails;
		if( exists )
		{
			object = scenePlug->object( scenePath );
			objectDetails = objectSummary( object );
		}
		else
		{
			objectDetails = "object=<missing>";
		}

		logInfo(
			prefix +
			" sceneNode=" + nodeNameOrFallback( scenePlug->node() ) +
			" path=" + pathString +
			" exists=" + std::string( exists ? "true" : "false" ) +
			" objectHash=" + objectHash.toString() +
			" " + childNamesSummary +
			" " + objectDetails
		);
	}
	catch( const std::exception &e )
	{
		logWarning( prefix + " failed to inspect scene path=" + pathString + " error=" + e.what() );
	}
}

void logRefreshDiagnostics( PaintPointsTool *tool, ScriptNode *script, Node *paintedNode, const char *stage )
{
	if( !refreshDiagnosticsEnabled() )
	{
		return;
	}

	if( !tool )
	{
		return;
	}

	std::string targetSummary = std::string( "targetNode=" ) + nodeNameOrFallback( paintedNode );
	if( paintedNode )
	{
		try
		{
			bp::object pythonToolNode{ NodePtr( paintedNode ) };
			const size_t pointCount = bp::len( pythonToolNode.attr( "pointRecords" )() );
			targetSummary += " authoredPointRecords=" + std::to_string( pointCount );
		}
		catch( const bp::error_already_set & )
		{
			PyErr_Clear();
			targetSummary += " authoredPointRecords=<error>";
		}
	}

	if( SceneView *sceneView = IECore::runTimeCast<SceneView>( tool->view() ) )
	{
		if( GafferScene::ScenePlug *viewInPlug = sceneView->getChild<GafferScene::ScenePlug>( "in" ) )
		{
			if( Plug *sourcePlug = viewInPlug->source() )
			{
				targetSummary += " viewInputSource=" + nodeNameOrFallback( sourcePlug->node() ) + "." + sourcePlug->getName().string();
			}
		}
	}

	logInfo( sequenceTag( "REFRESH" ) + std::string( stage ) + " " + targetSummary );

	if( !script )
	{
		logWarning( sequenceTag( "REFRESH" ) + std::string( stage ) + " script=<null>" );
		return;
	}

	std::vector<Node *> attachedPointsNodes;
	appendAttachedPointsNodesRecursive( script, attachedPointsNodes );
	if( attachedPointsNodes.empty() )
	{
		logInfo( sequenceTag( "REFRESH" ) + std::string( stage ) + " found no AttachedPoints nodes in script." );
		return;
	}

	for( Node *attachedPoints : attachedPointsNodes )
	{
		if( !attachedPoints )
		{
			continue;
		}

		StringPlug *outputLocationPlug = attachedPoints->getChild<StringPlug>( "outputLocation" );
		GafferScene::ScenePlug *attachedOutPlug = attachedPoints->getChild<GafferScene::ScenePlug>( "out" );
		Plug *pointsPlug = attachedPoints->getChild<Plug>( "points" );
		if( !outputLocationPlug || !attachedOutPlug || !pointsPlug )
		{
			logInfo( sequenceTag( "REFRESH" ) + std::string( stage ) + " attachedNode=" + attachedPoints->getName().string() + " missing expected plugs." );
			continue;
		}

		std::string outputLocation = outputLocationPlug->getValue();
		if( outputLocation.empty() )
		{
			outputLocation = "/scatter";
		}

		std::string header = sequenceTag( "REFRESH" ) + std::string( stage ) +
			" attachedNode=" + attachedPoints->getName().string() +
			" outputLocation=" + outputLocation;

		if( Plug *pointsSource = pointsPlug->source() )
		{
			header += " pointsSource=" + nodeNameOrFallback( pointsSource->node() ) + "." + pointsSource->getName().string();
		}
		else
		{
			header += " pointsSource=<none>";
		}

		logInfo( header );
		logScenePlugState( sequenceTag( "REFRESH" ) + std::string( stage ) + " attachedOut", attachedOutPlug, outputLocation );

		if( SceneView *sceneView = IECore::runTimeCast<SceneView>( tool->view() ) )
		{
			if( GafferScene::ScenePlug *viewInPlug = sceneView->getChild<GafferScene::ScenePlug>( "in" ) )
			{
				logScenePlugState( sequenceTag( "REFRESH" ) + std::string( stage ) + " sceneViewIn", viewInPlug, outputLocation );
			}
		}
	}
}

bp::object pythonNode( Node *node )
{
	return bp::object( NodePtr( node ) );
}

bp::object pythonPaintedPointsClass()
{
	return bp::import( "GafferScatterPaint" ).attr( "PaintedPoints" );
}

Node *findPaintedPointsNode( PaintPointsTool *tool, ScriptNode *script )
{
	if( !tool || !script )
	{
		return nullptr;
	}

	const std::string targetName = trimmed( tool->targetNodePlug()->getValue() );
	if( !targetName.empty() )
	{
		if( Node *target = script->descendant<Node>( targetName ) )
		{
			if( isPaintedPointsNode( target ) )
			{
				return target;
			}
		}
	}

	if( Node *focus = script->getFocus() )
	{
		if( isPaintedPointsNode( focus ) )
		{
			return focus;
		}
	}

	if( const StandardSet *selection = script->selection() )
	{
		for( size_t i = 0, e = selection->size(); i < e; ++i )
		{
			if( const IECore::RunTimeTyped *selected = selection->member( i ) )
			{
				if( isPaintedPointsNode( selected ) )
				{
					if( const Node *node = IECore::runTimeCast<const Node>( selected ) )
					{
						return const_cast<Node *>( node );
					}
				}
			}
		}
	}

	return findPaintedPointsRecursive( script );
}

Node *createPaintedPointsNode( PaintPointsTool *tool, ScriptNode *script )
{
	if( !tool || !script )
	{
		return nullptr;
	}

	std::string nodeName = "PaintedPoints";
	int suffix = 1;
	while( script->getChild<Node>( nodeName ) )
	{
		++suffix;
		nodeName = "PaintedPoints" + std::to_string( suffix );
	}

	bp::object nodeObject = pythonPaintedPointsClass()( nodeName );
	Node *node = bp::extract<Node *>( nodeObject );
	bp::object pythonScript = pythonNode( script );
	pythonScript.attr( "addChild" )( nodeObject );
	bp::object selection = pythonScript.attr( "selection" )();
	selection.attr( "clear" )();
	selection.attr( "add" )( nodeObject );
	pythonScript.attr( "setFocus" )( nodeObject );
	tool->targetNodePlug()->setValue( node->relativeName( script ) );
	return node;
}

Node *targetNodeOrCreate( PaintPointsTool *tool, ScriptNode *script )
{
	if( !tool || !script )
	{
		return nullptr;
	}

	if( Node *node = findPaintedPointsNode( tool, script ) )
	{
		tool->targetNodePlug()->setValue( node->relativeName( script ) );
		return node;
	}

	return createPaintedPointsNode( tool, script );
}

std::pair<std::uint64_t, std::uint64_t> ensureLayerAndStroke( PaintPointsTool *tool, Node *node )
{
	auto &cacheByTool = layerStrokeCacheByTool();

	const std::string layerName = trimmed( tool->layerNamePlug()->getValue() ).empty() ? "Layer 1" : trimmed( tool->layerNamePlug()->getValue() );
	const std::string strokeName = trimmed( tool->strokeNamePlug()->getValue() ).empty() ? "Stroke 1" : trimmed( tool->strokeNamePlug()->getValue() );
	const auto cacheIt = cacheByTool.find( tool );
	if(
		cacheIt != cacheByTool.end() &&
		cacheIt->second.node == node &&
		cacheIt->second.layerName == layerName &&
		cacheIt->second.strokeName == strokeName
	)
	{
		const auto &ids = cacheIt->second.ids;
		logInfo(
			sequenceTag( "LAYER" ) +
			"ensureLayerAndStroke cacheHit=true layerId=" + std::to_string( ids.first ) +
			" strokeId=" + std::to_string( ids.second )
		);
		return ids;
	}

	bp::object pythonToolNode = pythonNode( node );
	if( PyObject_HasAttrString( pythonToolNode.ptr(), "ensureLayerAndStroke" ) )
	{
		bp::dict result = bp::extract<bp::dict>( pythonToolNode.attr( "ensureLayerAndStroke" )( layerName, strokeName ) );
		const std::uint64_t layerId = uint64FromObject( result.get( "layerId", bp::object() ) );
		const std::uint64_t strokeId = uint64FromObject( result.get( "strokeId", bp::object() ) );
		logInfo(
			sequenceTag( "LAYER" ) +
			"ensureLayerAndStroke layerCreated=" + std::string( bp::extract<bool>( result.get( "layerCreated", bp::object( false ) ) ) ? "true" : "false" ) +
			" strokeCreated=" + std::string( bp::extract<bool>( result.get( "strokeCreated", bp::object( false ) ) ) ? "true" : "false" ) +
			" loadStoreMs=" + std::to_string( bp::extract<double>( result.get( "loadStoreMs", bp::object( 0.0 ) ) ) ) +
			" loadResolvePathMs=" + std::to_string( bp::extract<double>( result.get( "loadResolvePathMs", bp::object( 0.0 ) ) ) ) +
			" loadReadBytesMs=" + std::to_string( bp::extract<double>( result.get( "loadReadBytesMs", bp::object( 0.0 ) ) ) ) +
			" loadBlobExtractMs=" + std::to_string( bp::extract<double>( result.get( "loadBlobExtractMs", bp::object( 0.0 ) ) ) ) +
			" loadUnpackMs=" + std::to_string( bp::extract<double>( result.get( "loadUnpackMs", bp::object( 0.0 ) ) ) ) +
			" loadPopulateMetadataMs=" + std::to_string( bp::extract<double>( result.get( "loadPopulateMetadataMs", bp::object( 0.0 ) ) ) ) +
			" unpackChecksumMs=" + std::to_string( bp::extract<double>( result.get( "unpackChecksumMs", bp::object( 0.0 ) ) ) ) +
			" unpackHeaderMs=" + std::to_string( bp::extract<double>( result.get( "unpackHeaderMs", bp::object( 0.0 ) ) ) ) +
			" unpackNodeMs=" + std::to_string( bp::extract<double>( result.get( "unpackNodeMs", bp::object( 0.0 ) ) ) ) +
			" unpackLockMs=" + std::to_string( bp::extract<double>( result.get( "unpackLockMs", bp::object( 0.0 ) ) ) ) +
			" unpackScenePathsMs=" + std::to_string( bp::extract<double>( result.get( "unpackScenePathsMs", bp::object( 0.0 ) ) ) ) +
			" unpackLayersMs=" + std::to_string( bp::extract<double>( result.get( "unpackLayersMs", bp::object( 0.0 ) ) ) ) +
			" unpackStrokesMs=" + std::to_string( bp::extract<double>( result.get( "unpackStrokesMs", bp::object( 0.0 ) ) ) ) +
			" unpackChunksMs=" + std::to_string( bp::extract<double>( result.get( "unpackChunksMs", bp::object( 0.0 ) ) ) ) +
			" unpackPointsMs=" + std::to_string( bp::extract<double>( result.get( "unpackPointsMs", bp::object( 0.0 ) ) ) ) +
			" unpackSelectionSetsMs=" + std::to_string( bp::extract<double>( result.get( "unpackSelectionSetsMs", bp::object( 0.0 ) ) ) ) +
			" unpackDiagnosticsMs=" + std::to_string( bp::extract<double>( result.get( "unpackDiagnosticsMs", bp::object( 0.0 ) ) ) ) +
			" unpackUpgradesMs=" + std::to_string( bp::extract<double>( result.get( "unpackUpgradesMs", bp::object( 0.0 ) ) ) ) +
			" unpackPointBackupsMs=" + std::to_string( bp::extract<double>( result.get( "unpackPointBackupsMs", bp::object( 0.0 ) ) ) ) +
			" layerLookupMs=" + std::to_string( bp::extract<double>( result.get( "layerLookupMs", bp::object( 0.0 ) ) ) ) +
			" strokeLookupMs=" + std::to_string( bp::extract<double>( result.get( "strokeLookupMs", bp::object( 0.0 ) ) ) ) +
			" writeMs=" + std::to_string( bp::extract<double>( result.get( "writeMs", bp::object( 0.0 ) ) ) ) +
			" writePopulateMetadataMs=" + std::to_string( bp::extract<double>( result.get( "writePopulateMetadataMs", bp::object( 0.0 ) ) ) ) +
			" writeLockMs=" + std::to_string( bp::extract<double>( result.get( "writeLockMs", bp::object( 0.0 ) ) ) ) +
			" writePackMs=" + std::to_string( bp::extract<double>( result.get( "writePackMs", bp::object( 0.0 ) ) ) ) +
			" packHeaderMs=" + std::to_string( bp::extract<double>( result.get( "packHeaderMs", bp::object( 0.0 ) ) ) ) +
			" packNodeMs=" + std::to_string( bp::extract<double>( result.get( "packNodeMs", bp::object( 0.0 ) ) ) ) +
			" packLockMs=" + std::to_string( bp::extract<double>( result.get( "packLockMs", bp::object( 0.0 ) ) ) ) +
			" packScenePathsMs=" + std::to_string( bp::extract<double>( result.get( "packScenePathsMs", bp::object( 0.0 ) ) ) ) +
			" packLayersMs=" + std::to_string( bp::extract<double>( result.get( "packLayersMs", bp::object( 0.0 ) ) ) ) +
			" packStrokesMs=" + std::to_string( bp::extract<double>( result.get( "packStrokesMs", bp::object( 0.0 ) ) ) ) +
			" packChunksMs=" + std::to_string( bp::extract<double>( result.get( "packChunksMs", bp::object( 0.0 ) ) ) ) +
			" packPointsMs=" + std::to_string( bp::extract<double>( result.get( "packPointsMs", bp::object( 0.0 ) ) ) ) +
			" packPointsReserveMs=" + std::to_string( bp::extract<double>( result.get( "packPointsReserveMs", bp::object( 0.0 ) ) ) ) +
			" packPointsReserveReallocated=" + std::to_string( bp::extract<double>( result.get( "packPointsReserveReallocated", bp::object( 0.0 ) ) ) ) +
			" packPointsCapacityBeforeBytes=" + std::to_string( bp::extract<double>( result.get( "packPointsCapacityBeforeBytes", bp::object( 0.0 ) ) ) ) +
			" packPointsCapacityAfterBytes=" + std::to_string( bp::extract<double>( result.get( "packPointsCapacityAfterBytes", bp::object( 0.0 ) ) ) ) +
			" packPointsResizeMs=" + std::to_string( bp::extract<double>( result.get( "packPointsResizeMs", bp::object( 0.0 ) ) ) ) +
			" packPointsFillMs=" + std::to_string( bp::extract<double>( result.get( "packPointsFillMs", bp::object( 0.0 ) ) ) ) +
			" packPointsCopyMs=" + std::to_string( bp::extract<double>( result.get( "packPointsCopyMs", bp::object( 0.0 ) ) ) ) +
			" packPointsChecksumInlineMs=" + std::to_string( bp::extract<double>( result.get( "packPointsChecksumInlineMs", bp::object( 0.0 ) ) ) ) +
			" packPointsChecksumMs=" + std::to_string( bp::extract<double>( result.get( "packPointsChecksumMs", bp::object( 0.0 ) ) ) ) +
			" packSelectionSetsMs=" + std::to_string( bp::extract<double>( result.get( "packSelectionSetsMs", bp::object( 0.0 ) ) ) ) +
			" packDiagnosticsMs=" + std::to_string( bp::extract<double>( result.get( "packDiagnosticsMs", bp::object( 0.0 ) ) ) ) +
			" packUpgradesMs=" + std::to_string( bp::extract<double>( result.get( "packUpgradesMs", bp::object( 0.0 ) ) ) ) +
			" packPointBackupsMs=" + std::to_string( bp::extract<double>( result.get( "packPointBackupsMs", bp::object( 0.0 ) ) ) ) +
			" packFinalChecksumMs=" + std::to_string( bp::extract<double>( result.get( "packFinalChecksumMs", bp::object( 0.0 ) ) ) ) +
			" packFinalHeaderWriteMs=" + std::to_string( bp::extract<double>( result.get( "packFinalHeaderWriteMs", bp::object( 0.0 ) ) ) ) +
			" packFinalBufferMs=" + std::to_string( bp::extract<double>( result.get( "packFinalBufferMs", bp::object( 0.0 ) ) ) ) +
			" writeResolvePathMs=" + std::to_string( bp::extract<double>( result.get( "writeResolvePathMs", bp::object( 0.0 ) ) ) ) +
			" writeBackupMs=" + std::to_string( bp::extract<double>( result.get( "writeBackupMs", bp::object( 0.0 ) ) ) ) +
			" writeBytesMs=" + std::to_string( bp::extract<double>( result.get( "writeBytesMs", bp::object( 0.0 ) ) ) ) +
			" writeClearBlobMs=" + std::to_string( bp::extract<double>( result.get( "writeClearBlobMs", bp::object( 0.0 ) ) ) ) +
			" writeReleaseLockMs=" + std::to_string( bp::extract<double>( result.get( "writeReleaseLockMs", bp::object( 0.0 ) ) ) ) +
			" writeBlobObjectMs=" + std::to_string( bp::extract<double>( result.get( "writeBlobObjectMs", bp::object( 0.0 ) ) ) ) +
			" writeBlobObjectResizeMs=" + std::to_string( bp::extract<double>( result.get( "writeBlobObjectResizeMs", bp::object( 0.0 ) ) ) ) +
			" writeBlobObjectCopyMs=" + std::to_string( bp::extract<double>( result.get( "writeBlobObjectCopyMs", bp::object( 0.0 ) ) ) ) +
			" writeBlobPlugSetMs=" + std::to_string( bp::extract<double>( result.get( "writeBlobPlugSetMs", bp::object( 0.0 ) ) ) ) +
			" writeSetBlobMs=" + std::to_string( bp::extract<double>( result.get( "writeSetBlobMs", bp::object( 0.0 ) ) ) ) +
			" totalMs=" + std::to_string( bp::extract<double>( result.get( "totalMs", bp::object( 0.0 ) ) ) ) +
			" layerId=" + std::to_string( layerId ) +
			" strokeId=" + std::to_string( strokeId )
		);
		cacheByTool[tool] = { node, layerName, strokeName, std::make_pair( layerId, strokeId ) };
		return { layerId, strokeId };
	}

	const std::uint64_t layerId = bp::extract<std::uint64_t>( pythonToolNode.attr( "ensureLayer" )( layerName ) );
	const std::uint64_t strokeId = bp::extract<std::uint64_t>( pythonToolNode.attr( "ensureStroke" )( bp::object( layerId ), strokeName ) );
	logInfo(
		sequenceTag( "LAYER" ) +
		"ensureLayerAndStroke legacyFallback=true layerId=" + std::to_string( layerId ) +
		" strokeId=" + std::to_string( strokeId )
	);
	cacheByTool[tool] = { node, layerName, strokeName, std::make_pair( layerId, strokeId ) };
	return { layerId, strokeId };
}

void invalidateLayerStrokeCache( PaintPointsTool *tool )
{
	layerStrokeCacheByTool().erase( tool );
}
