#pragma once

std::optional<Imath::V2f> rasterPosition( const PaintPointsTool *tool, const IECore::LineSegment3f &eventLine )
{
	if( !tool )
	{
		return std::nullopt;
	}

	const GafferUI::ViewportGadget *viewport = tool->view() ? tool->view()->viewportGadget() : nullptr;
	const GafferSceneUI::SceneGadget *scene = viewport ? IECore::runTimeCast<const GafferSceneUI::SceneGadget>( viewport->getPrimaryChild() ) : nullptr;
	if( !viewport || !scene )
	{
		return std::nullopt;
	}

	return viewport->gadgetToRasterSpace( eventLine.p0, scene );
}

bool marqueeHasArea( const std::optional<Imath::V2f> &startRaster, const std::optional<Imath::V2f> &endRaster )
{
	if( !startRaster || !endRaster )
	{
		return false;
	}

	const Imath::V2f delta = *endRaster - *startRaster;
	return std::max( std::abs( delta.x ), std::abs( delta.y ) ) >= 4.0f;
}

bool appendUniqueRasterPoint( std::vector<Imath::V2f> &points, const Imath::V2f &point )
{
	if( !points.empty() )
	{
		const Imath::V2f delta = point - points.back();
		if( delta.length2() < 1.0f )
		{
			return false;
		}
	}
	points.push_back( point );
	return true;
}

std::vector<Imath::V2f> closedRasterPolygon(
	const std::vector<Imath::V2f> &rasterPath,
	const std::optional<Imath::V2f> &startRaster,
	const std::optional<Imath::V2f> &endRaster
)
{
	std::vector<Imath::V2f> polygon;
	polygon.reserve( rasterPath.size() + 2 );
	if( startRaster )
	{
		polygon.push_back( *startRaster );
	}
	for( const Imath::V2f &point : rasterPath )
	{
		appendUniqueRasterPoint( polygon, point );
	}
	if( endRaster )
	{
		appendUniqueRasterPoint( polygon, *endRaster );
	}
	if( polygon.size() >= 3 )
	{
		const Imath::V2f delta = polygon.front() - polygon.back();
		if( delta.length2() >= 1.0f )
		{
			polygon.push_back( polygon.front() );
		}
	}
	return polygon;
}

bool pointInPolygon( const std::vector<Imath::V2f> &polygon, const Imath::V2f &point )
{
	if( polygon.size() < 4 )
	{
		return false;
	}
	bool inside = false;
	for( size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++ )
	{
		const Imath::V2f &pi = polygon[i];
		const Imath::V2f &pj = polygon[j];
		const float denominator = std::abs( pj.y - pi.y ) < 1e-8f ? 1e-8f : ( pj.y - pi.y );
		const bool intersects =
			( ( pi.y > point.y ) != ( pj.y > point.y ) ) &&
			( point.x < ( ( pj.x - pi.x ) * ( point.y - pi.y ) / denominator ) + pi.x );
		if( intersects )
		{
			inside = !inside;
		}
	}
	return inside;
}

void appendUniqueId( std::vector<std::uint64_t> &ids, std::uint64_t id )
{
	if( !id )
	{
		return;
	}

	if( std::find( ids.begin(), ids.end(), id ) == ids.end() )
	{
		ids.push_back( id );
	}
}

