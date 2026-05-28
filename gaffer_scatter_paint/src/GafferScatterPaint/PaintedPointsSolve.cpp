#include "PaintedPointsPrivate.h"

size_t PaintedPoints::relaxSelection()
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	const std::set<std::uint64_t> pointIds = selectedPointIds( store );
	if( pointIds.empty() )
	{
		return 0;
	}

	const ScenePlug *scene = inPlug();
	const int frame = currentFrame( Context::current() );
	const int relaxObjective = relaxObjectivePlug()->getValue();
	bp::list points = bp::extract<bp::list>( store["points"] );
	std::map<std::uint64_t, std::vector<bp::dict>> pointsByStroke;
	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		bp::dict point = bp::extract<bp::dict>( *it );
		pointsByStroke[dictValue<std::uint64_t>( point, "strokeId", 0 )].push_back( point );
	}

	std::set<std::uint64_t> changedStrokeIds;
	size_t updatedCount = 0;

	for( const auto &entry : pointsByStroke )
	{
		const std::vector<bp::dict> &strokePointsForId = entry.second;
		if( strokePointsForId.size() < 2 )
		{
			continue;
		}

		for( size_t i = 0; i < strokePointsForId.size(); ++i )
		{
			bp::dict point = strokePointsForId[i];
			const std::uint64_t pointId = dictValue<std::uint64_t>( point, "pointId", 0 );
			if( !pointIds.count( pointId ) )
			{
				continue;
			}

			const size_t previousIndex = ( i > 0 ) ? ( i - 1 ) : i;
			const size_t nextIndex = ( i + 1 < strokePointsForId.size() ) ? ( i + 1 ) : i;
			if( previousIndex == i && nextIndex == i )
			{
				continue;
			}

			Imath::V3f previousWorld;
			Imath::V3f currentWorld;
			Imath::V3f nextWorld;
			if(
				!resolvedWorldReference( scene, store, strokePointsForId[previousIndex], previousWorld ) ||
				!resolvedWorldReference( scene, store, point, currentWorld ) ||
				!resolvedWorldReference( scene, store, strokePointsForId[nextIndex], nextWorld )
			)
			{
				continue;
			}

			Imath::V3f smoothedWorld = currentWorld;
			if( relaxObjective == static_cast<int>( g_relaxObjectiveEvenRedistribution ) )
			{
				if( previousIndex == i )
				{
					smoothedWorld = ( currentWorld + nextWorld ) / 2.0f;
				}
				else if( nextIndex == i )
				{
					smoothedWorld = ( previousWorld + currentWorld ) / 2.0f;
				}
				else
				{
					smoothedWorld = ( previousWorld + nextWorld ) / 2.0f;
				}
			}
			else if( previousIndex == i )
			{
				smoothedWorld = ( currentWorld + nextWorld ) / 2.0f;
			}
			else if( nextIndex == i )
			{
				smoothedWorld = ( previousWorld + currentWorld ) / 2.0f;
			}
			else
			{
				smoothedWorld = ( previousWorld + currentWorld + nextWorld ) / 3.0f;
			}
			const PointSurfaceReference reference = pointSurfaceReference( store, point );
			ReprojectSurfaceCandidate candidate;
			if( !reprojectSurfaceCandidate( scene, reference, candidate ) )
			{
				continue;
			}

			ReprojectHit hit;
			if( !closestReprojectHit( candidate, smoothedWorld, hit ) )
			{
				continue;
			}

			point["targetPathId"] = internPath( store, "scenePaths", hit.targetPath );
			point["instanceId"] = hit.instanceId;
			point["instanceSourcePathId"] = internPath( store, "instanceSourcePaths", hit.instanceSourcePath );
			point["triangleIndex"] = hit.triangleIndex;
			point["barycentric"] = listFromVector( hit.barycentric );
			point["restObjectP"] = listFromVector( hit.objectPoint );
			point["restWorldP"] = listFromVector( hit.worldPoint );
			point["restUV"] = listFromVector2( hit.uv );
			point["restNormal"] = listFromVector( hit.objectNormal );
			point["restUp"] = listFromVector( hit.objectUp );
			point["valid"] = true;
			point["lastValidFrame"] = frame;
			point["anchorModeUsed"] = 0;
			point["topologyGeneration"] = static_cast<std::uint32_t>( std::max( frame, 0 ) );

			changedStrokeIds.insert( entry.first );
			updatedCount += 1;
		}
	}

	if( !updatedCount )
	{
		return 0;
	}

	refreshDerivedData( store, changedStrokeIds );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return updatedCount;
}

