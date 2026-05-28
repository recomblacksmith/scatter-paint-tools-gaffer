#pragma once

#include "GafferPointCloudPlus/Export.h"
#include "GafferPointCloudPlus/TypeIds.h"

#include "Gaffer/NumericPlug.h"
#include "Gaffer/StringPlug.h"
#include "GafferScene/SceneProcessor.h"

namespace GafferPointCloudPlus
{

class GAFFERPOINTCLOUDPLUS_API PointCloudPlus : public GafferScene::SceneProcessor
{

    public :

        GAFFER_NODE_DECLARE_TYPE( GafferPointCloudPlus::PointCloudPlus, PointCloudPlusTypeId, GafferScene::SceneProcessor );

        explicit PointCloudPlus( const std::string &name = defaultName<PointCloudPlus>() );
        ~PointCloudPlus() override;

        Gaffer::StringPlug *outputLocationPlug();
        const Gaffer::StringPlug *outputLocationPlug() const;

        Gaffer::StringPlug *pointTypePlug();
        const Gaffer::StringPlug *pointTypePlug() const;

        Gaffer::IntPlug *modePlug();
        const Gaffer::IntPlug *modePlug() const;

        Gaffer::StringPlug *filterPlug();
        const Gaffer::StringPlug *filterPlug() const;

        Gaffer::IntPlug *distributionPlug();
        const Gaffer::IntPlug *distributionPlug() const;

        Gaffer::BoolPlug *useDensityPlug();
        const Gaffer::BoolPlug *useDensityPlug() const;

        Gaffer::FloatPlug *densityPlug();
        const Gaffer::FloatPlug *densityPlug() const;

        Gaffer::IntPlug *pointCountPlug();
        const Gaffer::IntPlug *pointCountPlug() const;

        Gaffer::IntPlug *distributionSeedPlug();
        const Gaffer::IntPlug *distributionSeedPlug() const;

        Gaffer::FloatPlug *jitteringPlug();
        const Gaffer::FloatPlug *jitteringPlug() const;

        Gaffer::StringPlug *fileNamePlug();
        const Gaffer::StringPlug *fileNamePlug() const;

        Gaffer::StringPlug *primPathPlug();
        const Gaffer::StringPlug *primPathPlug() const;

        Gaffer::StringPlug *purposePlug();
        const Gaffer::StringPlug *purposePlug() const;

        Gaffer::FloatPlug *framePlug();
        const Gaffer::FloatPlug *framePlug() const;

        Gaffer::FloatPlug *frameOffsetPlug();
        const Gaffer::FloatPlug *frameOffsetPlug() const;

        Gaffer::IntPlug *animationBehaviorPlug();
        const Gaffer::IntPlug *animationBehaviorPlug() const;

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

IE_CORE_DECLAREPTR( PointCloudPlus )

} // namespace GafferPointCloudPlus
