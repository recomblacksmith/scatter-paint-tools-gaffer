#include "PaintPointsToolPrivate.h"

using namespace Gaffer;
using namespace GafferScatterPaintUI;
using namespace GafferScatterPaintUI::PaintPointsToolPrivate;
using namespace GafferSceneUI;
using namespace IECoreScene;
namespace bp = boost::python;

namespace
{

bool appendUniqueRasterPoint( std::vector<Imath::V2f> &points, const Imath::V2f &point )
{
	if( !points.empty() )
	{
		const Imath::V2f delta = point - points.back();
		if( delta.length2() < 1.0f )
		{
			return false;
		}
	}
	points.push_back( point );
	return true;
}

} // namespace

int PaintPointsTool::resolvedPointCount( const StrokePoints &strokePoints ) const
{
	return static_cast<int>( std::count_if( strokePoints.begin(), strokePoints.end(), []( const HitRecord &hit ) {
		return hit.attachmentResolved;
	} ) );
}

bool PaintPointsTool::appendDragHit( const HitRecord &hit )
{
	if( !m_dragStroke.empty() )
	{
		const HitRecord &previous = m_dragStroke.back();
		const Imath::V3f delta = hit.point - previous.point;
		const float distance = delta.length();
		const float spacing = sampleSpacing( this );
		if( previous.path == hit.path && distance < spacing )
		{
			logInfo(
				sequenceTag( "APPEND" ) + "Rejected drag hit on " + hit.path +
				" due to spacing threshold (distance=" + std::to_string( distance ) +
				", spacing=" + std::to_string( spacing ) + ")"
			);
			return false;
		}
	}

	m_dragStroke.push_back( hit );
	m_dragStrokeSourcePath = hit.path;
	const int resolved = resolvedPointCount( m_dragStroke );
	const int fallback = static_cast<int>( m_dragStroke.size() ) - resolved;
	logInfo(
		sequenceTag( "APPEND" ) + "Accepted drag hit on " + hit.path +
		" point=(" + std::to_string( hit.point.x ) + ", " + std::to_string( hit.point.y ) + ", " + std::to_string( hit.point.z ) +
		") strokeCount=" + std::to_string( m_dragStroke.size() ) +
		" resolved=" + std::to_string( resolved ) +
		" fallback=" + std::to_string( fallback )
	);
	return true;
}

void PaintPointsTool::clearDragState()
{
	if( m_dragActive || !m_dragStroke.empty() )
	{
		logInfo(
			sequenceTag( "CLEAR" ) + "Clearing drag state (active=" + std::string( m_dragActive ? "true" : "false" ) +
			", points=" + std::to_string( m_dragStroke.size() ) + ")"
		);
	}
	m_dragStroke.clear();
	m_dragStrokeSourcePath.clear();
	m_dragRasterPath.clear();
	m_dragStartRaster.reset();
	m_dragCurrentRaster.reset();
	m_dragActive = false;
}

void PaintPointsTool::requestViewportRender( const char *reason )
{
	const std::string reasonString = reason ? std::string( reason ) : std::string( "commit" );
	if( GafferUI::ViewportGadget *viewport = viewportGadget() )
	{
		viewport->renderRequestSignal()( viewport );
		logInfo( sequenceTag( "COMMIT" ) + "Requested viewport redraw after " + reasonString + "." );
	}
	else
	{
		logWarning( sequenceTag( "COMMIT" ) + "Unable to request viewport redraw after " + reasonString + " because no viewport gadget was available." );
	}
}

