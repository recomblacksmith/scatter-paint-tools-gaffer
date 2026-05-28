#include "PaintPointsToolPrivate.h"

#include <chrono>

using namespace Gaffer;
using namespace GafferScatterPaintUI;
using namespace GafferScatterPaintUI::PaintPointsToolPrivate;
using namespace GafferSceneUI;
using namespace IECoreScene;
namespace bp = boost::python;

namespace
{

using Clock = std::chrono::steady_clock;

double elapsedMilliseconds( const Clock::time_point &start )
{
	return std::chrono::duration<double, std::milli>( Clock::now() - start ).count();
}

}


bool PaintPointsTool::isMirroredControlPlug( const Gaffer::Plug *plug ) const
{
	return
		plug == relaxObjectivePlug() ||
		plug == pointsPlug() ||
		plug == rotationModePlug() ||
		plug == scaleJitterPlug() ||
		plug == widthJitterPlug() ||
		plug == targetFilterPlug() ||
		plug == targetSetFilterPlug() ||
		plug == surfaceModePlug() ||
		plug == paintThroughModePlug() ||
		plug == globalModePrecedencePlug() ||
		plug == pressureDefaultsEnabledPlug() ||
		plug == pressureDefaultsMappingModePlug() ||
		plug == pressureDefaultsDensityCurvePlug() ||
		plug == pressureDefaultsSoftnessCurvePlug();
}

void PaintPointsTool::invalidatePointCountOverlayCache()
{
	m_cachedPointCountNode = nullptr;
	m_cachedPointCount.reset();
}

