#include "PaintedPointsPrivate.h"

#include <limits>
#include <mutex>
#include <unordered_map>

using namespace Gaffer;
using namespace GafferScatterPaint;

namespace
{

std::mutex g_interactivePaintStateMutex;
std::unordered_map<const PaintedPoints *, InteractivePaintState> g_interactivePaintStates;

std::uint32_t internOverlayPath( std::vector<std::string> &paths, const std::string &path )
{
	if( path.empty() )
	{
		return 0;
	}

	for( std::size_t i = 0; i < paths.size(); ++i )
	{
		if( paths[i] == path )
		{
			return static_cast<std::uint32_t>( i + 1 );
		}
	}

	paths.push_back( path );
	return static_cast<std::uint32_t>( paths.size() );
}

bool interactivePaintStateEmpty( const InteractivePaintState &state )
{
	return state.revision == 0 && state.points.empty() && state.erasedPointIds.empty();
}

InteractivePaintState interactivePaintStateLocked( const PaintedPoints *node )
{
	auto it = g_interactivePaintStates.find( node );
	return it != g_interactivePaintStates.end() ? it->second : InteractivePaintState();
}

void setInteractivePaintStateLocked( const PaintedPoints *node, const InteractivePaintState &state )
{
	if( interactivePaintStateEmpty( state ) )
	{
		g_interactivePaintStates.erase( node );
	}
	else
	{
		g_interactivePaintStates[node] = state;
	}
}

void setInteractiveRevisionPlug( PaintedPoints *node, std::uint64_t revision )
{
	const std::uint64_t clamped = std::min<std::uint64_t>( revision, static_cast<std::uint64_t>( std::numeric_limits<int>::max() ) );
	if( ScriptNode *script = node->scriptNode() )
	{
		UndoScope undoDisabled( script, UndoScope::Disabled );
		node->interactiveRevisionPlug()->setValue( static_cast<int>( clamped ) );
	}
	else
	{
		node->interactiveRevisionPlug()->setValue( static_cast<int>( clamped ) );
	}
}

void clearPendingPaintBlobPlug( PaintedPoints *node )
{
	if( ScriptNode *script = node->scriptNode() )
	{
		UndoScope undoDisabled( script, UndoScope::Disabled );
		node->pendingPaintBlobPlug()->setValue( emptyBlobObject() );
	}
	else
	{
		node->pendingPaintBlobPlug()->setValue( emptyBlobObject() );
	}
}

void enactInteractivePaintState( PaintedPoints *node, const InteractivePaintState &newState )
{
	InteractivePaintState oldState;
	{
		std::lock_guard<std::mutex> lock( g_interactivePaintStateMutex );
		oldState = interactivePaintStateLocked( node );
	}

	Action::enact(
		node,
		[node, newState]() {
			{
				std::lock_guard<std::mutex> lock( g_interactivePaintStateMutex );
				setInteractivePaintStateLocked( node, newState );
			}
			setInteractiveRevisionPlug( node, newState.revision );
			clearPendingPaintBlobPlug( node );
		},
		[node, oldState]() {
			{
				std::lock_guard<std::mutex> lock( g_interactivePaintStateMutex );
				setInteractivePaintStateLocked( node, oldState );
			}
			setInteractiveRevisionPlug( node, oldState.revision );
			clearPendingPaintBlobPlug( node );
		}
	);
}

InteractivePaintState appendedInteractivePaintState(
	const InteractivePaintState &baseState,
	const CacheSchema &visibleSchema,
	const std::vector<PointRecord> &authoredPoints
)
{
	InteractivePaintState result = baseState;
	result.revision = baseState.revision + 1;

	for( const PointRecord &visiblePoint : authoredPoints )
	{
		PointRecord overlayPoint = visiblePoint;
		if( visiblePoint.targetPathId > 0 && static_cast<std::size_t>( visiblePoint.targetPathId ) <= visibleSchema.scenePaths.size() )
		{
			overlayPoint.targetPathId = internOverlayPath( result.scenePaths, visibleSchema.scenePaths[visiblePoint.targetPathId - 1] );
		}
		if( visiblePoint.instanceSourcePathId > 0 && static_cast<std::size_t>( visiblePoint.instanceSourcePathId ) <= visibleSchema.instanceSourcePaths.size() )
		{
			overlayPoint.instanceSourcePathId = internOverlayPath( result.instanceSourcePaths, visibleSchema.instanceSourcePaths[visiblePoint.instanceSourcePathId - 1] );
		}
		result.points.push_back( overlayPoint );
		result.erasedPointIds.erase( visiblePoint.pointId );
	}

	return result;
}

InteractivePaintState erasedInteractivePaintState(
	const InteractivePaintState &baseState,
	const std::unordered_set<std::uint64_t> &removedPointIds
)
{
	InteractivePaintState result = baseState;
	result.revision = baseState.revision + 1;

	std::unordered_set<std::uint64_t> overlayPointIds;
	overlayPointIds.reserve( result.points.size() );
	for( const PointRecord &point : result.points )
	{
		overlayPointIds.insert( point.pointId );
	}

	result.points.erase(
		std::remove_if(
			result.points.begin(), result.points.end(),
			[&removedPointIds]( const PointRecord &point ) {
				return removedPointIds.count( point.pointId ) != 0;
			}
		),
		result.points.end()
	);

	for( const std::uint64_t pointId : removedPointIds )
	{
		if( overlayPointIds.count( pointId ) )
		{
			continue;
		}
		result.erasedPointIds.insert( pointId );
	}

	return result;
}

InteractivePaintState clearedInteractivePaintState( const InteractivePaintState &baseState )
{
	InteractivePaintState result;
	result.revision = baseState.revision + 1;
	return result;
}

} // namespace

