#include "PaintPointsToolPrivate.h"

using namespace Gaffer;
using namespace GafferScatterPaintUI;
using namespace GafferScatterPaintUI::PaintPointsToolPrivate;
using namespace GafferSceneUI;
using namespace IECoreScene;
namespace bp = boost::python;

GAFFER_NODE_DEFINE_TYPE( PaintPointsTool );

size_t PaintPointsTool::g_firstPlugIndex = 0;

PaintPointsTool::PaintPointsTool( SceneView *view, const std::string &name )
	: SelectionTool( view, name )
{
	storeIndexOfNextChild( g_firstPlugIndex );
	addChild( new IntPlug( "mode", Plug::In, 0 ) );
	addChild( new FloatPlug( "brushSize", Plug::In, 0.1f, 0.0f ) );
	addChild( new IntPlug( "points", Plug::In, 1, 1 ) );
	addChild( new FloatPlug( "density", Plug::In, 1.0f, 0.0f ) );
	addChild( new FloatPlug( "softness", Plug::In, 1.0f, 0.0f ) );
	addChild( new FloatPlug( "spacing", Plug::In, 0.1f, 0.0f ) );
	addChild( new IntPlug( "rotationMode", Plug::In, 0, 0, 2 ) );
	addChild( new FloatPlug( "scaleJitter", Plug::In, 0.0f, 0.0f ) );
	addChild( new FloatPlug( "widthJitter", Plug::In, 0.0f, 0.0f ) );
	addChild( new IntPlug( "frameMode", Plug::In, g_frameModePersistent, g_frameModePersistent, g_frameModeOverride ) );
	addChild( new IntPlug( "frameStart", Plug::In, 0 ) );
	addChild( new IntPlug( "frameEnd", Plug::In, 0 ) );
	addChild( new BoolPlug( "muteBehavior", Plug::In, false ) );
	addChild( new BoolPlug( "soloBehavior", Plug::In, false ) );
	addChild( new IntPlug( "relaxObjective", Plug::In, g_relaxObjectivePreserveSilhouette, g_relaxObjectivePreserveSilhouette, g_relaxObjectiveEvenRedistribution ) );
	addChild( new StringPlug( "targetFilter", Plug::In, "" ) );
	addChild( new StringPlug( "targetSetFilter", Plug::In, "" ) );
	addChild( new IntPlug( "surfaceMode", Plug::In, g_surfaceModeViewportMesh ) );
	addChild( new IntPlug( "paintThroughMode", Plug::In, g_paintThroughModeFrontMost ) );
	addChild( new IntPlug( "globalModePrecedence", Plug::In, g_modePrecedenceStrokeWins ) );
	addChild( new BoolPlug( "pressureDefaultsEnabled", Plug::In, true ) );
	addChild( new IntPlug( "pressureDefaultsMappingMode", Plug::In, g_pressureMappingDirect ) );
	addChild( new ObjectPlug( "pressureDefaultsDensityCurve", Plug::In, new IECore::CompoundObject() ) );
	addChild( new ObjectPlug( "pressureDefaultsSoftnessCurve", Plug::In, new IECore::CompoundObject() ) );
	addChild( new IntPlug( "eraseSpace", Plug::In, g_eraseSpaceVisible, g_eraseSpaceVisible, g_eraseSpaceAttachment ) );
	addChild( new FloatPlug( "pressureValue", Plug::In, 1.0f, 0.0f, 1.0f ) );
	addChild( new StringPlug( "targetNode" ) );
	addChild( new StringPlug( "layerName", Plug::In, "Layer 1" ) );
	addChild( new StringPlug( "strokeName", Plug::In, "Stroke 1" ) );
	addChild( new IntPlug( "layerEditAction", Plug::In, g_layerEditRename, g_layerEditRename, g_layerEditSetMode ) );
	addChild( new BoolPlug( "layerVisible", Plug::In, true ) );
	addChild( new BoolPlug( "layerMute", Plug::In, false ) );
	addChild( new BoolPlug( "layerSolo", Plug::In, false ) );
	addChild( new IntPlug( "layerFrameStart", Plug::In, 0 ) );
	addChild( new IntPlug( "layerFrameEnd", Plug::In, 0 ) );
	addChild( new IntPlug( "layerMoveToIndex", Plug::In, 0 ) );
	addChild( new IntPlug( "layerMode", Plug::In, g_frameModePersistent, g_frameModePersistent, g_frameModeOverride ) );
	addChild( new IntPlug( "strokeEditAction", Plug::In, g_strokeEditRename, g_strokeEditRename, g_strokeEditSplit ) );
	addChild( new IntPlug( "strokeMoveToIndex", Plug::In, 0 ) );
	addChild( new StringPlug( "strokeMergeTarget", Plug::In, "" ) );
	addChild( new IntPlug( "previewCount", Plug::In, 8, 1 ) );
	addChild( new IntPlug( "primedPointCount", Plug::In, 0, 0 ) );
	addChild( new StringPlug( "status", Plug::In, "Compiled tool shell ready." ) );
	activePlug()->setValue( true );

	layerModePlug()->setValue( frameModePlug()->getValue() );
	layerFrameStartPlug()->setValue( frameStartPlug()->getValue() );
	layerFrameEndPlug()->setValue( frameEndPlug()->getValue() );
	layerMutePlug()->setValue( muteBehaviorPlug()->getValue() );
	layerSoloPlug()->setValue( soloBehaviorPlug()->getValue() );
	
	m_brushRingGadget = new BrushRingGadget();
	view->viewportGadget()->addChild( m_brushRingGadget );
	m_pointCountGadget = new PointCountGadget();
	m_pointCountGadget->setTransform( Imath::M44f().translate( Imath::V3f( 10, 20, 0 ) ) );
	if( auto *pointCountGadget = static_cast<PointCountGadget *>( m_pointCountGadget.get() ) )
	{
		pointCountGadget->setText( "0 Points" );
	}
	view->viewportGadget()->setChild( "__scatterPaintPointCount", m_pointCountGadget );

	m_plugDirtiedConnection = plugDirtiedSignal().connect(
		[this]( const Gaffer::Plug *plug ) {
			plugDirtied( plug );
		}
	);
	m_contextChangedConnection = view->contextChangedSignal().connect(
		[this]( Gaffer::GraphComponent * ) {
			contextChanged();
		}
	);
	logInfo( sequenceTag( "INIT" ) + "Constructed tool and connecting viewport signals." );
	if( activePlug()->getValue() )
	{
		connectViewportSignals();
		contextChanged();
	}
}

PaintPointsTool::~PaintPointsTool()
{
}

void PaintPointsTool::contextChanged()
{
	if( !activePlug()->getValue() )
	{
		return;
	}

	const Gaffer::Context *context = view() ? view()->context() : nullptr;
	if( !context )
	{
		return;
	}

	const int frame = static_cast<int>( std::round( context->getFrame() ) );
	if( frameStartPlug()->getValue() == frame && frameEndPlug()->getValue() == frame )
	{
		return;
	}

	m_syncingControlState = true;
	frameStartPlug()->setValue( frame );
	frameEndPlug()->setValue( frame );
	m_syncingControlState = false;
}
