#include "GafferScatterPlus/ScatterPlus.h"

#include "Gaffer/ArrayPlug.h"
#include "Gaffer/Context.h"

#include "GafferImage/ImagePlug.h"
#include "GafferImage/Sampler.h"

#include "IECore/CompoundObject.h"
#include "IECore/MurmurHash.h"
#include "IECore/NullObject.h"
#include "IECore/PathMatcherData.h"
#include "IECore/SimpleTypedData.h"
#include "IECore/StringAlgo.h"
#include "IECore/VectorTypedData.h"

#include "IECoreScene/MeshPrimitive.h"
#include "IECoreScene/PointsPrimitive.h"
#include "IECoreScene/PrimitiveVariable.h"

#include "Imath/ImathBox.h"
#include "Imath/ImathEuler.h"
#include "Imath/ImathMatrix.h"
#include "Imath/ImathMatrixAlgo.h"
#include "Imath/ImathVec.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <random>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace Gaffer;
using namespace GafferImage;
using namespace GafferScatterPlus;
using namespace GafferScene;
using namespace IECore;
using namespace IECoreScene;

namespace
{

enum Distribution
{
    RandomDistribution = 0,
    PrimitiveCenterDistribution = 1,
    ImageDistribution = 2,
};

enum PrototypeMode
{
    FirstPrototypeMode = 0,
    RandomPrototypeMode = 1,
    CyclePrototypeMode = 2,
    IndexPrototypeMode = 3,
    IdPrimitiveVariablePrototypeMode = 4,
    LuminancePrototypeMode = 5,
};

enum VarianceMode
{
    ContinuousVarianceMode = 0,
    QuantizedVarianceMode = 1,
};

enum RotationOrder
{
    XYZRotationOrder = 0,
    XZYRotationOrder = 1,
    YXZRotationOrder = 2,
    YZXRotationOrder = 3,
    ZXYRotationOrder = 4,
    ZYXRotationOrder = 5,
};

enum SpaceMode
{
    ObjectSpaceMode = 0,
    WorldSpaceMode = 1,
    ReferenceSpaceMode = 2,
};

enum CollisionMode
{
    CollisionOffMode = 0,
    CollisionBoundsMode = 1,
    CollisionEllipsoidMode = 2,
};

enum CollisionOrder
{
    CollisionOrderGenerated = 0,
    CollisionOrderRandom = 1,
};

struct SampledPoint
{
    Imath::V3f localPosition = Imath::V3f( 0.0f );
    Imath::V3f position = Imath::V3f( 0.0f );
    Imath::V3f referencePosition = Imath::V3f( 0.0f );
    Imath::V3f normal = Imath::V3f( 0.0f, 1.0f, 0.0f );
    Imath::V2f uv = Imath::V2f( 0.5f, 0.5f );
    std::string sourcePath;
    int seed = 0;
    int id = 0;
    int selectionId = 0;
    bool hasSelectionId = false;
    float width = 0.1f;
    float density = 1.0f;
    float imageValue = 1.0f;
    float probability = 1.0f;
    float timeOffset = 0.0f;
};

struct PrototypeRecord
{
    ScenePlug::ScenePath path;
    std::string pathString;
    ConstObjectPtr object;
    ConstCompoundObjectPtr attributes;
    Imath::Box3f bound;
    int geometryId = -1;
    float probability = 1.0f;
};

struct InstanceRecord
{
    std::string name;
    size_t prototypeIndex = 0;
    int seed = 0;
    int id = 0;
    float timeOffset = 0.0f;
    SampledPoint sample;
    Imath::V3f scatterPosition = Imath::V3f( 0.0f );
    Imath::V3f scatterRotation = Imath::V3f( 0.0f );
    Imath::V3f scatterScale = Imath::V3f( 1.0f );
    Imath::Quatf orientation = Imath::Quatf();
    Imath::M44f transform = Imath::M44f();
    Imath::Box3f bound;
    Imath::M44f evaluationTransform = Imath::M44f();
    Imath::Box3f evaluationBound;
    Imath::V3f evaluationCenter = Imath::V3f( 0.0f );
    Imath::V3f evaluationExtents = Imath::V3f( 0.0f );
    Imath::V3f evaluationAxes[3] = {
        Imath::V3f( 1.0f, 0.0f, 0.0f ),
        Imath::V3f( 0.0f, 1.0f, 0.0f ),
        Imath::V3f( 0.0f, 0.0f, 1.0f )
    };
};

struct GeneratedState
{
    ConstPointsPrimitivePtr helperPoints;
    std::vector<PrototypeRecord> prototypes;
    std::vector<InstanceRecord> instances;
    std::vector<InternedString> instanceNames;
    Imath::Box3f instancesBound;
    Imath::Box3f outputBound;
};

ScenePlug::ScenePath rootPath()
{
    return ScenePlug::ScenePath();
}

ScenePlug::ScenePath outputRootPath( const ScatterPlus *node )
{
    return ScenePlug::stringToPath(
        node->outputLocationPlug()->getValue().empty() ? "/scatterPlus" : node->outputLocationPlug()->getValue()
    );
}

ScenePlug::ScenePath pointsPath( const ScatterPlus *node )
{
    ScenePlug::ScenePath result = outputRootPath( node );
    result.push_back( InternedString( "points" ) );
    return result;
}

ScenePlug::ScenePath instancesPath( const ScatterPlus *node )
{
    ScenePlug::ScenePath result = outputRootPath( node );
    result.push_back( InternedString( "instances" ) );
    return result;
}

bool isPrefixPath( const ScenePlug::ScenePath &prefix, const ScenePlug::ScenePath &path )
{
    if( prefix.size() > path.size() )
    {
        return false;
    }
    for( size_t i = 0; i < prefix.size(); ++i )
    {
        if( prefix[i] != path[i] )
        {
            return false;
        }
    }
    return true;
}

bool isInputPath( const ScenePlug *scene, const ScenePlug::ScenePath &path )
{
    return scene->exists( path );
}

bool isWithinGeneratedTree( const ScatterPlus *node, const ScenePlug::ScenePath &path )
{
    return isPrefixPath( outputRootPath( node ), path );
}

bool isGeneratedAncestor( const ScatterPlus *node, const ScenePlug::ScenePath &path )
{
    const ScenePlug::ScenePath outputRoot = outputRootPath( node );
    return isPrefixPath( path, outputRoot ) && path.size() < outputRoot.size();
}

bool isSyntheticBranch( const ScenePlug *scene, const ScatterPlus *node, const ScenePlug::ScenePath &path )
{
    return isWithinGeneratedTree( node, path ) && !isInputPath( scene, path );
}

bool isOutputRoot( const ScatterPlus *node, const ScenePlug::ScenePath &path )
{
    return path == outputRootPath( node );
}

bool isPointsLeaf( const ScatterPlus *node, const ScenePlug::ScenePath &path )
{
    return path == pointsPath( node );
}

bool isInstancesRoot( const ScatterPlus *node, const ScenePlug::ScenePath &path )
{
    return path == instancesPath( node );
}

bool isInstanceLeaf( const ScatterPlus *node, const ScenePlug::ScenePath &path )
{
    const ScenePlug::ScenePath instancesRoot = instancesPath( node );
    return path.size() == instancesRoot.size() + 1 && isPrefixPath( instancesRoot, path );
}

ConstInternedStringVectorDataPtr inputChildNamesForPath( const ScenePlug *scene, const ScenePlug::ScenePath &path )
{
    if( !scene->exists( path ) )
    {
        return new InternedStringVectorData();
    }
    return scene->childNames( path );
}

std::string trimmed( const std::string &value )
{
    const std::string whitespace = " \t\n\r";
    const size_t begin = value.find_first_not_of( whitespace );
    if( begin == std::string::npos )
    {
        return "";
    }
    const size_t end = value.find_last_not_of( whitespace );
    return value.substr( begin, end - begin + 1 );
}

std::vector<std::string> splitTokens( const std::string &value )
{
    std::vector<std::string> tokens;
    StringAlgo::tokenize( value, ' ', tokens );
    std::vector<std::string> output;
    for( const std::string &token : tokens )
    {
        const std::string clean = trimmed( token );
        if( !clean.empty() )
        {
            output.push_back( clean );
        }
    }
    return output;
}

std::vector<std::string> splitLocationComponents( const std::string &location )
{
    std::vector<std::string> result;
    std::string current;
    for( const char c : location )
    {
        if( c == '/' )
        {
            if( !current.empty() )
            {
                result.push_back( current );
                current.clear();
            }
            continue;
        }
        current += c;
    }
    if( !current.empty() )
    {
        result.push_back( current );
    }
    return result;
}

bool pathMatchesFilter( const std::string &path, const std::string &filter )
{
    const std::vector<std::string> tokens = splitTokens( filter );
    if( tokens.empty() )
    {
        return true;
    }

    bool included = false;
    bool hasInclude = false;
    for( const std::string &token : tokens )
    {
        const bool exclude = !token.empty() && token[0] == '-';
        const std::string pattern = exclude ? token.substr( 1 ) : token;
        if( pattern.empty() )
        {
            continue;
        }
        const bool match = StringAlgo::match( path, pattern );
        if( exclude && match )
        {
            return false;
        }
        if( !exclude )
        {
            hasInclude = true;
            if( match )
            {
                included = true;
            }
        }
    }
    return hasInclude ? included : true;
}

bool pathMatchesPrototypeFilters(
    const std::string &path,
    const std::string &filter,
    const IECore::StringVectorData *listData
)
{
    if( listData )
    {
        const std::vector<std::string> &values = listData->readable();
        if( !values.empty() )
        {
            for( const std::string &value : values )
            {
                if( pathMatchesFilter( path, value ) )
                {
                    return true;
                }
            }
            return false;
        }
    }
    return pathMatchesFilter( path, filter );
}

void collectCandidateMeshes(
    const ScenePlug *scene,
    const ScenePlug::ScenePath &path,
    const std::string &filter,
    std::vector<ScenePlug::ScenePath> &paths
)
{
    const std::string pathString = ScenePlug::pathToString( path );
    if( !path.empty() && pathMatchesFilter( pathString, filter ) )
    {
        ConstObjectPtr object = scene->object( path );
        if( runTimeCast<const MeshPrimitive>( object.get() ) )
        {
            paths.push_back( path );
        }
    }

    ConstInternedStringVectorDataPtr childNames = scene->childNames( path );
    for( const InternedString &child : childNames->readable() )
    {
        ScenePlug::ScenePath childPath = path;
        childPath.push_back( child );
        collectCandidateMeshes( scene, childPath, filter, paths );
    }
}

void collectPrototypePaths(
    const ScenePlug *scene,
    const ScenePlug::ScenePath &path,
    const std::string &filter,
    const IECore::StringVectorData *listData,
    std::vector<ScenePlug::ScenePath> &paths
)
{
    const std::string pathString = ScenePlug::pathToString( path );
    if( !path.empty() && pathMatchesPrototypeFilters( pathString, filter, listData ) )
    {
        ConstObjectPtr object = scene->object( path );
        if( object && !runTimeCast<const NullObject>( object.get() ) )
        {
            paths.push_back( path );
        }
    }

    ConstInternedStringVectorDataPtr childNames = scene->childNames( path );
    for( const InternedString &child : childNames->readable() )
    {
        ScenePlug::ScenePath childPath = path;
        childPath.push_back( child );
        collectPrototypePaths( scene, childPath, filter, listData, paths );
    }
}

void hashSceneBranch( const ScenePlug *scene, const ScenePlug::ScenePath &path, MurmurHash &h )
{
    h.append( ScenePlug::pathToString( path ) );
    h.append( scene->childNamesHash( path ) );
    h.append( scene->boundHash( path ) );
    h.append( scene->transformHash( path ) );
    h.append( scene->attributesHash( path ) );
    h.append( scene->objectHash( path ) );

    ConstInternedStringVectorDataPtr childNames = scene->childNames( path );
    for( const InternedString &child : childNames->readable() )
    {
        ScenePlug::ScenePath childPath = path;
        childPath.push_back( child );
        hashSceneBranch( scene, childPath, h );
    }
}

V3fVectorDataPtr v3fVectorData( const std::vector<Imath::V3f> &values, GeometricData::Interpretation interpretation )
{
    V3fVectorDataPtr result = new V3fVectorData();
    result->writable() = values;
    result->setInterpretation( interpretation );
    return result;
}

V2fVectorDataPtr v2fVectorData( const std::vector<Imath::V2f> &values, GeometricData::Interpretation interpretation )
{
    V2fVectorDataPtr result = new V2fVectorData();
    result->writable() = values;
    result->setInterpretation( interpretation );
    return result;
}

FloatVectorDataPtr floatVectorData( const std::vector<float> &values )
{
    FloatVectorDataPtr result = new FloatVectorData();
    result->writable() = values;
    return result;
}

Int64VectorDataPtr int64VectorData( const std::vector<int64_t> &values )
{
    Int64VectorDataPtr result = new Int64VectorData();
    result->writable() = values;
    return result;
}

IntVectorDataPtr intVectorData( const std::vector<int> &values )
{
    IntVectorDataPtr result = new IntVectorData();
    result->writable() = values;
    return result;
}

Color3fVectorDataPtr color3fVectorData( const std::vector<Imath::Color3f> &values )
{
    Color3fVectorDataPtr result = new Color3fVectorData();
    result->writable() = values;
    return result;
}

QuatfVectorDataPtr quatfVectorData( const std::vector<Imath::Quatf> &values )
{
    QuatfVectorDataPtr result = new QuatfVectorData();
    result->writable() = values;
    return result;
}

StringVectorDataPtr stringVectorData( const std::vector<std::string> &values )
{
    StringVectorDataPtr result = new StringVectorData();
    result->writable() = values;
    return result;
}

Imath::V3f cross( const Imath::V3f &a, const Imath::V3f &b )
{
    return Imath::V3f(
        ( a.y * b.z ) - ( a.z * b.y ),
        ( a.z * b.x ) - ( a.x * b.z ),
        ( a.x * b.y ) - ( a.y * b.x )
    );
}

Imath::V3f normalized( const Imath::V3f &value, const Imath::V3f &fallback )
{
    const float lengthSquared = value.dot( value );
    if( lengthSquared <= 0.0f )
    {
        return fallback;
    }
    return value / std::sqrt( lengthSquared );
}

Imath::V3f trianglePoint( const Imath::V3f &a, const Imath::V3f &b, const Imath::V3f &c, const Imath::V3f &barycentric )
{
    return ( a * barycentric.x ) + ( b * barycentric.y ) + ( c * barycentric.z );
}

Imath::V2f interpolateUv(
    const PrimitiveVariable &uvVariable,
    const std::vector<int> &ids,
    size_t faceVertexOffset,
    const Imath::V3f &barycentric
)
{
    if( const V2fVectorData *uvData = runTimeCast<const V2fVectorData>( uvVariable.data.get() ) )
    {
        const std::vector<Imath::V2f> &values = uvData->readable();
        auto sample = [&]( size_t corner ) -> Imath::V2f {
            const size_t faceIndex = faceVertexOffset + corner;
            size_t dataIndex = 0;
            if( uvVariable.interpolation == PrimitiveVariable::FaceVarying )
            {
                dataIndex = faceIndex;
            }
            else
            {
                dataIndex = static_cast<size_t>( ids[faceIndex] );
            }
            if( dataIndex >= values.size() )
            {
                return Imath::V2f( 0.5f, 0.5f );
            }
            return values[dataIndex];
        };
        const Imath::V2f a = sample( 0 );
        const Imath::V2f b = sample( 1 );
        const Imath::V2f c = sample( 2 );
        return ( a * barycentric.x ) + ( b * barycentric.y ) + ( c * barycentric.z );
    }
    if( const V3fVectorData *uvData = runTimeCast<const V3fVectorData>( uvVariable.data.get() ) )
    {
        const std::vector<Imath::V3f> &values = uvData->readable();
        auto sample = [&]( size_t corner ) -> Imath::V2f {
            const size_t faceIndex = faceVertexOffset + corner;
            size_t dataIndex = 0;
            if( uvVariable.interpolation == PrimitiveVariable::FaceVarying )
            {
                dataIndex = faceIndex;
            }
            else
            {
                dataIndex = static_cast<size_t>( ids[faceIndex] );
            }
            if( dataIndex >= values.size() )
            {
                return Imath::V2f( 0.5f, 0.5f );
            }
            const Imath::V3f value = values[dataIndex];
            return Imath::V2f( value.x, value.y );
        };
        const Imath::V2f a = sample( 0 );
        const Imath::V2f b = sample( 1 );
        const Imath::V2f c = sample( 2 );
        return ( a * barycentric.x ) + ( b * barycentric.y ) + ( c * barycentric.z );
    }
    return Imath::V2f( 0.5f, 0.5f );
}

float clamp01( float value )
{
    return std::max( 0.0f, std::min( value, 1.0f ) );
}

float quantizeValue( float value, float step, float limit )
{
    if( step > 0.0f )
    {
        value = std::round( value / step ) * step;
    }
    return std::max( -limit, std::min( value, limit ) );
}

Imath::V3f quantizeVector( const Imath::V3f &value, const Imath::V3f &step, const Imath::V3f &limit )
{
    return Imath::V3f(
        quantizeValue( value.x, step.x, limit.x ),
        quantizeValue( value.y, step.y, limit.y ),
        quantizeValue( value.z, step.z, limit.z )
    );
}

float numericDataValue( const Data *data, size_t index, float fallback )
{
    if( const FloatData *typed = runTimeCast<const FloatData>( data ) )
    {
        return typed->readable();
    }
    if( const IntData *typed = runTimeCast<const IntData>( data ) )
    {
        return static_cast<float>( typed->readable() );
    }
    if( const Int64Data *typed = runTimeCast<const Int64Data>( data ) )
    {
        return static_cast<float>( typed->readable() );
    }
    if( const FloatVectorData *typed = runTimeCast<const FloatVectorData>( data ) )
    {
        const std::vector<float> &values = typed->readable();
        return index < values.size() ? values[index] : fallback;
    }
    if( const IntVectorData *typed = runTimeCast<const IntVectorData>( data ) )
    {
        const std::vector<int> &values = typed->readable();
        return index < values.size() ? static_cast<float>( values[index] ) : fallback;
    }
    if( const Int64VectorData *typed = runTimeCast<const Int64VectorData>( data ) )
    {
        const std::vector<int64_t> &values = typed->readable();
        return index < values.size() ? static_cast<float>( values[index] ) : fallback;
    }
    return fallback;
}

float numericPrimitiveVariableValue(
    const PrimitiveVariable &variable,
    size_t faceIndex,
    const std::vector<int> &ids,
    size_t faceVertexOffset,
    const Imath::V3f &barycentric,
    float fallback
)
{
    if( !variable.data )
    {
        return fallback;
    }

    switch( variable.interpolation )
    {
        case PrimitiveVariable::Constant :
            return numericDataValue( variable.data.get(), 0, fallback );
        case PrimitiveVariable::Uniform :
            return numericDataValue( variable.data.get(), faceIndex, fallback );
        case PrimitiveVariable::Vertex :
        case PrimitiveVariable::Varying :
        case PrimitiveVariable::FaceVarying :
        {
            auto sample = [&]( size_t corner ) -> float {
                const size_t faceIndexOffset = faceVertexOffset + corner;
                const size_t dataIndex =
                    variable.interpolation == PrimitiveVariable::FaceVarying
                        ? faceIndexOffset
                        : static_cast<size_t>( ids[faceIndexOffset] );
                return numericDataValue( variable.data.get(), dataIndex, fallback );
            };
            const float a = sample( 0 );
            const float b = sample( 1 );
            const float c = sample( 2 );
            return ( a * barycentric.x ) + ( b * barycentric.y ) + ( c * barycentric.z );
        }
        default :
            return fallback;
    }
}

std::vector<SampledPoint> primitiveCenterPoints(
    const MeshPrimitive *mesh,
    const MeshPrimitive *referenceMesh,
    const std::string &sourcePath,
    const std::string &uvName,
    const std::string &idVariableName,
    const std::string &densityVariableName,
    int seed,
    float jittering,
    float densityMultiplier,
    float perPointDensity,
    const Imath::M44f &supportTransform,
    const Imath::M44f &referenceSupportTransform,
    int supportSpaceMode
)
{
    std::vector<SampledPoint> result;
    ConstV3fVectorDataPtr pData = mesh->variableData<V3fVectorData>( "P", PrimitiveVariable::Vertex );
    ConstIntVectorDataPtr verticesPerFace = mesh->verticesPerFace();
    ConstIntVectorDataPtr vertexIds = mesh->vertexIds();
    if( !pData || !verticesPerFace || !vertexIds )
    {
        return result;
    }

    PrimitiveVariable uvVariable;
    const bool hasUv = mesh->variables.find( uvName ) != mesh->variables.end();
    if( hasUv )
    {
        uvVariable = mesh->variables.at( uvName );
    }

    PrimitiveVariable idVariable;
    const bool hasIdVariable = !idVariableName.empty() && mesh->variables.find( idVariableName ) != mesh->variables.end();
    if( hasIdVariable )
    {
        idVariable = mesh->variables.at( idVariableName );
    }

    PrimitiveVariable densityVariable;
    const bool hasDensityVariable = !densityVariableName.empty() && mesh->variables.find( densityVariableName ) != mesh->variables.end();
    if( hasDensityVariable )
    {
        densityVariable = mesh->variables.at( densityVariableName );
    }

    const std::vector<Imath::V3f> &points = pData->readable();
    const std::vector<int> &counts = verticesPerFace->readable();
    const std::vector<int> &ids = vertexIds->readable();
    ConstV3fVectorDataPtr referencePData = referenceMesh ? referenceMesh->variableData<V3fVectorData>( "P", PrimitiveVariable::Vertex ) : nullptr;
    ConstIntVectorDataPtr referenceVerticesPerFace = referenceMesh ? referenceMesh->verticesPerFace() : nullptr;
    ConstIntVectorDataPtr referenceVertexIds = referenceMesh ? referenceMesh->vertexIds() : nullptr;
    const bool hasReferenceTopology =
        referencePData &&
        referenceVerticesPerFace &&
        referenceVertexIds &&
        referenceVerticesPerFace->readable() == counts &&
        referenceVertexIds->readable() == ids;
    const std::vector<Imath::V3f> *referencePoints = hasReferenceTopology ? &referencePData->readable() : nullptr;
    std::mt19937 rng( static_cast<std::uint32_t>( seed ) );
    std::uniform_real_distribution<float> unit( -1.0f, 1.0f );

    size_t offset = 0;
    int pointId = 0;
    for( size_t faceIndex = 0; faceIndex < counts.size(); ++faceIndex )
    {
        const int count = counts[faceIndex];
        if( count < 3 || offset + static_cast<size_t>( count ) > ids.size() )
        {
            offset += std::max( count, 0 );
            continue;
        }

        Imath::V3f center( 0.0f );
        for( int corner = 0; corner < count; ++corner )
        {
            center += points[ids[offset + corner]];
        }
        center /= static_cast<float>( count );

        const Imath::V3f a = points[ids[offset]];
        const Imath::V3f b = points[ids[offset + 1]];
        const Imath::V3f c = points[ids[offset + 2]];
        Imath::V3f normal = normalized( cross( b - a, c - a ), Imath::V3f( 0.0f, 1.0f, 0.0f ) );
        Imath::V3f referenceCenter( 0.0f );
        Imath::V3f referenceNormal = normal;
        if( referencePoints )
        {
            for( int corner = 0; corner < count; ++corner )
            {
                referenceCenter += ( *referencePoints )[ids[offset + corner]];
            }
            referenceCenter /= static_cast<float>( count );

            const Imath::V3f refA = ( *referencePoints )[ids[offset]];
            const Imath::V3f refB = ( *referencePoints )[ids[offset + 1]];
            const Imath::V3f refC = ( *referencePoints )[ids[offset + 2]];
            referenceNormal = normalized( cross( refB - refA, refC - refA ), normal );
        }
        else
        {
            referenceCenter = center;
        }
        if( jittering > 0.0f )
        {
            const Imath::V3f jitter( unit( rng ), unit( rng ), unit( rng ) );
            center += jitter * jittering;
            referenceCenter += jitter * jittering;
        }

        SampledPoint point;
        point.localPosition = center;
        point.referencePosition = referenceCenter * referenceSupportTransform;
        switch( supportSpaceMode )
        {
            case WorldSpaceMode :
                point.position = center * supportTransform;
                point.normal = normalized( normal * supportTransform, normal );
                break;
            case ReferenceSpaceMode :
                point.position = point.referencePosition;
                point.normal = normalized( referenceNormal * referenceSupportTransform, referenceNormal );
                break;
            case ObjectSpaceMode :
            default :
                point.position = center;
                point.normal = normal;
                break;
        }
        point.uv = hasUv ? interpolateUv( uvVariable, ids, offset, Imath::V3f( 1.0f / 3.0f ) ) : Imath::V2f( 0.5f, 0.5f );
        point.sourcePath = sourcePath;
        point.seed = seed + static_cast<int>( faceIndex );
        point.id = pointId++;
        point.selectionId = hasIdVariable
            ? static_cast<int>( std::lround( numericPrimitiveVariableValue( idVariable, faceIndex, ids, offset, Imath::V3f( 1.0f / 3.0f ), static_cast<float>( point.id ) ) ) )
            : point.id;
        point.hasSelectionId = hasIdVariable;
        const float sampledDensity = hasDensityVariable
            ? clamp01( numericPrimitiveVariableValue( densityVariable, faceIndex, ids, offset, Imath::V3f( 1.0f / 3.0f ), perPointDensity ) )
            : perPointDensity;
        point.imageValue = sampledDensity;
        point.density = densityMultiplier * sampledDensity;
        point.probability = clamp01( point.density );
        result.push_back( point );
        offset += static_cast<size_t>( count );
    }

    return result;
}

std::vector<SampledPoint> randomSurfacePoints(
    const MeshPrimitive *mesh,
    const MeshPrimitive *referenceMesh,
    const std::string &uvName,
    const std::string &idVariableName,
    const std::string &densityVariableName,
    int pointCount,
    int seed,
    float jittering,
    float densityMultiplier,
    const ScenePlug::ScenePath &supportPath,
    const ImagePlug *imagePlug,
    const std::string &viewName,
    const Imath::M44f &supportTransform,
    const Imath::M44f &referenceSupportTransform,
    int supportSpaceMode
)
{
    std::vector<SampledPoint> result;
    if( pointCount <= 0 )
    {
        return result;
    }

    ConstV3fVectorDataPtr pData = mesh->variableData<V3fVectorData>( "P", PrimitiveVariable::Vertex );
    ConstIntVectorDataPtr verticesPerFace = mesh->verticesPerFace();
    ConstIntVectorDataPtr vertexIds = mesh->vertexIds();
    if( !pData || !verticesPerFace || !vertexIds )
    {
        return result;
    }

    PrimitiveVariable uvVariable;
    const bool hasUv = mesh->variables.find( uvName ) != mesh->variables.end();
    if( hasUv )
    {
        uvVariable = mesh->variables.at( uvName );
    }

    PrimitiveVariable idVariable;
    const bool hasIdVariable = !idVariableName.empty() && mesh->variables.find( idVariableName ) != mesh->variables.end();
    if( hasIdVariable )
    {
        idVariable = mesh->variables.at( idVariableName );
    }

    PrimitiveVariable densityVariable;
    const bool hasDensityVariable = !densityVariableName.empty() && mesh->variables.find( densityVariableName ) != mesh->variables.end();
    if( hasDensityVariable )
    {
        densityVariable = mesh->variables.at( densityVariableName );
    }

    struct Triangle
    {
        Imath::V3f a;
        Imath::V3f b;
        Imath::V3f c;
        Imath::V3f normal;
        Imath::V2f uvA = Imath::V2f( 0.5f, 0.5f );
        Imath::V2f uvB = Imath::V2f( 0.5f, 0.5f );
        Imath::V2f uvC = Imath::V2f( 0.5f, 0.5f );
        Imath::V3f referenceA;
        Imath::V3f referenceB;
        Imath::V3f referenceC;
        Imath::V3f referenceNormal;
        float area = 0.0f;
        size_t faceIndex = 0;
        size_t faceVertexOffset = 0;
    };

    const std::vector<Imath::V3f> &points = pData->readable();
    const std::vector<int> &counts = verticesPerFace->readable();
    const std::vector<int> &ids = vertexIds->readable();
    ConstV3fVectorDataPtr referencePData = referenceMesh ? referenceMesh->variableData<V3fVectorData>( "P", PrimitiveVariable::Vertex ) : nullptr;
    ConstIntVectorDataPtr referenceVerticesPerFace = referenceMesh ? referenceMesh->verticesPerFace() : nullptr;
    ConstIntVectorDataPtr referenceVertexIds = referenceMesh ? referenceMesh->vertexIds() : nullptr;
    const bool hasReferenceTopology =
        referencePData &&
        referenceVerticesPerFace &&
        referenceVertexIds &&
        referenceVerticesPerFace->readable() == counts &&
        referenceVertexIds->readable() == ids;
    const std::vector<Imath::V3f> *referencePoints = hasReferenceTopology ? &referencePData->readable() : nullptr;
    std::vector<Triangle> triangles;
    std::vector<float> cumulative;

    size_t offset = 0;
    float totalArea = 0.0f;
    for( const int count : counts )
    {
        if( count < 3 || offset + static_cast<size_t>( count ) > ids.size() )
        {
            offset += std::max( count, 0 );
            continue;
        }

        const Imath::V3f a = points[ids[offset]];
        for( int corner = 1; corner + 1 < count; ++corner )
        {
            const Imath::V3f b = points[ids[offset + corner]];
            const Imath::V3f c = points[ids[offset + corner + 1]];
            const Imath::V3f crossValue = cross( b - a, c - a );
            const float area = 0.5f * std::sqrt( std::max( crossValue.dot( crossValue ), 0.0f ) );
            if( area <= 0.0f )
            {
                continue;
            }
            Triangle triangle;
            triangle.a = a;
            triangle.b = b;
            triangle.c = c;
            triangle.normal = normalized( crossValue, Imath::V3f( 0.0f, 1.0f, 0.0f ) );
            triangle.referenceA = a;
            triangle.referenceB = b;
            triangle.referenceC = c;
            triangle.referenceNormal = triangle.normal;
            if( referencePoints )
            {
                triangle.referenceA = ( *referencePoints )[ids[offset]];
                triangle.referenceB = ( *referencePoints )[ids[offset + corner]];
                triangle.referenceC = ( *referencePoints )[ids[offset + corner + 1]];
                triangle.referenceNormal = normalized( cross( triangle.referenceB - triangle.referenceA, triangle.referenceC - triangle.referenceA ), triangle.normal );
            }
            if( hasUv )
            {
                triangle.uvA = interpolateUv( uvVariable, ids, offset, Imath::V3f( 1.0f, 0.0f, 0.0f ) );
                triangle.uvB = interpolateUv( uvVariable, ids, offset, Imath::V3f( 0.0f, 1.0f, 0.0f ) );
                triangle.uvC = interpolateUv( uvVariable, ids, offset, Imath::V3f( 0.0f, 0.0f, 1.0f ) );
            }
            triangle.area = area;
            triangle.faceIndex = cumulative.size();
            triangle.faceVertexOffset = offset;
            totalArea += area;
            triangles.push_back( triangle );
            cumulative.push_back( totalArea );
        }

        offset += static_cast<size_t>( count );
    }

    if( triangles.empty() || totalArea <= 0.0f )
    {
        return result;
    }

    std::mt19937 rng( static_cast<std::uint32_t>( seed ) );
    std::uniform_real_distribution<float> unit( 0.0f, 1.0f );
    result.reserve( static_cast<size_t>( pointCount ) );

    const std::string defaultView = ImagePlug::defaultViewName;
    const std::string resolvedView = viewName.empty() ? defaultView : viewName;

    Sampler *sampler = nullptr;
    std::unique_ptr<Sampler> ownedSampler;
    if( imagePlug )
    {
        ImagePlug::GlobalScope scope( Context::current() );
        scope.set( ImagePlug::viewNameContextName, &resolvedView );
        const Imath::Box2i dataWindow = imagePlug->dataWindow( &resolvedView );
        if( !dataWindow.isEmpty() )
        {
            ownedSampler.reset( new Sampler( imagePlug, "R", dataWindow, Sampler::Clamp ) );
            sampler = ownedSampler.get();
        }
    }

    for( int i = 0; i < pointCount; ++i )
    {
        const float areaSample = unit( rng ) * totalArea;
        const auto cumulativeIt = std::lower_bound( cumulative.begin(), cumulative.end(), areaSample );
        const size_t triangleIndex = std::min<size_t>( static_cast<size_t>( std::distance( cumulative.begin(), cumulativeIt ) ), triangles.size() - 1 );
        const Triangle &triangle = triangles[triangleIndex];

        const float r1 = unit( rng );
        const float r2 = unit( rng );
        const float u = std::sqrt( r1 );
        const Imath::V3f barycentric( 1.0f - u, u * ( 1.0f - r2 ), u * r2 );

        SampledPoint point;
        point.localPosition = trianglePoint( triangle.a, triangle.b, triangle.c, barycentric );
        point.referencePosition = trianglePoint( triangle.referenceA, triangle.referenceB, triangle.referenceC, barycentric ) * referenceSupportTransform;
        point.uv =
            ( triangle.uvA * barycentric.x ) +
            ( triangle.uvB * barycentric.y ) +
            ( triangle.uvC * barycentric.z );
        point.sourcePath = ScenePlug::pathToString( supportPath );
        point.seed = seed + i;
        point.id = i;
        point.selectionId = hasIdVariable
            ? static_cast<int>( std::lround( numericPrimitiveVariableValue( idVariable, triangle.faceIndex, ids, triangle.faceVertexOffset, barycentric, static_cast<float>( point.id ) ) ) )
            : point.id;
        point.hasSelectionId = hasIdVariable;

        if( jittering > 0.0f )
        {
            const float offset = ( unit( rng ) * 2.0f - 1.0f ) * jittering;
            point.localPosition += triangle.normal * offset;
            point.referencePosition += triangle.referenceNormal * offset;
        }

        float imageValue = 1.0f;
        if( sampler )
        {
            const Imath::Box2i dataWindow = imagePlug->dataWindow( &resolvedView );
            const float x = dataWindow.min.x + clamp01( point.uv.x ) * std::max( dataWindow.size().x - 1, 0 );
            const float y = dataWindow.min.y + clamp01( point.uv.y ) * std::max( dataWindow.size().y - 1, 0 );
            imageValue = clamp01( sampler->sample( x + 0.5f, y + 0.5f ) );
        }

        const float sampledDensity = hasDensityVariable
            ? clamp01( numericPrimitiveVariableValue( densityVariable, triangle.faceIndex, ids, triangle.faceVertexOffset, barycentric, 1.0f ) )
            : 1.0f;
        point.imageValue = clamp01( imageValue * sampledDensity );
        point.density = densityMultiplier * point.imageValue;
        point.probability = clamp01( point.density );
        if( point.probability <= 0.0f )
        {
            continue;
        }
        if( point.probability < 1.0f && unit( rng ) > point.probability )
        {
            continue;
        }

        switch( supportSpaceMode )
        {
            case WorldSpaceMode :
                point.position = point.localPosition * supportTransform;
                point.normal = normalized( triangle.normal * supportTransform, triangle.normal );
                break;
            case ReferenceSpaceMode :
                point.position = point.referencePosition;
                point.normal = normalized( triangle.referenceNormal * referenceSupportTransform, triangle.referenceNormal );
                break;
            case ObjectSpaceMode :
            default :
                point.position = point.localPosition;
                point.normal = triangle.normal;
                break;
        }
        result.push_back( point );
    }

    return result;
}

PointsPrimitivePtr pointsPrimitiveFromSamples(
    const std::vector<SampledPoint> &samples,
    const std::string &pointType,
    const std::string &densityPrimitiveVariable,
    const std::string &referencePosition,
    const std::string &uvVariableName,
    const std::string &idVariable,
    const std::string &geometryIdAttribute,
    const std::string &probabilityAttribute
)
{
    std::vector<Imath::V3f> positions;
    std::vector<Imath::V3f> referencePositions;
    std::vector<Imath::V3f> normals;
    std::vector<Imath::V2f> uvs;
    std::vector<Imath::Color3f> colors;
    std::vector<float> widths;
    std::vector<float> densityValues;
    std::vector<float> probabilityValues;
    std::vector<float> timeOffsets;
    std::vector<int64_t> ids;
    std::vector<int> selectionIds;
    std::vector<int> seeds;
    std::vector<int> geometryIds;
    std::vector<std::string> sourcePaths;
    const Imath::Color3f displayColor( 0.0f, 0.0f, 120.0f / 255.0f );

    positions.reserve( samples.size() );
    referencePositions.reserve( samples.size() );
    normals.reserve( samples.size() );
    uvs.reserve( samples.size() );
    colors.reserve( samples.size() );
    widths.reserve( samples.size() );
    densityValues.reserve( samples.size() );
    probabilityValues.reserve( samples.size() );
    timeOffsets.reserve( samples.size() );
    ids.reserve( samples.size() );
    selectionIds.reserve( samples.size() );
    seeds.reserve( samples.size() );
    geometryIds.reserve( samples.size() );
    sourcePaths.reserve( samples.size() );

    for( size_t i = 0; i < samples.size(); ++i )
    {
        const SampledPoint &sample = samples[i];
        positions.push_back( sample.position );
        referencePositions.push_back( sample.referencePosition );
        normals.push_back( sample.normal );
        uvs.push_back( sample.uv );
        colors.push_back( displayColor );
        widths.push_back( sample.width );
        densityValues.push_back( sample.density );
        probabilityValues.push_back( sample.probability );
        timeOffsets.push_back( sample.timeOffset );
        ids.push_back( static_cast<int64_t>( sample.id ) );
        selectionIds.push_back( sample.selectionId );
        seeds.push_back( sample.seed );
        geometryIds.push_back( sample.selectionId );
        sourcePaths.push_back( sample.sourcePath );
    }

    PointsPrimitivePtr primitive = new PointsPrimitive(
        v3fVectorData( positions, GeometricData::Interpretation::Point ),
        floatVectorData( widths )
    );
    primitive->variables["type"] = PrimitiveVariable( PrimitiveVariable::Constant, new StringData( pointType ) );
    primitive->variables["width"] = PrimitiveVariable( PrimitiveVariable::Vertex, floatVectorData( widths ) );
    primitive->variables["id"] = PrimitiveVariable( PrimitiveVariable::Vertex, int64VectorData( ids ) );
    primitive->variables["seed"] = PrimitiveVariable( PrimitiveVariable::Vertex, intVectorData( seeds ) );
    primitive->variables["sourcePath"] = PrimitiveVariable( PrimitiveVariable::Vertex, stringVectorData( sourcePaths ) );
    primitive->variables["N"] = PrimitiveVariable( PrimitiveVariable::Vertex, v3fVectorData( normals, GeometricData::Interpretation::Normal ) );
    primitive->variables["scatterColor"] = PrimitiveVariable( PrimitiveVariable::Vertex, color3fVectorData( colors ) );
    primitive->variables["Cs"] = PrimitiveVariable( PrimitiveVariable::Vertex, color3fVectorData( colors ) );
    primitive->variables[uvVariableName.empty() ? "uv" : uvVariableName] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        v2fVectorData( uvs, GeometricData::Interpretation::UV )
    );
    primitive->variables[densityPrimitiveVariable.empty() ? "density" : densityPrimitiveVariable] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        floatVectorData( densityValues )
    );
    primitive->variables[referencePosition.empty() ? "referencePosition" : referencePosition] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        v3fVectorData( referencePositions, GeometricData::Interpretation::Point )
    );
    primitive->variables[idVariable.empty() ? "id" : idVariable] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        intVectorData( selectionIds )
    );
    primitive->variables[geometryIdAttribute.empty() ? "geometryId" : geometryIdAttribute] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        intVectorData( geometryIds )
    );
    primitive->variables[probabilityAttribute.empty() ? "probability" : probabilityAttribute] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        floatVectorData( probabilityValues )
    );
    primitive->variables["timeOffset"] = PrimitiveVariable( PrimitiveVariable::Vertex, floatVectorData( timeOffsets ) );
    return primitive;
}

