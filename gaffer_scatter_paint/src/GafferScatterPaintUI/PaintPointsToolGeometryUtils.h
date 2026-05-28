#pragma once

std::string scenePathString( const GafferScene::ScenePlug::ScenePath &path )
{
	if( path.empty() )
	{
		return "/";
	}

	std::string result;
	for( const auto &segment : path )
	{
		result += "/" + segment.string();
	}
	return result.empty() ? "/" : result;
}

float vectorLengthSquared( const Imath::V3f &vector )
{
	return vector.dot( vector );
}

PaintPointsTool::StrokePoints densifiedStroke(
	const PaintPointsTool::StrokePoints &strokePoints,
	float spacing
)
{
	if( strokePoints.size() < 2 )
	{
		return strokePoints;
	}

	PaintPointsTool::StrokePoints result;
	result.push_back( strokePoints.front() );

	for( size_t i = 1; i < strokePoints.size(); ++i )
	{
		const PaintPointsTool::HitRecord &previous = strokePoints[i - 1];
		const PaintPointsTool::HitRecord &current = strokePoints[i];
		const Imath::V3f delta = current.point - previous.point;
		const float distance = delta.length();
		const int steps = std::max( 1, static_cast<int>( std::ceil( distance / spacing ) ) );

		for( int step = 1; step <= steps; ++step )
		{
			const float t = static_cast<float>( step ) / static_cast<float>( steps );
			PaintPointsTool::HitRecord sample = previous;
			sample.point = previous.point + ( delta * t );
			sample.normal = ( previous.normal * ( 1.0f - t ) ) + ( current.normal * t );
			if( sample.normal.length2() > 0.0f )
			{
				sample.normal.normalize();
			}
			sample.barycentric = ( previous.barycentric * ( 1.0f - t ) ) + ( current.barycentric * t );
			sample.attachmentResolved = previous.attachmentResolved && current.attachmentResolved;
			sample.sampleOrigin = PaintPointsTool::SampleOrigin::DensifiedHit;
			sample.path = t < 1.0f ? previous.path : current.path;
			sample.triangleIndex = t < 1.0f ? previous.triangleIndex : current.triangleIndex;
			result.push_back( sample );
		}
	}

	return result;
}

std::pair<Imath::V3f, Imath::V3f> closestPointOnTriangle(
	const Imath::V3f &point,
	const Imath::V3f &a,
	const Imath::V3f &b,
	const Imath::V3f &c
)
{
	const Imath::V3f ab = b - a;
	const Imath::V3f ac = c - a;
	const Imath::V3f ap = point - a;

	const float d1 = ab.dot( ap );
	const float d2 = ac.dot( ap );
	if( d1 <= 0.0f && d2 <= 0.0f )
	{
		return { a, Imath::V3f( 1.0f, 0.0f, 0.0f ) };
	}

	const Imath::V3f bp = point - b;
	const float d3 = ab.dot( bp );
	const float d4 = ac.dot( bp );
	if( d3 >= 0.0f && d4 <= d3 )
	{
		return { b, Imath::V3f( 0.0f, 1.0f, 0.0f ) };
	}

	const float vc = ( d1 * d4 ) - ( d3 * d2 );
	if( vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f )
	{
		const float v = d1 != d3 ? d1 / ( d1 - d3 ) : 0.0f;
		return { a + ( ab * v ), Imath::V3f( 1.0f - v, v, 0.0f ) };
	}

	const Imath::V3f cp = point - c;
	const float d5 = ab.dot( cp );
	const float d6 = ac.dot( cp );
	if( d6 >= 0.0f && d5 <= d6 )
	{
		return { c, Imath::V3f( 0.0f, 0.0f, 1.0f ) };
	}

	const float vb = ( d5 * d2 ) - ( d1 * d6 );
	if( vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f )
	{
		const float w = d2 != d6 ? d2 / ( d2 - d6 ) : 0.0f;
		return { a + ( ac * w ), Imath::V3f( 1.0f - w, 0.0f, w ) };
	}

	const float va = ( d3 * d6 ) - ( d5 * d4 );
	if( va <= 0.0f && ( d4 - d3 ) >= 0.0f && ( d5 - d6 ) >= 0.0f )
	{
		const Imath::V3f edge = c - b;
		const float denom = ( d4 - d3 ) + ( d5 - d6 );
		const float w = denom ? ( d4 - d3 ) / denom : 0.0f;
		return { b + ( edge * w ), Imath::V3f( 0.0f, 1.0f - w, w ) };
	}

	const float denom = va + vb + vc;
	if( !denom )
	{
		return { a, Imath::V3f( 1.0f, 0.0f, 0.0f ) };
	}

	const float v = vb / denom;
	const float w = vc / denom;
	const float u = 1.0f - v - w;
	return { ( a * u ) + ( b * v ) + ( c * w ), Imath::V3f( u, v, w ) };
}

bool buildTriangleCacheEntry( IECore::ConstObjectPtr object, PaintPointsTool::TriangleCacheEntry &entry )
{
	entry = PaintPointsTool::TriangleCacheEntry();
	entry.mesh = IECore::runTimeCast<const MeshPrimitive>( object.get() );
	if( !entry.mesh )
	{
		return false;
	}

	const auto *positionsData = entry.mesh->variableData<V3fVectorData>( "P", PrimitiveVariable::Vertex );
	const IntVectorData *verticesPerFace = entry.mesh->verticesPerFace();
	const IntVectorData *vertexIds = entry.mesh->vertexIds();
	if( !positionsData || !verticesPerFace || !vertexIds )
	{
		return false;
	}

	entry.positions = positionsData->readable();
	const auto &verticesPerFaceReadable = verticesPerFace->readable();
	const auto &vertexIdsReadable = vertexIds->readable();

	size_t offset = 0;
	int triangleIndex = 0;
	for( const int faceVertexCount : verticesPerFaceReadable )
	{
		if( faceVertexCount < 3 )
		{
			offset += std::max( faceVertexCount, 0 );
			continue;
		}

		std::vector<int> faceIndices;
		faceIndices.reserve( faceVertexCount );
		for( int i = 0; i < faceVertexCount; ++i )
		{
			faceIndices.push_back( vertexIdsReadable[offset + i] );
		}

		const int anchor = faceIndices[0];
		for( int faceOffset = 1; faceOffset < faceVertexCount - 1; ++faceOffset )
		{
			entry.triangles.push_back( { triangleIndex, anchor, faceIndices[faceOffset], faceIndices[faceOffset + 1] } );
			++triangleIndex;
		}

		offset += faceVertexCount;
	}

	return !entry.positions.empty() && !entry.triangles.empty();
}