InteractivePaintState GafferScatterPaint::interactivePaintState( const PaintedPoints *node )
{
	std::lock_guard<std::mutex> lock( g_interactivePaintStateMutex );
	return interactivePaintStateLocked( node );
}

std::uint64_t GafferScatterPaint::interactivePaintRevision( const PaintedPoints *node )
{
	std::lock_guard<std::mutex> lock( g_interactivePaintStateMutex );
	return interactivePaintStateLocked( node ).revision;
}

void GafferScatterPaint::eraseInteractiveOverlayForNode( const PaintedPoints *node )
{
	std::lock_guard<std::mutex> lock( g_interactivePaintStateMutex );
	g_interactivePaintStates.erase( node );
}

void GafferScatterPaint::applyInteractiveOverlayToSchema( const PaintedPoints *node, CacheSchema &schema )
{
	const InteractivePaintState state = interactivePaintState( node );
	if( state.points.empty() && state.erasedPointIds.empty() )
	{
		return;
	}

	if( !state.erasedPointIds.empty() )
	{
		schema.points.erase(
			std::remove_if(
				schema.points.begin(), schema.points.end(),
				[&state]( const PointRecord &point ) {
					return state.erasedPointIds.count( point.pointId ) != 0;
				}
			),
			schema.points.end()
		);
	}

	std::uint64_t maxPointId = schema.nextIds.point > 0 ? schema.nextIds.point - 1 : 0;
	for( const PointRecord &overlayPoint : state.points )
	{
		PointRecord visiblePoint = overlayPoint;
		if( overlayPoint.targetPathId > 0 && static_cast<std::size_t>( overlayPoint.targetPathId ) <= state.scenePaths.size() )
		{
			visiblePoint.targetPathId = internSchemaPath( schema.scenePaths, state.scenePaths[overlayPoint.targetPathId - 1] );
		}
		if( overlayPoint.instanceSourcePathId > 0 && static_cast<std::size_t>( overlayPoint.instanceSourcePathId ) <= state.instanceSourcePaths.size() )
		{
			visiblePoint.instanceSourcePathId = internSchemaPath( schema.instanceSourcePaths, state.instanceSourcePaths[overlayPoint.instanceSourcePathId - 1] );
		}
		schema.points.push_back( visiblePoint );
		maxPointId = std::max( maxPointId, visiblePoint.pointId );
	}

	schema.nextIds.point = std::max( schema.nextIds.point, maxPointId + 1 );
}

void GafferScatterPaint::appendInteractivePoints( PaintedPoints *node, const CacheSchema &visibleSchema, const std::vector<PointRecord> &authoredPoints )
{
	if( authoredPoints.empty() )
	{
		return;
	}

	const InteractivePaintState baseState = interactivePaintState( node );
	enactInteractivePaintState( node, appendedInteractivePaintState( baseState, visibleSchema, authoredPoints ) );
}

void GafferScatterPaint::eraseInteractivePoints( PaintedPoints *node, const std::unordered_set<std::uint64_t> &removedPointIds )
{
	if( removedPointIds.empty() )
	{
		return;
	}

	const InteractivePaintState baseState = interactivePaintState( node );
	enactInteractivePaintState( node, erasedInteractivePaintState( baseState, removedPointIds ) );
}

void GafferScatterPaint::clearInteractiveOverlay( PaintedPoints *node )
{
	const InteractivePaintState baseState = interactivePaintState( node );
	if( interactivePaintStateEmpty( baseState ) )
	{
		clearPendingPaintBlobPlug( node );
		return;
	}

	enactInteractivePaintState( node, clearedInteractivePaintState( baseState ) );
}