void PaintPointsTool::refreshSceneGadget( const std::string &reason )
{
	GafferSceneUI::SceneGadget *scene = sceneGadget();
	GafferUI::ViewportGadget *viewport = viewportGadget();

	if( !scene || !viewport )
	{
		logWarning( sequenceTag( "COMMIT" ) + "Unable to refresh SceneGadget after " + reason + " (scene or viewport unavailable)." );
		return;
	}

	ScriptNode *script = toolScriptNode( this );
	IECore::PathMatcher priorityPaths = affectedOutputLocations( script );

	logInfo(
		sequenceTag( "COMMIT" ) + "Refreshing SceneGadget after " + reason +
		": state=" + sceneGadgetStateName( scene->state() ) +
		" priorityPathCount=" + std::to_string( priorityPaths.size() )
	);

	if( !priorityPaths.isEmpty() )
	{
		scene->setPriorityPaths( priorityPaths );
	}
	else
	{
		scene->setPriorityPaths( IECore::PathMatcher() );
	}

	requestViewportRender( reason.c_str() );

	logInfo( sequenceTag( "COMMIT" ) + "SceneGadget refresh requested after " + reason + ": state=" + sceneGadgetStateName( scene->state() ) );
}

bool PaintPointsTool::buttonPress( GafferUI::Gadget *gadget, const GafferUI::ButtonEvent &event )
{
	logInfo(
		sequenceTag( "PRESS" ) + "buttonPress button=" + std::to_string( static_cast<int>( event.button ) ) +
		" buttons=" + std::to_string( static_cast<int>( event.buttons ) ) +
		" active=" + std::string( activePlug()->getValue() ? "true" : "false" )
	);
	if( !activePlug()->getValue() )
	{
		logInfo( sequenceTag( "PRESS" ) + "Ignoring buttonPress because tool is inactive." );
		return false;
	}

	if( event.modifiers & GafferUI::ModifiableEvent::Alt )
	{
		logInfo( sequenceTag( "PRESS" ) + "Passing buttonPress through because Alt navigation modifier is held." );
		return false;
	}

	if( event.button != GafferUI::ButtonEvent::Left || event.buttons != GafferUI::ButtonEvent::Left )
	{
		logInfo( sequenceTag( "PRESS" ) + "Ignoring buttonPress because it is not a left-button-only press." );
		return false;
	}

	clearDragState();
	m_dragStartRaster = rasterPosition( this, event.line );
	m_dragCurrentRaster = m_dragStartRaster;
	if( m_dragStartRaster )
	{
		m_dragRasterPath.push_back( *m_dragStartRaster );
	}
	const auto hit = hitPoint( event.line );
	if( !hit )
	{
		if( isSelectLassoMode( this ) && m_dragStartRaster )
		{
			statusPlug()->setValue( "SelectLasso armed marquee selection." );
			return true;
		}
		logInfo( sequenceTag( "PRESS" ) + "buttonPress found no scene hit under cursor." );
		return false;
	}

	logInfo(
		sequenceTag( "PRESS" ) + "buttonPress resolved hit path=" + hit->path +
		" point=(" + std::to_string( hit->point.x ) + ", " + std::to_string( hit->point.y ) + ", " + std::to_string( hit->point.z ) + ")"
	);
	if( isEraseMode( this ) )
	{
		m_dragActive = false;
		const bool appended = appendDragHit( *hit );
		if( appended )
		{
			statusPlug()->setValue( "Erase mode armed brush erase on " + hit->path + "." );
		}
		return appended;
	}
	if( isSelectBrushMode( this ) )
	{
		clearDragState();
		const size_t selected = commitHitPoint( *hit );
		return selected > 0;
	}
	if( isSelectLassoMode( this ) )
	{
		m_dragActive = false;
		m_dragCurrentRaster = rasterPosition( this, event.line );
		const bool appended = appendDragHit( *hit );
		if( appended )
		{
			statusPlug()->setValue( "SelectLasso armed brush or marquee selection on " + hit->path + "." );
		}
		return appended;
	}
	if( isRelaxMode( this ) || isReprojectMode( this ) )
	{
		m_dragActive = false;
		m_dragCurrentRaster = rasterPosition( this, event.line );
		const bool appended = appendDragHit( *hit );
		if( appended )
		{
			statusPlug()->setValue( toolModeName( this ) + " armed selection stroke on " + hit->path + "." );
		}
		return appended;
	}
	if( isLayerEditMode( this ) || isStrokeEditMode( this ) )
	{
		clearDragState();
		const size_t changed = commitHitPoint( *hit );
		return changed > 0;
	}
	m_dragActive = false;
	const bool appended = appendDragHit( *hit );
	if( appended )
	{
		statusPlug()->setValue( "Paint mode armed stroke on " + hit->path + "." );
	}
	logInfo( sequenceTag( "PRESS" ) + "buttonPress append result=" + std::string( appended ? "accepted" : "rejected" ) );
	return true;
}

