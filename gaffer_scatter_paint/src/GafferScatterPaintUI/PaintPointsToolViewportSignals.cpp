#include "PaintPointsToolPrivate.h"

using namespace Gaffer;
using namespace GafferScatterPaintUI;
using namespace GafferScatterPaintUI::PaintPointsToolPrivate;
using namespace GafferSceneUI;
using namespace IECoreScene;
namespace bp = boost::python;

namespace
{

bool isMirroredNodePlug( const Gaffer::Plug *plug )
{
	if( !plug )
	{
		return false;
	}

	const std::string plugName = plug->fullName();
	return
		plugName.find( ".relaxObjective" ) != std::string::npos ||
		plugName.find( ".brushDefaults.points" ) != std::string::npos ||
		plugName.find( ".brushDefaults.rotationMode" ) != std::string::npos ||
		plugName.find( ".brushDefaults.scaleJitter" ) != std::string::npos ||
		plugName.find( ".brushDefaults.widthJitter" ) != std::string::npos ||
		plugName.find( ".targetFilter" ) != std::string::npos ||
		plugName.find( ".targetSetFilter" ) != std::string::npos ||
		plugName.find( ".surfaceMode" ) != std::string::npos ||
		plugName.find( ".paintThroughMode" ) != std::string::npos ||
		plugName.find( ".globalModePrecedence" ) != std::string::npos ||
		plugName.find( ".pressureDefaults.enabled" ) != std::string::npos ||
		plugName.find( ".pressureDefaults.mappingMode" ) != std::string::npos ||
		plugName.find( ".pressureDefaults.densityCurve" ) != std::string::npos ||
		plugName.find( ".pressureDefaults.softnessCurve" ) != std::string::npos;
}

}


void PaintPointsTool::connectViewportSignals()
{
	if(
		m_buttonPressConnection.connected() ||
		m_mouseMoveConnection.connected() ||
		m_enterConnection.connected() ||
		m_leaveConnection.connected() ||
		m_dragBeginConnection.connected() ||
		m_dragEnterConnection.connected() ||
		m_dragMoveConnection.connected() ||
		m_dragEndConnection.connected()
	)
	{
		return;
	}

	GafferUI::ViewportGadget *viewport = viewportGadget();
	GafferSceneUI::SceneGadget *scene = sceneGadget();
	if( !viewport || !scene )
	{
		logWarning( sequenceTag( "INIT" ) + "No viewport/scene gadget available; tool signals were not connected." );
		return;
	}

	logInfo( sequenceTag( "INIT" ) + "Connecting scene gadget button/drag signals." );

	m_buttonPressConnection = scene->buttonPressSignal().connectFront(
		[this]( GafferUI::Gadget *gadget, const GafferUI::ButtonEvent &event ) {
			return buttonPress( gadget, event );
		}
	);
	m_enterConnection = scene->enterSignal().connectFront(
		[this]( GafferUI::Gadget *gadget, const GafferUI::ButtonEvent &event ) {
			return enter( gadget, event );
		}
	);
	m_leaveConnection = scene->leaveSignal().connectFront(
		[this]( GafferUI::Gadget *gadget, const GafferUI::ButtonEvent &event ) {
			return leave( gadget, event );
		}
	);

	m_mouseMoveConnection = scene->mouseMoveSignal().connectFront(
		[this]( GafferUI::Gadget *gadget, const GafferUI::ButtonEvent &event ) {
			return mouseMove( gadget, event );
		}
	);
	m_dragBeginConnection = scene->dragBeginSignal().connectFront(
		[this]( GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event ) {
			return dragBegin( gadget, event );
		}
	);
	m_dragEnterConnection = scene->dragEnterSignal().connectFront(
		[this]( GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event ) {
			return dragEnter( gadget, event );
		}
	);
	m_dragMoveConnection = scene->dragMoveSignal().connectFront(
		[this]( GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event ) {
			return dragMove( gadget, event );
		}
	);
	m_dragEndConnection = scene->dragEndSignal().connectFront(
		[this]( GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event ) {
			return dragEnd( gadget, event );
		}
	);
}

void PaintPointsTool::disconnectViewportSignals()
{
	m_buttonPressConnection.disconnect();
	m_mouseMoveConnection.disconnect();
	m_enterConnection.disconnect();
	m_leaveConnection.disconnect();
	m_dragBeginConnection.disconnect();
	m_dragEnterConnection.disconnect();
	m_dragMoveConnection.disconnect();
	m_dragEndConnection.disconnect();
	clearDragState();
	GafferUI::Pointer::setCurrent( "" );
}