void PaintPointsTool::refreshPointCountOverlay( Gaffer::Node *node, std::optional<int> knownCount )
{
	const auto totalStart = Clock::now();
	double countQueryMs = 0.0;
	double setTextMs = 0.0;
	double renderRequestMs = 0.0;
	std::string source = "gadgetMissing";
	bool textChanged = false;
	bool shouldRequestRender = false;
	auto setOverlayText = [&]( PointCountGadget *gadget, const std::string &text ) {
		const std::string previous = gadget->text();
		const auto setTextStart = Clock::now();
		gadget->setText( text );
		setTextMs += elapsedMilliseconds( setTextStart );
		textChanged = textChanged || previous != text;
		shouldRequestRender = shouldRequestRender || previous != text;
	};
	auto requestOverlayRender = [&]( const char *reason ) {
		if( !shouldRequestRender )
		{
			return;
		}
		const auto renderRequestStart = Clock::now();
		requestViewportRender( reason );
		renderRequestMs += elapsedMilliseconds( renderRequestStart );
	};

	auto *pointCountGadget = static_cast<PointCountGadget *>( m_pointCountGadget.get() );
	if( !pointCountGadget )
	{
		logInfo( sequenceTag( "HUD" ) + "pointCountOverlay source=" + source + " totalMs=" + std::to_string( elapsedMilliseconds( totalStart ) ) );
		return;
	}

	pointCountGadget->setVisible( activePlug()->getValue() );
	if( !activePlug()->getValue() )
	{
		source = "inactive";
		logInfo( sequenceTag( "HUD" ) + "pointCountOverlay source=" + source + " totalMs=" + std::to_string( elapsedMilliseconds( totalStart ) ) );
		return;
	}

	ScriptNode *script = toolScriptNode( this );
	if( !node && script )
	{
		node = findPaintedPointsNode( this, script );
	}

	if( !node )
	{
		source = "missingNode";
		invalidatePointCountOverlayCache();
		setOverlayText( pointCountGadget, "0 Points" );
		requestOverlayRender( "point count overlay refresh" );
		logInfo(
			sequenceTag( "HUD" ) +
			"pointCountOverlay source=" + source +
			" countQueryMs=" + std::to_string( countQueryMs ) +
			" setTextMs=" + std::to_string( setTextMs ) +
			" renderRequestMs=" + std::to_string( renderRequestMs ) +
			" textChanged=" + std::string( textChanged ? "true" : "false" ) +
			" totalMs=" + std::to_string( elapsedMilliseconds( totalStart ) )
		);
		return;
	}

	if( knownCount )
	{
		source = "knownCount";
		m_cachedPointCountNode = node;
		m_cachedPointCount = knownCount;
		setOverlayText( pointCountGadget, std::to_string( *knownCount ) + " Points" );
		requestOverlayRender( "point count overlay refresh" );
		logInfo(
			sequenceTag( "HUD" ) +
			"pointCountOverlay source=" + source +
			" countQueryMs=" + std::to_string( countQueryMs ) +
			" setTextMs=" + std::to_string( setTextMs ) +
			" renderRequestMs=" + std::to_string( renderRequestMs ) +
			" textChanged=" + std::string( textChanged ? "true" : "false" ) +
			" totalMs=" + std::to_string( elapsedMilliseconds( totalStart ) )
		);
		return;
	}

	if( node == m_cachedPointCountNode && m_cachedPointCount )
	{
		source = "cachedCount";
		setOverlayText( pointCountGadget, std::to_string( *m_cachedPointCount ) + " Points" );
		requestOverlayRender( "point count overlay refresh" );
		logInfo(
			sequenceTag( "HUD" ) +
			"pointCountOverlay source=" + source +
			" countQueryMs=" + std::to_string( countQueryMs ) +
			" setTextMs=" + std::to_string( setTextMs ) +
			" renderRequestMs=" + std::to_string( renderRequestMs ) +
			" textChanged=" + std::string( textChanged ? "true" : "false" ) +
			" totalMs=" + std::to_string( elapsedMilliseconds( totalStart ) )
		);
		return;
	}

	try
	{
		if( IntPlug *authoredPointCount = nodePlug<IntPlug>( node, "authoredPointCount" ) )
		{
			source = "nodePlug";
			const auto countQueryStart = Clock::now();
			const int totalPoints = authoredPointCount->getValue();
			countQueryMs = elapsedMilliseconds( countQueryStart );
			m_cachedPointCountNode = node;
			m_cachedPointCount = totalPoints;
			setOverlayText( pointCountGadget, std::to_string( totalPoints ) + " Points" );
			requestOverlayRender( "point count overlay refresh" );
			logInfo(
				sequenceTag( "HUD" ) +
				"pointCountOverlay source=" + source +
				" countQueryMs=" + std::to_string( countQueryMs ) +
				" setTextMs=" + std::to_string( setTextMs ) +
				" renderRequestMs=" + std::to_string( renderRequestMs ) +
				" textChanged=" + std::string( textChanged ? "true" : "false" ) +
				" totalMs=" + std::to_string( elapsedMilliseconds( totalStart ) )
			);
			return;
		}

		IECorePython::ScopedGILLock gilLock;
		bp::object pythonToolNode = pythonNode( node );
		const auto countQueryStart = Clock::now();
		int totalPoints = 0;
		if( PyObject_HasAttrString( pythonToolNode.ptr(), "_PaintedPoints__authoredPointCount" ) )
		{
			source = "pythonCachedAttr";
			totalPoints = bp::extract<int>( pythonToolNode.attr( "_PaintedPoints__authoredPointCount" ) );
		}
		else
		{
			source = "pythonQuery";
			totalPoints = bp::extract<int>( pythonToolNode.attr( "totalAuthoredPointCount" )() );
		}
		countQueryMs = elapsedMilliseconds( countQueryStart );
		m_cachedPointCountNode = node;
		m_cachedPointCount = totalPoints;
		setOverlayText( pointCountGadget, std::to_string( totalPoints ) + " Points" );
	}
	catch( const std::exception &exception )
	{
		source = "pythonError";
		logWarning( sequenceTag( "HUD" ) + "Unable to refresh point count overlay: " + std::string( exception.what() ) );
		invalidatePointCountOverlayCache();
		setOverlayText( pointCountGadget, "0 Points" );
	}

	requestOverlayRender( "point count overlay refresh" );
	logInfo(
		sequenceTag( "HUD" ) +
		"pointCountOverlay source=" + source +
		" countQueryMs=" + std::to_string( countQueryMs ) +
		" setTextMs=" + std::to_string( setTextMs ) +
		" renderRequestMs=" + std::to_string( renderRequestMs ) +
		" textChanged=" + std::string( textChanged ? "true" : "false" ) +
		" totalMs=" + std::to_string( elapsedMilliseconds( totalStart ) )
	);
}

