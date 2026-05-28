#include "PaintPointsToolPrivate.h"

using namespace Gaffer;
using namespace GafferScatterPaintUI;
using namespace GafferScatterPaintUI::PaintPointsToolPrivate;
using namespace GafferSceneUI;
using namespace IECoreScene;
namespace bp = boost::python;

size_t PaintPointsTool::commitHitPoint( const HitRecord &hit )
{
	if( isSelectBrushMode( this ) || isSelectLassoMode( this ) || isLayerEditMode( this ) || isStrokeEditMode( this ) )
	{
		ScriptNode *script = toolScriptNode( this );
		if( !script )
		{
			const std::string modeName = toolModeName( this );
			statusPlug()->setValue( modeName + " selection failed: no ScriptNode available." );
			return 0;
		}

		IECorePython::ScopedGILLock gilLock;
		Node *node = findPaintedPointsNode( this, script );
		if( !node )
		{
			const std::string modeName = toolModeName( this );
			statusPlug()->setValue( modeName + " selection failed: no PaintedPoints target was found." );
			return 0;
		}

		const bp::dict currentSelection =
			isStrokeEditMode( this ) && strokeEditActionPlug()->getValue() == g_strokeEditSplit ?
			currentSelectionState( node ) :
			selectionResultFromHit( node, hit, std::max( 0.001f, brushSizePlug()->getValue() ) );
		if( isLayerEditMode( this ) )
		{
			return applyLayerEditSelection( this, node, currentSelection );
		}
		if( isStrokeEditMode( this ) )
		{
			return applyStrokeEditSelection( this, node, currentSelection );
		}
		const size_t pointCount = bp::len( bp::extract<bp::list>( currentSelection["pointIds"] ) );
		const size_t strokeCount = bp::len( bp::extract<bp::list>( currentSelection["strokeIds"] ) );
		const std::string modeName = isSelectLassoMode( this ) ? "SelectLasso" : "SelectBrush";
		statusPlug()->setValue(
			modeName + " selected " + std::to_string( pointCount ) +
			" points across " + std::to_string( strokeCount ) +
			" strokes on " + hit.path + "."
		);
		return pointCount;
	}

	ScriptNode *script = toolScriptNode( this );
	if( !script )
	{
		logWarning( "Click placement requested without a ScriptNode." );
		statusPlug()->setValue( "Paint mode click commit failed: no ScriptNode available." );
		return 0;
	}

	IECorePython::ScopedGILLock gilLock;
	Node *node = nullptr;
	size_t committed = 0;
	{
		UndoScope undoScope( script );
		node = targetNodeOrCreate( this, script );
		if( !node )
		{
			logWarning( "Click placement failed because no PaintedPoints target could be found or created." );
			statusPlug()->setValue( "Paint mode click commit failed: no PaintedPoints target was available." );
			return 0;
		}
		syncMirroredControlsToNode( node );

		const auto ids = ensureLayerAndStroke( this, node );
		logRefreshDiagnostics( this, script, node, "beforeClickCommit" );
		logInfo(
			sequenceTag( "CLICK" ) + "Committing single hit to node=" + node->getName().string() +
			" layerId=" + std::to_string( ids.first ) +
			" strokeId=" + std::to_string( ids.second ) +
			" path=" + hit.path
		);
		bp::object pythonToolNode = pythonNode( node );
		bp::list samples;
		samples.append( brushSampleFromHit( this, hit, 0 ) );
		samples = expandedPaintSamples( this, samples, nullptr, nullptr, nullptr );
		const bp::list points = bp::extract<bp::list>( pythonToolNode.attr( "brushPaintPoints" )( samples ) );
		committed = bp::extract<size_t>( pythonToolNode.attr( "paintStrokeCommit" )( bp::object( ids.second ), points, true ) );
		logInfo(
			sequenceTag( "CLICK" ) + "Committed single hit on " + hit.path +
			" committed=" + std::to_string( committed )
		);
		logRefreshDiagnostics( this, script, node, "afterClickCommitBeforeRender" );
	}
	// UndoScope destructed -> dirty propagation flushed before refresh.
	refreshSceneGadget( "click commit" );
	logRefreshDiagnostics( this, script, node, "afterClickCommitAfterRender" );
	statusPlug()->setValue( "Paint mode committed " + std::to_string( committed ) + " points from scene hit on " + hit.path + "." );
	return committed;
}
