#pragma once

#include "GafferScatterPlus/Export.h"
#include "GafferScatterPlus/TypeIds.h"

#include "Gaffer/CompoundNumericPlug.h"
#include "Gaffer/NumericPlug.h"
#include "Gaffer/StringPlug.h"
#include "Gaffer/TypedObjectPlug.h"
#include "GafferImage/ImagePlug.h"
#include "GafferScene/SceneProcessor.h"

namespace GafferScatterPlus
{

class GAFFERSCATTERPLUS_API ScatterPlus : public GafferScene::SceneProcessor
{
    public :

        GAFFER_NODE_DECLARE_TYPE( GafferScatterPlus::ScatterPlus, ScatterPlusTypeId, GafferScene::SceneProcessor );

        explicit ScatterPlus( const std::string &name = defaultName<ScatterPlus>() );
        ~ScatterPlus() override;

        GafferScene::ScenePlug *supportScenePlug();
        const GafferScene::ScenePlug *supportScenePlug() const;

        GafferScene::ScenePlug *prototypesPlug();
        const GafferScene::ScenePlug *prototypesPlug() const;

        GafferImage::ImagePlug *imagePlug();
        const GafferImage::ImagePlug *imagePlug() const;

        Gaffer::StringPlug *outputLocationPlug();
        const Gaffer::StringPlug *outputLocationPlug() const;

        Gaffer::StringPlug *pointTypePlug();
        const Gaffer::StringPlug *pointTypePlug() const;

        Gaffer::StringPlug *supportPlug();
        const Gaffer::StringPlug *supportPlug() const;

        Gaffer::StringPlug *densityPrimitiveVariablePlug();
        const Gaffer::StringPlug *densityPrimitiveVariablePlug() const;

        Gaffer::StringPlug *referencePositionPlug();
        const Gaffer::StringPlug *referencePositionPlug() const;

        Gaffer::StringPlug *uvPlug();
        const Gaffer::StringPlug *uvPlug() const;

        Gaffer::StringPlug *prototypeRootsPlug();
        const Gaffer::StringPlug *prototypeRootsPlug() const;

        Gaffer::StringVectorDataPlug *prototypeRootsListPlug();
        const Gaffer::StringVectorDataPlug *prototypeRootsListPlug() const;

        Gaffer::IntPlug *distributionPlug();
        const Gaffer::IntPlug *distributionPlug() const;

        Gaffer::IntPlug *prototypeModePlug();
        const Gaffer::IntPlug *prototypeModePlug() const;

        Gaffer::IntPlug *prototypeIndexPlug();
        const Gaffer::IntPlug *prototypeIndexPlug() const;

        Gaffer::StringPlug *idVariablePlug();
        const Gaffer::StringPlug *idVariablePlug() const;

        Gaffer::StringPlug *geometryIdAttributePlug();
        const Gaffer::StringPlug *geometryIdAttributePlug() const;

        Gaffer::StringPlug *probabilityAttributePlug();
        const Gaffer::StringPlug *probabilityAttributePlug() const;

        Gaffer::FloatPlug *densityPlug();
        const Gaffer::FloatPlug *densityPlug() const;

        Gaffer::IntPlug *pointCountPlug();
        const Gaffer::IntPlug *pointCountPlug() const;

        Gaffer::IntPlug *seedPlug();
        const Gaffer::IntPlug *seedPlug() const;

        Gaffer::IntPlug *varianceModePlug();
        const Gaffer::IntPlug *varianceModePlug() const;

        Gaffer::FloatPlug *jitteringPlug();
        const Gaffer::FloatPlug *jitteringPlug() const;

        Gaffer::V3fPlug *positionPlug();
        const Gaffer::V3fPlug *positionPlug() const;

        Gaffer::V3fPlug *orientationPlug();
        const Gaffer::V3fPlug *orientationPlug() const;

        Gaffer::V3fPlug *scalePlug();
        const Gaffer::V3fPlug *scalePlug() const;

        Gaffer::IntPlug *rotationOrderPlug();
        const Gaffer::IntPlug *rotationOrderPlug() const;

        Gaffer::BoolPlug *useSupportNormalsPlug();
        const Gaffer::BoolPlug *useSupportNormalsPlug() const;

        Gaffer::V3fPlug *positionVariancePlug();
        const Gaffer::V3fPlug *positionVariancePlug() const;

        Gaffer::V3fPlug *positionVarianceStepPlug();
        const Gaffer::V3fPlug *positionVarianceStepPlug() const;

        Gaffer::V3fPlug *rotationVariancePlug();
        const Gaffer::V3fPlug *rotationVariancePlug() const;

        Gaffer::V3fPlug *rotationVarianceStepPlug();
        const Gaffer::V3fPlug *rotationVarianceStepPlug() const;

        Gaffer::V3fPlug *scaleVariancePlug();
        const Gaffer::V3fPlug *scaleVariancePlug() const;

        Gaffer::V3fPlug *scaleVarianceStepPlug();
        const Gaffer::V3fPlug *scaleVarianceStepPlug() const;

        Gaffer::BoolPlug *uniformScaleVariancePlug();
        const Gaffer::BoolPlug *uniformScaleVariancePlug() const;

        Gaffer::FloatPlug *timeOffsetPlug();
        const Gaffer::FloatPlug *timeOffsetPlug() const;

        Gaffer::FloatPlug *timeVariancePlug();
        const Gaffer::FloatPlug *timeVariancePlug() const;

        Gaffer::IntPlug *timeVarianceSamplesPlug();
        const Gaffer::IntPlug *timeVarianceSamplesPlug() const;

        Gaffer::IntPlug *supportSpacePlug();
        const Gaffer::IntPlug *supportSpacePlug() const;

        Gaffer::FloatPlug *referenceFramePlug();
        const Gaffer::FloatPlug *referenceFramePlug() const;

        Gaffer::IntPlug *decimationSpacePlug();
        const Gaffer::IntPlug *decimationSpacePlug() const;

        Gaffer::IntPlug *collisionModePlug();
        const Gaffer::IntPlug *collisionModePlug() const;

        Gaffer::FloatPlug *collisionScaleMultiplierPlug();
        const Gaffer::FloatPlug *collisionScaleMultiplierPlug() const;

        Gaffer::IntPlug *collisionDetectionOrderPlug();
        const Gaffer::IntPlug *collisionDetectionOrderPlug() const;

        Gaffer::IntPlug *collisionSeedPlug();
        const Gaffer::IntPlug *collisionSeedPlug() const;

        Gaffer::IntPlug *decimationSeedPlug();
        const Gaffer::IntPlug *decimationSeedPlug() const;

        Gaffer::FloatPlug *decimateValuePlug();
        const Gaffer::FloatPlug *decimateValuePlug() const;

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

IE_CORE_DECLAREPTR( ScatterPlus )

} // namespace GafferScatterPlus