void PaintPointsTool::syncMirroredControlsFromNode( Gaffer::Node *node )
{
	if( !node )
	{
		return;
	}

	m_syncingControlState = true;
	try
	{
		if( IntPlug *plug = nodePlug<IntPlug>( node, "relaxObjective" ) )
		{
			relaxObjectivePlug()->setValue( plug->getValue() );
		}
		if( IntPlug *plug = nodePlug<IntPlug>( node, "brushDefaults.rotationMode" ) )
		{
			rotationModePlug()->setValue( plug->getValue() );
		}
		if( IntPlug *plug = nodePlug<IntPlug>( node, "brushDefaults.points" ) )
		{
			pointsPlug()->setValue( plug->getValue() );
		}
		if( FloatPlug *plug = nodePlug<FloatPlug>( node, "brushDefaults.scaleJitter" ) )
		{
			scaleJitterPlug()->setValue( plug->getValue() );
		}
		if( FloatPlug *plug = nodePlug<FloatPlug>( node, "brushDefaults.widthJitter" ) )
		{
			widthJitterPlug()->setValue( plug->getValue() );
		}
		if( StringPlug *plug = nodePlug<StringPlug>( node, "targetFilter" ) )
		{
			targetFilterPlug()->setValue( plug->getValue() );
		}
		if( StringPlug *plug = nodePlug<StringPlug>( node, "targetSetFilter" ) )
		{
			targetSetFilterPlug()->setValue( plug->getValue() );
		}
		if( IntPlug *plug = nodePlug<IntPlug>( node, "surfaceMode" ) )
		{
			surfaceModePlug()->setValue( plug->getValue() );
		}
		if( IntPlug *plug = nodePlug<IntPlug>( node, "paintThroughMode" ) )
		{
			paintThroughModePlug()->setValue( plug->getValue() );
		}
		if( IntPlug *plug = nodePlug<IntPlug>( node, "globalModePrecedence" ) )
		{
			globalModePrecedencePlug()->setValue( plug->getValue() );
		}
		if( BoolPlug *plug = nodePlug<BoolPlug>( node, "pressureDefaults.enabled" ) )
		{
			pressureDefaultsEnabledPlug()->setValue( plug->getValue() );
		}
		if( IntPlug *plug = nodePlug<IntPlug>( node, "pressureDefaults.mappingMode" ) )
		{
			pressureDefaultsMappingModePlug()->setValue( plug->getValue() );
		}
		if( ObjectPlug *plug = nodePlug<ObjectPlug>( node, "pressureDefaults.densityCurve" ) )
		{
			pressureDefaultsDensityCurvePlug()->setValue( plug->getValue() );
		}
		if( ObjectPlug *plug = nodePlug<ObjectPlug>( node, "pressureDefaults.softnessCurve" ) )
		{
			pressureDefaultsSoftnessCurvePlug()->setValue( plug->getValue() );
		}
	}
	catch( const std::exception &exception )
	{
		logWarning( sequenceTag( "SYNC" ) + "Unable to mirror controls from node: " + std::string( exception.what() ) );
	}
	m_syncingControlState = false;
	const int primedPointCount = primedPointCountPlug()->getValue();
	if( node != m_cachedPointCountNode )
	{
		invalidatePointCountOverlayCache();
		if( primedPointCount > 0 )
		{
			refreshPointCountOverlay( node, primedPointCount );
		}
		else
		{
			refreshPointCountOverlay( node );
		}
	}
	else
	{
		refreshPointCountOverlay( node, primedPointCount > 0 ? std::optional<int>( primedPointCount ) : m_cachedPointCount );
	}
}

