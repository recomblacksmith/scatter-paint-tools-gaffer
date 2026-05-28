#include "PaintedPointsPrivate.h"

#include <cmath>
#include <cstdint>
#include <set>
#include <unordered_map>

namespace GafferScatterPaint
{

namespace
{

constexpr std::uint32_t g_validationChunkPointLimit = 8192;

void addCategory( std::set<int> &categories, ValidationCategory category )
{
	categories.insert( static_cast<int>( category ) );
}

bool isKnownAnchorMode( AnchorMode mode )
{
	switch( mode )
	{
		case AnchorMode::Barycentric :
		case AnchorMode::HybridFallback :
		case AnchorMode::UVFallback :
		case AnchorMode::Reprojected :
			return true;
	}

	return false;
}

} // namespace

bp::dict validateStore( const PaintedPoints *node, const bp::dict &store, const std::string &loadError )
{
	const CacheSchema schema = dictToCacheSchema( store );
	const int schemaVersion = dictValue<int>( store, "schemaVersion", CacheHeader::schemaVersion );

	std::uint64_t invalidPointCount = 0;
	std::uint64_t invalidStrokeCount = 0;
	std::uint64_t topologyMismatchCount = 0;
	std::uint64_t resolvedAttachmentCount = 0;
	std::uint64_t unresolvedAttachmentCount = 0;
	std::set<std::string> failingTargetPaths;
	std::set<int> validationCategories;

	std::set<std::uint64_t> layerIds;
	std::set<std::uint64_t> strokeIds;
	std::set<std::uint64_t> pointIds;
	std::set<std::uint64_t> selectionSetIds;

	std::unordered_map<std::uint64_t, std::vector<const StrokeRecord *>> strokesByLayer;
	for( const StrokeRecord &stroke : schema.strokes )
	{
		strokesByLayer[stroke.layerId].push_back( &stroke );
	}

	std::unordered_map<std::uint64_t, std::vector<const PointRecord *>> pointsByStroke;
	for( const PointRecord &point : schema.points )
	{
		pointsByStroke[point.strokeId].push_back( &point );
	}

	std::unordered_map<std::uint64_t, std::vector<const PointChunkRecord *>> chunksByStroke;
	for( const PointChunkRecord &chunk : schema.chunks )
	{
		chunksByStroke[chunk.strokeId].push_back( &chunk );
	}

	if( !loadError.empty() )
	{
		topologyMismatchCount += 1;
		addCategory( validationCategories, ValidationCategory::Cache );
	}

	for( const LayerRecord &layer : schema.layers )
	{
		if( !layerIds.insert( layer.layerId ).second || layer.name.empty() )
		{
			topologyMismatchCount += 1;
			addCategory( validationCategories, ValidationCategory::Topology );
		}

		auto layerStrokes = strokesByLayer[layer.layerId];
		std::sort(
			layerStrokes.begin(), layerStrokes.end(),
			[]( const StrokeRecord *a, const StrokeRecord *b ) {
				return a->order < b->order;
			}
		);

		const std::uint64_t expectedFirstStrokeId = layerStrokes.empty() ? 0 : layerStrokes.front()->strokeId;
		const std::uint64_t expectedLastStrokeId = layerStrokes.empty() ? 0 : layerStrokes.back()->strokeId;
		if( layer.firstStrokeId != expectedFirstStrokeId )
		{
			topologyMismatchCount += 1;
			addCategory( validationCategories, ValidationCategory::Topology );
		}
		if( layer.lastStrokeId != expectedLastStrokeId )
		{
			topologyMismatchCount += 1;
			addCategory( validationCategories, ValidationCategory::Topology );
		}

		for( const StrokeRecord *stroke : layerStrokes )
		{
			if( !strokeIds.insert( stroke->strokeId ).second || stroke->name.empty() )
			{
				invalidStrokeCount += 1;
				addCategory( validationCategories, ValidationCategory::Cache );
			}

			const auto &strokePoints = pointsByStroke[stroke->strokeId];
			if( stroke->pointCount != strokePoints.size() )
			{
				topologyMismatchCount += 1;
				addCategory( validationCategories, ValidationCategory::Topology );
			}

			auto strokeChunks = chunksByStroke[stroke->strokeId];
			const std::size_t expectedChunkCount = std::max<std::size_t>(
				1,
				( strokePoints.size() + g_validationChunkPointLimit - 1 ) / g_validationChunkPointLimit
			);
			if( strokeChunks.size() != expectedChunkCount )
			{
				topologyMismatchCount += 1;
				addCategory( validationCategories, ValidationCategory::Topology );
			}

			std::sort(
				strokeChunks.begin(), strokeChunks.end(),
				[]( const PointChunkRecord *a, const PointChunkRecord *b ) {
					return a->chunkIndex < b->chunkIndex;
				}
			);

			if( !strokeChunks.empty() )
			{
				if( stroke->firstChunkId != strokeChunks.front()->chunkId )
				{
					topologyMismatchCount += 1;
					addCategory( validationCategories, ValidationCategory::Topology );
				}
				if( stroke->lastChunkId != strokeChunks.back()->chunkId )
				{
					topologyMismatchCount += 1;
					addCategory( validationCategories, ValidationCategory::Topology );
				}

				std::uint32_t expectedPointStart = 0;
				std::uint32_t accumulatedPointCount = 0;
				for( std::size_t i = 0; i < strokeChunks.size(); ++i )
				{
					const PointChunkRecord &chunk = *strokeChunks[i];
					if( chunk.chunkIndex != i )
					{
						topologyMismatchCount += 1;
						addCategory( validationCategories, ValidationCategory::Topology );
					}
					if( chunk.pointStart != expectedPointStart )
					{
						topologyMismatchCount += 1;
						addCategory( validationCategories, ValidationCategory::Topology );
					}

					const std::uint32_t remainingPointCount = static_cast<std::uint32_t>( strokePoints.size() ) - accumulatedPointCount;
					const std::uint32_t expectedPointCount = remainingPointCount == 0 ? 0 : std::min<std::uint32_t>( g_validationChunkPointLimit, remainingPointCount );
					if( chunk.pointCount != expectedPointCount )
					{
						topologyMismatchCount += 1;
						addCategory( validationCategories, ValidationCategory::Topology );
					}
					if( chunk.deleted )
					{
						topologyMismatchCount += 1;
						addCategory( validationCategories, ValidationCategory::Topology );
					}

					expectedPointStart += chunk.pointCount;
					accumulatedPointCount += chunk.pointCount;
				}

				if( accumulatedPointCount != strokePoints.size() )
				{
					topologyMismatchCount += 1;
					addCategory( validationCategories, ValidationCategory::Topology );
				}
			}
			else if( stroke->firstChunkId != 0 || stroke->lastChunkId != 0 )
			{
				topologyMismatchCount += 1;
				addCategory( validationCategories, ValidationCategory::Topology );
			}

			for( const PointRecord *point : strokePoints )
			{
				if( !pointIds.insert( point->pointId ).second )
				{
					invalidPointCount += 1;
					addCategory( validationCategories, ValidationCategory::Cache );
				}

				if( point->layerId != layer.layerId )
				{
					topologyMismatchCount += 1;
					addCategory( validationCategories, ValidationCategory::Topology );
				}
				if( point->strokeId != stroke->strokeId )
				{
					topologyMismatchCount += 1;
					addCategory( validationCategories, ValidationCategory::Topology );
				}

				if( !point->valid )
				{
					invalidPointCount += 1;
					addCategory( validationCategories, ValidationCategory::Attachment );
				}

				const float barycentricSum = point->barycentric[0] + point->barycentric[1] + point->barycentric[2];
				if( std::abs( barycentricSum - 1.0f ) > 0.01f )
				{
					invalidPointCount += 1;
					addCategory( validationCategories, ValidationCategory::Attachment );
				}

				if( point->targetPathId > schema.scenePaths.size() )
				{
					invalidPointCount += 1;
					topologyMismatchCount += 1;
					addCategory( validationCategories, ValidationCategory::Cache );
					failingTargetPaths.insert( "invalidTargetPathId:" + std::to_string( point->targetPathId ) );
				}

				if( point->instanceSourcePathId > schema.instanceSourcePaths.size() )
				{
					invalidPointCount += 1;
					topologyMismatchCount += 1;
					addCategory( validationCategories, ValidationCategory::Cache );
				}

				if( !isKnownAnchorMode( point->anchorModeUsed ) )
				{
					invalidPointCount += 1;
					addCategory( validationCategories, ValidationCategory::Attachment );
				}

				if( point->anchorModeUsed != AnchorMode::Reprojected )
				{
					resolvedAttachmentCount += 1;
				}
				else
				{
					unresolvedAttachmentCount += 1;
				}
			}
		}
	}

	for( const StrokeRecord &stroke : schema.strokes )
	{
		if( !layerIds.count( stroke.layerId ) )
		{
			invalidStrokeCount += 1;
			addCategory( validationCategories, ValidationCategory::Cache );
		}
	}

	for( const PointChunkRecord &chunk : schema.chunks )
	{
		if( !strokeIds.count( chunk.strokeId ) )
		{
			topologyMismatchCount += 1;
			addCategory( validationCategories, ValidationCategory::Topology );
		}
	}

	for( const SelectionSetRecord &selectionSet : schema.selectionSets )
	{
		if( !selectionSetIds.insert( selectionSet.selectionSetId ).second || selectionSet.name.empty() )
		{
			topologyMismatchCount += 1;
			addCategory( validationCategories, ValidationCategory::Cache );
		}

		for( const std::uint64_t pointId : selectionSet.pointIds )
		{
			if( !pointIds.count( pointId ) )
			{
				topologyMismatchCount += 1;
				addCategory( validationCategories, ValidationCategory::Cache );
			}
		}

		for( const std::uint64_t strokeId : selectionSet.strokeIds )
		{
			if( !strokeIds.count( strokeId ) )
			{
				topologyMismatchCount += 1;
				addCategory( validationCategories, ValidationCategory::Cache );
			}
		}
	}

	for( const std::uint64_t pointId : schema.currentSelection.pointIds )
	{
		if( !pointIds.count( pointId ) )
		{
			topologyMismatchCount += 1;
			addCategory( validationCategories, ValidationCategory::Cache );
		}
	}

	for( const std::uint64_t strokeId : schema.currentSelection.strokeIds )
	{
		if( !strokeIds.count( strokeId ) )
		{
			topologyMismatchCount += 1;
			addCategory( validationCategories, ValidationCategory::Cache );
		}
	}

	if( schema.lock.mode != LockMode::SessionAware )
	{
		topologyMismatchCount += 1;
		addCategory( validationCategories, ValidationCategory::Lock );
	}
	if( !schema.lock.user.empty() || !schema.lock.host.empty() || !schema.lock.sessionId.empty() || !schema.lock.timestampUtc.empty() || !schema.lock.scriptPath.empty() || !schema.lock.projectPath.empty() )
	{
		if( schema.lock.user.empty() )
		{
			addCategory( validationCategories, ValidationCategory::Lock );
		}
	}

	if( schemaVersion != static_cast<int>( CacheHeader::schemaVersion ) )
	{
		topologyMismatchCount += 1;
		addCategory( validationCategories, ValidationCategory::UpgradeState );
	}

	if( node && !findAttachedPointsForPaintedNode( node ) )
	{
		addCategory( validationCategories, ValidationCategory::ExportReadiness );
	}

	const std::string summaryPrefix = loadError.empty() ? "" : "Invalid scatter paint blob. ";
	const std::string summary =
		summaryPrefix +
		std::to_string( schema.layers.size() ) + " layers, " +
		std::to_string( schema.strokes.size() ) + " strokes, " +
		std::to_string( schema.points.size() ) + " points, " +
		std::to_string( schema.selectionSets.size() ) + " selection sets, " +
		std::to_string( resolvedAttachmentCount ) + " resolved attachments, " +
		std::to_string( unresolvedAttachmentCount ) + " fallback attachments";

	bp::list failingTargetPathsList;
	for( const std::string &path : failingTargetPaths )
	{
		failingTargetPathsList.append( path );
	}

	bp::list categories;
	const std::set<int> effectiveCategories = validationCategories.empty() ? std::set<int>{ static_cast<int>( ValidationCategory::Diagnostics ) } : validationCategories;
	for( const int category : effectiveCategories )
	{
		categories.append( category );
	}

	bp::dict diagnostics;
	diagnostics["invalidPointCount"] = invalidPointCount;
	diagnostics["invalidStrokeCount"] = invalidStrokeCount;
	diagnostics["topologyMismatchCount"] = topologyMismatchCount;
	diagnostics["failingFrame"] = 0;
	diagnostics["failingTargetPaths"] = failingTargetPathsList;
	diagnostics["lastErrorMessage"] = loadError;
	diagnostics["validationSummary"] = summary;
	diagnostics["categories"] = categories;
	diagnostics["validationCategories"] = categories;
	return diagnostics;
}

} // namespace GafferScatterPaint
