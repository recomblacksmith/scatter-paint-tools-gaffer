#include "GafferPointCloudPlus/PointCloudPlus.h"

#include "Gaffer/Context.h"
#include "Gaffer/StringPlug.h"

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
#include "Imath/ImathVec.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace Gaffer;
using namespace GafferPointCloudPlus;
using namespace GafferScene;
using namespace IECore;
using namespace IECoreScene;

namespace
{

enum Mode
{
    GeometryMode = 0,
    FileMode = 1,
};

enum Distribution
{
    RandomDistribution = 0,
    PrimitiveCenterDistribution = 1,
};

enum AnimationBehavior
{
    HoldBehavior = 0,
    RepeatBehavior = 1,
};

ScenePlug::ScenePath rootPath()
{
    return ScenePlug::ScenePath();
}

ScenePlug::ScenePath outputPath( const PointCloudPlus *node )
{
    return ScenePlug::stringToPath(
        node->outputLocationPlug()->getValue().empty()
            ? "/pointCloud"
            : node->outputLocationPlug()->getValue()
    );
}

bool isPrefixPath( const ScenePlug::ScenePath &path, const ScenePlug::ScenePath &candidate )
{
    if( path.size() > candidate.size() )
    {
        return false;
    }
    for( size_t i = 0; i < path.size(); ++i )
    {
        if( path[i] != candidate[i] )
        {
            return false;
        }
    }
    return true;
}

bool isGeneratedPath( const ScenePlug::ScenePath &path, const ScenePlug::ScenePath &generatedPath )
{
    return isPrefixPath( path, generatedPath );
}

bool isInputPath( const ScenePlug *scene, const ScenePlug::ScenePath &path )
{
    return scene->exists( path );
}

bool isSyntheticBranch( const ScenePlug *scene, const ScenePlug::ScenePath &path, const ScenePlug::ScenePath &generatedPath )
{
    return isGeneratedPath( path, generatedPath ) && !isInputPath( scene, path );
}

bool isGeneratedLeaf( const ScenePlug::ScenePath &path, const ScenePlug::ScenePath &generatedPath )
{
    return path == generatedPath;
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

float fileModeEvaluationFrame( const PointCloudPlus *node, const Context *context )
{
    const float baseFrame = node->framePlug()->getValue() + node->frameOffsetPlug()->getValue();
    if( node->animationBehaviorPlug()->getValue() == RepeatBehavior )
    {
        return context->getFrame() + baseFrame;
    }
    return baseFrame;
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

std::vector<std::string> splitTokens( const std::string &filter )
{
    std::vector<std::string> tokens;
    StringAlgo::tokenize( filter, ' ', tokens );
    std::vector<std::string> output;
    output.reserve( tokens.size() );
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
    const std::vector<InternedString> &children = childNames->readable();
    for( const InternedString &child : children )
    {
        ScenePlug::ScenePath childPath = path;
        childPath.push_back( child );
        collectCandidateMeshes( scene, childPath, filter, paths );
    }
}

void collectCandidatePoints(
    const ScenePlug *scene,
    const ScenePlug::ScenePath &path,
    std::vector<ScenePlug::ScenePath> &paths
)
{
    if( !path.empty() )
    {
        ConstObjectPtr object = scene->object( path );
        if( runTimeCast<const PointsPrimitive>( object.get() ) )
        {
            paths.push_back( path );
        }
    }

    ConstInternedStringVectorDataPtr childNames = scene->childNames( path );
    const std::vector<InternedString> &children = childNames->readable();
    for( const InternedString &child : children )
    {
        ScenePlug::ScenePath childPath = path;
        childPath.push_back( child );
        collectCandidatePoints( scene, childPath, paths );
    }
}

V3fVectorDataPtr v3fVectorData( const std::vector<Imath::V3f> &values, GeometricData::Interpretation interpretation )
{
    V3fVectorDataPtr result = new V3fVectorData();
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

StringVectorDataPtr stringVectorData( const std::vector<std::string> &values )
{
    StringVectorDataPtr result = new StringVectorData();
    result->writable() = values;
    return result;
}

Imath::V3f trianglePoint(
    const Imath::V3f &a,
    const Imath::V3f &b,
    const Imath::V3f &c,
    const Imath::V3f &barycentric
)
{
    return ( a * barycentric.x ) + ( b * barycentric.y ) + ( c * barycentric.z );
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

struct SampledPoint
{
    Imath::V3f position = Imath::V3f( 0.0f );
    Imath::V3f normal = Imath::V3f( 0.0f, 1.0f, 0.0f );
    std::string sourcePath;
    int seed = 0;
    float width = 0.1f;
};

float meshArea( const MeshPrimitive *mesh )
{
    ConstV3fVectorDataPtr pData = mesh->variableData<V3fVectorData>( "P", PrimitiveVariable::Vertex );
    ConstIntVectorDataPtr verticesPerFace = mesh->verticesPerFace();
    ConstIntVectorDataPtr vertexIds = mesh->vertexIds();
    if( !pData || !verticesPerFace || !vertexIds )
    {
        return 0.0f;
    }

    const std::vector<Imath::V3f> &points = pData->readable();
    const std::vector<int> &counts = verticesPerFace->readable();
    const std::vector<int> &ids = vertexIds->readable();

    size_t offset = 0;
    float totalArea = 0.0f;
    for( int count : counts )
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
            totalArea += 0.5f * std::sqrt( std::max( crossValue.dot( crossValue ), 0.0f ) );
        }

        offset += static_cast<size_t>( count );
    }

    return totalArea;
}

std::vector<SampledPoint> primitiveCenterPoints(
    const MeshPrimitive *mesh,
    const std::string &sourcePath,
    int seed,
    float jittering
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

    const std::vector<Imath::V3f> &points = pData->readable();
    const std::vector<int> &counts = verticesPerFace->readable();
    const std::vector<int> &ids = vertexIds->readable();
    std::mt19937 rng( static_cast<std::uint32_t>( seed ) );
    std::uniform_real_distribution<float> unit( -1.0f, 1.0f );

    size_t offset = 0;
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
        if( jittering > 0.0f )
        {
            center += Imath::V3f( unit( rng ), unit( rng ), unit( rng ) ) * jittering;
        }

        SampledPoint point;
        point.position = center;
        point.normal = normal;
        point.sourcePath = sourcePath;
        point.seed = seed + static_cast<int>( faceIndex );
        result.push_back( point );
        offset += static_cast<size_t>( count );
    }

    return result;
}

std::vector<SampledPoint> randomSurfacePoints(
    const MeshPrimitive *mesh,
    const std::string &sourcePath,
    int pointCount,
    int seed,
    float jittering
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

    struct Triangle
    {
        Imath::V3f a;
        Imath::V3f b;
        Imath::V3f c;
        Imath::V3f normal;
        float area = 0.0f;
    };

    const std::vector<Imath::V3f> &points = pData->readable();
    const std::vector<int> &counts = verticesPerFace->readable();
    const std::vector<int> &ids = vertexIds->readable();
    std::vector<Triangle> triangles;
    std::vector<float> cumulative;

    size_t offset = 0;
    float totalArea = 0.0f;
    for( int count : counts )
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
            triangle.area = area;
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
    for( int i = 0; i < pointCount; ++i )
    {
        const float areaSample = unit( rng ) * totalArea;
        const auto cumulativeIt = std::lower_bound( cumulative.begin(), cumulative.end(), areaSample );
        const size_t triangleIndex = std::min<size_t>(
            static_cast<size_t>( std::distance( cumulative.begin(), cumulativeIt ) ),
            triangles.size() - 1
        );
        const Triangle &triangle = triangles[triangleIndex];

        const float r1 = unit( rng );
        const float r2 = unit( rng );
        const float u = std::sqrt( r1 );
        const Imath::V3f barycentric(
            1.0f - u,
            u * ( 1.0f - r2 ),
            u * r2
        );

        SampledPoint point;
        point.position = trianglePoint( triangle.a, triangle.b, triangle.c, barycentric );
        point.normal = triangle.normal;
        point.sourcePath = sourcePath;
        point.seed = seed + i;
        if( jittering > 0.0f )
        {
            point.position += triangle.normal * ( ( unit( rng ) * 2.0f - 1.0f ) * jittering );
        }
        result.push_back( point );
    }

    return result;
}

PointsPrimitivePtr pointsPrimitiveFromSamples( const std::vector<SampledPoint> &samples, const std::string &pointType )
{
    std::vector<Imath::V3f> positions;
    std::vector<Imath::V3f> normals;
    std::vector<Imath::Color3f> colors;
    std::vector<float> widths;
    std::vector<int64_t> ids;
    std::vector<int> seeds;
    std::vector<std::string> sourcePaths;
    const Imath::Color3f displayColor( 0.0f, 0.0f, 120.0f / 255.0f );
    positions.reserve( samples.size() );
    normals.reserve( samples.size() );
    colors.reserve( samples.size() );
    widths.reserve( samples.size() );
    ids.reserve( samples.size() );
    seeds.reserve( samples.size() );
    sourcePaths.reserve( samples.size() );

    for( size_t i = 0; i < samples.size(); ++i )
    {
        const SampledPoint &sample = samples[i];
        positions.push_back( sample.position );
        normals.push_back( sample.normal );
        colors.push_back( displayColor );
        widths.push_back( sample.width );
        ids.push_back( static_cast<int64_t>( i ) );
        seeds.push_back( sample.seed );
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
    return primitive;
}

ConstPointsPrimitivePtr geometryModePoints( const PointCloudPlus *node )
{
    std::vector<ScenePlug::ScenePath> candidatePaths;
    collectCandidateMeshes( node->inPlug(), ScenePlug::ScenePath(), node->filterPlug()->getValue(), candidatePaths );
    if( candidatePaths.empty() )
    {
        return new PointsPrimitive( new V3fVectorData() );
    }

    const int seed = node->distributionSeedPlug()->getValue();
    const float jittering = node->jitteringPlug()->getValue();
    std::vector<SampledPoint> samples;
    if( node->distributionPlug()->getValue() == PrimitiveCenterDistribution )
    {
        for( size_t i = 0; i < candidatePaths.size(); ++i )
        {
            ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( node->inPlug()->object( candidatePaths[i] ).get() );
            if( !mesh )
            {
                continue;
            }

            std::vector<SampledPoint> meshSamples = primitiveCenterPoints(
                mesh.get(),
                ScenePlug::pathToString( candidatePaths[i] ),
                seed + static_cast<int>( i * 1000 ),
                jittering
            );
            samples.insert( samples.end(), meshSamples.begin(), meshSamples.end() );
        }
    }
    else
    {
        std::vector<ConstMeshPrimitivePtr> meshes;
        std::vector<ScenePlug::ScenePath> meshPaths;
        std::vector<float> areas;
        float totalArea = 0.0f;

        for( const ScenePlug::ScenePath &candidatePath : candidatePaths )
        {
            ConstMeshPrimitivePtr mesh = runTimeCast<const MeshPrimitive>( node->inPlug()->object( candidatePath ).get() );
            if( !mesh )
            {
                continue;
            }

            const float area = meshArea( mesh.get() );
            if( area <= 0.0f )
            {
                continue;
            }

            meshes.push_back( mesh );
            meshPaths.push_back( candidatePath );
            areas.push_back( area );
            totalArea += area;
        }

        if( meshes.empty() || totalArea <= 0.0f )
        {
            return new PointsPrimitive( new V3fVectorData() );
        }

        int pointCount = node->pointCountPlug()->getValue();
        if( node->useDensityPlug()->getValue() )
        {
            pointCount = std::max( 1, static_cast<int>( std::lround( node->densityPlug()->getValue() * totalArea ) ) );
        }

        std::vector<int> meshPointCounts( meshes.size(), 0 );
        std::vector<std::pair<float, size_t>> remainders;
        int assigned = 0;
        for( size_t i = 0; i < meshes.size(); ++i )
        {
            const float exactCount = ( areas[i] / totalArea ) * static_cast<float>( pointCount );
            const int count = static_cast<int>( std::floor( exactCount ) );
            meshPointCounts[i] = count;
            assigned += count;
            remainders.emplace_back( exactCount - static_cast<float>( count ), i );
        }

        std::sort(
            remainders.begin(),
            remainders.end(),
            []( const std::pair<float, size_t> &a, const std::pair<float, size_t> &b ) {
                if( a.first == b.first )
                {
                    return a.second < b.second;
                }
                return a.first > b.first;
            }
        );

        for( int i = 0; i < pointCount - assigned && i < static_cast<int>( remainders.size() ); ++i )
        {
            meshPointCounts[remainders[i].second] += 1;
        }

        for( size_t i = 0; i < meshes.size(); ++i )
        {
            if( meshPointCounts[i] <= 0 )
            {
                continue;
            }

            std::vector<SampledPoint> meshSamples = randomSurfacePoints(
                meshes[i].get(),
                ScenePlug::pathToString( meshPaths[i] ),
                meshPointCounts[i],
                seed + static_cast<int>( i * 1000 ),
                jittering
            );
            samples.insert( samples.end(), meshSamples.begin(), meshSamples.end() );
        }
    }

    return pointsPrimitiveFromSamples( samples, node->pointTypePlug()->getValue() );
}

ScenePlug::ScenePath fileInputPath( const PointCloudPlus *node )
{
    if( !node->primPathPlug()->getValue().empty() )
    {
        return ScenePlug::stringToPath( node->primPathPlug()->getValue() );
    }

    std::vector<ScenePlug::ScenePath> candidatePaths;
    collectCandidatePoints( node->inPlug(), ScenePlug::ScenePath(), candidatePaths );
    return candidatePaths.empty() ? ScenePlug::ScenePath() : candidatePaths.front();
}

ConstPointsPrimitivePtr fileModePoints( const PointCloudPlus *node )
{
    const ScenePlug::ScenePath inputPath = fileInputPath( node );
    if( inputPath.empty() )
    {
        return new PointsPrimitive( new V3fVectorData() );
    }

    ConstPointsPrimitivePtr points = runTimeCast<const PointsPrimitive>( node->inPlug()->object( inputPath ).get() );
    if( !points )
    {
        return new PointsPrimitive( new V3fVectorData() );
    }

    PointsPrimitivePtr result = runTimeCast<PointsPrimitive>( points->copy() );
    result->variables["type"] = PrimitiveVariable( PrimitiveVariable::Constant, new StringData( node->pointTypePlug()->getValue() ) );
    return result;
}

ConstPointsPrimitivePtr outputPoints( const PointCloudPlus *node, const Context *context )
{
    if( node->modePlug()->getValue() == FileMode )
    {
        Context::EditableScope scope( context );
        scope.setFrame( fileModeEvaluationFrame( node, context ) );
        return fileModePoints( node );
    }
    return geometryModePoints( node );
}

} // namespace

GAFFER_NODE_DEFINE_TYPE( PointCloudPlus );

size_t PointCloudPlus::g_firstPlugIndex = 0;

PointCloudPlus::PointCloudPlus( const std::string &name )
    : SceneProcessor( name )
{
    storeIndexOfNextChild( g_firstPlugIndex );

    addChild( new StringPlug( "outputLocation", Plug::In, "/pointCloud" ) );
    addChild( new StringPlug( "pointType", Plug::In, "gl:point" ) );
    addChild( new IntPlug( "mode", Plug::In, GeometryMode, GeometryMode, FileMode ) );
    addChild( new StringPlug( "filter", Plug::In, "*" ) );
    addChild( new IntPlug( "distribution", Plug::In, RandomDistribution, RandomDistribution, PrimitiveCenterDistribution ) );
    addChild( new BoolPlug( "useDensity", Plug::In, false ) );
    addChild( new FloatPlug( "density", Plug::In, 1.0f, 0.0f ) );
    addChild( new IntPlug( "pointCount", Plug::In, 100, 1 ) );
    addChild( new IntPlug( "distributionSeed", Plug::In, 0 ) );
    addChild( new FloatPlug( "jittering", Plug::In, 0.0f, 0.0f ) );
    addChild( new StringPlug( "fileName", Plug::In, "" ) );
    addChild( new StringPlug( "primPath", Plug::In, "" ) );
    addChild( new StringPlug( "purpose", Plug::In, "default" ) );
    addChild( new FloatPlug( "frame", Plug::In, 0.0f ) );
    addChild( new FloatPlug( "frameOffset", Plug::In, 0.0f ) );
    addChild( new IntPlug( "animationBehavior", Plug::In, HoldBehavior, HoldBehavior, RepeatBehavior ) );
}

PointCloudPlus::~PointCloudPlus()
{
}

StringPlug *PointCloudPlus::outputLocationPlug() { return getChild<StringPlug>( g_firstPlugIndex ); }
const StringPlug *PointCloudPlus::outputLocationPlug() const { return getChild<StringPlug>( g_firstPlugIndex ); }
StringPlug *PointCloudPlus::pointTypePlug() { return getChild<StringPlug>( g_firstPlugIndex + 1 ); }
const StringPlug *PointCloudPlus::pointTypePlug() const { return getChild<StringPlug>( g_firstPlugIndex + 1 ); }
IntPlug *PointCloudPlus::modePlug() { return getChild<IntPlug>( g_firstPlugIndex + 2 ); }
const IntPlug *PointCloudPlus::modePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 2 ); }
StringPlug *PointCloudPlus::filterPlug() { return getChild<StringPlug>( g_firstPlugIndex + 3 ); }
const StringPlug *PointCloudPlus::filterPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 3 ); }
IntPlug *PointCloudPlus::distributionPlug() { return getChild<IntPlug>( g_firstPlugIndex + 4 ); }
const IntPlug *PointCloudPlus::distributionPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 4 ); }
BoolPlug *PointCloudPlus::useDensityPlug() { return getChild<BoolPlug>( g_firstPlugIndex + 5 ); }
const BoolPlug *PointCloudPlus::useDensityPlug() const { return getChild<BoolPlug>( g_firstPlugIndex + 5 ); }
FloatPlug *PointCloudPlus::densityPlug() { return getChild<FloatPlug>( g_firstPlugIndex + 6 ); }
const FloatPlug *PointCloudPlus::densityPlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 6 ); }
IntPlug *PointCloudPlus::pointCountPlug() { return getChild<IntPlug>( g_firstPlugIndex + 7 ); }
const IntPlug *PointCloudPlus::pointCountPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 7 ); }
IntPlug *PointCloudPlus::distributionSeedPlug() { return getChild<IntPlug>( g_firstPlugIndex + 8 ); }
const IntPlug *PointCloudPlus::distributionSeedPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 8 ); }
FloatPlug *PointCloudPlus::jitteringPlug() { return getChild<FloatPlug>( g_firstPlugIndex + 9 ); }
const FloatPlug *PointCloudPlus::jitteringPlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 9 ); }
StringPlug *PointCloudPlus::fileNamePlug() { return getChild<StringPlug>( g_firstPlugIndex + 10 ); }
const StringPlug *PointCloudPlus::fileNamePlug() const { return getChild<StringPlug>( g_firstPlugIndex + 10 ); }
StringPlug *PointCloudPlus::primPathPlug() { return getChild<StringPlug>( g_firstPlugIndex + 11 ); }
const StringPlug *PointCloudPlus::primPathPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 11 ); }
StringPlug *PointCloudPlus::purposePlug() { return getChild<StringPlug>( g_firstPlugIndex + 12 ); }
const StringPlug *PointCloudPlus::purposePlug() const { return getChild<StringPlug>( g_firstPlugIndex + 12 ); }
FloatPlug *PointCloudPlus::framePlug() { return getChild<FloatPlug>( g_firstPlugIndex + 13 ); }
const FloatPlug *PointCloudPlus::framePlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 13 ); }
FloatPlug *PointCloudPlus::frameOffsetPlug() { return getChild<FloatPlug>( g_firstPlugIndex + 14 ); }
const FloatPlug *PointCloudPlus::frameOffsetPlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 14 ); }
IntPlug *PointCloudPlus::animationBehaviorPlug() { return getChild<IntPlug>( g_firstPlugIndex + 15 ); }
const IntPlug *PointCloudPlus::animationBehaviorPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 15 ); }