bool PaintPointsTool::enter( GafferUI::Gadget *gadget, const GafferUI::ButtonEvent &event )
{
	if( activePlug()->getValue() )
	{
		GafferUI::Pointer::setCurrent( "crossHair" );
	}
	return false;
}

bool PaintPointsTool::leave( GafferUI::Gadget *gadget, const GafferUI::ButtonEvent &event )
{
	GafferUI::Pointer::setCurrent( "" );
	if( auto *brushGadget = static_cast<BrushRingGadget*>( m_brushRingGadget.get() ) )
	{
		brushGadget->setVisible( false );
	}
	return false;
}


void PaintPointsTool::updateBrushGadget( const IECore::LineSegment3f &line )
{
	auto *brushGadget = static_cast<BrushRingGadget*>( m_brushRingGadget.get() );
	if( !brushGadget ) return;

	if( !activePlug()->getValue() )
	{
		brushGadget->setVisible( false );
		return;
	}

	const auto hit = hitPoint( line );
	if( hit )
	{
		brushGadget->setRing( hit->point, hit->normal, std::max( 0.001f, brushSizePlug()->getValue() ) );
		brushGadget->setVisible( true );
	}
	else
	{
		brushGadget->setVisible( false );
	}
}

bool PaintPointsTool::mouseMove( GafferUI::Gadget *gadget, const GafferUI::ButtonEvent &event )
{
	updateBrushGadget( event.line );

	if ( activePlug()->getValue() )
	{
		GafferUI::Pointer::setCurrent( "crossHair" );
	}

	if( !m_dragActive || event.buttons != GafferUI::ButtonEvent::Left )
	{
		return false;
	}

	if( event.modifiers & GafferUI::ModifiableEvent::Alt )
	{
		return false;
	}

	m_dragCurrentRaster = rasterPosition( this, event.line );
	if( isSelectLassoMode( this ) && !m_dragStroke.empty() && m_dragCurrentRaster )
	{
		return true;
	}

	const auto hit = hitPoint( event.line );
	if( !hit )
	{
		return false;
	}

	return true;
}