PointsPrimitivePtr pointsPrimitiveFromInstances(
    const std::vector<InstanceRecord> &instances,
    const std::vector<PrototypeRecord> &prototypes,
    int rotationOrder,
    const std::string &pointType,
    const std::string &densityPrimitiveVariable,
    const std::string &referencePosition,
    const std::string &uvVariableName,
    const std::string &idVariable,
    const std::string &geometryIdAttribute,
    const std::string &probabilityAttribute
)
{
    std::vector<Imath::V3f> positions;
    std::vector<Imath::V3f> referencePositions;
    std::vector<Imath::V3f> normals;
    std::vector<Imath::V2f> uvs;
    std::vector<Imath::Color3f> colors;
    std::vector<Imath::V3f> scatterPositions;
    std::vector<Imath::V3f> scatterRotations;
    std::vector<Imath::V3f> scatterScales;
    std::vector<Imath::V3f> scales;
    std::vector<Imath::Quatf> orientations;
    std::vector<float> widths;
    std::vector<float> densityValues;
    std::vector<float> probabilityValues;
    std::vector<float> timeOffsets;
    std::vector<int64_t> ids;
    std::vector<int> selectionIds;
    std::vector<int> seeds;
    std::vector<int> geometryIds;
    std::vector<int> prototypeIndices;
    std::vector<std::string> sourcePaths;
    const Imath::Color3f displayColor( 0.0f, 0.0f, 120.0f / 255.0f );

    positions.reserve( instances.size() );
    referencePositions.reserve( instances.size() );
    normals.reserve( instances.size() );
    uvs.reserve( instances.size() );
    colors.reserve( instances.size() );
    scatterPositions.reserve( instances.size() );
    scatterRotations.reserve( instances.size() );
    scatterScales.reserve( instances.size() );
    scales.reserve( instances.size() );
    orientations.reserve( instances.size() );
    widths.reserve( instances.size() );
    densityValues.reserve( instances.size() );
    probabilityValues.reserve( instances.size() );
    timeOffsets.reserve( instances.size() );
    ids.reserve( instances.size() );
    selectionIds.reserve( instances.size() );
    seeds.reserve( instances.size() );
    geometryIds.reserve( instances.size() );
    prototypeIndices.reserve( instances.size() );
    sourcePaths.reserve( instances.size() );

    for( const InstanceRecord &instance : instances )
    {
        const PrototypeRecord &prototype = prototypes[instance.prototypeIndex];
        positions.push_back( Imath::V3f( instance.transform[3][0], instance.transform[3][1], instance.transform[3][2] ) );
        referencePositions.push_back( instance.sample.referencePosition );
        normals.push_back( instance.sample.normal );
        uvs.push_back( instance.sample.uv );
        colors.push_back( displayColor );
        scatterPositions.push_back( instance.scatterPosition );
        scatterRotations.push_back( instance.scatterRotation );
        scatterScales.push_back( instance.scatterScale );
        scales.push_back( instance.scatterScale );
        orientations.push_back( instance.orientation );
        widths.push_back( instance.sample.width );
        densityValues.push_back( instance.sample.density );
        probabilityValues.push_back( instance.sample.probability );
        timeOffsets.push_back( instance.timeOffset );
        ids.push_back( static_cast<int64_t>( instance.id ) );
        selectionIds.push_back( instance.sample.selectionId );
        seeds.push_back( instance.seed );
        geometryIds.push_back( prototype.geometryId >= 0 ? prototype.geometryId : static_cast<int>( instance.prototypeIndex ) );
        prototypeIndices.push_back( static_cast<int>( instance.prototypeIndex ) );
        sourcePaths.push_back( instance.sample.sourcePath );
    }

    PointsPrimitivePtr primitive = new PointsPrimitive(
        v3fVectorData( positions, GeometricData::Interpretation::Point ),
        floatVectorData( widths )
    );
    primitive->variables["type"] = PrimitiveVariable( PrimitiveVariable::Constant, new StringData( pointType ) );
    primitive->variables["width"] = PrimitiveVariable( PrimitiveVariable::Vertex, floatVectorData( widths ) );
    primitive->variables["id"] = PrimitiveVariable( PrimitiveVariable::Vertex, int64VectorData( ids ) );
    primitive->variables["seed"] = PrimitiveVariable( PrimitiveVariable::Vertex, intVectorData( seeds ) );
    primitive->variables["sourcePath"] = PrimitiveVariable( PrimitiveVariable::Vertex, stringVectorData( sourcePaths ) );
    primitive->variables["N"] = PrimitiveVariable( PrimitiveVariable::Vertex, v3fVectorData( normals, GeometricData::Interpretation::Normal ) );
    primitive->variables["scatterColor"] = PrimitiveVariable( PrimitiveVariable::Vertex, color3fVectorData( colors ) );
    primitive->variables["Cs"] = PrimitiveVariable( PrimitiveVariable::Vertex, color3fVectorData( colors ) );
    primitive->variables[uvVariableName.empty() ? "uv" : uvVariableName] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        v2fVectorData( uvs, GeometricData::Interpretation::UV )
    );
    primitive->variables[densityPrimitiveVariable.empty() ? "density" : densityPrimitiveVariable] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        floatVectorData( densityValues )
    );
    primitive->variables[referencePosition.empty() ? "referencePosition" : referencePosition] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        v3fVectorData( referencePositions, GeometricData::Interpretation::Point )
    );
    primitive->variables[idVariable.empty() ? "id" : idVariable] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        intVectorData( selectionIds )
    );
    primitive->variables[geometryIdAttribute.empty() ? "geometryId" : geometryIdAttribute] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        intVectorData( geometryIds )
    );
    primitive->variables[probabilityAttribute.empty() ? "probability" : probabilityAttribute] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        floatVectorData( probabilityValues )
    );
    primitive->variables["prototypeIndex"] = PrimitiveVariable( PrimitiveVariable::Vertex, intVectorData( prototypeIndices ) );
    primitive->variables["orientation"] = PrimitiveVariable( PrimitiveVariable::Vertex, quatfVectorData( orientations ) );
    primitive->variables["scale"] = PrimitiveVariable( PrimitiveVariable::Vertex, v3fVectorData( scales, GeometricData::Interpretation::Vector ) );
    primitive->variables["timeOffset"] = PrimitiveVariable( PrimitiveVariable::Vertex, floatVectorData( timeOffsets ) );
    primitive->variables["scatter_position"] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        v3fVectorData( scatterPositions, GeometricData::Interpretation::Vector )
    );
    primitive->variables["scatter_rotation"] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        v3fVectorData( scatterRotations, GeometricData::Interpretation::Vector )
    );
    primitive->variables["scatter_scale"] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        v3fVectorData( scatterScales, GeometricData::Interpretation::Vector )
    );
    primitive->variables["scatter_normal"] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        v3fVectorData( normals, GeometricData::Interpretation::Normal )
    );
    primitive->variables["scatter_rotation_order"] = PrimitiveVariable(
        PrimitiveVariable::Constant,
        new IntData( rotationOrder )
    );
    primitive->variables["scatter_time_offset"] = PrimitiveVariable(
        PrimitiveVariable::Vertex,
        floatVectorData( timeOffsets )
    );
    return primitive;
}