void PaintPointsTool::syncMirroredControlsToNode( Gaffer::Node *node, const Gaffer::Plug *sourcePlug )
{
	if( !node )
	{
		return;
	}

	auto shouldSync = [sourcePlug]( const Gaffer::Plug *toolPlug ) {
		return !sourcePlug || sourcePlug == toolPlug;
	};

	if( shouldSync( relaxObjectivePlug() ) )
	{
		if( IntPlug *plug = nodePlug<IntPlug>( node, "relaxObjective" ) )
		{
			plug->setValue( relaxObjectivePlug()->getValue() );
		}
	}
	if( shouldSync( pointsPlug() ) )
	{
		if( IntPlug *plug = nodePlug<IntPlug>( node, "brushDefaults.points" ) )
		{
			plug->setValue( pointsPlug()->getValue() );
		}
	}
	if( shouldSync( rotationModePlug() ) )
	{
		if( IntPlug *plug = nodePlug<IntPlug>( node, "brushDefaults.rotationMode" ) )
		{
			plug->setValue( rotationModePlug()->getValue() );
		}
	}
	if( shouldSync( scaleJitterPlug() ) )
	{
		if( FloatPlug *plug = nodePlug<FloatPlug>( node, "brushDefaults.scaleJitter" ) )
		{
			plug->setValue( scaleJitterPlug()->getValue() );
		}
	}
	if( shouldSync( widthJitterPlug() ) )
	{
		if( FloatPlug *plug = nodePlug<FloatPlug>( node, "brushDefaults.widthJitter" ) )
		{
			plug->setValue( widthJitterPlug()->getValue() );
		}
	}
	if( shouldSync( targetFilterPlug() ) )
	{
		if( StringPlug *plug = nodePlug<StringPlug>( node, "targetFilter" ) )
		{
			plug->setValue( targetFilterPlug()->getValue() );
		}
	}
	if( shouldSync( targetSetFilterPlug() ) )
	{
		if( StringPlug *plug = nodePlug<StringPlug>( node, "targetSetFilter" ) )
		{
			plug->setValue( targetSetFilterPlug()->getValue() );
		}
	}
	if( shouldSync( surfaceModePlug() ) )
	{
		if( IntPlug *plug = nodePlug<IntPlug>( node, "surfaceMode" ) )
		{
			plug->setValue( surfaceModePlug()->getValue() );
		}
	}
	if( shouldSync( paintThroughModePlug() ) )
	{
		if( IntPlug *plug = nodePlug<IntPlug>( node, "paintThroughMode" ) )
		{
			plug->setValue( paintThroughModePlug()->getValue() );
		}
	}
	if( shouldSync( globalModePrecedencePlug() ) )
	{
		if( IntPlug *plug = nodePlug<IntPlug>( node, "globalModePrecedence" ) )
		{
			plug->setValue( globalModePrecedencePlug()->getValue() );
		}
	}
	if( shouldSync( pressureDefaultsEnabledPlug() ) )
	{
		if( BoolPlug *plug = nodePlug<BoolPlug>( node, "pressureDefaults.enabled" ) )
		{
			plug->setValue( pressureDefaultsEnabledPlug()->getValue() );
		}
	}
	if( shouldSync( pressureDefaultsMappingModePlug() ) )
	{
		if( IntPlug *plug = nodePlug<IntPlug>( node, "pressureDefaults.mappingMode" ) )
		{
			plug->setValue( pressureDefaultsMappingModePlug()->getValue() );
		}
	}
	if( shouldSync( pressureDefaultsDensityCurvePlug() ) )
	{
		if( ObjectPlug *plug = nodePlug<ObjectPlug>( node, "pressureDefaults.densityCurve" ) )
		{
			plug->setValue( pressureDefaultsDensityCurvePlug()->getValue() );
		}
	}
	if( shouldSync( pressureDefaultsSoftnessCurvePlug() ) )
	{
		if( ObjectPlug *plug = nodePlug<ObjectPlug>( node, "pressureDefaults.softnessCurve" ) )
		{
			plug->setValue( pressureDefaultsSoftnessCurvePlug()->getValue() );
		}
	}
}
