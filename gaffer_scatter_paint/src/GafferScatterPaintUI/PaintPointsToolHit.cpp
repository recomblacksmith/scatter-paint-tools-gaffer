#include "PaintPointsToolPrivate.h"

#include "GafferScene/SetAlgo.h"

using namespace Gaffer;
using namespace GafferScatterPaintUI;
using namespace GafferScatterPaintUI::PaintPointsToolPrivate;
using namespace GafferSceneUI;
using namespace IECoreScene;
namespace bp = boost::python;

namespace
{

std::vector<std::string> splitFilterTokens( const std::string &value )
{
	std::vector<std::string> result;
	std::string current;
	for( const char c : value )
	{
		if( std::isspace( static_cast<unsigned char>( c ) ) || c == ',' || c == ';' )
		{
			if( !current.empty() )
			{
				result.push_back( current );
				current.clear();
			}
			continue;
		}
		current.push_back( c );
	}

	if( !current.empty() )
	{
		result.push_back( current );
	}

	return result;
}

bool pathMatchesToolFilters(
	const GafferScene::ScenePlug *scene,
	const GafferScatterPaintUI::PaintPointsTool *tool,
	const GafferScene::ScenePlug::ScenePath &path
)
{
	if( !scene || !tool || path.empty() )
	{
		return false;
	}

	const std::string pathString = GafferScene::ScenePlug::pathToString( path );
	bool matched = false;

	const std::string targetFilter = trimmed( tool->targetFilterPlug()->getValue() );
	if( !targetFilter.empty() )
	{
		IECore::PathMatcher includeMatcher;
		IECore::PathMatcher excludeMatcher;
		bool hasIncludeTokens = false;
		for( const std::string &token : splitFilterTokens( targetFilter ) )
		{
			if( token.empty() )
			{
				continue;
			}
			if( token[0] == '-' )
			{
				if( token.size() > 1 )
				{
					excludeMatcher.addPath( token.substr( 1 ) );
				}
			}
			else
			{
				includeMatcher.addPath( token );
				hasIncludeTokens = true;
			}
		}

		const bool excluded = ( excludeMatcher.match( pathString ) & IECore::PathMatcher::ExactMatch ) != 0;
		const bool included = !hasIncludeTokens || ( ( includeMatcher.match( pathString ) & IECore::PathMatcher::ExactMatch ) != 0 );
		matched = included && !excluded;
	}

	const std::string targetSetFilter = trimmed( tool->targetSetFilterPlug()->getValue() );
	if( !targetSetFilter.empty() )
	{
		try
		{
			const IECore::PathMatcher matcher = GafferScene::SetAlgo::evaluateSetExpression( targetSetFilter, scene );
			matched = matched || ( ( matcher.match( pathString ) & IECore::PathMatcher::ExactMatch ) != 0 );
		}
		catch( const std::exception & )
		{
		}
	}

	return matched || ( targetFilter.empty() && targetSetFilter.empty() );
}

GafferScene::ScenePlug::ScenePath filteredHitPath(
	const GafferScene::ScenePlug *scene,
	const GafferScatterPaintUI::PaintPointsTool *tool,
	GafferScene::ScenePlug::ScenePath path
)
{
	while( !path.empty() )
	{
		if( pathMatchesToolFilters( scene, tool, path ) )
		{
			return path;
		}
		path.pop_back();
	}

	return GafferScene::ScenePlug::ScenePath();
}

} // namespace

