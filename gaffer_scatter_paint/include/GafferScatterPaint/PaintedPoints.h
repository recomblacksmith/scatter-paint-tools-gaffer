#pragma once

#include "GafferScatterPaint/BrushSampleData.h"
#include "GafferScatterPaint/CacheFormat.h"
#include "GafferScatterPaint/Export.h"
#include "GafferScatterPaint/TypeIds.h"

#include "Gaffer/Plug.h"
#include "Gaffer/NumericPlug.h"
#include "Gaffer/StringPlug.h"
#include "Gaffer/TypedObjectPlug.h"
#include "Gaffer/TypedPlug.h"
#include "GafferScene/SceneProcessor.h"

#include "boost/python/object.hpp"

#include <cstdint>

namespace GafferScatterPaint
{

class GAFFERSCATTERPAINT_API PaintedPoints : public GafferScene::SceneProcessor
{

	public :

		GAFFER_NODE_DECLARE_TYPE( GafferScatterPaint::PaintedPoints, PaintedPointsTypeId, GafferScene::SceneProcessor );

		explicit PaintedPoints( const std::string &name = defaultName<PaintedPoints>() );
		~PaintedPoints() override;

		std::uint64_t createLayer( const std::string &name = "" );
		std::uint64_t ensureLayer( const std::string &name );
		std::uint64_t deleteLayer( const boost::python::object &layerIdentifier );
		std::uint64_t renameLayer( const boost::python::object &layerIdentifier, const std::string &newName );
		std::uint64_t moveLayer( const boost::python::object &layerIdentifier, int newIndex );
		std::uint64_t mergeLayers( const boost::python::object &sourceLayerIdentifier, const boost::python::object &destinationLayerIdentifier );
		std::uint64_t setLayerVisible( const boost::python::object &layerIdentifier, bool visible );
		std::uint64_t setLayerMute( const boost::python::object &layerIdentifier, bool mute );
		std::uint64_t setLayerSolo( const boost::python::object &layerIdentifier, bool solo );
		std::uint64_t setLayerTimeRange( const boost::python::object &layerIdentifier, int frameStart, int frameEnd );
		std::uint64_t createStroke( const boost::python::object &layerIdentifier, const std::string &name = "" );
		std::uint64_t ensureStroke( const boost::python::object &layerIdentifier, const std::string &name );
		boost::python::object ensureLayerAndStroke( const std::string &layerName = "", const std::string &strokeName = "" );
		std::uint64_t deleteStroke( const boost::python::object &strokeIdentifier );
		std::uint64_t renameStroke( const boost::python::object &strokeIdentifier, const std::string &newName );
		std::uint64_t moveStroke( const boost::python::object &strokeIdentifier, int newIndex );
		std::uint64_t mergeStrokes( const boost::python::object &sourceStrokeIdentifier, const boost::python::object &destinationStrokeIdentifier );
		size_t paintStrokeCommit( const boost::python::object &strokeIdentifier, const boost::python::object &points, bool append = true );
		boost::python::object brushPaintCommit( const boost::python::object &strokeIdentifier, const boost::python::object &samples, bool append = true );
		boost::python::object brushPaintCommitNative( const boost::python::object &strokeIdentifier, const boost::python::object &samples, bool append = true );
		boost::python::object brushPaintCommitTyped( const boost::python::object &strokeIdentifier, const BrushSampleDataList &samples, bool append = true );
		size_t eraseCommit( const boost::python::object &strokeIdentifier, const boost::python::object &pointIds = boost::python::object(), const boost::python::object &fraction = boost::python::object() );
		boost::python::object brushErasePoints( const boost::python::object &samples, float radius, int eraseSpace = 0 );
		boost::python::object lastStrokeId() const;
		size_t mutatePoints( const boost::python::object &mutator );
		size_t relaxSelection();
		size_t reprojectSelection();
		std::string splitStrokeBySelection();
		std::uint64_t createSelectionSet( const std::string &name = "" );
		std::uint64_t renameSelectionSet( const boost::python::object &selectionIdentifier, const std::string &newName );
		std::uint64_t deleteSelectionSet( const boost::python::object &selectionIdentifier );
		boost::python::object storeCurrentSelection( const boost::python::object &selectionIdentifier = boost::python::object(), const boost::python::object &name = boost::python::object() );
		boost::python::object setCurrentSelection( const boost::python::object &pointIds = boost::python::object(), const boost::python::object &strokeIds = boost::python::object() );
		CacheSchema visibleSchema() const;
		boost::python::object cacheSnapshot() const;
		boost::python::object layerRecords() const;
		boost::python::object strokeRecords() const;
		boost::python::object pointRecords() const;
		std::uint64_t interactiveRevision() const;
		boost::python::object mutateCacheStore( const boost::python::object &mutator );
		boost::python::object seedBenchmarkStroke( std::uint64_t pointCount, const std::string &layerName = "", const std::string &strokeName = "" );
		std::string validateCache();
		std::string validateAttachments();
		std::uint32_t migrateCacheMode();
		std::string relinkCache();
		boost::python::object upgradeCache();
		std::string exportAuthoredCache();
		std::string exportEvaluatedPoints();
		std::string exportGafferScene();
		std::string exportUSD();
		std::string exportAlembic();
		std::string exportInterchange();
		std::string exportDiagnostics();
		std::string freezeBakeSelection( const boost::python::object &startFrame = boost::python::object(), const boost::python::object &endFrame = boost::python::object() );
		std::string freezeBakeToStaticNode( const boost::python::object &startFrame = boost::python::object(), const boost::python::object &endFrame = boost::python::object() );
		std::string freezeBakeEvaluatedToStaticNode( const boost::python::object &startFrame = boost::python::object(), const boost::python::object &endFrame = boost::python::object() );
		size_t compactCache();