Imath::Eulerf::Order imathRotationOrder( int value )
{
    switch( value )
    {
        case XYZRotationOrder : return Imath::Eulerf::XYZ;
        case XZYRotationOrder : return Imath::Eulerf::XZY;
        case YXZRotationOrder : return Imath::Eulerf::YXZ;
        case YZXRotationOrder : return Imath::Eulerf::YZX;
        case ZXYRotationOrder : return Imath::Eulerf::ZXY;
        case ZYXRotationOrder :
        default :
            return Imath::Eulerf::ZYX;
    }
}

Imath::M44f matrixFromEuler( const Imath::V3f &degrees, int rotationOrder )
{
    Imath::Eulerf euler(
        degrees.x * static_cast<float>( M_PI / 180.0 ),
        degrees.y * static_cast<float>( M_PI / 180.0 ),
        degrees.z * static_cast<float>( M_PI / 180.0 ),
        imathRotationOrder( rotationOrder )
    );
    return euler.toMatrix44();
}

Imath::M44f alignMatrixFromNormal( const Imath::V3f &normal )
{
    const Imath::V3f yAxis = normalized( normal, Imath::V3f( 0.0f, 1.0f, 0.0f ) );
    const Imath::V3f reference = std::abs( yAxis.z ) < 0.999f ? Imath::V3f( 0.0f, 0.0f, 1.0f ) : Imath::V3f( 1.0f, 0.0f, 0.0f );
    const Imath::V3f xAxis = normalized( cross( reference, yAxis ), Imath::V3f( 1.0f, 0.0f, 0.0f ) );
    const Imath::V3f zAxis = normalized( cross( yAxis, xAxis ), Imath::V3f( 0.0f, 0.0f, 1.0f ) );

    Imath::M44f result;
    result.makeIdentity();
    result[0][0] = xAxis.x;
    result[0][1] = xAxis.y;
    result[0][2] = xAxis.z;
    result[1][0] = yAxis.x;
    result[1][1] = yAxis.y;
    result[1][2] = yAxis.z;
    result[2][0] = zAxis.x;
    result[2][1] = zAxis.y;
    result[2][2] = zAxis.z;
    return result;
}

