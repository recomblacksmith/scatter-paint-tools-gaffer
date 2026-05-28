#pragma once

#include "GafferScatterPaint/Export.h"
#include "GafferScatterPaint/TypeIds.h"

#include "Gaffer/NumericPlug.h"
#include "Gaffer/Plug.h"
#include "Gaffer/StringPlug.h"
#include "Gaffer/TypedObjectPlug.h"
#include "Gaffer/TypedPlug.h"
#include "GafferScene/SceneProcessor.h"

namespace GafferScatterPaint
{

class GAFFERSCATTERPAINT_API AttachedPoints : public GafferScene::SceneProcessor
{

	public :

		GAFFER_NODE_DECLARE_TYPE( GafferScatterPaint::AttachedPoints, AttachedPointsTypeId, GafferScene::SceneProcessor );

		explicit AttachedPoints( const std::string &name = defaultName<AttachedPoints>() );
		~AttachedPoints() override;

		GafferScene::ScenePlug *pointsPlug();
		const GafferScene::ScenePlug *pointsPlug() const;

		Gaffer::StringPlug *outputLocationPlug();
		const Gaffer::StringPlug *outputLocationPlug() const;

		Gaffer::StringPlug *pointTypePlug();
		const Gaffer::StringPlug *pointTypePlug() const;

		Gaffer::StringPlug *includeAttributesPlug();
		const Gaffer::StringPlug *includeAttributesPlug() const;

		Gaffer::IntPlug *exportPresetPlug();
		const Gaffer::IntPlug *exportPresetPlug() const;

		Gaffer::IntPlug *surfaceSolveModePlug();
		const Gaffer::IntPlug *surfaceSolveModePlug() const;

		Gaffer::BoolPlug *allowCrossMeshReprojectPlug();
		const Gaffer::BoolPlug *allowCrossMeshReprojectPlug() const;

		Gaffer::BoolPlug *keepLastValidOutputPlug();
		const Gaffer::BoolPlug *keepLastValidOutputPlug() const;

		Gaffer::BoolPlug *strictUnresolvedPlug();
		const Gaffer::BoolPlug *strictUnresolvedPlug() const;

		Gaffer::BoolPlug *debugColorPlug();
		const Gaffer::BoolPlug *debugColorPlug() const;

		Gaffer::IntPlug *cacheVersionPlug();
		const Gaffer::IntPlug *cacheVersionPlug() const;

		Gaffer::IntPlug *resolvedPointCountPlug();
		const Gaffer::IntPlug *resolvedPointCountPlug() const;

		Gaffer::IntPlug *unresolvedPointCountPlug();
		const Gaffer::IntPlug *unresolvedPointCountPlug() const;

		Gaffer::IntPlug *lastValidFramePlug();
		const Gaffer::IntPlug *lastValidFramePlug() const;

		Gaffer::IntPlug *invalidPointCountPlug();
		const Gaffer::IntPlug *invalidPointCountPlug() const;

		Gaffer::IntPlug *invalidStrokeCountPlug();
		const Gaffer::IntPlug *invalidStrokeCountPlug() const;

		Gaffer::IntPlug *failingFramePlug();
		const Gaffer::IntPlug *failingFramePlug() const;

		Gaffer::ObjectPlug *failingTargetPathsPlug();
		const Gaffer::ObjectPlug *failingTargetPathsPlug() const;

		Gaffer::ObjectPlug *attachmentFailureReasonsPlug();
		const Gaffer::ObjectPlug *attachmentFailureReasonsPlug() const;

		Gaffer::IntPlug *topologyMismatchCountPlug();
		const Gaffer::IntPlug *topologyMismatchCountPlug() const;

		Gaffer::StringPlug *solveStatusPlug();
		const Gaffer::StringPlug *solveStatusPlug() const;

		Gaffer::ObjectPlug *validationCategoriesPlug();
		const Gaffer::ObjectPlug *validationCategoriesPlug() const;

		void affects( const Gaffer::Plug *input, AffectedPlugsContainer &outputs ) const override;

	protected :

		void hash( const Gaffer::ValuePlug *output, const Gaffer::Context *context, IECore::MurmurHash &h ) const override;
		void compute( Gaffer::ValuePlug *output, const Gaffer::Context *context ) const override;

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

IE_CORE_DECLAREPTR( AttachedPoints )

} // namespace GafferScatterPaint