std::optional<PaintPointsTool::HitRecord> PaintPointsTool::hitPoint( const IECore::LineSegment3f &eventLine ) const
{
	const GafferSceneUI::SceneGadget *scene = sceneGadget();
	if( !scene )
	{
		return std::nullopt;
	}

	GafferScene::ScenePlug::ScenePath path;
	Imath::V3f hitPoint;
	if( !scene->objectAt( eventLine, path, hitPoint ) )
	{
		return std::nullopt;
	}

	const GafferScene::ScenePlug *scenePlug = scene->getScene();
	path = filteredHitPath( scenePlug, this, path );
	if( path.empty() )
	{
		return std::nullopt;
	}

	HitRecord hit;
	hit.path = scenePathString( path );
	hit.point = hitPoint;
	hit.normal = scene->normalAt( eventLine ).value_or( Imath::V3f( 0.0f, 1.0f, 0.0f ) );
	hit.triangleIndex = 0;
	hit.barycentric = Imath::V3f( 1.0f, 0.0f, 0.0f );
	hit.attachmentResolved = false;
	hit.sampleOrigin = PaintPointsTool::SampleOrigin::RawHit;

	if( const auto refined = refineMeshHit( path, hitPoint, hit.normal ) )
	{
		return refined;
	}

	return hit;
}

std::optional<PaintPointsTool::HitRecord> PaintPointsTool::refineMeshHit(
	const GafferScene::ScenePlug::ScenePath &path,
	const Imath::V3f &worldHitPoint,
	const Imath::V3f &fallbackNormal
) const
{
	const GafferSceneUI::SceneGadget *scene = sceneGadget();
	if( !scene )
	{
		return std::nullopt;
	}

	const GafferScene::ScenePlug *scenePlug = scene->getScene();
	if( !scenePlug || path.empty() || !scenePlug->exists( path ) )
	{
		return std::nullopt;
	}

	IECore::ConstObjectPtr sceneObject;
	try
	{
		sceneObject = scenePlug->object( path );
	}
	catch( const std::exception & )
	{
		return std::nullopt;
	}

	if( !sceneObject )
	{
		return std::nullopt;
	}

	std::shared_ptr<TriangleCacheEntry> triangleData;
	const auto cacheIt = m_triangleCache.find( sceneObject.get() );
	if( cacheIt != m_triangleCache.end() )
	{
		triangleData = cacheIt->second;
	}
	else
	{
		auto newEntry = std::make_shared<TriangleCacheEntry>();
		if( !buildTriangleCacheEntry( sceneObject, *newEntry ) )
		{
			m_triangleCache[sceneObject.get()] = nullptr;
			return std::nullopt;
		}
		m_triangleCache[sceneObject.get()] = newEntry;
		triangleData = std::move( newEntry );
	}

	if( !triangleData || triangleData->triangles.empty() )
	{
		return std::nullopt;
	}

	Imath::M44f objectToWorld;
	Imath::M44f worldToObject;
	try
	{
		objectToWorld = scenePlug->fullTransform( path );
		worldToObject = objectToWorld.inverse();
	}
	catch( const std::exception & )
	{
		return std::nullopt;
	}

	const Imath::V3f objectHitPoint = worldHitPoint * worldToObject;

	bool found = false;
	float bestDistance = 0.0f;
	HitRecord bestHit;
	bestHit.path = scenePathString( path );
	bestHit.normal = fallbackNormal;

	for( const auto &triangle : triangleData->triangles )
	{
		const int ia = triangle[1];
		const int ib = triangle[2];
		const int ic = triangle[3];
		if(
			ia < 0 || ib < 0 || ic < 0 ||
			static_cast<size_t>( ia ) >= triangleData->positions.size() ||
			static_cast<size_t>( ib ) >= triangleData->positions.size() ||
			static_cast<size_t>( ic ) >= triangleData->positions.size()
		)
		{
			continue;
		}

		const Imath::V3f &a = triangleData->positions[ia];
		const Imath::V3f &b = triangleData->positions[ib];
		const Imath::V3f &c = triangleData->positions[ic];
		const auto closest = closestPointOnTriangle( objectHitPoint, a, b, c );
		const float distance = vectorLengthSquared( objectHitPoint - closest.first );
		if( !found || distance < bestDistance )
		{
			found = true;
			bestDistance = distance;
			bestHit.point = closest.first * objectToWorld;
			bestHit.triangleIndex = triangle[0];
			bestHit.barycentric = closest.second;
			bestHit.attachmentResolved = true;
			bestHit.sampleOrigin = PaintPointsTool::SampleOrigin::RawHit;
		}
	}

	if( !found )
	{
		return std::nullopt;
	}

	return bestHit;
}