Imath::M44f transformFromSample(
    const SampledPoint &sample,
    bool useSupportNormals,
    const Imath::V3f &position,
    const Imath::V3f &orientation,
    const Imath::V3f &scale,
    const Imath::V3f &positionOffset,
    const Imath::V3f &rotationOffset,
    const Imath::V3f &scaleOffset,
    int rotationOrder
)
{
    Imath::M44f result;
    result.makeIdentity();

    const Imath::V3f finalScale(
        std::max( 0.001f, scale.x + scaleOffset.x ),
        std::max( 0.001f, scale.y + scaleOffset.y ),
        std::max( 0.001f, scale.z + scaleOffset.z )
    );
    Imath::M44f align;
    align.makeIdentity();
    if( useSupportNormals )
    {
        align = alignMatrixFromNormal( sample.normal );
    }
    const Imath::M44f rotation = matrixFromEuler( orientation + rotationOffset, rotationOrder );

    Imath::M44f scaleMatrix;
    scaleMatrix.makeIdentity();
    scaleMatrix.scale( finalScale );

    result = scaleMatrix * rotation * align;
    const Imath::V3f translate = sample.position + position + positionOffset;
    result[3][0] = translate.x;
    result[3][1] = translate.y;
    result[3][2] = translate.z;
    return result;
}

Imath::V3f finalScaleFromOffsets( const Imath::V3f &scale, const Imath::V3f &scaleOffset )
{
    return Imath::V3f(
        std::max( 0.001f, scale.x + scaleOffset.x ),
        std::max( 0.001f, scale.y + scaleOffset.y ),
        std::max( 0.001f, scale.z + scaleOffset.z )
    );
}

Imath::M44f rotationMatrixFromSample(
    const SampledPoint &sample,
    bool useSupportNormals,
    const Imath::V3f &orientation,
    const Imath::V3f &rotationOffset,
    int rotationOrder
)
{
    Imath::M44f align;
    align.makeIdentity();
    if( useSupportNormals )
    {
        align = alignMatrixFromNormal( sample.normal );
    }
    return matrixFromEuler( orientation + rotationOffset, rotationOrder ) * align;
}

Imath::Box3f transformedBound( const Imath::Box3f &bound, const Imath::M44f &transform )
{
    if( bound.isEmpty() )
    {
        return bound;
    }

    Imath::Box3f result;
    const Imath::V3f min = bound.min;
    const Imath::V3f max = bound.max;
    const Imath::V3f corners[8] = {
        Imath::V3f( min.x, min.y, min.z ),
        Imath::V3f( min.x, min.y, max.z ),
        Imath::V3f( min.x, max.y, min.z ),
        Imath::V3f( min.x, max.y, max.z ),
        Imath::V3f( max.x, min.y, min.z ),
        Imath::V3f( max.x, min.y, max.z ),
        Imath::V3f( max.x, max.y, min.z ),
        Imath::V3f( max.x, max.y, max.z ),
    };
    for( const Imath::V3f &corner : corners )
    {
        result.extendBy( corner * transform );
    }
    return result;
}

std::string instanceName( size_t index )
{
    std::ostringstream stream;
    stream << "instance" << std::setw( 4 ) << std::setfill( '0' ) << index;
    return stream.str();
}

bool parseInstanceIndex( const ScenePlug::ScenePath &path, size_t &index )
{
    if( path.empty() )
    {
        return false;
    }
    const std::string name = path.back().string();
    if( name.rfind( "instance", 0 ) != 0 )
    {
        return false;
    }
    const std::string digits = name.substr( 8 );
    if( digits.empty() )
    {
        return false;
    }
    try
    {
        index = static_cast<size_t>( std::stoul( digits ) );
        return true;
    }
    catch( ... )
    {
        return false;
    }
}

int intAttribute( const PrototypeRecord &record, const std::string &name, int fallback )
{
    if( !record.attributes )
    {
        return fallback;
    }
    const auto it = record.attributes->members().find( name );
    if( it == record.attributes->members().end() )
    {
        return fallback;
    }
    if( const IntData *data = runTimeCast<const IntData>( it->second.get() ) )
    {
        return data->readable();
    }
    return fallback;
}

float floatAttribute( const PrototypeRecord &record, const std::string &name, float fallback )
{
    if( !record.attributes )
    {
        return fallback;
    }
    const auto it = record.attributes->members().find( name );
    if( it == record.attributes->members().end() )
    {
        return fallback;
    }
    if( const FloatData *data = runTimeCast<const FloatData>( it->second.get() ) )
    {
        return data->readable();
    }
    return fallback;
}

ConstMeshPrimitivePtr meshAtFrame( const ScenePlug *scene, const ScenePlug::ScenePath &path, float frame )
{
    Context::EditableScope scope( Context::current() );
    scope.setFrame( frame );
    return runTimeCast<const MeshPrimitive>( scene->object( path ).get() );
}

Imath::M44f fullTransformAtFrame( const ScenePlug *scene, const ScenePlug::ScenePath &path, float frame )
{
    Context::EditableScope scope( Context::current() );
    scope.setFrame( frame );
    return scene->fullTransform( path );
}

PrototypeRecord prototypeRecord( const ScenePlug *scene, const ScenePlug::ScenePath &path, const ScatterPlus *node )
{
    PrototypeRecord record;
    record.path = path;
    record.pathString = ScenePlug::pathToString( path );
    record.object = scene->object( path );
    record.attributes = scene->attributes( path );
    record.bound = scene->bound( path );
    record.geometryId = intAttribute( record, node->geometryIdAttributePlug()->getValue(), -1 );
    record.probability = std::max( 0.0f, floatAttribute( record, node->probabilityAttributePlug()->getValue(), 1.0f ) );
    return record;
}

