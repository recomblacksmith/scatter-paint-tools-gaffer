#pragma once

#include "GafferScatterPaint/BrushSampleData.h"
#include "GafferScatterPaintUI/PaintPointsTool.h"

#include "Gaffer/GraphComponent.h"
#include "Gaffer/Node.h"
#include "Gaffer/NumericPlug.h"
#include "Gaffer/ScriptNode.h"
#include "Gaffer/StandardSet.h"
#include "Gaffer/StringPlug.h"
#include "Gaffer/TypedObjectPlug.h"
#include "Gaffer/UndoScope.h"
#include "GafferScene/ScenePlug.h"
#include "GafferSceneUI/SceneGadget.h"
#include "GafferSceneUI/SceneView.h"
#include "GafferUI/ButtonEvent.h"
#include "GafferUI/DragDropEvent.h"
#include "GafferUI/Gadget.h"
#include "GafferUI/Pointer.h"
#include "GafferUI/Style.h"
#include "GafferUI/ViewportGadget.h"

#include "IECore/MessageHandler.h"
#include "IECore/NullObject.h"
#include "IECore/PathMatcher.h"
#include "IECore/RunTimeTyped.h"
#include "IECorePython/ScopedGILLock.h"
#include "IECoreScene/MeshPrimitive.h"
#include "IECoreScene/PointsPrimitive.h"

#include "Imath/ImathBox.h"
#include "Imath/ImathMatrix.h"
#include "Imath/ImathVec.h"

#include "boost/python.hpp"