size_t PaintedPoints::reprojectSelection()
{
	std::string loadError;
	bp::dict store = loadStore( this, &loadError );
	const std::set<std::uint64_t> pointIds = selectedPointIds( store );
	if( pointIds.empty() )
	{
		return 0;
	}

	const ScenePlug *scene = inPlug();
	const int frame = currentFrame( Context::current() );
	const std::vector<PointSurfaceReference> filteredReferences = filteredSurfaceReferences( scene, this );
	const std::set<std::string> selectedInstancePaths = selectedInstanceResolvedPaths( store, pointIds );
	bp::list points = bp::extract<bp::list>( store["points"] );
	std::set<std::uint64_t> changedStrokeIds;
	size_t updatedCount = 0;

	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		bp::dict point = bp::extract<bp::dict>( *it );
		const std::uint64_t pointId = dictValue<std::uint64_t>( point, "pointId", 0 );
		if( !pointIds.count( pointId ) )
		{
			continue;
		}

		Imath::V3f referenceWorld;
		if( !resolvedWorldReference( scene, store, point, referenceWorld ) )
		{
			continue;
		}

		const PointSurfaceReference reference = pointSurfaceReference( store, point );
		std::vector<PointSurfaceReference> candidateReferences;
		std::set<std::string> seenCandidatePaths;
		appendUniqueReference( candidateReferences, seenCandidatePaths, reference );
		for( const PointSurfaceReference &filteredReference : filteredReferences )
		{
			if( canReprojectToCandidate( reference, filteredReference, selectedInstancePaths ) )
			{
				appendUniqueReference( candidateReferences, seenCandidatePaths, filteredReference );
			}
		}

		ReprojectHit hit;
		bool foundHit = false;
		for( const PointSurfaceReference &candidateReference : candidateReferences )
		{
			ReprojectSurfaceCandidate candidate;
			if( !reprojectSurfaceCandidate( scene, candidateReference, candidate ) )
			{
				continue;
			}

			ReprojectHit candidateHit;
			if( !closestReprojectHit( candidate, referenceWorld, candidateHit ) )
			{
				continue;
			}

			if( !foundHit || candidateHit.distanceSquared < hit.distanceSquared )
			{
				hit = candidateHit;
				foundHit = true;
			}
		}

		if( !foundHit )
		{
			continue;
		}

		point["targetPathId"] = internPath( store, "scenePaths", hit.targetPath );
		point["instanceId"] = hit.instanceId;
		point["instanceSourcePathId"] = internPath( store, "instanceSourcePaths", hit.instanceSourcePath );
		point["triangleIndex"] = hit.triangleIndex;
		point["barycentric"] = listFromVector( hit.barycentric );
		point["restObjectP"] = listFromVector( hit.objectPoint );
		point["restWorldP"] = listFromVector( hit.worldPoint );
		point["restUV"] = listFromVector2( hit.uv );
		point["restNormal"] = listFromVector( hit.objectNormal );
		point["restUp"] = listFromVector( hit.objectUp );
		point["valid"] = true;
		point["lastValidFrame"] = frame;
		point["anchorModeUsed"] = 0;
		point["topologyGeneration"] = static_cast<std::uint32_t>( std::max( frame, 0 ) );

		changedStrokeIds.insert( dictValue<std::uint64_t>( point, "strokeId", 0 ) );
		updatedCount += 1;
	}

	if( !updatedCount )
	{
		return 0;
	}

	refreshDerivedData( store, changedStrokeIds );
	writeStore( this, store );
	syncStateFromStore( this, store, loadError );
	return updatedCount;
}
