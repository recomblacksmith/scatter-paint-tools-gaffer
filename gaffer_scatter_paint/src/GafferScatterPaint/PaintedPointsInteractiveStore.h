#pragma once

#include "GafferScatterPaint/CacheFormat.h"

#include <cstdint>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace GafferScatterPaint
{

class PaintedPoints;

struct InteractivePaintState
{
	std::uint64_t revision = 0;
	std::vector<std::string> scenePaths;
	std::vector<std::string> instanceSourcePaths;
	std::vector<PointRecord> points;
	std::set<std::uint64_t> erasedPointIds;
};

InteractivePaintState interactivePaintState( const PaintedPoints *node );
std::uint64_t interactivePaintRevision( const PaintedPoints *node );
void eraseInteractiveOverlayForNode( const PaintedPoints *node );
void applyInteractiveOverlayToSchema( const PaintedPoints *node, CacheSchema &schema );
void appendInteractivePoints( PaintedPoints *node, const CacheSchema &visibleSchema, const std::vector<PointRecord> &authoredPoints );
void eraseInteractivePoints( PaintedPoints *node, const std::unordered_set<std::uint64_t> &removedPointIds );
void clearInteractiveOverlay( PaintedPoints *node );

} // namespace GafferScatterPaint