IECore::RunTimeTypedPtr PaintPointsTool::dragBegin( GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event )
{
	logInfo(
		sequenceTag( "BEGIN" ) + "dragBegin button=" + std::to_string( static_cast<int>( event.button ) ) +
		" buttons=" + std::to_string( static_cast<int>( event.buttons ) ) +
		" active=" + std::string( activePlug()->getValue() ? "true" : "false" )
	);
	if( !activePlug()->getValue() )
	{
		logInfo( sequenceTag( "BEGIN" ) + "Ignoring dragBegin because tool is inactive." );
		return nullptr;
	}

	if( isSelectBrushMode( this ) )
	{
		logInfo( sequenceTag( "BEGIN" ) + "Ignoring dragBegin because SelectBrush uses click selection." );
		return nullptr;
	}

	if( isEraseMode( this ) || isSelectLassoMode( this ) || isRelaxMode( this ) || isReprojectMode( this ) )
	{
		if( event.modifiers & GafferUI::ModifiableEvent::Alt )
		{
			logInfo( sequenceTag( "BEGIN" ) + "Passing dragBegin through because Alt navigation modifier is held." );
			return nullptr;
		}

		if( event.buttons != GafferUI::ButtonEvent::Left )
		{
			logInfo( sequenceTag( "BEGIN" ) + "Ignoring dragBegin because left button is not held." );
			return nullptr;
		}

		m_dragStartRaster = rasterPosition( this, event.line );
		m_dragCurrentRaster = m_dragStartRaster;
		const auto hit = hitPoint( event.line );
		if( !hit )
		{
			if( isSelectLassoMode( this ) && m_dragStartRaster )
			{
				const std::optional<Imath::V2f> dragStartRaster = m_dragStartRaster;
				clearDragState();
				m_dragStartRaster = dragStartRaster;
				m_dragCurrentRaster = dragStartRaster;
				if( dragStartRaster )
				{
					m_dragRasterPath.push_back( *dragStartRaster );
				}
				m_dragActive = true;
				const std::string modeName = toolModeName( this );
				statusPlug()->setValue( modeName + " collecting lasso selection." );
				GafferUI::Pointer::setCurrent( "values" );
				return new IECore::NullObject;
			}
			logInfo( sequenceTag( "BEGIN" ) + toolModeName( this ) + " dragBegin found no scene hit under cursor." );
			return nullptr;
		}

		const bool reusesBufferedStart =
			!m_dragStroke.empty() &&
			m_dragStroke.back().path == hit->path &&
			( m_dragStroke.back().point - hit->point ).length2() < 1e-8f;
		if( !reusesBufferedStart )
		{
			clearDragState();
		}

		m_dragActive = true;
		const bool appended = reusesBufferedStart ? true : appendDragHit( *hit );
		const std::string modeName = toolModeName( this );
		statusPlug()->setValue( appended ? modeName + " collecting drag samples." : modeName + " drag sample was rejected." );
		GafferUI::Pointer::setCurrent( "values" );
		return new IECore::NullObject;
	}

	if( isLayerEditMode( this ) || isStrokeEditMode( this ) )
	{
		logInfo( sequenceTag( "BEGIN" ) + "Ignoring dragBegin because edit modes use click-driven actions." );
		return nullptr;
	}

	if( event.modifiers & GafferUI::ModifiableEvent::Alt )
	{
		logInfo( sequenceTag( "BEGIN" ) + "Passing dragBegin through because Alt navigation modifier is held." );
		return nullptr;
	}

	if( event.buttons != GafferUI::ButtonEvent::Left )
	{
		logInfo( sequenceTag( "BEGIN" ) + "Ignoring dragBegin because left button is not held." );
		return nullptr;
	}

	const auto hit = hitPoint( event.line );
	if( !hit )
	{
		logInfo( sequenceTag( "BEGIN" ) + "dragBegin found no scene hit under cursor." );
		return nullptr;
	}

	const bool reusesBufferedStart =
		!m_dragStroke.empty() &&
		m_dragStroke.back().path == hit->path &&
		( m_dragStroke.back().point - hit->point ).length2() < 1e-8f;
	if( reusesBufferedStart )
	{
		logInfo( sequenceTag( "BEGIN" ) + "dragBegin reusing buffered press hit as drag start sample." );
	}
	else
	{
		clearDragState();
	}

	m_dragActive = true;
	logInfo(
		sequenceTag( "BEGIN" ) + "dragBegin started on path=" + hit->path +
		" point=(" + std::to_string( hit->point.x ) + ", " + std::to_string( hit->point.y ) + ", " + std::to_string( hit->point.z ) + ")"
	);
	const bool appended = reusesBufferedStart ? true : appendDragHit( *hit );
	logInfo( sequenceTag( "BEGIN" ) + "dragBegin append result=" + std::string( appended ? "accepted" : "rejected" ) );
	GafferUI::Pointer::setCurrent( "values" );
	return new IECore::NullObject;
}

