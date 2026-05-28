#pragma once

bool modeEditFromSelection( Node *node, const bp::dict &currentSelection, std::uint64_t &layerId, std::uint64_t &strokeId )
{
	layerId = selectedLayerId( node, currentSelection );
	strokeId = selectedStrokeId( currentSelection );
	return layerId || strokeId;
}

size_t moveLayerFromSelection( const PaintPointsTool *tool, Node *node, const bp::dict &currentSelection )
{
	PaintPointsTool *mutableTool = const_cast<PaintPointsTool *>( tool );
	const std::uint64_t layerId = selectedLayerId( node, currentSelection );
	if( !layerId )
	{
		mutableTool->statusPlug()->setValue( "LayerEdit move failed: no selected layer could be resolved." );
		return 0;
	}

	bp::object pythonToolNode = pythonNode( node );
	const int newIndex = tool->layerMoveToIndexPlug()->getValue();
	pythonToolNode.attr( "moveLayer" )( layerId, newIndex );
	mutableTool->statusPlug()->setValue(
		"LayerEdit moved layer " + std::to_string( layerId ) +
		" to index " + std::to_string( newIndex ) + "."
	);
	return 1;
}

size_t setModeFromSelection( const PaintPointsTool *tool, Node *node, const bp::dict &currentSelection )
{
	PaintPointsTool *mutableTool = const_cast<PaintPointsTool *>( tool );
	std::uint64_t layerId = 0;
	std::uint64_t strokeId = 0;
	if( !modeEditFromSelection( node, currentSelection, layerId, strokeId ) )
	{
		mutableTool->statusPlug()->setValue( "LayerEdit mode failed: no selected layer or stroke could be resolved." );
		return 0;
	}

	const int modeValue = normalizedFrameMode( tool->layerModePlug()->getValue() );
	bp::object pythonToolNode = pythonNode( node );
	bp::dict globals;
	globals["layerId"] = layerId;
	globals["strokeId"] = strokeId;
	globals["modeValue"] = modeValue;
	bp::exec(
		"def __paint_points_tool_mutator(store):\n"
		"    for layer in store['layers']:\n"
		"        if int(layer.get('layerId', 0)) == int(layerId):\n"
		"            layer['mode'] = int(modeValue)\n"
		"            break\n"
		"    if int(strokeId):\n"
		"        for stroke in store['strokes']:\n"
		"            if int(stroke.get('strokeId', 0)) == int(strokeId):\n"
		"                stroke['mode'] = int(modeValue)\n"
		"                break\n",
		globals,
		globals
	);
	bp::object mutator = globals["__paint_points_tool_mutator"];
	pythonToolNode.attr( "mutateCacheStore" )( mutator );

	mutableTool->statusPlug()->setValue(
		std::string( "LayerEdit set mode " ) + frameModeName( modeValue ) +
		" on layer " + std::to_string( layerId ) +
		( strokeId ? ( std::string( " and stroke " ) + std::to_string( strokeId ) ) : std::string() ) + "."
	);
	return 1;
}

size_t applySelectionAction( const PaintPointsTool *tool, Node *node, const bp::dict &currentSelection )
{
	PaintPointsTool *mutableTool = const_cast<PaintPointsTool *>( tool );
	const size_t pointCount = bp::len( bp::extract<bp::list>( currentSelection.get( "pointIds", bp::list() ) ) );
	const size_t strokeCount = bp::len( bp::extract<bp::list>( currentSelection.get( "strokeIds", bp::list() ) ) );
	const std::string modeName = toolModeName( tool );
	if( !pointCount && !strokeCount )
	{
		mutableTool->statusPlug()->setValue( modeName + " failed: no points are currently selected." );
		return 0;
	}

	bp::object pythonToolNode = pythonNode( node );
	const size_t updatedCount = isReprojectMode( tool ) ?
		bp::extract<size_t>( pythonToolNode.attr( "reprojectSelection" )() ) :
		bp::extract<size_t>( pythonToolNode.attr( "relaxSelection" )() );
	if( !updatedCount )
	{
		mutableTool->statusPlug()->setValue( modeName + " made no changes to the current selection." );
		return 0;
	}

	mutableTool->statusPlug()->setValue(
		modeName + " updated " + std::to_string( updatedCount ) +
		" points from selection (" + std::to_string( pointCount ) +
		" points, " + std::to_string( strokeCount ) + " strokes)."
	);
	return updatedCount;
}

std::string selectionActionHelp( const PaintPointsTool *tool )
{
	if( isReprojectMode( tool ) )
	{
		return "Reproject uses click or drag selection to update anchors for the current selection.";
	}
	return "Relax uses click or drag selection to smooth the current selection in anchor space using the current relax objective.";
}