		Gaffer::StringPlug *targetFilterPlug();
		const Gaffer::StringPlug *targetFilterPlug() const;

		Gaffer::StringPlug *targetSetFilterPlug();
		const Gaffer::StringPlug *targetSetFilterPlug() const;

		Gaffer::IntPlug *surfaceModePlug();
		const Gaffer::IntPlug *surfaceModePlug() const;

		Gaffer::IntPlug *paintThroughModePlug();
		const Gaffer::IntPlug *paintThroughModePlug() const;

		Gaffer::IntPlug *relaxObjectivePlug();
		const Gaffer::IntPlug *relaxObjectivePlug() const;

		Gaffer::IntPlug *cacheModePlug();
		const Gaffer::IntPlug *cacheModePlug() const;

		Gaffer::StringPlug *cachePathPlug();
		const Gaffer::StringPlug *cachePathPlug() const;

		Gaffer::IntPlug *cachePathModePlug();
		const Gaffer::IntPlug *cachePathModePlug() const;

		Gaffer::StringPlug *projectRootPlug();
		const Gaffer::StringPlug *projectRootPlug() const;

		Gaffer::IntPlug *lockModePlug();
		const Gaffer::IntPlug *lockModePlug() const;

		Gaffer::BoolPlug *backupEnabledPlug();
		const Gaffer::BoolPlug *backupEnabledPlug() const;

		Gaffer::IntPlug *backupPolicyPlug();
		const Gaffer::IntPlug *backupPolicyPlug() const;

		Gaffer::IntPlug *compactionModePlug();
		const Gaffer::IntPlug *compactionModePlug() const;

		Gaffer::IntPlug *globalModePrecedencePlug();
		const Gaffer::IntPlug *globalModePrecedencePlug() const;

		Gaffer::Color3fPlug *defaultColorPlug();
		const Gaffer::Color3fPlug *defaultColorPlug() const;

		Gaffer::Plug *pressureDefaultsPlug();
		const Gaffer::Plug *pressureDefaultsPlug() const;

		Gaffer::BoolPlug *pressureDefaultsEnabledPlug();
		const Gaffer::BoolPlug *pressureDefaultsEnabledPlug() const;

		Gaffer::IntPlug *pressureDefaultsMappingModePlug();
		const Gaffer::IntPlug *pressureDefaultsMappingModePlug() const;

		Gaffer::Plug *brushDefaultsPlug();
		const Gaffer::Plug *brushDefaultsPlug() const;

		Gaffer::FloatPlug *brushDefaultsSizePlug();
		const Gaffer::FloatPlug *brushDefaultsSizePlug() const;

		Gaffer::FloatPlug *brushDefaultsDensityPlug();
		const Gaffer::FloatPlug *brushDefaultsDensityPlug() const;

		Gaffer::FloatPlug *brushDefaultsSoftnessPlug();
		const Gaffer::FloatPlug *brushDefaultsSoftnessPlug() const;

		Gaffer::FloatPlug *brushDefaultsSpacingPlug();
		const Gaffer::FloatPlug *brushDefaultsSpacingPlug() const;

		Gaffer::IntPlug *brushDefaultsPointsPlug();
		const Gaffer::IntPlug *brushDefaultsPointsPlug() const;

		Gaffer::IntPlug *brushDefaultsRotationModePlug();
		const Gaffer::IntPlug *brushDefaultsRotationModePlug() const;

		Gaffer::FloatPlug *brushDefaultsScaleJitterPlug();
		const Gaffer::FloatPlug *brushDefaultsScaleJitterPlug() const;

		Gaffer::FloatPlug *brushDefaultsWidthJitterPlug();
		const Gaffer::FloatPlug *brushDefaultsWidthJitterPlug() const;

		Gaffer::ObjectPlug *layersPlug();
		const Gaffer::ObjectPlug *layersPlug() const;

		Gaffer::ObjectPlug *selectionSetsPlug();
		const Gaffer::ObjectPlug *selectionSetsPlug() const;

		Gaffer::ObjectPlug *cacheBlobPlug();
		const Gaffer::ObjectPlug *cacheBlobPlug() const;

		Gaffer::ObjectPlug *pendingPaintBlobPlug();
		const Gaffer::ObjectPlug *pendingPaintBlobPlug() const;

