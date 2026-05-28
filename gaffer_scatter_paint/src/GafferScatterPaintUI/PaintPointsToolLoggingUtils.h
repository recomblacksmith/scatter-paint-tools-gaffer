#pragma once

bool diagnosticsEnabled()
{
	static const bool g_enabled = []() {
		const char *value = std::getenv( "GAFFER_SCATTER_PAINT_DIAGNOSTICS" );
		if( !value || !value[0] )
		{
			return false;
		}

		const std::string normalized = value;
		return normalized != "0" && normalized != "false" && normalized != "FALSE";
	}();

	return g_enabled;
}

bool refreshDiagnosticsEnabled()
{
	if( !diagnosticsEnabled() )
	{
		return false;
	}

	static const bool g_enabled = []() {
		const char *value = std::getenv( "GAFFER_SCATTER_PAINT_REFRESH_DIAGNOSTICS" );
		if( !value || !value[0] )
		{
			return false;
		}

		const std::string normalized = value;
		return normalized != "0" && normalized != "false" && normalized != "FALSE";
	}();

	return g_enabled;
}

std::string sequenceTag( const char *tag )
{
	return std::string( "[" ) + tag + "] ";
}

void logInfo( const std::string &message )
{
	if( !diagnosticsEnabled() )
	{
		return;
	}

	IECore::msg( IECore::Msg::Info, g_logContext, message );
	std::cerr << "INFO : " << g_logContext << " : " << message << std::endl;
}

void logWarning( const std::string &message )
{
	IECore::msg( IECore::Msg::Warning, g_logContext, message );
	std::cerr << "WARNING : " << g_logContext << " : " << message << std::endl;
}

ScriptNode *toolScriptNode( const PaintPointsTool *tool )
{
	if( !tool || !tool->view() )
	{
		return nullptr;
	}

	return const_cast<ScriptNode *>( tool->view()->scriptNode() );
}

std::string trimmed( const std::string &value )
{
	const auto begin = value.find_first_not_of( " \t\n\r" );
	if( begin == std::string::npos )
	{
		return "";
	}

	const auto end = value.find_last_not_of( " \t\n\r" );
	return value.substr( begin, end - begin + 1 );
}

float sampleSpacing( const PaintPointsTool *tool )
{
	return std::max( 0.001f, tool->brushSizePlug()->getValue() * tool->spacingPlug()->getValue() );
}

bool isEraseMode( const PaintPointsTool *tool )
{
	return tool->modePlug()->getValue() == g_modeErase;
}

bool isSelectBrushMode( const PaintPointsTool *tool )
{
	return tool->modePlug()->getValue() == g_modeSelectBrush;
}

bool isSelectLassoMode( const PaintPointsTool *tool )
{
	return tool->modePlug()->getValue() == g_modeSelectLasso;
}

bool isRelaxMode( const PaintPointsTool *tool )
{
	return tool->modePlug()->getValue() == g_modeRelax;
}

bool isReprojectMode( const PaintPointsTool *tool )
{
	return tool->modePlug()->getValue() == g_modeReproject;
}

bool isLayerEditMode( const PaintPointsTool *tool )
{
	return tool->modePlug()->getValue() == g_modeLayerEdit;
}

bool isStrokeEditMode( const PaintPointsTool *tool )
{
	return tool->modePlug()->getValue() == g_modeStrokeEdit;
}

std::string toolModeName( const PaintPointsTool *tool )
{
	if( isRelaxMode( tool ) )
	{
		return "Relax";
	}
	if( isReprojectMode( tool ) )
	{
		return "Reproject";
	}
	if( isLayerEditMode( tool ) )
	{
		return "LayerEdit";
	}
	if( isStrokeEditMode( tool ) )
	{
		return "StrokeEdit";
	}
	if( isSelectLassoMode( tool ) )
	{
		return "SelectLasso";
	}
	if( isSelectBrushMode( tool ) )
	{
		return "SelectBrush";
	}
	if( isEraseMode( tool ) )
	{
		return "Erase";
	}
	return "Paint";
}

std::string relaxObjectiveName( int objective )
{
	switch( objective )
	{
		case g_relaxObjectiveEvenRedistribution :
			return "Even Redistribution";
		case g_relaxObjectivePreserveSilhouette :
		default :
			return "Preserve Silhouette";
	}
}

std::string layerEditActionName( int action )
{
	switch( action )
	{
		case g_layerEditSetVisible :
			return "set visible";
		case g_layerEditSetMute :
			return "set mute";
		case g_layerEditSetSolo :
			return "set solo";
		case g_layerEditSetTimeRange :
			return "set time range";
		case g_layerEditMove :
			return "move";
		case g_layerEditSetMode :
			return "set mode";
		case g_layerEditRename :
		default :
			return "rename";
	}
}

std::string strokeEditActionName( int action )
{
	switch( action )
	{
		case g_strokeEditDelete :
			return "delete";
		case g_strokeEditMove :
			return "move";
		case g_strokeEditMerge :
			return "merge";
		case g_strokeEditSplit :
			return "split by selection";
		case g_strokeEditRename :
		default :
			return "rename";
	}
}

float resolvedPressureValue( const PaintPointsTool *tool )
{
	if( !tool )
	{
		return 1.0f;
	}

	if( !tool->pressureDefaultsEnabledPlug()->getValue() )
	{
		return 1.0f;
	}

	float pressure = std::clamp( tool->pressureValuePlug()->getValue(), 0.0f, 1.0f );
	if( tool->pressureDefaultsMappingModePlug()->getValue() != g_pressureMappingDirect )
	{
		pressure *= pressure;
	}

	return std::max( 0.0f, pressure );
}

float pressureScaledDensity( const PaintPointsTool *tool )
{
	const float density = std::max( 0.0f, tool->densityPlug()->getValue() );
	return density * resolvedPressureValue( tool );
}

int effectivePointsPerDab( const PaintPointsTool *tool )
{
	const int points = std::max( 1, tool->pointsPlug()->getValue() );
	const float scaledDensity = std::max( 0.0f, pressureScaledDensity( tool ) );
	return std::max( 1, static_cast<int>( std::lround( static_cast<double>( points ) * scaledDensity ) ) );
}

float pressureScaledSoftness( const PaintPointsTool *tool )
{
	const float softness = std::max( 0.0f, tool->softnessPlug()->getValue() );
	return softness * resolvedPressureValue( tool );
}

int normalizedFrameMode( int value )
{
	return std::clamp( value, g_frameModePersistent, g_frameModeOverride );
}

std::string frameModeName( int value )
{
	switch( normalizedFrameMode( value ) )
	{
		case g_frameModeAdditive :
			return "Additive";
		case g_frameModeOverride :
			return "Override";
		case g_frameModePersistent :
		default :
			return "Persistent";
	}
}

std::string eraseSpaceName( int value )
{
	return value == g_eraseSpaceAttachment ? "attachment-space" : "visible-space";
}
