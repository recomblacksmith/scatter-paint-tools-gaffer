#pragma once

#include "GafferScatterPaintUI/Export.h"
#include "GafferScatterPaintUI/TypeIds.h"

#include "Gaffer/NumericPlug.h"
#include "Gaffer/Signals.h"
#include "Gaffer/StringPlug.h"
#include "Gaffer/TypedObjectPlug.h"
#include "GafferScene/ScenePlug.h"
#include "GafferSceneUI/SelectionTool.h"
#include "GafferUI/TextGadget.h"

#include "IECore/LineSegment.h"
#include "IECore/Object.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace GafferSceneUI
{

class SceneGadget;
class SceneView;

} // namespace GafferSceneUI

namespace Gaffer
{

class Node;

} // namespace Gaffer

namespace GafferUI
{

class ButtonEvent;
class DragDropEvent;
class Gadget;
class ViewportGadget;

} // namespace GafferUI

namespace GafferScatterPaintUI
{

class GAFFERSCATTERPAINTUI_API PaintPointsTool : public GafferSceneUI::SelectionTool
{

	public :

		GAFFER_NODE_DECLARE_TYPE( GafferScatterPaintUI::PaintPointsTool, PaintPointsToolTypeId, GafferSceneUI::SelectionTool );

		explicit PaintPointsTool( GafferSceneUI::SceneView *view, const std::string &name = defaultName<PaintPointsTool>() );
		~PaintPointsTool() override;

		Gaffer::IntPlug *modePlug();
		const Gaffer::IntPlug *modePlug() const;

		Gaffer::FloatPlug *brushSizePlug();
		const Gaffer::FloatPlug *brushSizePlug() const;

		Gaffer::IntPlug *pointsPlug();
		const Gaffer::IntPlug *pointsPlug() const;

		Gaffer::FloatPlug *densityPlug();
		const Gaffer::FloatPlug *densityPlug() const;

		Gaffer::FloatPlug *softnessPlug();
		const Gaffer::FloatPlug *softnessPlug() const;

		Gaffer::FloatPlug *spacingPlug();
		const Gaffer::FloatPlug *spacingPlug() const;

		Gaffer::IntPlug *rotationModePlug();
		const Gaffer::IntPlug *rotationModePlug() const;

		Gaffer::FloatPlug *scaleJitterPlug();
		const Gaffer::FloatPlug *scaleJitterPlug() const;

		Gaffer::FloatPlug *widthJitterPlug();
		const Gaffer::FloatPlug *widthJitterPlug() const;

		Gaffer::IntPlug *frameModePlug();
		const Gaffer::IntPlug *frameModePlug() const;

		Gaffer::IntPlug *frameStartPlug();
		const Gaffer::IntPlug *frameStartPlug() const;

		Gaffer::IntPlug *frameEndPlug();
		const Gaffer::IntPlug *frameEndPlug() const;

		Gaffer::BoolPlug *muteBehaviorPlug();
		const Gaffer::BoolPlug *muteBehaviorPlug() const;

		Gaffer::BoolPlug *soloBehaviorPlug();
		const Gaffer::BoolPlug *soloBehaviorPlug() const;

		Gaffer::IntPlug *relaxObjectivePlug();
		const Gaffer::IntPlug *relaxObjectivePlug() const;

		Gaffer::StringPlug *targetFilterPlug();
		const Gaffer::StringPlug *targetFilterPlug() const;

		Gaffer::StringPlug *targetSetFilterPlug();
		const Gaffer::StringPlug *targetSetFilterPlug() const;

		Gaffer::IntPlug *surfaceModePlug();
		const Gaffer::IntPlug *surfaceModePlug() const;

		Gaffer::IntPlug *paintThroughModePlug();
		const Gaffer::IntPlug *paintThroughModePlug() const;

		Gaffer::IntPlug *globalModePrecedencePlug();
		const Gaffer::IntPlug *globalModePrecedencePlug() const;