bool PaintPointsTool::dragEnter( const GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event )
{
	const bool accepted =
		m_dragActive &&
		event.sourceGadget == gadget &&
		event.data &&
		event.data->isInstanceOf( IECore::NullObjectTypeId );
	logInfo(
		sequenceTag( "MOVE" ) + "dragEnter accepted=" + std::string( accepted ? "true" : "false" ) +
		" dragActive=" + std::string( m_dragActive ? "true" : "false" )
	);
	return accepted;
}

bool PaintPointsTool::dragMove( GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event )
{
	updateBrushGadget( event.line );

	logInfo(
		sequenceTag( "MOVE" ) + "dragMove active=" + std::string( activePlug()->getValue() ? "true" : "false" ) +
		" dragActive=" + std::string( m_dragActive ? "true" : "false" )
	);
	if( !activePlug()->getValue() || !m_dragActive )
	{
		logInfo( sequenceTag( "MOVE" ) + "Ignoring dragMove because tool is inactive or drag is not active." );
		return false;
	}

	if( event.modifiers & GafferUI::ModifiableEvent::Alt )
	{
		logInfo( sequenceTag( "MOVE" ) + "Passing dragMove through because Alt navigation modifier is held." );
		return false;
	}

	m_dragCurrentRaster = rasterPosition( this, event.line );
	if( isSelectLassoMode( this ) && m_dragCurrentRaster )
	{
		appendUniqueRasterPoint( m_dragRasterPath, *m_dragCurrentRaster );
		if( const auto hit = hitPoint( event.line ) )
		{
			logInfo(
				sequenceTag( "MOVE" ) + "dragMove resolved hit path=" + hit->path +
				" point=(" + std::to_string( hit->point.x ) + ", " + std::to_string( hit->point.y ) + ", " + std::to_string( hit->point.z ) + ")"
			);
			const bool appended = appendDragHit( *hit );
			logInfo(
				sequenceTag( "MOVE" ) + "dragMove append result=" + std::string( appended ? "accepted" : "rejected" ) +
				" bufferedPoints=" + std::to_string( m_dragStroke.size() )
			);
		}
		return true;
	}

	const auto hit = hitPoint( event.line );
	if( !hit )
	{
		logInfo( sequenceTag( "MOVE" ) + "dragMove found no scene hit under cursor." );
		return false;
	}

	logInfo(
		sequenceTag( "MOVE" ) + "dragMove resolved hit path=" + hit->path +
		" point=(" + std::to_string( hit->point.x ) + ", " + std::to_string( hit->point.y ) + ", " + std::to_string( hit->point.z ) + ")"
	);
	const bool appended = appendDragHit( *hit );
	logInfo(
		sequenceTag( "MOVE" ) + "dragMove append result=" + std::string( appended ? "accepted" : "rejected" ) +
		" bufferedPoints=" + std::to_string( m_dragStroke.size() )
	);
	return true;
}