size_t weightedPrototypeIndex( const std::vector<PrototypeRecord> &prototypes, std::mt19937 &rng )
{
    if( prototypes.empty() )
    {
        return 0;
    }

    float totalWeight = 0.0f;
    for( const PrototypeRecord &prototype : prototypes )
    {
        totalWeight += std::max( 0.0f, prototype.probability );
    }
    if( totalWeight <= 0.0f )
    {
        return static_cast<size_t>( rng() ) % prototypes.size();
    }

    std::uniform_real_distribution<float> unit( 0.0f, totalWeight );
    float target = unit( rng );
    float running = 0.0f;
    for( size_t i = 0; i < prototypes.size(); ++i )
    {
        running += std::max( 0.0f, prototypes[i].probability );
        if( target <= running )
        {
            return i;
        }
    }
    return prototypes.size() - 1;
}

size_t prototypeIndexForSample(
    const ScatterPlus *node,
    const std::vector<PrototypeRecord> &prototypes,
    const SampledPoint &sample,
    size_t sampleIndex,
    std::mt19937 &rng
)
{
    if( prototypes.empty() )
    {
        return 0;
    }

    switch( node->prototypeModePlug()->getValue() )
    {
        case FirstPrototypeMode :
            return 0;
        case RandomPrototypeMode :
            return weightedPrototypeIndex( prototypes, rng );
        case CyclePrototypeMode :
            return sampleIndex % prototypes.size();
        case IndexPrototypeMode :
            return std::min<size_t>( std::max( node->prototypeIndexPlug()->getValue(), 0 ), prototypes.size() - 1 );
        case IdPrimitiveVariablePrototypeMode :
        {
            for( size_t i = 0; i < prototypes.size(); ++i )
            {
                if( prototypes[i].geometryId == sample.selectionId )
                {
                    return i;
                }
            }
            return static_cast<size_t>( std::abs( sample.selectionId ) ) % prototypes.size();
        }
        case LuminancePrototypeMode :
        {
            const float clamped = clamp01( sample.imageValue );
            const int index = std::min<int>( static_cast<int>( std::floor( clamped * static_cast<float>( prototypes.size() ) ) ), prototypes.size() - 1 );
            return std::max( 0, index );
        }
        default :
            return 0;
    }
}

Imath::V3f randomSignedVector( std::mt19937 &rng, const Imath::V3f &limit )
{
    std::uniform_real_distribution<float> unit( -1.0f, 1.0f );
    return Imath::V3f( unit( rng ) * limit.x, unit( rng ) * limit.y, unit( rng ) * limit.z );
}

float randomSignedFloat( std::mt19937 &rng, float limit )
{
    std::uniform_real_distribution<float> unit( -1.0f, 1.0f );
    return unit( rng ) * limit;
}

float sampledTimeOffset( std::mt19937 &rng, float baseOffset, float variance, int sampleCount )
{
    if( variance <= 0.0f )
    {
        return baseOffset;
    }
    if( sampleCount <= 1 )
    {
        return baseOffset + randomSignedFloat( rng, variance );
    }

    std::uniform_int_distribution<int> bucket( 0, sampleCount - 1 );
    const float step = ( variance * 2.0f ) / static_cast<float>( sampleCount - 1 );
    return baseOffset - variance + ( step * static_cast<float>( bucket( rng ) ) );
}

Imath::V3f convertedPosition(
    const Imath::V3f &position,
    const Imath::M44f &supportTransform,
    bool sourceWorldSpace,
    bool targetWorldSpace
)
{
    if( sourceWorldSpace == targetWorldSpace )
    {
        return position;
    }
    return targetWorldSpace ? position * supportTransform : position * supportTransform.inverse();
}

Imath::M44f transformForSpaceMode(
    int spaceMode,
    const Imath::M44f &supportTransform,
    const Imath::M44f &referenceSupportTransform
)
{
    Imath::M44f result;
    result.makeIdentity();
    switch( spaceMode )
    {
        case WorldSpaceMode :
            return supportTransform;
        case ReferenceSpaceMode :
            return referenceSupportTransform;
        case ObjectSpaceMode :
        default :
            return result;
    }
}

Imath::M44f convertedTransform(
    const Imath::M44f &transform,
    const Imath::M44f &supportTransform,
    const Imath::M44f &referenceSupportTransform,
    int sourceSpaceMode,
    int targetSpaceMode
)
{
    const Imath::M44f sourceSpaceTransform = transformForSpaceMode( sourceSpaceMode, supportTransform, referenceSupportTransform );
    const Imath::M44f targetSpaceTransform = transformForSpaceMode( targetSpaceMode, supportTransform, referenceSupportTransform );
    return transform * sourceSpaceTransform.inverse() * targetSpaceTransform;
}

Imath::Box3f convertedBound(
    const Imath::Box3f &bound,
    const Imath::M44f &supportTransform,
    bool sourceWorldSpace,
    bool targetWorldSpace
)
{
    if( sourceWorldSpace == targetWorldSpace || bound.isEmpty() )
    {
        return bound;
    }
    return transformedBound( bound, targetWorldSpace ? supportTransform : supportTransform.inverse() );
}

void orientedBoxFromTransform(
    const Imath::Box3f &localBound,
    const Imath::M44f &transform,
    Imath::V3f &center,
    Imath::V3f axes[3],
    Imath::V3f &extents
)
{
    const Imath::V3f localCenter = ( localBound.min + localBound.max ) * 0.5f;
    const Imath::V3f localExtents = ( localBound.max - localBound.min ) * 0.5f;
    center = localCenter * transform;

    for( int axis = 0; axis < 3; ++axis )
    {
        Imath::V3f transformedAxis( transform[axis][0], transform[axis][1], transform[axis][2] );
        const float length = transformedAxis.length();
        if( length > 1e-6f )
        {
            axes[axis] = transformedAxis / length;
            extents[axis] = localExtents[axis] * length;
        }
        else
        {
            axes[axis] = axis == 0 ? Imath::V3f( 1.0f, 0.0f, 0.0f ) : axis == 1 ? Imath::V3f( 0.0f, 1.0f, 0.0f ) : Imath::V3f( 0.0f, 0.0f, 1.0f );
            extents[axis] = 0.0f;
        }
    }
}

bool orientedBoxesOverlap(
    const Imath::V3f &centerA,
    const Imath::V3f axesA[3],
    const Imath::V3f &extentsA,
    const Imath::V3f &centerB,
    const Imath::V3f axesB[3],
    const Imath::V3f &extentsB
)
{
    float rotation[3][3];
    float absRotation[3][3];
    for( int i = 0; i < 3; ++i )
    {
        for( int j = 0; j < 3; ++j )
        {
            rotation[i][j] = axesA[i].dot( axesB[j] );
            absRotation[i][j] = std::abs( rotation[i][j] ) + 1e-6f;
        }
    }

    const Imath::V3f translationWorld = centerB - centerA;
    Imath::V3f translation(
        translationWorld.dot( axesA[0] ),
        translationWorld.dot( axesA[1] ),
        translationWorld.dot( axesA[2] )
    );

    auto extent = []( const Imath::V3f &v, int axis ) {
        return axis == 0 ? v.x : axis == 1 ? v.y : v.z;
    };

    for( int i = 0; i < 3; ++i )
    {
        float radiusA = extent( extentsA, i );
        float radiusB = 0.0f;
        for( int j = 0; j < 3; ++j )
        {
            radiusB += extent( extentsB, j ) * absRotation[i][j];
        }
        if( std::abs( translation[i] ) > radiusA + radiusB )
        {
            return false;
        }
    }

    for( int j = 0; j < 3; ++j )
    {
        float radiusA = 0.0f;
        float radiusB = extent( extentsB, j );
        for( int i = 0; i < 3; ++i )
        {
            radiusA += extent( extentsA, i ) * absRotation[i][j];
        }
        const float projectedTranslation = std::abs(
            translation[0] * rotation[0][j] +
            translation[1] * rotation[1][j] +
            translation[2] * rotation[2][j]
        );
        if( projectedTranslation > radiusA + radiusB )
        {
            return false;
        }
    }

    for( int i = 0; i < 3; ++i )
    {
        for( int j = 0; j < 3; ++j )
        {
            const float radiusA =
                extent( extentsA, ( i + 1 ) % 3 ) * absRotation[( i + 2 ) % 3][j] +
                extent( extentsA, ( i + 2 ) % 3 ) * absRotation[( i + 1 ) % 3][j];
            const float radiusB =
                extent( extentsB, ( j + 1 ) % 3 ) * absRotation[i][( j + 2 ) % 3] +
                extent( extentsB, ( j + 2 ) % 3 ) * absRotation[i][( j + 1 ) % 3];
            const float projectedTranslation = std::abs(
                translation[( i + 2 ) % 3] * rotation[( i + 1 ) % 3][j] -
                translation[( i + 1 ) % 3] * rotation[( i + 2 ) % 3][j]
            );
            if( projectedTranslation > radiusA + radiusB )
            {
                return false;
            }
        }
    }

    return true;
}

float proceduralDecimationScore( const Imath::V3f &position, int seed )
{
    auto hashCombine = []( std::uint32_t hash, std::uint32_t value ) {
        hash ^= value + 0x9e3779b9u + ( hash << 6 ) + ( hash >> 2 );
        return hash;
    };

    const std::int32_t qx = static_cast<std::int32_t>( std::lround( position.x * 1000.0f ) );
    const std::int32_t qy = static_cast<std::int32_t>( std::lround( position.y * 1000.0f ) );
    const std::int32_t qz = static_cast<std::int32_t>( std::lround( position.z * 1000.0f ) );

    std::uint32_t hash = 2166136261u;
    hash = hashCombine( hash, static_cast<std::uint32_t>( seed ) );
    hash = hashCombine( hash, static_cast<std::uint32_t>( qx ) );
    hash = hashCombine( hash, static_cast<std::uint32_t>( qy ) );
    hash = hashCombine( hash, static_cast<std::uint32_t>( qz ) );

    std::mt19937 rng( hash );
    std::uniform_real_distribution<float> unit( 0.0f, 1.0f );
    return unit( rng );
}

float decimationScore(
    const SampledPoint &sample,
    const ScatterPlus *node,
    const Imath::M44f &supportTransform,
    const Imath::M44f &referenceSupportTransform,
    int supportSpaceMode,
    bool hasImageInput
)
{
    const bool hasDrivenScore = hasImageInput || sample.probability < 0.999f || sample.imageValue < 0.999f;
    if( hasDrivenScore )
    {
        return clamp01( sample.imageValue > 0.0f ? sample.imageValue : sample.probability );
    }

    const int evaluationSpaceMode = node->decimationSpacePlug()->getValue();
    const Imath::M44f sourceSpaceTransform = transformForSpaceMode( supportSpaceMode, supportTransform, referenceSupportTransform );
    const Imath::M44f evaluationSpaceTransform = transformForSpaceMode( evaluationSpaceMode, supportTransform, referenceSupportTransform );
    const Imath::V3f evaluationPosition = sample.position * sourceSpaceTransform.inverse() * evaluationSpaceTransform;
    return proceduralDecimationScore( evaluationPosition, node->decimationSeedPlug()->getValue() );
}

Imath::Box3f scaledBound( const Imath::Box3f &bound, float scaleMultiplier )
{
    if( bound.isEmpty() )
    {
        return bound;
    }

    Imath::Box3f scaled = bound;
    const Imath::V3f center = ( bound.min + bound.max ) * 0.5f;
    const Imath::V3f half = ( bound.max - bound.min ) * 0.5f * scaleMultiplier;
    scaled.min = center - half;
    scaled.max = center + half;
    return scaled;
}

bool overlapsAny(
    const Imath::Box3f &bound,
    const std::vector<InstanceRecord> &instances,
    float scaleMultiplier
)
{
    if( instances.empty() || bound.isEmpty() )
    {
        return false;
    }

    const Imath::Box3f scaled = scaledBound( bound, scaleMultiplier );
    for( const InstanceRecord &instance : instances )
    {
        const Imath::Box3f instanceBound = scaledBound( instance.evaluationBound, scaleMultiplier );
        if( !instanceBound.isEmpty() && scaled.intersects( instanceBound ) )
        {
            return true;
        }
    }
    return false;
}

bool orientedBoxOverlapsAny(
    const Imath::V3f &center,
    const Imath::V3f axes[3],
    const Imath::V3f &extents,
    const std::vector<InstanceRecord> &instances,
    float scaleMultiplier
)
{
    if( instances.empty() )
    {
        return false;
    }

    const Imath::V3f scaledExtents = extents * scaleMultiplier;
    for( const InstanceRecord &instance : instances )
    {
        const Imath::V3f instanceExtents = instance.evaluationExtents * scaleMultiplier;
        if(
            orientedBoxesOverlap(
                center,
                axes,
                scaledExtents,
                instance.evaluationCenter,
                instance.evaluationAxes,
                instanceExtents
            )
        )
        {
            return true;
        }
    }
    return false;
}

bool ellipsoidOverlapsAny(
    const Imath::Box3f &bound,
    const std::vector<InstanceRecord> &instances,
    float scaleMultiplier
)
{
    if( instances.empty() || bound.isEmpty() )
    {
        return false;
    }

    const Imath::Box3f scaled = scaledBound( bound, scaleMultiplier );
    const Imath::V3f center = ( scaled.min + scaled.max ) * 0.5f;
    const Imath::V3f radius = ( scaled.max - scaled.min ) * 0.5f;
    for( const InstanceRecord &instance : instances )
    {
        if( instance.evaluationBound.isEmpty() )
        {
            continue;
        }

        const Imath::Box3f instanceScaled = scaledBound( instance.evaluationBound, scaleMultiplier );
        const Imath::V3f instanceCenter = ( instanceScaled.min + instanceScaled.max ) * 0.5f;
        const Imath::V3f instanceRadius = ( instanceScaled.max - instanceScaled.min ) * 0.5f;
        const Imath::V3f combinedRadius = radius + instanceRadius;

        float normalizedDistance = 0.0f;
        for( int axis = 0; axis < 3; ++axis )
        {
            const float axisRadius = std::max( 1e-6f, combinedRadius[axis] );
            const float axisDistance = ( center - instanceCenter )[axis] / axisRadius;
            normalizedDistance += axisDistance * axisDistance;
        }

        if( normalizedDistance <= 1.0f )
        {
            return true;
        }
    }
    return false;
}