		Gaffer::BoolPlug *pressureDefaultsEnabledPlug();
		const Gaffer::BoolPlug *pressureDefaultsEnabledPlug() const;

		Gaffer::IntPlug *pressureDefaultsMappingModePlug();
		const Gaffer::IntPlug *pressureDefaultsMappingModePlug() const;

		Gaffer::ObjectPlug *pressureDefaultsDensityCurvePlug();
		const Gaffer::ObjectPlug *pressureDefaultsDensityCurvePlug() const;

		Gaffer::ObjectPlug *pressureDefaultsSoftnessCurvePlug();
		const Gaffer::ObjectPlug *pressureDefaultsSoftnessCurvePlug() const;

		Gaffer::IntPlug *eraseSpacePlug();
		const Gaffer::IntPlug *eraseSpacePlug() const;

		Gaffer::FloatPlug *pressureValuePlug();
		const Gaffer::FloatPlug *pressureValuePlug() const;

		Gaffer::StringPlug *targetNodePlug();
		const Gaffer::StringPlug *targetNodePlug() const;

		Gaffer::StringPlug *layerNamePlug();
		const Gaffer::StringPlug *layerNamePlug() const;

		Gaffer::StringPlug *strokeNamePlug();
		const Gaffer::StringPlug *strokeNamePlug() const;

		Gaffer::IntPlug *layerEditActionPlug();
		const Gaffer::IntPlug *layerEditActionPlug() const;

		Gaffer::BoolPlug *layerVisiblePlug();
		const Gaffer::BoolPlug *layerVisiblePlug() const;

		Gaffer::BoolPlug *layerMutePlug();
		const Gaffer::BoolPlug *layerMutePlug() const;

		Gaffer::BoolPlug *layerSoloPlug();
		const Gaffer::BoolPlug *layerSoloPlug() const;

		Gaffer::IntPlug *layerFrameStartPlug();
		const Gaffer::IntPlug *layerFrameStartPlug() const;

		Gaffer::IntPlug *layerFrameEndPlug();
		const Gaffer::IntPlug *layerFrameEndPlug() const;

		Gaffer::IntPlug *layerMoveToIndexPlug();
		const Gaffer::IntPlug *layerMoveToIndexPlug() const;

		Gaffer::IntPlug *layerModePlug();
		const Gaffer::IntPlug *layerModePlug() const;

		Gaffer::IntPlug *strokeEditActionPlug();
		const Gaffer::IntPlug *strokeEditActionPlug() const;

		Gaffer::IntPlug *strokeMoveToIndexPlug();
		const Gaffer::IntPlug *strokeMoveToIndexPlug() const;

		Gaffer::StringPlug *strokeMergeTargetPlug();
		const Gaffer::StringPlug *strokeMergeTargetPlug() const;

		Gaffer::IntPlug *previewCountPlug();
		const Gaffer::IntPlug *previewCountPlug() const;

		Gaffer::IntPlug *primedPointCountPlug();
		const Gaffer::IntPlug *primedPointCountPlug() const;

		Gaffer::StringPlug *statusPlug();

		const Gaffer::StringPlug *statusPlug() const;

		size_t commitDemoStroke();
		size_t eraseLastStroke();

	public :

		enum class SampleOrigin
		{
			RawHit = 0,
			DensifiedHit = 1,
		};

		struct HitRecord
		{
			std::string path;
			Imath::V3f point;
			Imath::V3f normal;
			int triangleIndex = 0;
			Imath::V3f barycentric = Imath::V3f( 1.0f, 0.0f, 0.0f );
			bool attachmentResolved = false;
			SampleOrigin sampleOrigin = SampleOrigin::RawHit;
		};

		using StrokePoints = std::vector<HitRecord>;

		struct TriangleCacheEntry;

	private :