bool PaintPointsTool::dragEnd( GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event )
{
	logInfo(
		sequenceTag( "END" ) + "dragEnd dragActive=" + std::string( m_dragActive ? "true" : "false" ) +
		" bufferedPoints=" + std::to_string( m_dragStroke.size() )
	);
	if( event.modifiers & GafferUI::ModifiableEvent::Alt )
	{
		logInfo( sequenceTag( "END" ) + "Passing dragEnd through because Alt navigation modifier is held." );
		clearDragState();
		if( activePlug()->getValue() ) GafferUI::Pointer::setCurrent( "crossHair" );
		return false;
	}
	if( activePlug()->getValue() ) GafferUI::Pointer::setCurrent( "crossHair" );
	m_dragCurrentRaster = rasterPosition( this, event.line );
	if( isSelectLassoMode( this ) && m_dragCurrentRaster )
	{
		appendUniqueRasterPoint( m_dragRasterPath, *m_dragCurrentRaster );
	}
	if( !m_dragActive || m_dragStroke.empty() )
	{
		if( isSelectLassoMode( this ) && m_dragActive && marqueeHasArea( m_dragStartRaster, m_dragCurrentRaster ) )
		{
			ScriptNode *script = toolScriptNode( this );
			if( !script )
			{
				statusPlug()->setValue( "SelectLasso selection failed: no ScriptNode available." );
				clearDragState();
				return false;
			}

			IECorePython::ScopedGILLock gilLock;
			Node *node = findPaintedPointsNode( this, script );
			if( !node )
			{
				statusPlug()->setValue( "SelectLasso selection failed: no PaintedPoints target was found." );
				clearDragState();
				return false;
			}

			size_t pointCount = 0;
			size_t strokeCount = 0;
			{
				UndoScope undoScope( script );
				syncMirroredControlsToNode( node );
				const bp::dict currentSelection = selectionResultFromLasso( node, viewportGadget(), m_dragRasterPath, m_dragStartRaster, m_dragCurrentRaster );
				pointCount = bp::len( bp::extract<bp::list>( currentSelection["pointIds"] ) );
				strokeCount = bp::len( bp::extract<bp::list>( currentSelection["strokeIds"] ) );
			}
			clearDragState();
			statusPlug()->setValue(
				"SelectLasso selected " + std::to_string( pointCount ) +
				" points across " + std::to_string( strokeCount ) + " strokes using lasso."
			);
			return pointCount > 0;
		}

		logInfo( sequenceTag( "END" ) + "dragEnd had no active drag or buffered points; probing final hit for click placement." );
		if( const auto hit = hitPoint( event.line ) )
		{
			logInfo(
				sequenceTag( "END" ) + "dragEnd recovered hit path=" + hit->path +
				" point=(" + std::to_string( hit->point.x ) + ", " + std::to_string( hit->point.y ) + ", " + std::to_string( hit->point.z ) + ")"
			);
			StrokePoints recoveredStroke;
			recoveredStroke.push_back( *hit );
			clearDragState();
			if( isSelectLassoMode( this ) || isRelaxMode( this ) || isReprojectMode( this ) )
			{
				ScriptNode *script = toolScriptNode( this );
				if( !script )
				{
					statusPlug()->setValue( toolModeName( this ) + " selection failed: no ScriptNode available." );
					return false;
				}
				IECorePython::ScopedGILLock gilLock;
				Node *node = findPaintedPointsNode( this, script );
				if( !node )
				{
					statusPlug()->setValue( toolModeName( this ) + " selection failed: no PaintedPoints target was found." );
					return false;
				}
				size_t changed = 0;
				{
					UndoScope undoScope( script );
					syncMirroredControlsToNode( node );
					const bp::dict currentSelection = selectionResultFromStroke( node, recoveredStroke, std::max( 0.001f, brushSizePlug()->getValue() ) );
					if( isSelectLassoMode( this ) )
					{
						const size_t pointCount = bp::len( bp::extract<bp::list>( currentSelection["pointIds"] ) );
						const size_t strokeCount = bp::len( bp::extract<bp::list>( currentSelection["strokeIds"] ) );
						statusPlug()->setValue(
							"SelectLasso selected " + std::to_string( pointCount ) +
							" points across " + std::to_string( strokeCount ) +
							" strokes on " + hit->path + "."
						);
						return pointCount > 0;
					}
					changed = applySelectionAction( this, node, currentSelection );
				}
				if( changed )
				{
					refreshSceneGadget( toolModeName( this ) + " selection" );
				}
				return changed > 0;
			}
			if( isEraseMode( this ) )
			{
				const size_t removed = eraseStrokePoints( recoveredStroke );
				return removed > 0;
			}
			const size_t committed = commitStrokePoints( recoveredStroke );
			return committed > 0;
		}

		logInfo( sequenceTag( "END" ) + "dragEnd found no final hit to commit." );
		clearDragState();
		return false;
	}

	if( const auto hit = hitPoint( event.line ) )
	{
		logInfo(
			sequenceTag( "END" ) + "dragEnd resolved final hit path=" + hit->path +
			" point=(" + std::to_string( hit->point.x ) + ", " + std::to_string( hit->point.y ) + ", " + std::to_string( hit->point.z ) + ")"
		);
		const bool appended = appendDragHit( *hit );
		logInfo(
			sequenceTag( "END" ) + "dragEnd append result=" + std::string( appended ? "accepted" : "rejected" ) +
			" bufferedPoints=" + std::to_string( m_dragStroke.size() )
		);
	}
	else
	{
		logInfo( sequenceTag( "END" ) + "dragEnd found no final hit under cursor before commit." );
	}

	StrokePoints strokePoints = m_dragStroke;
	const std::vector<Imath::V2f> dragRasterPath = m_dragRasterPath;
	const std::optional<Imath::V2f> dragStartRaster = m_dragStartRaster;
	const std::optional<Imath::V2f> dragEndRaster = m_dragCurrentRaster;
	clearDragState();
	if( isEraseMode( this ) )
	{
		return eraseStrokePoints( strokePoints ) > 0;
	}
	if( isSelectLassoMode( this ) || isRelaxMode( this ) || isReprojectMode( this ) )
	{
		ScriptNode *script = toolScriptNode( this );
		if( !script )
		{
			statusPlug()->setValue( toolModeName( this ) + " selection failed: no ScriptNode available." );
			return false;
		}

		IECorePython::ScopedGILLock gilLock;
		Node *node = findPaintedPointsNode( this, script );
		if( !node )
		{
			statusPlug()->setValue( toolModeName( this ) + " selection failed: no PaintedPoints target was found." );
			return false;
		}

		const float radius = std::max( 0.001f, brushSizePlug()->getValue() );
		size_t changed = 0;
		{
			UndoScope undoScope( script );
			syncMirroredControlsToNode( node );
			const bp::dict currentSelection =
				isSelectLassoMode( this ) ?
				selectionResultFromLasso( node, viewportGadget(), dragRasterPath, dragStartRaster, dragEndRaster ) :
				selectionResultFromStroke( node, strokePoints, radius );
			if( isSelectLassoMode( this ) )
			{
				const size_t pointCount = bp::len( bp::extract<bp::list>( currentSelection["pointIds"] ) );
				const size_t strokeCount = bp::len( bp::extract<bp::list>( currentSelection["strokeIds"] ) );
				statusPlug()->setValue(
					std::string( "SelectLasso selected " ) + std::to_string( pointCount ) +
					" points across " + std::to_string( strokeCount ) + " strokes using lasso."
				);
				return pointCount > 0;
			}
			changed = applySelectionAction( this, node, currentSelection );
		}
		if( changed )
		{
			refreshSceneGadget( toolModeName( this ) + " selection" );
		}
		return changed > 0;
	}
	if( isLayerEditMode( this ) )
	{
		statusPlug()->setValue( layerEditActionHelp( this ) );
		return 0;
	}
	if( isStrokeEditMode( this ) )
	{
		statusPlug()->setValue( strokeEditActionHelp( this ) );
		return 0;
	}

	if( strokePoints.size() >= 2 && strokePoints.front().path == strokePoints.back().path )
	{
		const float spacing = sampleSpacing( this );
		const size_t originalCount = strokePoints.size();
		strokePoints = densifiedStroke( strokePoints, spacing );
		logInfo(
			sequenceTag( "END" ) + "densified stroke from " + std::to_string( originalCount ) +
			" to " + std::to_string( strokePoints.size() ) + " samples using spacing=" + std::to_string( spacing )
		);
	}
	logInfo( sequenceTag( "END" ) + "dragEnd committing buffered stroke with " + std::to_string( strokePoints.size() ) + " points." );
	commitStrokePoints( strokePoints );
	return true;
}