size_t applyLayerEditSelection( const PaintPointsTool *tool, Node *node, const bp::dict &currentSelection )
{
	const std::uint64_t layerId = selectedLayerId( node, currentSelection );
	if( !layerId )
	{
		const_cast<PaintPointsTool *>( tool )->statusPlug()->setValue( "LayerEdit failed: no selected layer could be resolved." );
		return 0;
	}

	bp::object pythonToolNode = pythonNode( node );
	const int action = tool->layerEditActionPlug()->getValue();
	PaintPointsTool *mutableTool = const_cast<PaintPointsTool *>( tool );
	switch( action )
	{
		case g_layerEditMove :
			return moveLayerFromSelection( tool, node, currentSelection );
		case g_layerEditSetMode :
			return setModeFromSelection( tool, node, currentSelection );
		case g_layerEditSetVisible :
		{
			const bool visible = tool->layerVisiblePlug()->getValue();
			pythonToolNode.attr( "setLayerVisible" )( layerId, visible );
			mutableTool->statusPlug()->setValue(
				"LayerEdit set layer " + std::to_string( layerId ) +
				" visible=" + std::string( visible ? "true" : "false" ) + "."
			);
			break;
		}
		case g_layerEditSetMute :
		{
			const bool mute = tool->layerMutePlug()->getValue();
			pythonToolNode.attr( "setLayerMute" )( layerId, mute );
			mutableTool->statusPlug()->setValue(
				"LayerEdit set layer " + std::to_string( layerId ) +
				" mute=" + std::string( mute ? "true" : "false" ) + "."
			);
			break;
		}
		case g_layerEditSetSolo :
		{
			const bool solo = tool->layerSoloPlug()->getValue();
			pythonToolNode.attr( "setLayerSolo" )( layerId, solo );
			mutableTool->statusPlug()->setValue(
				"LayerEdit set layer " + std::to_string( layerId ) +
				" solo=" + std::string( solo ? "true" : "false" ) + "."
			);
			break;
		}
		case g_layerEditSetTimeRange :
		{
			const int frameStart = tool->layerFrameStartPlug()->getValue();
			const int frameEnd = tool->layerFrameEndPlug()->getValue();
			pythonToolNode.attr( "setLayerTimeRange" )( layerId, frameStart, frameEnd );
			mutableTool->statusPlug()->setValue(
				"LayerEdit set layer " + std::to_string( layerId ) +
				" time range to [" + std::to_string( frameStart ) + ", " + std::to_string( frameEnd ) + "]."
			);
			break;
		}
		case g_layerEditRename :
		default :
		{
			const std::string newName = trimmed( tool->layerNamePlug()->getValue() );
			if( newName.empty() )
			{
				mutableTool->statusPlug()->setValue( "LayerEdit failed: layerName is empty." );
				return 0;
			}
			pythonToolNode.attr( "renameLayer" )( layerId, newName );
			mutableTool->statusPlug()->setValue(
				"LayerEdit renamed layer " + std::to_string( layerId ) + " to " + newName + "."
			);
			break;
		}
	}
	return 1;
}

std::string layerEditActionHelp( const PaintPointsTool *tool )
{
	if( !tool )
	{
		return "LayerEdit uses click selection to resolve the target layer.";
	}

	switch( tool->layerEditActionPlug()->getValue() )
	{
		case g_layerEditMove :
			return "LayerEdit uses click selection and layerMoveToIndex to reorder the selected layer.";
		case g_layerEditSetMode :
			return "LayerEdit uses click selection and frameMode to set the selected layer and stroke frame mode.";
		case g_layerEditSetVisible :
			return "LayerEdit uses click selection and layerVisible to set the selected layer visibility.";
		case g_layerEditSetMute :
			return "LayerEdit uses click selection and muteBehavior to set the selected layer mute state.";
		case g_layerEditSetSolo :
			return "LayerEdit uses click selection and soloBehavior to set the selected layer solo state.";
		case g_layerEditSetTimeRange :
			return "LayerEdit uses click selection and frameStart/frameEnd to set the selected layer time range.";
		case g_layerEditRename :
		default :
			return "LayerEdit uses click selection and layerName to rename the selected layer.";
	}
}