bool overlapsAny(
    const InstanceRecord &candidate,
    const std::vector<InstanceRecord> &instances,
    float scaleMultiplier,
    int collisionMode
)
{
    switch( collisionMode )
    {
        case CollisionBoundsMode :
            return orientedBoxOverlapsAny(
                candidate.evaluationCenter,
                candidate.evaluationAxes,
                candidate.evaluationExtents,
                instances,
                scaleMultiplier
            );
        case CollisionEllipsoidMode :
            return ellipsoidOverlapsAny( candidate.evaluationBound, instances, scaleMultiplier );
        default :
            return false;
    }
}

GeneratedState generatedState( const ScatterPlus *node )
{
    GeneratedState state;

    std::vector<ScenePlug::ScenePath> supportPaths;
    collectCandidateMeshes( node->supportScenePlug(), rootPath(), node->supportPlug()->getValue(), supportPaths );
    if( supportPaths.empty() )
    {
        state.helperPoints = new PointsPrimitive( new V3fVectorData() );
        return state;
    }

    const ScenePlug::ScenePath &supportPath = supportPaths.front();
    ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( node->supportScenePlug()->object( supportPath ).get() );
    if( !mesh )
    {
        state.helperPoints = new PointsPrimitive( new V3fVectorData() );
        return state;
    }

    const Imath::M44f supportTransform = node->supportScenePlug()->fullTransform( supportPath );
    const float referenceFrame = node->referenceFramePlug()->getValue();
    ConstMeshPrimitivePtr referenceMesh = meshAtFrame( node->supportScenePlug(), supportPath, referenceFrame );
    if( !referenceMesh )
    {
        referenceMesh = mesh;
    }
    const Imath::M44f referenceSupportTransform = fullTransformAtFrame( node->supportScenePlug(), supportPath, referenceFrame );
    const int supportSpaceMode = node->supportSpacePlug()->getValue();
    const std::string sourcePath = ScenePlug::pathToString( supportPath );
    const int seed = node->seedPlug()->getValue();
    const float jittering = node->jitteringPlug()->getValue();
    const float densityValue = std::max( node->densityPlug()->getValue(), 0.0f );
    const int pointCount = std::max( 1, static_cast<int>( std::lround( node->pointCountPlug()->getValue() * std::max( densityValue, 0.0f ) ) ) );
    const std::string viewName = ImagePlug::defaultViewName;
    const ImagePlug *inputImage = runTimeCast<const ImagePlug>( node->imagePlug()->getInput() );

    std::vector<SampledPoint> samples;
    switch( node->distributionPlug()->getValue() )
    {
        case PrimitiveCenterDistribution :
            samples = primitiveCenterPoints(
                mesh.get(),
                referenceMesh.get(),
                sourcePath,
                node->uvPlug()->getValue(),
                node->idVariablePlug()->getValue(),
                node->densityPrimitiveVariablePlug()->getValue(),
                seed,
                jittering,
                densityValue,
                densityValue,
                supportTransform,
                referenceSupportTransform,
                supportSpaceMode
            );
            break;
        case ImageDistribution :
        case RandomDistribution :
        default :
            samples = randomSurfacePoints(
                mesh.get(),
                referenceMesh.get(),
                node->uvPlug()->getValue(),
                node->idVariablePlug()->getValue(),
                node->densityPrimitiveVariablePlug()->getValue(),
                pointCount,
                seed,
                jittering,
                densityValue,
                supportPath,
                inputImage,
                viewName,
                supportTransform,
                referenceSupportTransform,
                supportSpaceMode
            );
            break;
    }

    if( node->distributionPlug()->getValue() != ImageDistribution && inputImage )
    {
        for( SampledPoint &sample : samples )
        {
            sample.imageValue = clamp01( sample.imageValue );
            sample.density = densityValue * sample.imageValue;
        }
    }

    if( node->prototypeModePlug()->getValue() == IdPrimitiveVariablePrototypeMode && inputImage )
    {
        for( SampledPoint &sample : samples )
        {
            if( !sample.hasSelectionId )
            {
                sample.selectionId = static_cast<int>( std::lround( clamp01( sample.imageValue ) * 255.0f ) );
                sample.hasSelectionId = true;
            }
        }
    }

    std::vector<ScenePlug::ScenePath> prototypePaths;
    collectPrototypePaths(
        node->prototypesPlug(),
        rootPath(),
        node->prototypeRootsPlug()->getValue(),
        node->prototypeRootsListPlug()->getValue().get(),
        prototypePaths
    );
    if( prototypePaths.empty() )
    {
        state.helperPoints = pointsPrimitiveFromSamples(
            samples,
            node->pointTypePlug()->getValue(),
            node->densityPrimitiveVariablePlug()->getValue(),
            node->referencePositionPlug()->getValue(),
            node->uvPlug()->getValue(),
            node->idVariablePlug()->getValue(),
            node->geometryIdAttributePlug()->getValue(),
            node->probabilityAttributePlug()->getValue()
        );
        state.outputBound = state.helperPoints->bound();
        return state;
    }

    for( const ScenePlug::ScenePath &path : prototypePaths )
    {
        state.prototypes.push_back( prototypeRecord( node->prototypesPlug(), path, node ) );
    }

    std::vector<size_t> instanceOrder( samples.size() );
    for( size_t i = 0; i < samples.size(); ++i )
    {
        instanceOrder[i] = i;
    }

    std::mt19937 collisionRng( static_cast<std::uint32_t>( node->collisionSeedPlug()->getValue() ) );
    if( node->collisionDetectionOrderPlug()->getValue() == CollisionOrderRandom )
    {
        std::shuffle( instanceOrder.begin(), instanceOrder.end(), collisionRng );
    }

    std::mt19937 rng( static_cast<std::uint32_t>( seed ) );
    const bool quantizedVariance = node->varianceModePlug()->getValue() == QuantizedVarianceMode;
    const bool uniformScaleVariance = node->uniformScaleVariancePlug()->getValue();
    const float timeOffset = node->timeOffsetPlug()->getValue();
    const float timeVariance = std::max( 0.0f, node->timeVariancePlug()->getValue() );
    const int timeVarianceSamples = std::max( 1, node->timeVarianceSamplesPlug()->getValue() );
    const bool hasImageInput = inputImage != nullptr;

    state.instances.reserve( samples.size() );
    state.instanceNames.reserve( samples.size() );
    for( const size_t orderedIndex : instanceOrder )
    {
        const SampledPoint &sample = samples[orderedIndex];
        if( node->decimateValuePlug()->getValue() > 0.0f )
        {
            const float score = decimationScore( sample, node, supportTransform, referenceSupportTransform, supportSpaceMode, hasImageInput );
            if( score < node->decimateValuePlug()->getValue() )
            {
                continue;
            }
        }

        size_t prototypeIndex = prototypeIndexForSample( node, state.prototypes, sample, orderedIndex, rng );

        Imath::V3f positionOffset = randomSignedVector( rng, node->positionVariancePlug()->getValue() );
        Imath::V3f rotationOffset = randomSignedVector( rng, node->rotationVariancePlug()->getValue() );
        Imath::V3f scaleOffset = randomSignedVector( rng, node->scaleVariancePlug()->getValue() );
        if( uniformScaleVariance )
        {
            const float scalar = ( scaleOffset.x + scaleOffset.y + scaleOffset.z ) / 3.0f;
            scaleOffset = Imath::V3f( scalar );
        }
        if( quantizedVariance )
        {
            positionOffset = quantizeVector( positionOffset, node->positionVarianceStepPlug()->getValue(), node->positionVariancePlug()->getValue() );
            rotationOffset = quantizeVector( rotationOffset, node->rotationVarianceStepPlug()->getValue(), node->rotationVariancePlug()->getValue() );
            scaleOffset = quantizeVector( scaleOffset, node->scaleVarianceStepPlug()->getValue(), node->scaleVariancePlug()->getValue() );
        }

        const float sampleTimeOffset = sampledTimeOffset( rng, timeOffset, timeVariance, timeVarianceSamples );
        const Imath::V3f scatterPosition = node->positionPlug()->getValue() + positionOffset;
        const Imath::V3f scatterRotation = node->orientationPlug()->getValue() + rotationOffset;
        const Imath::V3f scatterScale = finalScaleFromOffsets( node->scalePlug()->getValue(), scaleOffset );
        const Imath::M44f rotationMatrix = rotationMatrixFromSample(
            sample,
            node->useSupportNormalsPlug()->getValue(),
            node->orientationPlug()->getValue(),
            rotationOffset,
            node->rotationOrderPlug()->getValue()
        );

        InstanceRecord record;
        record.name = instanceName( state.instances.size() );
        record.prototypeIndex = prototypeIndex;
        record.seed = sample.seed;
        record.id = sample.id;
        record.timeOffset = sampleTimeOffset;
        record.sample = sample;
        record.scatterPosition = scatterPosition;
        record.scatterRotation = scatterRotation;
        record.scatterScale = scatterScale;
        record.orientation = Imath::extractQuat( rotationMatrix );
        record.transform = transformFromSample(
            sample,
            node->useSupportNormalsPlug()->getValue(),
            node->positionPlug()->getValue(),
            node->orientationPlug()->getValue(),
            node->scalePlug()->getValue(),
            positionOffset,
            rotationOffset,
            scaleOffset,
            node->rotationOrderPlug()->getValue()
        );
        record.bound = transformedBound( state.prototypes[prototypeIndex].bound, record.transform );
        record.evaluationTransform = convertedTransform(
            record.transform,
            supportTransform,
            referenceSupportTransform,
            supportSpaceMode,
            node->decimationSpacePlug()->getValue()
        );
        orientedBoxFromTransform(
            state.prototypes[prototypeIndex].bound,
            record.evaluationTransform,
            record.evaluationCenter,
            record.evaluationAxes,
            record.evaluationExtents
        );
        record.evaluationBound = transformedBound( state.prototypes[prototypeIndex].bound, record.evaluationTransform );

        if(
            node->collisionModePlug()->getValue() != CollisionOffMode &&
            overlapsAny(
                record,
                state.instances,
                std::max( 0.001f, node->collisionScaleMultiplierPlug()->getValue() ),
                node->collisionModePlug()->getValue()
            )
        )
        {
            continue;
        }

        state.instancesBound.extendBy( record.bound );
        state.instanceNames.push_back( InternedString( record.name ) );
        state.instances.push_back( record );
    }

    state.helperPoints = pointsPrimitiveFromInstances(
        state.instances,
        state.prototypes,
        node->rotationOrderPlug()->getValue(),
        node->pointTypePlug()->getValue(),
        node->densityPrimitiveVariablePlug()->getValue(),
        node->referencePositionPlug()->getValue(),
        node->uvPlug()->getValue(),
        node->idVariablePlug()->getValue(),
        node->geometryIdAttributePlug()->getValue(),
        node->probabilityAttributePlug()->getValue()
    );
    state.outputBound = state.helperPoints->bound();
    state.outputBound.extendBy( state.instancesBound );
    return state;
}

MurmurHash generatedStateHash( const ScatterPlus *node )
{
    MurmurHash h;
    node->outputLocationPlug()->hash( h );
    node->pointTypePlug()->hash( h );
    node->supportPlug()->hash( h );
    node->densityPrimitiveVariablePlug()->hash( h );
    node->referencePositionPlug()->hash( h );
    node->uvPlug()->hash( h );
    node->prototypeRootsPlug()->hash( h );
    node->prototypeRootsListPlug()->hash( h );
    node->distributionPlug()->hash( h );
    node->prototypeModePlug()->hash( h );
    node->prototypeIndexPlug()->hash( h );
    node->idVariablePlug()->hash( h );
    node->geometryIdAttributePlug()->hash( h );
    node->probabilityAttributePlug()->hash( h );
    node->densityPlug()->hash( h );
    node->pointCountPlug()->hash( h );
    node->seedPlug()->hash( h );
    node->varianceModePlug()->hash( h );
    node->jitteringPlug()->hash( h );
    node->positionPlug()->hash( h );
    node->orientationPlug()->hash( h );
    node->scalePlug()->hash( h );
    node->rotationOrderPlug()->hash( h );
    node->useSupportNormalsPlug()->hash( h );
    node->positionVariancePlug()->hash( h );
    node->positionVarianceStepPlug()->hash( h );
    node->rotationVariancePlug()->hash( h );
    node->rotationVarianceStepPlug()->hash( h );
    node->scaleVariancePlug()->hash( h );
    node->scaleVarianceStepPlug()->hash( h );
    node->uniformScaleVariancePlug()->hash( h );
    node->timeOffsetPlug()->hash( h );
    node->timeVariancePlug()->hash( h );
    node->timeVarianceSamplesPlug()->hash( h );
    node->supportSpacePlug()->hash( h );
    node->referenceFramePlug()->hash( h );
    node->decimationSpacePlug()->hash( h );
    node->collisionModePlug()->hash( h );
    node->collisionScaleMultiplierPlug()->hash( h );
    node->collisionDetectionOrderPlug()->hash( h );
    node->collisionSeedPlug()->hash( h );
    node->decimationSeedPlug()->hash( h );
    node->decimateValuePlug()->hash( h );
    hashSceneBranch( node->supportScenePlug(), rootPath(), h );
    h.append( node->supportScenePlug()->globalsHash() );
    hashSceneBranch( node->prototypesPlug(), rootPath(), h );
    h.append( node->prototypesPlug()->globalsHash() );
    if( const ImagePlug *image = runTimeCast<const ImagePlug>( node->imagePlug()->getInput() ) )
    {
        h.append( image->channelNamesHash() );
        h.append( image->dataWindowHash() );
        const Imath::Box2i dataWindow = image->dataWindow();
        if( !dataWindow.isEmpty() )
        {
            h.append( image->channelDataHash( "R", ImagePlug::tileOrigin( dataWindow.min ) ) );
        }
    }
    return h;
}

} // namespace