void PaintPointsTool::plugDirtied( const Gaffer::Plug *plug )
{
	if( plug == activePlug() )
	{
		m_pointCountGadget->setVisible( activePlug()->getValue() );
		if( activePlug()->getValue() )
		{
			logInfo( sequenceTag( "INIT" ) + "Tool activated; connecting scene gadget signals." );
			connectViewportSignals();
			invalidateLayerStrokeCache( this );
			contextChanged();
			ScriptNode *script = toolScriptNode( this );
			Node *node = script ? findPaintedPointsNode( this, script ) : nullptr;
			const int primedPointCount = primedPointCountPlug()->getValue();
			if( node && primedPointCount > 0 )
			{
				refreshPointCountOverlay( node, primedPointCount );
			}
			else
			{
				refreshPointCountOverlay();
			}
		}
		else
		{
			logInfo( sequenceTag( "INIT" ) + "Tool deactivated; disconnecting scene gadget signals." );
			m_cachedPointCountNode = nullptr;
			m_cachedPointCount.reset();
			primedPointCountPlug()->setValue( 0 );
			invalidateLayerStrokeCache( this );
			disconnectViewportSignals();
		}
		return;
	}

	if( m_syncingControlState )
	{
		return;
	}

	if(
		plug == frameModePlug() || plug == layerModePlug() ||
		plug == frameStartPlug() || plug == layerFrameStartPlug() ||
		plug == frameEndPlug() || plug == layerFrameEndPlug() ||
		plug == muteBehaviorPlug() || plug == layerMutePlug() ||
		plug == soloBehaviorPlug() || plug == layerSoloPlug()
	)
	{
		m_syncingControlState = true;
		if( plug == frameModePlug() )
		{
			layerModePlug()->setValue( normalizedFrameMode( frameModePlug()->getValue() ) );
		}
		else if( plug == layerModePlug() )
		{
			frameModePlug()->setValue( normalizedFrameMode( layerModePlug()->getValue() ) );
		}
		else if( plug == frameStartPlug() )
		{
			layerFrameStartPlug()->setValue( frameStartPlug()->getValue() );
		}
		else if( plug == layerFrameStartPlug() )
		{
			frameStartPlug()->setValue( layerFrameStartPlug()->getValue() );
		}
		else if( plug == frameEndPlug() )
		{
			layerFrameEndPlug()->setValue( frameEndPlug()->getValue() );
		}
		else if( plug == layerFrameEndPlug() )
		{
			frameEndPlug()->setValue( layerFrameEndPlug()->getValue() );
		}
		else if( plug == muteBehaviorPlug() )
		{
			layerMutePlug()->setValue( muteBehaviorPlug()->getValue() );
		}
		else if( plug == layerMutePlug() )
		{
			muteBehaviorPlug()->setValue( layerMutePlug()->getValue() );
		}
		else if( plug == soloBehaviorPlug() )
		{
			layerSoloPlug()->setValue( soloBehaviorPlug()->getValue() );
		}
		else if( plug == layerSoloPlug() )
		{
			soloBehaviorPlug()->setValue( layerSoloPlug()->getValue() );
		}
		m_syncingControlState = false;
		return;
	}

	if( plug == targetNodePlug() )
	{
		invalidateLayerStrokeCache( this );
		m_targetNodePlugSetConnection.disconnect();
		if( ScriptNode *script = toolScriptNode( this ) )
		{
			IECorePython::ScopedGILLock gilLock;
			if( Node *node = findPaintedPointsNode( this, script ) )
			{
				m_targetNodePlugSetConnection = node->plugSetSignal().connect(
					[this, node]( Gaffer::Plug *changedPlug ) {
						if( m_syncingControlState || !isMirroredNodePlug( changedPlug ) )
						{
							return;
						}
						syncMirroredControlsFromNode( node );
					}
				);
				syncMirroredControlsFromNode( node );
				if( primedPointCountPlug()->getValue() <= 0 )
				{
					primedPointCountPlug()->setValue( 0 );
				}
			}
			else
			{
				m_targetNodePlugSetConnection.disconnect();
				primedPointCountPlug()->setValue( 0 );
				refreshPointCountOverlay();
			}
		}
		else
		{
			m_targetNodePlugSetConnection.disconnect();
			primedPointCountPlug()->setValue( 0 );
			refreshPointCountOverlay();
		}
		return;
	}

	if( plug == layerNamePlug() || plug == strokeNamePlug() )
	{
		invalidateLayerStrokeCache( this );
	}

	if( isMirroredControlPlug( plug ) )
	{
		if( ScriptNode *script = toolScriptNode( this ) )
		{
			IECorePython::ScopedGILLock gilLock;
			if( Node *node = findPaintedPointsNode( this, script ) )
			{
				syncMirroredControlsToNode( node, plug );
			}
		}
	}
}

GafferUI::ViewportGadget *PaintPointsTool::viewportGadget()
{
	return view() ? view()->viewportGadget() : nullptr;
}

const GafferUI::ViewportGadget *PaintPointsTool::viewportGadget() const
{
	return view() ? view()->viewportGadget() : nullptr;
}

GafferSceneUI::SceneGadget *PaintPointsTool::sceneGadget()
{
	if( !viewportGadget() )
	{
		return nullptr;
	}

	return IECore::runTimeCast<GafferSceneUI::SceneGadget>( viewportGadget()->getPrimaryChild() );
}

const GafferSceneUI::SceneGadget *PaintPointsTool::sceneGadget() const
{
	if( !viewportGadget() )
	{
		return nullptr;
	}

	return IECore::runTimeCast<const GafferSceneUI::SceneGadget>( viewportGadget()->getPrimaryChild() );
}