#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace GafferScatterPaintUI
{

namespace PaintPointsToolPrivate
{

namespace bp = boost::python;
using GafferScatterPaint::BrushSampleData;
using GafferScatterPaint::BrushSampleDataList;

inline constexpr int g_modePaint = 0;
inline constexpr int g_modeErase = 1;
inline constexpr int g_modeSelectBrush = 2;
inline constexpr int g_modeSelectLasso = 3;
inline constexpr int g_modeRelax = 4;
inline constexpr int g_modeReproject = 5;
inline constexpr int g_modeLayerEdit = 6;
inline constexpr int g_modeStrokeEdit = 7;
inline constexpr int g_relaxObjectivePreserveSilhouette = 0;
inline constexpr int g_relaxObjectiveEvenRedistribution = 1;
inline constexpr int g_surfaceModeViewportMesh = 0;
inline constexpr int g_paintThroughModeFrontMost = 0;
inline constexpr int g_eraseSpaceVisible = 0;
inline constexpr int g_eraseSpaceAttachment = 1;
inline constexpr int g_modePrecedenceStrokeWins = 0;
inline constexpr int g_frameModePersistent = 0;
inline constexpr int g_frameModeAdditive = 1;
inline constexpr int g_frameModeOverride = 2;
inline constexpr int g_pressureMappingDirect = 0;
inline constexpr int g_layerEditRename = 0;
inline constexpr int g_layerEditSetVisible = 1;
inline constexpr int g_layerEditSetMute = 2;
inline constexpr int g_layerEditSetSolo = 3;
inline constexpr int g_layerEditSetTimeRange = 4;
inline constexpr int g_layerEditMove = 5;
inline constexpr int g_layerEditSetMode = 6;
inline constexpr int g_strokeEditRename = 0;
inline constexpr int g_strokeEditDelete = 1;
inline constexpr int g_strokeEditMove = 2;
inline constexpr int g_strokeEditMerge = 3;
inline constexpr int g_strokeEditSplit = 4;

inline constexpr const char *g_logContext = "PaintPointsTool";

class BrushRingGadget : public GafferUI::Gadget
{

	public :

		BrushRingGadget()
			: Gadget( "BrushRing" ), m_visible( false ), m_radius( 1.0f )
		{
		}

		void setRing( const Imath::V3f &center, const Imath::V3f &normal, float radius )
		{
			m_center = center;
			m_normal = normal;
			m_radius = radius;
			m_visible = true;
			dirty( DirtyType::Bound );
			dirty( DirtyType::Render );
		}

		void setVisible( bool visible )
		{
			if( m_visible != visible )
			{
				m_visible = visible;
				dirty( DirtyType::Bound );
				dirty( DirtyType::Render );
			}
		}

		Imath::Box3f bound() const override
		{
			if( !m_visible )
			{
				return Imath::Box3f();
			}

			const Imath::V3f r( m_radius, m_radius, m_radius );
			return Imath::Box3f( m_center - r, m_center + r );
		}

		Imath::Box3f renderBound() const override
		{
			Imath::Box3f b;
			b.makeInfinite();
			return b;
		}

	protected :

		void renderLayer( Layer layer, const GafferUI::Style *, RenderReason ) const override
		{
			if( layer != Layer::MidFront || !m_visible )
			{
				return;
			}

			Imath::M44f xform;
			xform.makeIdentity();
			Imath::V3f n = m_normal.normalized();
			Imath::V3f up( 0.0f, 1.0f, 0.0f );
			if( std::abs( n.dot( up ) ) > 0.99f )
			{
				up = Imath::V3f( 1.0f, 0.0f, 0.0f );
			}
			Imath::V3f right = up.cross( n ).normalized();
			up = n.cross( right ).normalized();
			xform[0][0] = right.x;
			xform[0][1] = right.y;
			xform[0][2] = right.z;
			xform[1][0] = up.x;
			xform[1][1] = up.y;
			xform[1][2] = up.z;
			xform[2][0] = n.x;
			xform[2][1] = n.y;
			xform[2][2] = n.z;
			xform[3][0] = m_center.x;
			xform[3][1] = m_center.y;
			xform[3][2] = m_center.z;

			glPushAttrib( GL_ENABLE_BIT | GL_CURRENT_BIT );
			glDisable( GL_TEXTURE_2D );
			glDisable( GL_LIGHTING );
			glColor4f( 1.0f, 0.0f, 0.0f, 1.0f );

			glPushMatrix();
			glMultMatrixf( xform.getValue() );
			glBegin( GL_LINE_LOOP );
			for( int i = 0; i < 32; ++i )
			{
				const float angle = 2.0f * M_PI * float( i ) / 32.0f;
				glVertex3f( std::cos( angle ) * m_radius, std::sin( angle ) * m_radius, 0.0f );
			}
			glEnd();
			glPopMatrix();
			glPopAttrib();
		}

		unsigned layerMask() const override
		{
			return m_visible ? static_cast<unsigned>( Layer::MidFront ) : 0;
		}

	private :

		bool m_visible;
		Imath::V3f m_center;
		Imath::V3f m_normal;
		float m_radius;

};

class PointCountGadget : public GafferUI::Gadget
{

	public :

		PointCountGadget()
			: Gadget( "PointCount" )
		{
		}

		void setText( const std::string &text )
		{
			if( m_text == text )
			{
				return;
			}

			m_text = text;
			dirty( DirtyType::Render );
		}

		const std::string &text() const
		{
			return m_text;
		}

	protected :

		void renderLayer( Layer layer, const GafferUI::Style *style, RenderReason reason ) const override
		{
			if( layer != Layer::Front || isSelectionRender( reason ) || m_text.empty() )
			{
				return;
			}

			const GafferUI::ViewportGadget *viewport = ancestor<GafferUI::ViewportGadget>();
			if( !viewport )
			{
				return;
			}

			GafferUI::ViewportGadget::RasterScope rasterScope( viewport );
			const Imath::V2i viewportSize = viewport->getViewport();
			const float textScale = 12.0f;
			const Imath::Box3f textBound = style->textBound( GafferUI::Style::LabelText, m_text );
			const float textWidth = textBound.size().x * textScale;
			const float textHeight = textBound.size().y * textScale;
			Imath::M44f transform = getTransform();
			transform[3][0] = viewportSize.x - textWidth - 20.0f;
			transform[3][1] = viewportSize.y - textHeight - 20.0f;

			glPushAttrib( GL_ENABLE_BIT | GL_CURRENT_BIT );
			glDisable( GL_LIGHTING );
			glDisable( GL_DEPTH_TEST );
			glDisable( GL_TEXTURE_2D );

			glPushMatrix();
			glMultMatrixf( transform.getValue() );
			glScalef( textScale, -textScale, textScale );
			style->renderText( GafferUI::Style::LabelText, m_text );
			glPopMatrix();

			glPopAttrib();
		}

		unsigned layerMask() const override
		{
			return static_cast<unsigned>( Layer::Front );
		}

		Imath::Box3f renderBound() const override
		{
			Imath::Box3f b;
			b.makeInfinite();
			return b;
		}

	private :

		std::string m_text;

};

} // namespace PaintPointsToolPrivate

struct PaintPointsTool::TriangleCacheEntry
{
	IECoreScene::ConstMeshPrimitivePtr mesh;
	std::vector<Imath::V3f> positions;
	std::vector<std::array<int, 4>> triangles;

	TriangleCacheEntry() = default;
};

namespace PaintPointsToolPrivate
{

std::string sequenceTag( const char *tag );
void logInfo( const std::string &message );
void logWarning( const std::string &message );

Gaffer::ScriptNode *toolScriptNode( const PaintPointsTool *tool );

std::string trimmed( const std::string &value );
float sampleSpacing( const PaintPointsTool *tool );
bool isEraseMode( const PaintPointsTool *tool );
bool isSelectBrushMode( const PaintPointsTool *tool );
bool isSelectLassoMode( const PaintPointsTool *tool );
bool isRelaxMode( const PaintPointsTool *tool );
bool isReprojectMode( const PaintPointsTool *tool );
bool isLayerEditMode( const PaintPointsTool *tool );
bool isStrokeEditMode( const PaintPointsTool *tool );
std::string toolModeName( const PaintPointsTool *tool );
std::string relaxObjectiveName( int objective );
std::string layerEditActionName( int action );
std::string strokeEditActionName( int action );

std::optional<Imath::V2f> rasterPosition( const PaintPointsTool *tool, const IECore::LineSegment3f &eventLine );
bool marqueeHasArea( const std::optional<Imath::V2f> &startRaster, const std::optional<Imath::V2f> &endRaster );

Imath::V3f vectorFromObject( const bp::object &value, const Imath::V3f &fallback = Imath::V3f( 0.0f ) );
std::uint64_t uint64FromObject( const bp::object &value, std::uint64_t fallback = 0 );

bp::object pythonNode( Gaffer::Node *node );
bool refreshDiagnosticsEnabled();

float pressureScaledDensity( const PaintPointsTool *tool );
float pressureScaledSoftness( const PaintPointsTool *tool );
int effectivePointsPerDab( const PaintPointsTool *tool );
int normalizedFrameMode( int value );
std::string frameModeName( int value );
std::string eraseSpaceName( int value );
std::uint64_t selectedLayerId( Gaffer::Node *node, const bp::dict &currentSelection );
std::uint64_t selectedStrokeId( const bp::dict &currentSelection );

	Gaffer::Node *findPaintedPointsNode( PaintPointsTool *tool, Gaffer::ScriptNode *script );
Gaffer::Node *targetNodeOrCreate( PaintPointsTool *tool, Gaffer::ScriptNode *script );
void invalidateLayerStrokeCache( PaintPointsTool *tool );
std::pair<std::uint64_t, std::uint64_t> ensureLayerAndStroke( PaintPointsTool *tool, Gaffer::Node *node );

IECore::PathMatcher affectedOutputLocations( Gaffer::ScriptNode *script );
void logRefreshDiagnostics( PaintPointsTool *tool, Gaffer::ScriptNode *script, Gaffer::Node *paintedNode, const char *stage );
std::string scenePathString( const GafferScene::ScenePlug::ScenePath &path );
float vectorLengthSquared( const Imath::V3f &vector );
std::pair<Imath::V3f, Imath::V3f> closestPointOnTriangle(
	const Imath::V3f &point,
	const Imath::V3f &a,
	const Imath::V3f &b,
	const Imath::V3f &c
);
bool buildTriangleCacheEntry( IECore::ConstObjectPtr object, PaintPointsTool::TriangleCacheEntry &entry );
std::string sceneGadgetStateName( GafferSceneUI::SceneGadget::State state );

bp::dict selectionResultFromStroke( Gaffer::Node *node, const PaintPointsTool::StrokePoints &strokePoints, float radius );
bp::dict selectionResultFromLasso(
	Gaffer::Node *node,
	const GafferUI::ViewportGadget *viewport,
	const std::vector<Imath::V2f> &rasterPath,
	const std::optional<Imath::V2f> &startRaster,
	const std::optional<Imath::V2f> &endRaster
);
bp::dict selectionResultFromMarquee(
	Gaffer::Node *node,
	const GafferUI::ViewportGadget *viewport,
	const std::optional<Imath::V2f> &startRaster,
	const std::optional<Imath::V2f> &endRaster
);
bp::dict selectionResultFromHit( Gaffer::Node *node, const PaintPointsTool::HitRecord &hit, float radius );
bp::dict currentSelectionState( Gaffer::Node *node );
size_t applySelectionAction( const PaintPointsTool *tool, Gaffer::Node *node, const bp::dict &currentSelection );
size_t applyLayerEditSelection( const PaintPointsTool *tool, Gaffer::Node *node, const bp::dict &currentSelection );
std::string layerEditActionHelp( const PaintPointsTool *tool );
size_t applyStrokeEditSelection( const PaintPointsTool *tool, Gaffer::Node *node, const bp::dict &currentSelection );
std::string strokeEditActionHelp( const PaintPointsTool *tool );

bp::list demoPoints( const PaintPointsTool *tool );
BrushSampleData brushSampleDataFromHit( const PaintPointsTool *tool, const PaintPointsTool::HitRecord &hit, int seed );
BrushSampleDataList brushSampleDataFromStroke(
	const PaintPointsTool *tool,
	const PaintPointsTool::StrokePoints &strokePoints,
	double *buildMs = nullptr
);
BrushSampleDataList expandBrushSampleData(
	const PaintPointsTool *tool,
	const BrushSampleDataList &samples,
	double *jitterMs = nullptr
);
bp::list brushSamplesToPythonList( const BrushSampleDataList &samples, double *constructMs = nullptr );
bp::dict brushSampleFromHit( const PaintPointsTool *tool, const PaintPointsTool::HitRecord &hit, int seed );
bp::list brushSamplesFromStroke( const PaintPointsTool *tool, const PaintPointsTool::StrokePoints &strokePoints );
bp::list expandedPaintSamples(
	const PaintPointsTool *tool,
	const bp::list &samples,
	double *baseDecodeMs = nullptr,
	double *jitterMs = nullptr,
	double *constructMs = nullptr
);
size_t authoredPointCount( const bp::list &points );
int resolvedAuthoredPointCount( const bp::list &points );
PaintPointsTool::StrokePoints densifiedStroke( const PaintPointsTool::StrokePoints &strokePoints, float spacing );

template<typename PlugType>
PlugType *nodePlug( Gaffer::Node *node, const char *name )
{
	return node ? node->descendant<PlugType>( name ) : nullptr;
}

} // namespace PaintPointsToolPrivate

} // namespace GafferScatterPaintUI