		Gaffer::IntPlug *interactiveRevisionPlug();
		const Gaffer::IntPlug *interactiveRevisionPlug() const;

		Gaffer::IntPlug *authoredPointCountPlug();
		const Gaffer::IntPlug *authoredPointCountPlug() const;

		Gaffer::IntPlug *invalidPointCountPlug();
		const Gaffer::IntPlug *invalidPointCountPlug() const;

		Gaffer::IntPlug *invalidStrokeCountPlug();
		const Gaffer::IntPlug *invalidStrokeCountPlug() const;

		Gaffer::IntPlug *failingFramePlug();
		const Gaffer::IntPlug *failingFramePlug() const;

		Gaffer::ObjectPlug *failingTargetPathsPlug();
		const Gaffer::ObjectPlug *failingTargetPathsPlug() const;

		Gaffer::StringPlug *lastErrorMessagePlug();
		const Gaffer::StringPlug *lastErrorMessagePlug() const;

		Gaffer::IntPlug *topologyMismatchCountPlug();
		const Gaffer::IntPlug *topologyMismatchCountPlug() const;

		Gaffer::StringPlug *validationSummaryPlug();
		const Gaffer::StringPlug *validationSummaryPlug() const;

		Gaffer::ObjectPlug *validationCategoriesPlug();
		const Gaffer::ObjectPlug *validationCategoriesPlug() const;

		Gaffer::StringPlug *cacheResolvedPathPlug();
		const Gaffer::StringPlug *cacheResolvedPathPlug() const;

		Gaffer::IntPlug *cacheVersionPlug();
		const Gaffer::IntPlug *cacheVersionPlug() const;

		Gaffer::StringPlug *cacheLockedByPlug();
		const Gaffer::StringPlug *cacheLockedByPlug() const;

		Gaffer::StringPlug *cacheLockedHostPlug();
		const Gaffer::StringPlug *cacheLockedHostPlug() const;

		Gaffer::StringPlug *cacheLockedTimePlug();
		const Gaffer::StringPlug *cacheLockedTimePlug() const;

		Gaffer::StringPlug *cacheLockedScriptPlug();
		const Gaffer::StringPlug *cacheLockedScriptPlug() const;

		void affects( const Gaffer::Plug *input, AffectedPlugsContainer &outputs ) const override;

	protected :

		void hashBound( const ScenePath &path, const Gaffer::Context *context, const GafferScene::ScenePlug *parent, IECore::MurmurHash &h ) const override;
		void hashTransform( const ScenePath &path, const Gaffer::Context *context, const GafferScene::ScenePlug *parent, IECore::MurmurHash &h ) const override;
		void hashAttributes( const ScenePath &path, const Gaffer::Context *context, const GafferScene::ScenePlug *parent, IECore::MurmurHash &h ) const override;
		void hashObject( const ScenePath &path, const Gaffer::Context *context, const GafferScene::ScenePlug *parent, IECore::MurmurHash &h ) const override;
		void hashChildNames( const ScenePath &path, const Gaffer::Context *context, const GafferScene::ScenePlug *parent, IECore::MurmurHash &h ) const override;
		void hashGlobals( const Gaffer::Context *context, const GafferScene::ScenePlug *parent, IECore::MurmurHash &h ) const override;
		void hashSetNames( const Gaffer::Context *context, const GafferScene::ScenePlug *parent, IECore::MurmurHash &h ) const override;
		void hashSet( const IECore::InternedString &setName, const Gaffer::Context *context, const GafferScene::ScenePlug *parent, IECore::MurmurHash &h ) const override;

		Imath::Box3f computeBound( const ScenePath &path, const Gaffer::Context *context, const GafferScene::ScenePlug *parent ) const override;
		Imath::M44f computeTransform( const ScenePath &path, const Gaffer::Context *context, const GafferScene::ScenePlug *parent ) const override;
		IECore::ConstCompoundObjectPtr computeAttributes( const ScenePath &path, const Gaffer::Context *context, const GafferScene::ScenePlug *parent ) const override;
		IECore::ConstObjectPtr computeObject( const ScenePath &path, const Gaffer::Context *context, const GafferScene::ScenePlug *parent ) const override;
		IECore::ConstInternedStringVectorDataPtr computeChildNames( const ScenePath &path, const Gaffer::Context *context, const GafferScene::ScenePlug *parent ) const override;
		IECore::ConstCompoundObjectPtr computeGlobals( const Gaffer::Context *context, const GafferScene::ScenePlug *parent ) const override;
		IECore::ConstInternedStringVectorDataPtr computeSetNames( const Gaffer::Context *context, const GafferScene::ScenePlug *parent ) const override;
		IECore::ConstPathMatcherDataPtr computeSet( const IECore::InternedString &setName, const Gaffer::Context *context, const GafferScene::ScenePlug *parent ) const override;

	private :

		static size_t g_firstPlugIndex;

};

IE_CORE_DECLAREPTR( PaintedPoints )

} // namespace GafferScatterPaint