GAFFER_NODE_DEFINE_TYPE( ScatterPlus );

size_t ScatterPlus::g_firstPlugIndex = 0;

ScatterPlus::ScatterPlus( const std::string &name )
    : SceneProcessor( name, 2, 2 )
{
    storeIndexOfNextChild( g_firstPlugIndex );

    addChild( new ImagePlug( "image" ) );
    addChild( new StringPlug( "outputLocation", Plug::In, "/scatterPlus" ) );
    addChild( new StringPlug( "pointType", Plug::In, "gl:point" ) );
    addChild( new StringPlug( "support", Plug::In, "*" ) );
    addChild( new StringPlug( "densityPrimitiveVariable", Plug::In, "density" ) );
    addChild( new StringPlug( "referencePosition", Plug::In, "referencePosition" ) );
    addChild( new StringPlug( "uv", Plug::In, "uv" ) );
    addChild( new StringPlug( "prototypeRoots", Plug::In, "*" ) );
    addChild( new StringVectorDataPlug( "prototypeRootsList", Plug::In, new StringVectorData() ) );
    addChild( new IntPlug( "distribution", Plug::In, RandomDistribution, RandomDistribution, ImageDistribution ) );
    addChild( new IntPlug( "prototypeMode", Plug::In, RandomPrototypeMode, FirstPrototypeMode, LuminancePrototypeMode ) );
    addChild( new IntPlug( "prototypeIndex", Plug::In, 0, 0 ) );
    addChild( new StringPlug( "idVariable", Plug::In, "id" ) );
    addChild( new StringPlug( "geometryIdAttribute", Plug::In, "geometryId" ) );
    addChild( new StringPlug( "probabilityAttribute", Plug::In, "probability" ) );
    addChild( new FloatPlug( "density", Plug::In, 1.0f, 0.0f ) );
    addChild( new IntPlug( "pointCount", Plug::In, 64, 1 ) );
    addChild( new IntPlug( "seed", Plug::In, 0 ) );
    addChild( new IntPlug( "varianceMode", Plug::In, ContinuousVarianceMode, ContinuousVarianceMode, QuantizedVarianceMode ) );
    addChild( new FloatPlug( "jittering", Plug::In, 0.0f, 0.0f ) );
    addChild( new V3fPlug( "position", Plug::In, Imath::V3f( 0.0f ) ) );
    addChild( new V3fPlug( "orientation", Plug::In, Imath::V3f( 0.0f ) ) );
    addChild( new V3fPlug( "scale", Plug::In, Imath::V3f( 1.0f ), Imath::V3f( 0.001f ) ) );
    addChild( new IntPlug( "rotationOrder", Plug::In, YXZRotationOrder, XYZRotationOrder, ZYXRotationOrder ) );
    addChild( new BoolPlug( "useSupportNormals", Plug::In, true ) );
    addChild( new V3fPlug( "positionVariance", Plug::In, Imath::V3f( 0.0f ), Imath::V3f( 0.0f ) ) );
    addChild( new V3fPlug( "positionVarianceStep", Plug::In, Imath::V3f( 0.0f ), Imath::V3f( 0.0f ) ) );
    addChild( new V3fPlug( "rotationVariance", Plug::In, Imath::V3f( 0.0f ), Imath::V3f( 0.0f ) ) );
    addChild( new V3fPlug( "rotationVarianceStep", Plug::In, Imath::V3f( 0.0f ), Imath::V3f( 0.0f ) ) );
    addChild( new V3fPlug( "scaleVariance", Plug::In, Imath::V3f( 0.0f ), Imath::V3f( 0.0f ) ) );
    addChild( new V3fPlug( "scaleVarianceStep", Plug::In, Imath::V3f( 0.0f ), Imath::V3f( 0.0f ) ) );
    addChild( new BoolPlug( "uniformScaleVariance", Plug::In, true ) );
    addChild( new FloatPlug( "timeOffset", Plug::In, 0.0f ) );
    addChild( new FloatPlug( "timeVariance", Plug::In, 0.0f, 0.0f ) );
    addChild( new IntPlug( "timeVarianceSamples", Plug::In, 1, 1 ) );
    addChild( new IntPlug( "supportSpace", Plug::In, ObjectSpaceMode, ObjectSpaceMode, ReferenceSpaceMode ) );
    addChild( new FloatPlug( "referenceFrame", Plug::In, 0.0f ) );
    addChild( new IntPlug( "decimationSpace", Plug::In, ObjectSpaceMode, ObjectSpaceMode, ReferenceSpaceMode ) );
    addChild( new IntPlug( "collisionMode", Plug::In, CollisionOffMode, CollisionOffMode, CollisionEllipsoidMode ) );
    addChild( new FloatPlug( "collisionScaleMultiplier", Plug::In, 1.0f, 0.001f ) );
    addChild( new IntPlug( "collisionDetectionOrder", Plug::In, CollisionOrderGenerated, CollisionOrderGenerated, CollisionOrderRandom ) );
    addChild( new IntPlug( "collisionSeed", Plug::In, 0 ) );
    addChild( new IntPlug( "decimationSeed", Plug::In, 0 ) );
    addChild( new FloatPlug( "decimateValue", Plug::In, 0.0f, 0.0f, 1.0f ) );
}

ScatterPlus::~ScatterPlus()
{
}

ScenePlug *ScatterPlus::supportScenePlug() { return inPlugs()->getChild<ScenePlug>( 0 ); }
const ScenePlug *ScatterPlus::supportScenePlug() const { return inPlugs()->getChild<ScenePlug>( 0 ); }
ScenePlug *ScatterPlus::prototypesPlug() { return inPlugs()->getChild<ScenePlug>( 1 ); }
const ScenePlug *ScatterPlus::prototypesPlug() const { return inPlugs()->getChild<ScenePlug>( 1 ); }
ImagePlug *ScatterPlus::imagePlug() { return getChild<ImagePlug>( g_firstPlugIndex ); }
const ImagePlug *ScatterPlus::imagePlug() const { return getChild<ImagePlug>( g_firstPlugIndex ); }
StringPlug *ScatterPlus::outputLocationPlug() { return getChild<StringPlug>( g_firstPlugIndex + 1 ); }
const StringPlug *ScatterPlus::outputLocationPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 1 ); }
StringPlug *ScatterPlus::pointTypePlug() { return getChild<StringPlug>( g_firstPlugIndex + 2 ); }
const StringPlug *ScatterPlus::pointTypePlug() const { return getChild<StringPlug>( g_firstPlugIndex + 2 ); }
StringPlug *ScatterPlus::supportPlug() { return getChild<StringPlug>( g_firstPlugIndex + 3 ); }
const StringPlug *ScatterPlus::supportPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 3 ); }
StringPlug *ScatterPlus::densityPrimitiveVariablePlug() { return getChild<StringPlug>( g_firstPlugIndex + 4 ); }
const StringPlug *ScatterPlus::densityPrimitiveVariablePlug() const { return getChild<StringPlug>( g_firstPlugIndex + 4 ); }
StringPlug *ScatterPlus::referencePositionPlug() { return getChild<StringPlug>( g_firstPlugIndex + 5 ); }
const StringPlug *ScatterPlus::referencePositionPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 5 ); }
StringPlug *ScatterPlus::uvPlug() { return getChild<StringPlug>( g_firstPlugIndex + 6 ); }
const StringPlug *ScatterPlus::uvPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 6 ); }
StringPlug *ScatterPlus::prototypeRootsPlug() { return getChild<StringPlug>( g_firstPlugIndex + 7 ); }
const StringPlug *ScatterPlus::prototypeRootsPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 7 ); }
StringVectorDataPlug *ScatterPlus::prototypeRootsListPlug() { return getChild<StringVectorDataPlug>( g_firstPlugIndex + 8 ); }
const StringVectorDataPlug *ScatterPlus::prototypeRootsListPlug() const { return getChild<StringVectorDataPlug>( g_firstPlugIndex + 8 ); }
IntPlug *ScatterPlus::distributionPlug() { return getChild<IntPlug>( g_firstPlugIndex + 9 ); }
const IntPlug *ScatterPlus::distributionPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 9 ); }
IntPlug *ScatterPlus::prototypeModePlug() { return getChild<IntPlug>( g_firstPlugIndex + 10 ); }
const IntPlug *ScatterPlus::prototypeModePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 10 ); }
IntPlug *ScatterPlus::prototypeIndexPlug() { return getChild<IntPlug>( g_firstPlugIndex + 11 ); }
const IntPlug *ScatterPlus::prototypeIndexPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 11 ); }
StringPlug *ScatterPlus::idVariablePlug() { return getChild<StringPlug>( g_firstPlugIndex + 12 ); }
const StringPlug *ScatterPlus::idVariablePlug() const { return getChild<StringPlug>( g_firstPlugIndex + 12 ); }
StringPlug *ScatterPlus::geometryIdAttributePlug() { return getChild<StringPlug>( g_firstPlugIndex + 13 ); }
const StringPlug *ScatterPlus::geometryIdAttributePlug() const { return getChild<StringPlug>( g_firstPlugIndex + 13 ); }
StringPlug *ScatterPlus::probabilityAttributePlug() { return getChild<StringPlug>( g_firstPlugIndex + 14 ); }
const StringPlug *ScatterPlus::probabilityAttributePlug() const { return getChild<StringPlug>( g_firstPlugIndex + 14 ); }
FloatPlug *ScatterPlus::densityPlug() { return getChild<FloatPlug>( g_firstPlugIndex + 15 ); }
const FloatPlug *ScatterPlus::densityPlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 15 ); }
IntPlug *ScatterPlus::pointCountPlug() { return getChild<IntPlug>( g_firstPlugIndex + 16 ); }
const IntPlug *ScatterPlus::pointCountPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 16 ); }
IntPlug *ScatterPlus::seedPlug() { return getChild<IntPlug>( g_firstPlugIndex + 17 ); }
const IntPlug *ScatterPlus::seedPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 17 ); }
IntPlug *ScatterPlus::varianceModePlug() { return getChild<IntPlug>( g_firstPlugIndex + 18 ); }
const IntPlug *ScatterPlus::varianceModePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 18 ); }
FloatPlug *ScatterPlus::jitteringPlug() { return getChild<FloatPlug>( g_firstPlugIndex + 19 ); }
const FloatPlug *ScatterPlus::jitteringPlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 19 ); }
V3fPlug *ScatterPlus::positionPlug() { return getChild<V3fPlug>( g_firstPlugIndex + 20 ); }
const V3fPlug *ScatterPlus::positionPlug() const { return getChild<V3fPlug>( g_firstPlugIndex + 20 ); }
V3fPlug *ScatterPlus::orientationPlug() { return getChild<V3fPlug>( g_firstPlugIndex + 21 ); }
const V3fPlug *ScatterPlus::orientationPlug() const { return getChild<V3fPlug>( g_firstPlugIndex + 21 ); }
V3fPlug *ScatterPlus::scalePlug() { return getChild<V3fPlug>( g_firstPlugIndex + 22 ); }
const V3fPlug *ScatterPlus::scalePlug() const { return getChild<V3fPlug>( g_firstPlugIndex + 22 ); }
IntPlug *ScatterPlus::rotationOrderPlug() { return getChild<IntPlug>( g_firstPlugIndex + 23 ); }
const IntPlug *ScatterPlus::rotationOrderPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 23 ); }
BoolPlug *ScatterPlus::useSupportNormalsPlug() { return getChild<BoolPlug>( g_firstPlugIndex + 24 ); }
const BoolPlug *ScatterPlus::useSupportNormalsPlug() const { return getChild<BoolPlug>( g_firstPlugIndex + 24 ); }
V3fPlug *ScatterPlus::positionVariancePlug() { return getChild<V3fPlug>( g_firstPlugIndex + 25 ); }
const V3fPlug *ScatterPlus::positionVariancePlug() const { return getChild<V3fPlug>( g_firstPlugIndex + 25 ); }
V3fPlug *ScatterPlus::positionVarianceStepPlug() { return getChild<V3fPlug>( g_firstPlugIndex + 26 ); }
const V3fPlug *ScatterPlus::positionVarianceStepPlug() const { return getChild<V3fPlug>( g_firstPlugIndex + 26 ); }
V3fPlug *ScatterPlus::rotationVariancePlug() { return getChild<V3fPlug>( g_firstPlugIndex + 27 ); }
const V3fPlug *ScatterPlus::rotationVariancePlug() const { return getChild<V3fPlug>( g_firstPlugIndex + 27 ); }
V3fPlug *ScatterPlus::rotationVarianceStepPlug() { return getChild<V3fPlug>( g_firstPlugIndex + 28 ); }
const V3fPlug *ScatterPlus::rotationVarianceStepPlug() const { return getChild<V3fPlug>( g_firstPlugIndex + 28 ); }
V3fPlug *ScatterPlus::scaleVariancePlug() { return getChild<V3fPlug>( g_firstPlugIndex + 29 ); }
const V3fPlug *ScatterPlus::scaleVariancePlug() const { return getChild<V3fPlug>( g_firstPlugIndex + 29 ); }
V3fPlug *ScatterPlus::scaleVarianceStepPlug() { return getChild<V3fPlug>( g_firstPlugIndex + 30 ); }
const V3fPlug *ScatterPlus::scaleVarianceStepPlug() const { return getChild<V3fPlug>( g_firstPlugIndex + 30 ); }
BoolPlug *ScatterPlus::uniformScaleVariancePlug() { return getChild<BoolPlug>( g_firstPlugIndex + 31 ); }
const BoolPlug *ScatterPlus::uniformScaleVariancePlug() const { return getChild<BoolPlug>( g_firstPlugIndex + 31 ); }
FloatPlug *ScatterPlus::timeOffsetPlug() { return getChild<FloatPlug>( g_firstPlugIndex + 32 ); }
const FloatPlug *ScatterPlus::timeOffsetPlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 32 ); }
FloatPlug *ScatterPlus::timeVariancePlug() { return getChild<FloatPlug>( g_firstPlugIndex + 33 ); }
const FloatPlug *ScatterPlus::timeVariancePlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 33 ); }
IntPlug *ScatterPlus::timeVarianceSamplesPlug() { return getChild<IntPlug>( g_firstPlugIndex + 34 ); }
const IntPlug *ScatterPlus::timeVarianceSamplesPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 34 ); }
IntPlug *ScatterPlus::supportSpacePlug() { return getChild<IntPlug>( g_firstPlugIndex + 35 ); }
const IntPlug *ScatterPlus::supportSpacePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 35 ); }
FloatPlug *ScatterPlus::referenceFramePlug() { return getChild<FloatPlug>( g_firstPlugIndex + 36 ); }
const FloatPlug *ScatterPlus::referenceFramePlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 36 ); }
IntPlug *ScatterPlus::decimationSpacePlug() { return getChild<IntPlug>( g_firstPlugIndex + 37 ); }
const IntPlug *ScatterPlus::decimationSpacePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 37 ); }
IntPlug *ScatterPlus::collisionModePlug() { return getChild<IntPlug>( g_firstPlugIndex + 38 ); }
const IntPlug *ScatterPlus::collisionModePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 38 ); }
FloatPlug *ScatterPlus::collisionScaleMultiplierPlug() { return getChild<FloatPlug>( g_firstPlugIndex + 39 ); }
const FloatPlug *ScatterPlus::collisionScaleMultiplierPlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 39 ); }
IntPlug *ScatterPlus::collisionDetectionOrderPlug() { return getChild<IntPlug>( g_firstPlugIndex + 40 ); }
const IntPlug *ScatterPlus::collisionDetectionOrderPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 40 ); }
IntPlug *ScatterPlus::collisionSeedPlug() { return getChild<IntPlug>( g_firstPlugIndex + 41 ); }
const IntPlug *ScatterPlus::collisionSeedPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 41 ); }
IntPlug *ScatterPlus::decimationSeedPlug() { return getChild<IntPlug>( g_firstPlugIndex + 42 ); }
const IntPlug *ScatterPlus::decimationSeedPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 42 ); }
FloatPlug *ScatterPlus::decimateValuePlug() { return getChild<FloatPlug>( g_firstPlugIndex + 43 ); }
const FloatPlug *ScatterPlus::decimateValuePlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 43 ); }