void collectSelectionFromStroke(
	Node *node,
	const PaintPointsTool::StrokePoints &strokePoints,
	float radius,
	std::vector<std::uint64_t> &pointIds,
	std::vector<std::uint64_t> &strokeIds,
	std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> *pointIdsByStroke = nullptr
)
{
	bp::object pythonToolNode = pythonNode( node );
	bp::list pointRecords = bp::extract<bp::list>( pythonToolNode.attr( "pointRecords" )() );
	const float radiusSquared = radius * radius;
	if( strokePoints.empty() )
	{
		return;
	}

	for( bp::stl_input_iterator<bp::object> it( pointRecords ), end; it != end; ++it )
	{
		bp::dict point = bp::extract<bp::dict>( *it );
		const std::string sourcePath = bp::extract<std::string>( point.get( "sourcePath", "" ) );
		const Imath::V3f pointPosition = vectorFromObject( point.get( "P", bp::object() ) );

		bool matchesStroke = false;
		for( const auto &sample : strokePoints )
		{
			if( sourcePath != sample.path )
			{
				continue;
			}
			if( ( pointPosition - sample.point ).length2() <= radiusSquared )
			{
				matchesStroke = true;
				break;
			}
		}
		if( !matchesStroke )
		{
			continue;
		}

		const std::uint64_t pointId = uint64FromObject( point.get( "pointId", bp::object() ) );
		const std::uint64_t strokeId = uint64FromObject( point.get( "strokeId", bp::object() ) );
		appendUniqueId( pointIds, pointId );
		appendUniqueId( strokeIds, strokeId );
		if( pointIdsByStroke && strokeId && pointId )
		{
			appendUniqueId( (*pointIdsByStroke)[strokeId], pointId );
		}
	}
}

bp::dict selectionResultFromIds(
	Node *node,
	const std::vector<std::uint64_t> &pointIds,
	const std::vector<std::uint64_t> &strokeIds
)
{
	bp::list pointIdList;
	for( const std::uint64_t pointId : pointIds )
	{
		pointIdList.append( pointId );
	}

	bp::list strokeIdList;
	for( const std::uint64_t strokeId : strokeIds )
	{
		strokeIdList.append( strokeId );
	}

	bp::object pythonToolNode = pythonNode( node );
	bp::object currentSelection = pythonToolNode.attr( "setCurrentSelection" )( pointIdList, strokeIdList );
	return bp::extract<bp::dict>( currentSelection );
}

bp::dict selectionResultFromStroke( Node *node, const PaintPointsTool::StrokePoints &strokePoints, float radius )
{
	std::vector<std::uint64_t> pointIds;
	std::vector<std::uint64_t> strokeIds;
	collectSelectionFromStroke( node, strokePoints, radius, pointIds, strokeIds );
	return selectionResultFromIds( node, pointIds, strokeIds );
}

bp::dict selectionResultFromMarquee(
	Node *node,
	const GafferUI::ViewportGadget *viewport,
	const std::optional<Imath::V2f> &startRaster,
	const std::optional<Imath::V2f> &endRaster
)
{
	if( !node || !viewport || !startRaster || !endRaster )
	{
		bp::dict result;
		result["pointIds"] = bp::list();
		result["strokeIds"] = bp::list();
		return result;
	}

	const Imath::V2f minRaster( std::min( startRaster->x, endRaster->x ), std::min( startRaster->y, endRaster->y ) );
	const Imath::V2f maxRaster( std::max( startRaster->x, endRaster->x ), std::max( startRaster->y, endRaster->y ) );

	bp::object pythonToolNode = pythonNode( node );
	bp::list pointRecords = bp::extract<bp::list>( pythonToolNode.attr( "pointRecords" )() );
	std::vector<std::uint64_t> pointIds;
	std::vector<std::uint64_t> strokeIds;
	for( bp::stl_input_iterator<bp::object> it( pointRecords ), end; it != end; ++it )
	{
		bp::dict point = bp::extract<bp::dict>( *it );
		const Imath::V3f pointPosition = vectorFromObject( point.get( "P", bp::object() ) );
		const Imath::V2f raster = viewport->worldToRasterSpace( pointPosition );
		if(
			raster.x < minRaster.x || raster.x > maxRaster.x ||
			raster.y < minRaster.y || raster.y > maxRaster.y
		)
		{
			continue;
		}

		appendUniqueId( pointIds, uint64FromObject( point.get( "pointId", bp::object() ) ) );
		appendUniqueId( strokeIds, uint64FromObject( point.get( "strokeId", bp::object() ) ) );
	}

	return selectionResultFromIds( node, pointIds, strokeIds );
}