void PointCloudPlus::affects( const Plug *input, AffectedPlugsContainer &outputs ) const
{
    SceneProcessor::affects( input, outputs );
    if(
        input == inPlug() ||
        input == outputLocationPlug() ||
        input == pointTypePlug() ||
        input == modePlug() ||
        input == filterPlug() ||
        input == distributionPlug() ||
        input == useDensityPlug() ||
        input == densityPlug() ||
        input == pointCountPlug() ||
        input == distributionSeedPlug() ||
        input == jitteringPlug() ||
        input == fileNamePlug() ||
        input == primPathPlug() ||
        input == purposePlug() ||
        input == framePlug() ||
        input == frameOffsetPlug() ||
        input == animationBehaviorPlug()
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

void PointCloudPlus::hash( const ValuePlug *output, const Context *context, MurmurHash &h ) const
{
    SceneProcessor::hash( output, context, h );
}

void PointCloudPlus::compute( ValuePlug *output, const Context *context ) const
{
    SceneProcessor::compute( output, context );
}

void PointCloudPlus::hashBound( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
    const ScenePath generatedPath = outputPath( this );
    if( isGeneratedLeaf( path, generatedPath ) )
    {
        h = outPlug()->objectHash( path );
        h.append( "PointCloudPlusBound" );
        return;
    }

    if( isGeneratedPath( path, generatedPath ) )
    {
        h = inPlug()->boundHash( path );
        h.append( generatedPath.size() );
        h.append( ScenePlug::pathToString( generatedPath ) );
        h.append( "PointCloudPlusAncestorBound" );
        return;
    }

    h = inPlug()->boundHash( path );
}

void PointCloudPlus::hashTransform( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
    const ScenePath generatedPath = outputPath( this );
    if( isGeneratedLeaf( path, generatedPath ) )
    {
        h.append( "PointCloudPlusTransform" );
        return;
    }

    if( isSyntheticBranch( inPlug(), path, generatedPath ) )
    {
        h.append( "PointCloudPlusSyntheticBranchTransform" );
        h.append( ScenePlug::pathToString( path ) );
        return;
    }

    h = inPlug()->transformHash( path );
}

void PointCloudPlus::hashAttributes( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
    const ScenePath generatedPath = outputPath( this );
    if( isGeneratedLeaf( path, generatedPath ) )
    {
        ScenePath parentPath = path;
        if( !parentPath.empty() )
        {
            parentPath.pop_back();
        }
        h = inPlug()->attributesHash( parentPath );
        h.append( "PointCloudPlusAttributes" );
        return;
    }

    if( isSyntheticBranch( inPlug(), path, generatedPath ) )
    {
        ScenePath parentPath = path;
        if( !parentPath.empty() )
        {
            parentPath.pop_back();
        }
        h = inPlug()->attributesHash( parentPath );
        h.append( "PointCloudPlusSyntheticBranchAttributes" );
        return;
    }

    h = inPlug()->attributesHash( path );
}

void PointCloudPlus::hashObject( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
    const ScenePath generatedPath = outputPath( this );
    if( isGeneratedLeaf( path, generatedPath ) )
    {
        const float evaluationFrame = modePlug()->getValue() == FileMode ? fileModeEvaluationFrame( this, context ) : context->getFrame();
        Context::EditableScope scope( context );
        scope.setFrame( evaluationFrame );
        hashSceneBranch( inPlug(), ScenePath(), h );
        outputLocationPlug()->hash( h );
        pointTypePlug()->hash( h );
        modePlug()->hash( h );
        filterPlug()->hash( h );
        distributionPlug()->hash( h );
        useDensityPlug()->hash( h );
        densityPlug()->hash( h );
        pointCountPlug()->hash( h );
        distributionSeedPlug()->hash( h );
        jitteringPlug()->hash( h );
        fileNamePlug()->hash( h );
        primPathPlug()->hash( h );
        purposePlug()->hash( h );
        framePlug()->hash( h );
        frameOffsetPlug()->hash( h );
        animationBehaviorPlug()->hash( h );
        h.append( evaluationFrame );
        return;
    }

    if( isSyntheticBranch( inPlug(), path, generatedPath ) )
    {
        h.append( "PointCloudPlusSyntheticBranchObject" );
        h.append( ScenePlug::pathToString( path ) );
        return;
    }

    h = inPlug()->objectHash( path );
}

void PointCloudPlus::hashChildNames( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
    const ScenePath cloudPath = outputPath( this );
    if( isGeneratedLeaf( path, cloudPath ) )
    {
        h.append( "PointCloudPlusLeafChildNames" );
        return;
    }

    if( isGeneratedPath( path, cloudPath ) )
    {
        h = inPlug()->childNamesHash( path );
        outputLocationPlug()->hash( h );
        h.append( ScenePlug::pathToString( path ) );
        return;
    }
    h = inPlug()->childNamesHash( path );
}

void PointCloudPlus::hashGlobals( const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
    h = inPlug()->globalsHash();
}

void PointCloudPlus::hashSetNames( const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
    h = inPlug()->setNamesHash();
}

void PointCloudPlus::hashSet( const InternedString &setName, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
    h = inPlug()->setHash( setName );
}

Imath::Box3f PointCloudPlus::computeBound( const ScenePath &path, const Context *context, const ScenePlug *parent ) const
{
    const ScenePath generatedPath = outputPath( this );
    if( isGeneratedLeaf( path, generatedPath ) )
    {
        ConstObjectPtr object = outPlug()->object( path );
        const PointsPrimitive *points = runTimeCast<const PointsPrimitive>( object.get() );
        return points ? points->bound() : Imath::Box3f();
    }

    if( isGeneratedPath( path, generatedPath ) )
    {
        Imath::Box3f bound = inPlug()->bound( path );
        if( path.size() < generatedPath.size() )
        {
            bound.extendBy( computeBound( generatedPath, context, parent ) );
        }
        return bound;
    }

    return inPlug()->bound( path );
}

Imath::M44f PointCloudPlus::computeTransform( const ScenePath &path, const Context *context, const ScenePlug *parent ) const
{
    const ScenePath generatedPath = outputPath( this );
    if( isGeneratedLeaf( path, generatedPath ) )
    {
        return Imath::M44f();
    }

    if( isSyntheticBranch( inPlug(), path, generatedPath ) )
    {
        return Imath::M44f();
    }

    return inPlug()->transform( path );
}

ConstCompoundObjectPtr PointCloudPlus::computeAttributes( const ScenePath &path, const Context *context, const ScenePlug *parent ) const
{
    const ScenePath generatedPath = outputPath( this );
    if( isGeneratedLeaf( path, generatedPath ) )
    {
        ScenePath parentPath = path;
        if( !parentPath.empty() )
        {
            parentPath.pop_back();
        }
        return inPlug()->attributes( parentPath );
    }

    if( isSyntheticBranch( inPlug(), path, generatedPath ) )
    {
        ScenePath parentPath = path;
        if( !parentPath.empty() )
        {
            parentPath.pop_back();
        }
        return inPlug()->attributes( parentPath );
    }

    return inPlug()->attributes( path );
}

ConstObjectPtr PointCloudPlus::computeObject( const ScenePath &path, const Context *context, const ScenePlug *parent ) const
{
    const ScenePath generatedPath = outputPath( this );
    if( isGeneratedLeaf( path, generatedPath ) )
    {
        return outputPoints( this, context );
    }

    if( isSyntheticBranch( inPlug(), path, generatedPath ) )
    {
        return IECore::NullObject::defaultNullObject();
    }

    return inPlug()->object( path );
}

ConstInternedStringVectorDataPtr PointCloudPlus::computeChildNames( const ScenePath &path, const Context *context, const ScenePlug *parent ) const
{
    const ScenePath cloudPath = outputPath( this );
    if( isGeneratedLeaf( path, cloudPath ) )
    {
        return new InternedStringVectorData();
    }

    if( !isGeneratedPath( path, cloudPath ) )
    {
        return inPlug()->childNames( path );
    }

    ConstInternedStringVectorDataPtr inputNames = inputChildNamesForPath( inPlug(), path );
    InternedStringVectorDataPtr result = new InternedStringVectorData();
    result->writable() = inputNames ? inputNames->readable() : std::vector<InternedString>();
    const std::vector<std::string> outputComponents = splitLocationComponents( outputLocationPlug()->getValue() );
    const std::vector<std::string> pathComponents = splitLocationComponents( ScenePlug::pathToString( path ) );

    InternedString childName;
    if( pathComponents.size() < outputComponents.size() )
    {
        childName = InternedString( outputComponents[pathComponents.size()] );
    }
    if(
        !childName.string().empty() &&
        std::find( result->writable().begin(), result->writable().end(), childName ) == result->writable().end()
    )
    {
        result->writable().push_back( childName );
    }
    return result;
}

ConstCompoundObjectPtr PointCloudPlus::computeGlobals( const Context *context, const ScenePlug *parent ) const
{
    return inPlug()->globals();
}

ConstInternedStringVectorDataPtr PointCloudPlus::computeSetNames( const Context *context, const ScenePlug *parent ) const
{
    return inPlug()->setNames();
}

ConstPathMatcherDataPtr PointCloudPlus::computeSet( const InternedString &setName, const Context *context, const ScenePlug *parent ) const
{
    return inPlug()->set( setName );
}