void ScatterPlus::affects( const Plug *input, AffectedPlugsContainer &outputs ) const
{
    SceneProcessor::affects( input, outputs );

    const bool affectsCompoundInput =
        input->parent() == positionPlug() ||
        input->parent() == orientationPlug() ||
        input->parent() == scalePlug() ||
        input->parent() == positionVariancePlug() ||
        input->parent() == positionVarianceStepPlug() ||
        input->parent() == rotationVariancePlug() ||
        input->parent() == rotationVarianceStepPlug() ||
        input->parent() == scaleVariancePlug() ||
        input->parent() == scaleVarianceStepPlug();

    if(
        input == supportScenePlug() ||
        input == prototypesPlug() ||
        input == imagePlug() ||
        input->ancestor<ImagePlug>() == imagePlug() ||
        input == outputLocationPlug() ||
        input == pointTypePlug() ||
        input == supportPlug() ||
        input == densityPrimitiveVariablePlug() ||
        input == referencePositionPlug() ||
        input == uvPlug() ||
        input == prototypeRootsPlug() ||
        input == prototypeRootsListPlug() ||
        input == distributionPlug() ||
        input == prototypeModePlug() ||
        input == prototypeIndexPlug() ||
        input == idVariablePlug() ||
        input == geometryIdAttributePlug() ||
        input == probabilityAttributePlug() ||
        input == densityPlug() ||
        input == pointCountPlug() ||
        input == seedPlug() ||
        input == varianceModePlug() ||
        input == jitteringPlug() ||
        input == positionPlug() ||
        input == orientationPlug() ||
        input == scalePlug() ||
        affectsCompoundInput ||
        input == rotationOrderPlug() ||
        input == useSupportNormalsPlug() ||
        input == positionVariancePlug() ||
        input == positionVarianceStepPlug() ||
        input == rotationVariancePlug() ||
        input == rotationVarianceStepPlug() ||
        input == scaleVariancePlug() ||
        input == scaleVarianceStepPlug() ||
        input == uniformScaleVariancePlug() ||
        input == timeOffsetPlug() ||
        input == timeVariancePlug() ||
        input == timeVarianceSamplesPlug() ||
        input == supportSpacePlug() ||
        input == referenceFramePlug() ||
        input == decimationSpacePlug() ||
        input == collisionModePlug() ||
        input == collisionScaleMultiplierPlug() ||
        input == collisionDetectionOrderPlug() ||
        input == collisionSeedPlug() ||
        input == decimationSeedPlug() ||
        input == decimateValuePlug()
    )
    {
        outputs.push_back( outPlug()->boundPlug() );
        outputs.push_back( outPlug()->transformPlug() );
        outputs.push_back( outPlug()->attributesPlug() );
        outputs.push_back( outPlug()->objectPlug() );
        outputs.push_back( outPlug()->childNamesPlug() );
        outputs.push_back( outPlug()->globalsPlug() );
    }
}

void ScatterPlus::hash( const ValuePlug *output, const Context *context, MurmurHash &h ) const
{
    SceneProcessor::hash( output, context, h );
}

void ScatterPlus::compute( ValuePlug *output, const Context *context ) const
{
    SceneProcessor::compute( output, context );
}

void ScatterPlus::hashBound( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
    if( isWithinGeneratedTree( this, path ) || isGeneratedAncestor( this, path ) )
    {
        h = generatedStateHash( this );
        h.append( ScenePlug::pathToString( path ) );
        h.append( "ScatterPlusBound" );
        return;
    }
    h = supportScenePlug()->boundHash( path );
}

void ScatterPlus::hashTransform( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
    if( isWithinGeneratedTree( this, path ) )
    {
        h = generatedStateHash( this );
        h.append( ScenePlug::pathToString( path ) );
        h.append( "ScatterPlusTransform" );
        return;
    }
    h = supportScenePlug()->transformHash( path );
}

void ScatterPlus::hashAttributes( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
    if( isWithinGeneratedTree( this, path ) )
    {
        h = generatedStateHash( this );
        h.append( ScenePlug::pathToString( path ) );
        h.append( "ScatterPlusAttributes" );
        return;
    }
    h = supportScenePlug()->attributesHash( path );
}

void ScatterPlus::hashObject( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
    if( isWithinGeneratedTree( this, path ) )
    {
        h = generatedStateHash( this );
        h.append( ScenePlug::pathToString( path ) );
        h.append( "ScatterPlusObject" );
        return;
    }
    h = supportScenePlug()->objectHash( path );
}

void ScatterPlus::hashChildNames( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
    const ScenePath outputRoot = outputRootPath( this );
    if( isPrefixPath( path, outputRoot ) || isWithinGeneratedTree( this, path ) )
    {
        h = generatedStateHash( this );
        h.append( ScenePlug::pathToString( path ) );
        h.append( "ScatterPlusChildNames" );
        return;
    }
    h = supportScenePlug()->childNamesHash( path );
}

void ScatterPlus::hashGlobals( const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
    h = supportScenePlug()->globalsHash();
}

void ScatterPlus::hashSetNames( const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
    h = supportScenePlug()->setNamesHash();
}

void ScatterPlus::hashSet( const InternedString &setName, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
    h = supportScenePlug()->setHash( setName );
}

Imath::Box3f ScatterPlus::computeBound( const ScenePath &path, const Context *context, const ScenePlug *parent ) const
{
    const GeneratedState state = generatedState( this );
    if( isPointsLeaf( this, path ) )
    {
        return state.helperPoints ? state.helperPoints->bound() : Imath::Box3f();
    }
    if( isInstancesRoot( this, path ) )
    {
        return state.instancesBound;
    }
    if( isInstanceLeaf( this, path ) )
    {
        size_t index = 0;
        if( parseInstanceIndex( path, index ) && index < state.instances.size() )
        {
            return state.instances[index].bound;
        }
        return Imath::Box3f();
    }
    if( isOutputRoot( this, path ) )
    {
        return state.outputBound;
    }
    if( isGeneratedAncestor( this, path ) )
    {
        Imath::Box3f bound = supportScenePlug()->bound( path );
        bound.extendBy( state.outputBound );
        return bound;
    }
    return supportScenePlug()->bound( path );
}

Imath::M44f ScatterPlus::computeTransform( const ScenePath &path, const Context *context, const ScenePlug *parent ) const
{
    if( isInstanceLeaf( this, path ) )
    {
        const GeneratedState state = generatedState( this );
        size_t index = 0;
        if( parseInstanceIndex( path, index ) && index < state.instances.size() )
        {
            return state.instances[index].transform;
        }
        return Imath::M44f();
    }
    if( isWithinGeneratedTree( this, path ) )
    {
        Imath::M44f identity;
        identity.makeIdentity();
        return identity;
    }
    return supportScenePlug()->transform( path );
}

ConstCompoundObjectPtr ScatterPlus::computeAttributes( const ScenePath &path, const Context *context, const ScenePlug *parent ) const
{
    if( isInstanceLeaf( this, path ) )
    {
        const GeneratedState state = generatedState( this );
        size_t index = 0;
        if( parseInstanceIndex( path, index ) && index < state.instances.size() )
        {
            const InstanceRecord &instance = state.instances[index];
            const PrototypeRecord &prototype = state.prototypes[instance.prototypeIndex];
            CompoundObjectPtr attrs = prototype.attributes ? prototype.attributes->copy() : new CompoundObject();
            attrs->members()["scatterPlus:prototypePath"] = new StringData( prototype.pathString );
            attrs->members()["scatterPlus:prototypeIndex"] = new IntData( static_cast<int>( instance.prototypeIndex ) );
            attrs->members()["scatterPlus:seed"] = new IntData( instance.seed );
            attrs->members()["scatterPlus:id"] = new IntData( instance.id );
            attrs->members()["scatterPlus:timeOffset"] = new FloatData( instance.timeOffset );
            return attrs;
        }
        return new CompoundObject();
    }

    if( isPointsLeaf( this, path ) )
    {
        CompoundObjectPtr attrs = new CompoundObject();
        attrs->members()["scatterPlus:branch"] = new StringData( "points" );
        return attrs;
    }

    if( isWithinGeneratedTree( this, path ) )
    {
        return new CompoundObject();
    }
    return supportScenePlug()->attributes( path );
}

ConstObjectPtr ScatterPlus::computeObject( const ScenePath &path, const Context *context, const ScenePlug *parent ) const
{
    const GeneratedState state = generatedState( this );
    if( isPointsLeaf( this, path ) )
    {
        return state.helperPoints;
    }
    if( isInstanceLeaf( this, path ) )
    {
        size_t index = 0;
        if( parseInstanceIndex( path, index ) && index < state.instances.size() )
        {
            return state.prototypes[state.instances[index].prototypeIndex].object;
        }
    }
    if( isSyntheticBranch( supportScenePlug(), this, path ) )
    {
        return IECore::NullObject::defaultNullObject();
    }
    return supportScenePlug()->object( path );
}

ConstInternedStringVectorDataPtr ScatterPlus::computeChildNames( const ScenePath &path, const Context *context, const ScenePlug *parent ) const
{
    if( isPointsLeaf( this, path ) || isInstanceLeaf( this, path ) )
    {
        return new InternedStringVectorData();
    }

    if( isInstancesRoot( this, path ) )
    {
        const GeneratedState state = generatedState( this );
        InternedStringVectorDataPtr result = new InternedStringVectorData();
        result->writable() = state.instanceNames;
        return result;
    }

    if( isOutputRoot( this, path ) )
    {
        ConstInternedStringVectorDataPtr inputNames = inputChildNamesForPath( supportScenePlug(), path );
        InternedStringVectorDataPtr result = new InternedStringVectorData();
        result->writable() = inputNames ? inputNames->readable() : std::vector<InternedString>();
        for( const InternedString child : { InternedString( "points" ), InternedString( "instances" ) } )
        {
            if( std::find( result->writable().begin(), result->writable().end(), child ) == result->writable().end() )
            {
                result->writable().push_back( child );
            }
        }
        return result;
    }

    if( isGeneratedAncestor( this, path ) )
    {
        ConstInternedStringVectorDataPtr inputNames = inputChildNamesForPath( supportScenePlug(), path );
        InternedStringVectorDataPtr result = new InternedStringVectorData();
        result->writable() = inputNames ? inputNames->readable() : std::vector<InternedString>();
        const std::vector<std::string> outputComponents = splitLocationComponents( outputLocationPlug()->getValue() );
        const std::vector<std::string> pathComponents = splitLocationComponents( ScenePlug::pathToString( path ) );
        if( pathComponents.size() < outputComponents.size() )
        {
            const InternedString nextChild( outputComponents[pathComponents.size()] );
            if( std::find( result->writable().begin(), result->writable().end(), nextChild ) == result->writable().end() )
            {
                result->writable().push_back( nextChild );
            }
        }
        return result;
    }

    return supportScenePlug()->childNames( path );
}

ConstCompoundObjectPtr ScatterPlus::computeGlobals( const Context *context, const ScenePlug *parent ) const
{
    return supportScenePlug()->globals();
}

ConstInternedStringVectorDataPtr ScatterPlus::computeSetNames( const Context *context, const ScenePlug *parent ) const
{
    return supportScenePlug()->setNames();
}

ConstPathMatcherDataPtr ScatterPlus::computeSet( const InternedString &setName, const Context *context, const ScenePlug *parent ) const
{
    return supportScenePlug()->set( setName );
}