bp::dict selectionResultFromLasso(
	Node *node,
	const GafferUI::ViewportGadget *viewport,
	const std::vector<Imath::V2f> &rasterPath,
	const std::optional<Imath::V2f> &startRaster,
	const std::optional<Imath::V2f> &endRaster
)
{
	if( !node || !viewport )
	{
		bp::dict result;
		result["pointIds"] = bp::list();
		result["strokeIds"] = bp::list();
		return result;
	}

	const std::vector<Imath::V2f> polygon = closedRasterPolygon( rasterPath, startRaster, endRaster );
	if( polygon.size() < 4 )
	{
		return selectionResultFromMarquee( node, viewport, startRaster, endRaster );
	}

	bp::object pythonToolNode = pythonNode( node );
	bp::list pointRecords = bp::extract<bp::list>( pythonToolNode.attr( "pointRecords" )() );
	std::vector<std::uint64_t> pointIds;
	std::vector<std::uint64_t> strokeIds;
	for( bp::stl_input_iterator<bp::object> it( pointRecords ), end; it != end; ++it )
	{
		bp::dict point = bp::extract<bp::dict>( *it );
		const Imath::V3f pointPosition = vectorFromObject( point.get( "P", bp::object() ) );
		const Imath::V2f raster = viewport->worldToRasterSpace( pointPosition );
		if( !pointInPolygon( polygon, raster ) )
		{
			continue;
		}

		appendUniqueId( pointIds, uint64FromObject( point.get( "pointId", bp::object() ) ) );
		appendUniqueId( strokeIds, uint64FromObject( point.get( "strokeId", bp::object() ) ) );
	}

	return selectionResultFromIds( node, pointIds, strokeIds );
}

bp::dict selectionResultFromHit( Node *node, const PaintPointsTool::HitRecord &hit, float radius )
{
	PaintPointsTool::StrokePoints strokePoints;
	strokePoints.push_back( hit );
	return selectionResultFromStroke( node, strokePoints, radius );
}

std::uint64_t selectedLayerId( Node *node, const bp::dict &currentSelection )
{
	bp::object pythonToolNode = pythonNode( node );
	bp::list strokeIds = bp::extract<bp::list>( currentSelection.get( "strokeIds", bp::list() ) );
	if( bp::len( strokeIds ) )
	{
		const std::uint64_t strokeId = uint64FromObject( strokeIds[0] );
		bp::list strokeRecords = bp::extract<bp::list>( pythonToolNode.attr( "strokeRecords" )() );
		for( bp::stl_input_iterator<bp::object> it( strokeRecords ), end; it != end; ++it )
		{
			bp::dict stroke = bp::extract<bp::dict>( *it );
			if( uint64FromObject( stroke.get( "strokeId", bp::object() ) ) == strokeId )
			{
				return uint64FromObject( stroke.get( "layerId", bp::object() ) );
			}
		}
	}

	bp::list pointIds = bp::extract<bp::list>( currentSelection.get( "pointIds", bp::list() ) );
	if( bp::len( pointIds ) )
	{
		const std::uint64_t pointId = uint64FromObject( pointIds[0] );
		bp::list pointRecords = bp::extract<bp::list>( pythonToolNode.attr( "pointRecords" )() );
		for( bp::stl_input_iterator<bp::object> it( pointRecords ), end; it != end; ++it )
		{
			bp::dict point = bp::extract<bp::dict>( *it );
			if( uint64FromObject( point.get( "pointId", bp::object() ) ) == pointId )
			{
				return uint64FromObject( point.get( "layerId", bp::object() ) );
			}
		}
	}

	return 0;
}

std::uint64_t selectedStrokeId( const bp::dict &currentSelection )
{
	bp::list strokeIds = bp::extract<bp::list>( currentSelection.get( "strokeIds", bp::list() ) );
	if( bp::len( strokeIds ) )
	{
		return uint64FromObject( strokeIds[0] );
	}
	return 0;
}

bp::dict currentSelectionState( Node *node )
{
	bp::object pythonToolNode = pythonNode( node );
	bp::dict snapshot = bp::extract<bp::dict>( pythonToolNode.attr( "cacheSnapshot" )() );
	return bp::extract<bp::dict>( snapshot.get( "currentSelection", bp::dict() ) );
}