		void plugDirtied( const Gaffer::Plug *plug );
		void contextChanged();
		bool isMirroredControlPlug( const Gaffer::Plug *plug ) const;
		void syncMirroredControlsFromNode( Gaffer::Node *node );
		void syncMirroredControlsToNode( Gaffer::Node *node, const Gaffer::Plug *sourcePlug = nullptr );
		void connectViewportSignals();
		void disconnectViewportSignals();
		GafferSceneUI::SceneGadget *sceneGadget();
		const GafferSceneUI::SceneGadget *sceneGadget() const;
		GafferUI::ViewportGadget *viewportGadget();
		const GafferUI::ViewportGadget *viewportGadget() const;
		std::optional<HitRecord> refineMeshHit( const GafferScene::ScenePlug::ScenePath &path, const Imath::V3f &worldHitPoint, const Imath::V3f &fallbackNormal ) const;
		std::optional<HitRecord> hitPoint( const IECore::LineSegment3f &eventLine ) const;
		bool appendDragHit( const HitRecord &hit );
		size_t eraseStrokePoints( const StrokePoints &strokePoints );
		size_t commitStrokePoints( const StrokePoints &strokePoints );
		size_t commitHitPoint( const HitRecord &hit );
		void requestViewportRender( const char *reason );
		void refreshSceneGadget( const std::string &reason );
		void invalidatePointCountOverlayCache();
		void refreshPointCountOverlay( Gaffer::Node *node = nullptr, std::optional<int> knownCount = std::nullopt );
		void clearDragState();
		int resolvedPointCount( const StrokePoints &strokePoints ) const;

		bool buttonPress( GafferUI::Gadget *gadget, const GafferUI::ButtonEvent &event );
		bool enter( GafferUI::Gadget *gadget, const GafferUI::ButtonEvent &event );
		bool leave( GafferUI::Gadget *gadget, const GafferUI::ButtonEvent &event );
		void updateBrushGadget( const IECore::LineSegment3f &line );
		bool mouseMove( GafferUI::Gadget *gadget, const GafferUI::ButtonEvent &event );
		IECore::RunTimeTypedPtr dragBegin( GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event );
		bool dragEnter( const GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event );
		bool dragMove( GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event );
		bool dragEnd( GafferUI::Gadget *gadget, const GafferUI::DragDropEvent &event );

		static size_t g_firstPlugIndex;
		Gaffer::Signals::ScopedConnection m_plugDirtiedConnection;
		Gaffer::Signals::ScopedConnection m_contextChangedConnection;
		Gaffer::Signals::ScopedConnection m_buttonPressConnection;
		GafferUI::GadgetPtr m_brushRingGadget;
		GafferUI::GadgetPtr m_pointCountGadget;
		Gaffer::Signals::ScopedConnection m_enterConnection;
		Gaffer::Signals::ScopedConnection m_leaveConnection;
		Gaffer::Signals::ScopedConnection m_dragBeginConnection;
		Gaffer::Signals::ScopedConnection m_mouseMoveConnection;
		Gaffer::Signals::ScopedConnection m_dragEnterConnection;
		Gaffer::Signals::ScopedConnection m_dragMoveConnection;
		Gaffer::Signals::ScopedConnection m_dragEndConnection;
		Gaffer::Signals::ScopedConnection m_targetNodePlugSetConnection;
		StrokePoints m_dragStroke;
		std::string m_dragStrokeSourcePath;
		std::vector<Imath::V2f> m_dragRasterPath;
		std::optional<Imath::V2f> m_dragStartRaster;
		std::optional<Imath::V2f> m_dragCurrentRaster;
		bool m_dragActive = false;
		bool m_syncingControlState = false;
		const Gaffer::Node *m_cachedPointCountNode = nullptr;
		std::optional<int> m_cachedPointCount;
		mutable std::unordered_map<const IECore::Object *, std::shared_ptr<TriangleCacheEntry>> m_triangleCache;

};

IE_CORE_DECLAREPTR( PaintPointsTool )

} // namespace GafferScatterPaintUI