size_t applyStrokeEditSelection( const PaintPointsTool *tool, Node *node, const bp::dict &currentSelection )
{
	PaintPointsTool *mutableTool = const_cast<PaintPointsTool *>( tool );
	bp::object pythonToolNode = pythonNode( node );
	const int action = tool->strokeEditActionPlug()->getValue();
	if( action == g_strokeEditSplit )
	{
		const size_t pointCount = bp::len( bp::extract<bp::list>( currentSelection.get( "pointIds", bp::list() ) ) );
		const size_t strokeCount = bp::len( bp::extract<bp::list>( currentSelection.get( "strokeIds", bp::list() ) ) );
		if( !pointCount && !strokeCount )
		{
			mutableTool->statusPlug()->setValue( "StrokeEdit split failed: no current subset selection was found." );
			return 0;
		}

		bp::object currentSelectionSetter = pythonToolNode.attr( "setCurrentSelection" );
		bp::list pointIds = bp::extract<bp::list>( currentSelection.get( "pointIds", bp::list() ) );
		currentSelectionSetter( pointIds, bp::list() );
		const std::string message = bp::extract<std::string>( pythonToolNode.attr( "splitStrokeBySelection" )() );
		mutableTool->statusPlug()->setValue( "StrokeEdit " + message );
		return 1;
	}

	const std::uint64_t strokeId = selectedStrokeId( currentSelection );
	if( !strokeId )
	{
		mutableTool->statusPlug()->setValue( "StrokeEdit failed: no selected stroke could be resolved." );
		return 0;
	}

	switch( action )
	{
		case g_strokeEditDelete :
		{
			pythonToolNode.attr( "deleteStroke" )( strokeId );
			mutableTool->statusPlug()->setValue(
				"StrokeEdit deleted stroke " + std::to_string( strokeId ) + "."
			);
			break;
		}
		case g_strokeEditMove :
		{
			const int newIndex = tool->strokeMoveToIndexPlug()->getValue();
			pythonToolNode.attr( "moveStroke" )( strokeId, newIndex );
			mutableTool->statusPlug()->setValue(
				"StrokeEdit moved stroke " + std::to_string( strokeId ) +
				" to index " + std::to_string( newIndex ) + "."
			);
			break;
		}
		case g_strokeEditMerge :
		{
			const std::string mergeTarget = trimmed( tool->strokeMergeTargetPlug()->getValue() );
			if( mergeTarget.empty() )
			{
				mutableTool->statusPlug()->setValue( "StrokeEdit failed: strokeMergeTarget is empty." );
				return 0;
			}

			bp::list strokeRecords = bp::extract<bp::list>( pythonToolNode.attr( "strokeRecords" )() );
			std::uint64_t targetStrokeId = 0;
			for( bp::stl_input_iterator<bp::object> it( strokeRecords ), end; it != end; ++it )
			{
				bp::dict stroke = bp::extract<bp::dict>( *it );
				if( trimmed( bp::extract<std::string>( stroke.get( "name", bp::object( "" ) ) ) ) == mergeTarget )
				{
					targetStrokeId = uint64FromObject( stroke.get( "strokeId", bp::object() ) );
					break;
				}
			}
			if( !targetStrokeId )
			{
				mutableTool->statusPlug()->setValue( "StrokeEdit failed: merge target stroke was not found." );
				return 0;
			}
			if( targetStrokeId == strokeId )
			{
				mutableTool->statusPlug()->setValue( "StrokeEdit skipped: selected stroke already matches the merge target." );
				return 0;
			}

			pythonToolNode.attr( "mergeStrokes" )( strokeId, targetStrokeId );
			mutableTool->statusPlug()->setValue(
				"StrokeEdit merged stroke " + std::to_string( strokeId ) +
				" into stroke " + std::to_string( targetStrokeId ) + "."
			);
			break;
		}
		case g_strokeEditRename :
		default :
		{
			const std::string newName = trimmed( tool->strokeNamePlug()->getValue() );
			if( newName.empty() )
			{
				mutableTool->statusPlug()->setValue( "StrokeEdit failed: strokeName is empty." );
				return 0;
			}
			pythonToolNode.attr( "renameStroke" )( strokeId, newName );
			mutableTool->statusPlug()->setValue(
				"StrokeEdit renamed stroke " + std::to_string( strokeId ) + " to " + newName + "."
			);
			break;
		}
	}
	return 1;
}

std::string strokeEditActionHelp( const PaintPointsTool *tool )
{
	if( !tool )
	{
		return "StrokeEdit uses click selection to resolve the target stroke.";
	}

	switch( tool->strokeEditActionPlug()->getValue() )
	{
		case g_strokeEditDelete :
			return "StrokeEdit uses click selection to delete the selected stroke.";
		case g_strokeEditMove :
			return "StrokeEdit uses click selection and strokeMoveToIndex to reorder the selected stroke.";
		case g_strokeEditMerge :
			return "StrokeEdit uses click selection and strokeMergeTarget to merge the selected stroke.";
		case g_strokeEditSplit :
			return "StrokeEdit uses the current subset selection to split affected strokes around the selected points.";
		case g_strokeEditRename :
		default :
			return "StrokeEdit uses click selection and strokeName to rename the selected stroke.";
	}
}
