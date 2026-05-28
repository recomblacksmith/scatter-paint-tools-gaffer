#pragma once

struct TriangleBound
{
	Imath::V3f min = Imath::V3f( 0.0f );
	Imath::V3f max = Imath::V3f( 0.0f );
	Imath::V3f centroid = Imath::V3f( 0.0f );
};

struct TriangleBvhNode
{
	Imath::V3f min = Imath::V3f( 0.0f );
	Imath::V3f max = Imath::V3f( 0.0f );
	int leftChild = -1;
	int rightChild = -1;
	int firstTriangle = 0;
	int triangleCount = 0;

	bool isLeaf() const
	{
		return leftChild < 0 && rightChild < 0;
	}
};

struct TriangleData
{
	std::vector<Imath::V3f> positions;
	std::vector<std::array<int, 4>> triangles;
	std::vector<int> triangleLookupOffsets;
	std::vector<int> triangleIds;
	std::vector<float> ax;
	std::vector<float> ay;
	std::vector<float> az;
	std::vector<float> bx;
	std::vector<float> by;
	std::vector<float> bz;
	std::vector<float> cx;
	std::vector<float> cy;
	std::vector<float> cz;
	std::vector<TriangleBound> bounds;
	std::vector<int> bvhTriangleOffsets;
	std::vector<TriangleBvhNode> bvhNodes;
	IECoreScene::ConstMeshPrimitivePtr mesh;
};

struct ReprojectSurfaceCandidate
{
	std::string resolvedPath;
	std::string targetPath;
	std::string instanceSourcePath;
	std::uint32_t instanceId = 0;
	ScenePlug::ScenePath scenePath;
	TriangleData triangleData;
	Imath::M44f fullTransform;
	Imath::M44f inverseTransform;
	float similarityScaleSquared = 1.0f;
	bool similarityTransform = false;
};

struct ReprojectHit
{
	std::string resolvedPath;
	std::string targetPath;
	std::string instanceSourcePath;
	std::uint32_t instanceId = 0;
	int triangleIndex = -1;
	Imath::V3f barycentric = Imath::V3f( 1.0f, 0.0f, 0.0f );
	Imath::V3f objectPoint = Imath::V3f( 0.0f );
	Imath::V3f worldPoint = Imath::V3f( 0.0f );
	Imath::V2f uv = Imath::V2f( 0.0f );
	Imath::V3f objectNormal = Imath::V3f( 0.0f, 1.0f, 0.0f );
	Imath::V3f objectUp = Imath::V3f( 0.0f, 0.0f, 1.0f );
	float distanceSquared = std::numeric_limits<float>::max();
};

inline Imath::V3f cross( const Imath::V3f &a, const Imath::V3f &b )
{
	return Imath::V3f(
		( a.y * b.z ) - ( a.z * b.y ),
		( a.z * b.x ) - ( a.x * b.z ),
		( a.x * b.y ) - ( a.y * b.x )
	);
}

inline Imath::V3f normalized( const Imath::V3f &vector, const Imath::V3f &defaultValue )
{
	const float lengthSquared = vector.dot( vector );
	if( lengthSquared <= 0.0f )
	{
		return defaultValue;
	}
	return vector / std::sqrt( lengthSquared );
}

inline bool similarityTransformScaleSquared( const Imath::M44f &transform, float &scaleSquared, float epsilon = 1e-4f )
{
	Imath::V3f axisX;
	Imath::V3f axisY;
	Imath::V3f axisZ;
	transform.multDirMatrix( Imath::V3f( 1.0f, 0.0f, 0.0f ), axisX );
	transform.multDirMatrix( Imath::V3f( 0.0f, 1.0f, 0.0f ), axisY );
	transform.multDirMatrix( Imath::V3f( 0.0f, 0.0f, 1.0f ), axisZ );

	const float scaleX2 = axisX.dot( axisX );
	const float scaleY2 = axisY.dot( axisY );
	const float scaleZ2 = axisZ.dot( axisZ );
	const float maxScale2 = std::max( scaleX2, std::max( scaleY2, scaleZ2 ) );
	if( maxScale2 <= epsilon )
	{
		scaleSquared = 1.0f;
		return false;
	}

	const float scaleTolerance = maxScale2 * epsilon;
	if(
		std::abs( scaleX2 - scaleY2 ) > scaleTolerance ||
		std::abs( scaleX2 - scaleZ2 ) > scaleTolerance ||
		std::abs( scaleY2 - scaleZ2 ) > scaleTolerance
	)
	{
		scaleSquared = 1.0f;
		return false;
	}

	const float orthogonalTolerance = maxScale2 * epsilon;
	if(
		std::abs( axisX.dot( axisY ) ) > orthogonalTolerance ||
		std::abs( axisX.dot( axisZ ) ) > orthogonalTolerance ||
		std::abs( axisY.dot( axisZ ) ) > orthogonalTolerance
	)
	{
		scaleSquared = 1.0f;
		return false;
	}

	scaleSquared = ( scaleX2 + scaleY2 + scaleZ2 ) / 3.0f;
	return true;
}

inline Imath::V3f orthogonalized( const Imath::V3f &vector, const Imath::V3f &normal, const Imath::V3f &defaultValue )
{
	const Imath::V3f projected = vector - ( normal * vector.dot( normal ) );
	return normalized( projected, defaultValue );
}

inline Imath::V3f trianglePoint( const Imath::V3f &a, const Imath::V3f &b, const Imath::V3f &c, const Imath::V3f &barycentric )
{
	return ( a * barycentric.x ) + ( b * barycentric.y ) + ( c * barycentric.z );
}

inline Imath::V3f componentMin( const Imath::V3f &a, const Imath::V3f &b )
{
	return Imath::V3f(
		std::min( a.x, b.x ),
		std::min( a.y, b.y ),
		std::min( a.z, b.z )
	);
}

inline Imath::V3f componentMax( const Imath::V3f &a, const Imath::V3f &b )
{
	return Imath::V3f(
		std::max( a.x, b.x ),
		std::max( a.y, b.y ),
		std::max( a.z, b.z )
	);
}

inline int longestAxis( const Imath::V3f &extent )
{
	if( extent.x >= extent.y && extent.x >= extent.z )
	{
		return 0;
	}
	if( extent.y >= extent.z )
	{
		return 1;
	}
	return 2;
}

inline float axisValue( const Imath::V3f &value, int axis )
{
	switch( axis )
	{
		case 1:
			return value.y;
		case 2:
			return value.z;
		default:
			return value.x;
	}
}

inline TriangleBound triangleBoundFromPoints( const Imath::V3f &a, const Imath::V3f &b, const Imath::V3f &c )
{
	TriangleBound result;
	result.min = componentMin( a, componentMin( b, c ) );
	result.max = componentMax( a, componentMax( b, c ) );
	result.centroid = ( a + b + c ) / 3.0f;
	return result;
}

inline TriangleBvhNode triangleBvhNodeBounds(
	const std::vector<TriangleBound> &bounds,
	const std::vector<int> &triangleOffsets,
	int firstTriangle,
	int triangleCount
)
{
	TriangleBvhNode node;
	node.firstTriangle = firstTriangle;
	node.triangleCount = triangleCount;
	if( triangleCount <= 0 )
	{
		return node;
	}

	const TriangleBound &firstBound = bounds[triangleOffsets[firstTriangle]];
	node.min = firstBound.min;
	node.max = firstBound.max;
	for( int i = 1; i < triangleCount; ++i )
	{
		const TriangleBound &triangleBound = bounds[triangleOffsets[firstTriangle + i]];
		node.min = componentMin( node.min, triangleBound.min );
		node.max = componentMax( node.max, triangleBound.max );
	}
	return node;
}

inline int buildMedianTriangleBvh(
	const std::vector<TriangleBound> &bounds,
	std::vector<int> &triangleOffsets,
	std::vector<TriangleBvhNode> &nodes,
	int firstTriangle,
	int triangleCount,
	int leafTriangleCount = 8
)
{
	const int nodeIndex = static_cast<int>( nodes.size() );
	nodes.push_back( triangleBvhNodeBounds( bounds, triangleOffsets, firstTriangle, triangleCount ) );
	if( triangleCount <= leafTriangleCount )
	{
		return nodeIndex;
	}

	Imath::V3f centroidMin( std::numeric_limits<float>::max() );
	Imath::V3f centroidMax( -std::numeric_limits<float>::max() );
	for( int i = 0; i < triangleCount; ++i )
	{
		const TriangleBound &triangleBound = bounds[triangleOffsets[firstTriangle + i]];
		centroidMin = componentMin( centroidMin, triangleBound.centroid );
		centroidMax = componentMax( centroidMax, triangleBound.centroid );
	}
	const int axis = longestAxis( centroidMax - centroidMin );
	const int middle = firstTriangle + ( triangleCount / 2 );
	std::nth_element(
		triangleOffsets.begin() + firstTriangle,
		triangleOffsets.begin() + middle,
		triangleOffsets.begin() + firstTriangle + triangleCount,
		[&bounds, axis]( int left, int right ) {
			return axisValue( bounds[left].centroid, axis ) < axisValue( bounds[right].centroid, axis );
		}
	);

	const int leftCount = middle - firstTriangle;
	const int rightCount = triangleCount - leftCount;
	if( leftCount <= 0 || rightCount <= 0 )
	{
		return nodeIndex;
	}

	nodes[nodeIndex].leftChild = buildMedianTriangleBvh( bounds, triangleOffsets, nodes, firstTriangle, leftCount, leafTriangleCount );
	nodes[nodeIndex].rightChild = buildMedianTriangleBvh( bounds, triangleOffsets, nodes, middle, rightCount, leafTriangleCount );
	return nodeIndex;
}

inline Imath::V3f triangleUp( const Imath::V3f &a, const Imath::V3f &b, const Imath::V3f &c, const Imath::V3f &normal )
{
	const Imath::V3f edgeAB = orthogonalized( b - a, normal, Imath::V3f( 0, 0, 1 ) );
	if( edgeAB.dot( edgeAB ) > 0.0f )
	{
		return edgeAB;
	}

	const Imath::V3f edgeAC = orthogonalized( c - a, normal, Imath::V3f( 0, 0, 1 ) );
	if( edgeAC.dot( edgeAC ) > 0.0f )
	{
		return edgeAC;
	}

	Imath::V3f fallbackAxis( 0, 1, 0 );
	if( std::abs( normal.dot( fallbackAxis ) ) > 0.999f )
	{
		fallbackAxis = Imath::V3f( 1, 0, 0 );
	}
	return orthogonalized( cross( fallbackAxis, normal ), normal, Imath::V3f( 0, 0, 1 ) );
}

inline bool meshTriangleData( IECore::ConstObjectPtr object, TriangleData &result )
{
	result = TriangleData();
	result.mesh = IECore::runTimeCast<const IECoreScene::MeshPrimitive>( object.get() );
	if( !result.mesh )
	{
		return false;
	}

	const auto *positionsData = result.mesh->variableData<IECore::V3fVectorData>( "P", IECoreScene::PrimitiveVariable::Vertex );
	const IECore::IntVectorData *verticesPerFace = result.mesh->verticesPerFace();
	const IECore::IntVectorData *vertexIds = result.mesh->vertexIds();
	if( !positionsData || !verticesPerFace || !vertexIds )
	{
		return false;
	}

	result.positions = positionsData->readable();
	const auto &verticesPerFaceReadable = verticesPerFace->readable();
	const auto &vertexIdsReadable = vertexIds->readable();
	int triangleCount = 0;
	for( int faceVertexCount : verticesPerFaceReadable )
	{
		if( faceVertexCount >= 3 )
		{
			triangleCount += faceVertexCount - 2;
		}
	}
	result.triangles.reserve( triangleCount );
	result.triangleIds.reserve( triangleCount );
	result.ax.reserve( triangleCount );
	result.ay.reserve( triangleCount );
	result.az.reserve( triangleCount );
	result.bx.reserve( triangleCount );
	result.by.reserve( triangleCount );
	result.bz.reserve( triangleCount );
	result.cx.reserve( triangleCount );
	result.cy.reserve( triangleCount );
	result.cz.reserve( triangleCount );
	result.bounds.reserve( triangleCount );

	size_t offset = 0;
	int triangleIndex = 0;
	for( int faceVertexCount : verticesPerFaceReadable )
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
			const int bIndex = faceIndices[faceOffset];
			const int cIndex = faceIndices[faceOffset + 1];
			result.triangles.push_back( { triangleIndex, anchor, bIndex, cIndex } );
			result.triangleIds.push_back( triangleIndex );
			const Imath::V3f &a = result.positions[anchor];
			const Imath::V3f &b = result.positions[bIndex];
			const Imath::V3f &c = result.positions[cIndex];
			result.ax.push_back( a.x );
			result.ay.push_back( a.y );
			result.az.push_back( a.z );
			result.bx.push_back( b.x );
			result.by.push_back( b.y );
			result.bz.push_back( b.z );
			result.cx.push_back( c.x );
			result.cy.push_back( c.y );
			result.cz.push_back( c.z );
			result.bounds.push_back( triangleBoundFromPoints( a, b, c ) );
			++triangleIndex;
		}

		offset += faceVertexCount;
	}

	result.triangleLookupOffsets.assign( triangleIndex, -1 );
	for( std::size_t i = 0; i < result.triangles.size(); ++i )
	{
		const int id = result.triangles[i][0];
		if( id >= 0 && static_cast<std::size_t>( id ) < result.triangleLookupOffsets.size() )
		{
			result.triangleLookupOffsets[id] = static_cast<int>( i );
		}
	}

	result.bvhTriangleOffsets.resize( result.triangles.size() );
	for( std::size_t i = 0; i < result.bvhTriangleOffsets.size(); ++i )
	{
		result.bvhTriangleOffsets[i] = static_cast<int>( i );
	}
	if( !result.bvhTriangleOffsets.empty() )
	{
		result.bvhNodes.reserve( result.bvhTriangleOffsets.size() * 2 );
		buildMedianTriangleBvh(
			result.bounds,
			result.bvhTriangleOffsets,
			result.bvhNodes,
			0,
			static_cast<int>( result.bvhTriangleOffsets.size() )
		);
	}

	return !result.positions.empty() && !result.triangles.empty();
}

template<typename T>
inline bool indexedValue( const std::vector<T> &data, int index, const T &defaultValue, T &result )
{
	if( index < 0 || static_cast<size_t>( index ) >= data.size() )
	{
		result = defaultValue;
		return false;
	}
	result = data[index];
	return true;
}

inline Imath::V3f triangleNormal( const IECoreScene::MeshPrimitive *mesh, const std::array<int, 4> &triangle, const Imath::V3f &barycentric, const Imath::V3f &fallbackNormal )
{
	const auto it = mesh->variables.find( "N" );
	if( it == mesh->variables.end() )
	{
		return fallbackNormal;
	}

	const IECoreScene::PrimitiveVariable &normalPrimvar = it->second;
	const auto *normalData = IECore::runTimeCast<const IECore::V3fVectorData>( normalPrimvar.data.get() );
	if( !normalData )
	{
		return fallbackNormal;
	}

	const std::vector<Imath::V3f> &normals = normalData->readable();
	const IECore::IntVectorData *normalIndicesData = normalPrimvar.indices.get();
	const std::vector<int> *normalIndices = normalIndicesData ? &normalIndicesData->readable() : nullptr;

	auto fetchNormal = [&]( int index, const Imath::V3f &defaultValue ) {
		Imath::V3f value;
		if( normalIndices )
		{
			int indexed = index;
			if( !indexedValue<int>( *normalIndices, index, index, indexed ) )
			{
				return defaultValue;
			}
			index = indexed;
		}
		if( !indexedValue<Imath::V3f>( normals, index, defaultValue, value ) )
		{
			return defaultValue;
		}
		return value;
	};

	if( normalPrimvar.interpolation == IECoreScene::PrimitiveVariable::Vertex || normalPrimvar.interpolation == IECoreScene::PrimitiveVariable::Varying )
	{
		const Imath::V3f a = fetchNormal( triangle[1], fallbackNormal );
		const Imath::V3f b = fetchNormal( triangle[2], fallbackNormal );
		const Imath::V3f c = fetchNormal( triangle[3], fallbackNormal );
		return normalized( trianglePoint( a, b, c, barycentric ), fallbackNormal );
	}

	if( normalPrimvar.interpolation == IECoreScene::PrimitiveVariable::FaceVarying )
	{
		const IECore::DataPtr expanded = normalPrimvar.expandedData();
		const IECore::V3fVectorData *expandedData = IECore::runTimeCast<const IECore::V3fVectorData>( expanded.get() );
		if( !expandedData )
		{
			return fallbackNormal;
		}
		const auto &expandedReadable = expandedData->readable();
		const int faceVaryingIndex = triangle[0] * 3;
		Imath::V3f a, b, c;
		indexedValue<Imath::V3f>( expandedReadable, faceVaryingIndex, fallbackNormal, a );
		indexedValue<Imath::V3f>( expandedReadable, faceVaryingIndex + 1, fallbackNormal, b );
		indexedValue<Imath::V3f>( expandedReadable, faceVaryingIndex + 2, fallbackNormal, c );
		return normalized( trianglePoint( a, b, c, barycentric ), fallbackNormal );
	}

	if( normalPrimvar.interpolation == IECoreScene::PrimitiveVariable::Uniform )
	{
		Imath::V3f value;
		indexedValue<Imath::V3f>( normals, triangle[0], fallbackNormal, value );
		return normalized( value, fallbackNormal );
	}

	return fallbackNormal;
}

inline bool triangleUV( const IECoreScene::MeshPrimitive *mesh, const std::array<int, 4> &triangle, const Imath::V3f &barycentric, Imath::V2f &uv )
{
	const auto it = mesh->variables.find( "uv" );
	if( it == mesh->variables.end() )
	{
		uv = Imath::V2f( barycentric.y, barycentric.z );
		return false;
	}

	const IECoreScene::PrimitiveVariable &uvPrimvar = it->second;
	const auto *uvData = IECore::runTimeCast<const IECore::V2fVectorData>( uvPrimvar.data.get() );
	if( !uvData )
	{
		uv = Imath::V2f( barycentric.y, barycentric.z );
		return false;
	}

	const std::vector<Imath::V2f> &uvs = uvData->readable();
	const IECore::IntVectorData *uvIndicesData = uvPrimvar.indices.get();
	const std::vector<int> *uvIndices = uvIndicesData ? &uvIndicesData->readable() : nullptr;

	auto fetchUV = [&]( int index, const Imath::V2f &defaultValue ) {
		Imath::V2f value;
		if( uvIndices )
		{
			int indexed = index;
			if( !indexedValue<int>( *uvIndices, index, index, indexed ) )
			{
				return defaultValue;
			}
			index = indexed;
		}
		if( !indexedValue<Imath::V2f>( uvs, index, defaultValue, value ) )
		{
			return defaultValue;
		}
		return value;
	};

	if( uvPrimvar.interpolation == IECoreScene::PrimitiveVariable::Vertex || uvPrimvar.interpolation == IECoreScene::PrimitiveVariable::Varying )
	{
		const Imath::V2f a = fetchUV( triangle[1], Imath::V2f( 0.0f ) );
		const Imath::V2f b = fetchUV( triangle[2], Imath::V2f( 0.0f ) );
		const Imath::V2f c = fetchUV( triangle[3], Imath::V2f( 0.0f ) );
		uv = ( a * barycentric.x ) + ( b * barycentric.y ) + ( c * barycentric.z );
		return true;
	}

	if( uvPrimvar.interpolation == IECoreScene::PrimitiveVariable::FaceVarying )
	{
		const IECore::DataPtr expanded = uvPrimvar.expandedData();
		const IECore::V2fVectorData *expandedData = IECore::runTimeCast<const IECore::V2fVectorData>( expanded.get() );
		if( expandedData )
		{
			const auto &expandedReadable = expandedData->readable();
			const int faceVaryingIndex = triangle[0] * 3;
			Imath::V2f a, b, c;
			indexedValue<Imath::V2f>( expandedReadable, faceVaryingIndex, Imath::V2f( 0.0f ), a );
			indexedValue<Imath::V2f>( expandedReadable, faceVaryingIndex + 1, Imath::V2f( 0.0f ), b );
			indexedValue<Imath::V2f>( expandedReadable, faceVaryingIndex + 2, Imath::V2f( 0.0f ), c );
			uv = ( a * barycentric.x ) + ( b * barycentric.y ) + ( c * barycentric.z );
			return true;
		}
	}

	uv = Imath::V2f( barycentric.y, barycentric.z );
	return false;
}

inline Imath::V3f closestPointOnTriangle( const Imath::V3f &a, const Imath::V3f &b, const Imath::V3f &c, Imath::V3f point, Imath::V3f &barycentric )
{
	const Imath::V3f ab = b - a;
	const Imath::V3f ac = c - a;
	const Imath::V3f ap = point - a;
	const float d1 = ab.dot( ap );
	const float d2 = ac.dot( ap );
	if( d1 <= 0.0f && d2 <= 0.0f )
	{
		barycentric = Imath::V3f( 1.0f, 0.0f, 0.0f );
		return a;
	}

	const Imath::V3f bp = point - b;
	const float d3 = ab.dot( bp );
	const float d4 = ac.dot( bp );
	if( d3 >= 0.0f && d4 <= d3 )
	{
		barycentric = Imath::V3f( 0.0f, 1.0f, 0.0f );
		return b;
	}

	const float vc = d1 * d4 - d3 * d2;
	if( vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f )
	{
		const float v = d1 / ( d1 - d3 );
		barycentric = Imath::V3f( 1.0f - v, v, 0.0f );
		return a + ( ab * v );
	}

	const Imath::V3f cp = point - c;
	const float d5 = ab.dot( cp );
	const float d6 = ac.dot( cp );
	if( d6 >= 0.0f && d5 <= d6 )
	{
		barycentric = Imath::V3f( 0.0f, 0.0f, 1.0f );
		return c;
	}

	const float vb = d5 * d2 - d1 * d6;
	if( vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f )
	{
		const float w = d2 / ( d2 - d6 );
		barycentric = Imath::V3f( 1.0f - w, 0.0f, w );
		return a + ( ac * w );
	}

	const float va = d3 * d6 - d5 * d4;
	if( va <= 0.0f && ( d4 - d3 ) >= 0.0f && ( d5 - d6 ) >= 0.0f )
	{
		const Imath::V3f bc = c - b;
		const float w = ( d4 - d3 ) / ( ( d4 - d3 ) + ( d5 - d6 ) );
		barycentric = Imath::V3f( 0.0f, 1.0f - w, w );
		return b + ( bc * w );
	}

	const float denominator = 1.0f / ( va + vb + vc );
	const float v = vb * denominator;
	const float w = vc * denominator;
	barycentric = Imath::V3f( 1.0f - v - w, v, w );
	return a + ( ab * v ) + ( ac * w );
}

struct PointSurfaceReference
{
	std::string targetPath;
	std::string instanceSourcePath;
	std::uint32_t instanceId = 0;
	std::string resolvedPath;
};

inline std::vector<std::string> splitFilterTokens( const std::string &value )
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

inline PointSurfaceReference pointSurfaceReferenceFromResolvedPath( const std::string &resolvedPath )
{
	PointSurfaceReference result;
	result.resolvedPath = resolvedPath;

	const std::string instanceMarker = "/instances/";
	const size_t markerPos = resolvedPath.find( instanceMarker );
	const size_t lastSlash = resolvedPath.find_last_of( '/' );
	if( markerPos != std::string::npos && lastSlash != std::string::npos && lastSlash > markerPos + instanceMarker.size() )
	{
		try
		{
			result.instanceId = static_cast<std::uint32_t>( std::stoul( resolvedPath.substr( lastSlash + 1 ) ) );
			result.instanceSourcePath = resolvedPath.substr( 0, lastSlash );
			result.targetPath = resolvedPath.substr( 0, markerPos );
			return result;
		}
		catch( ... )
		{
		}
	}

	result.targetPath = resolvedPath;
	return result;
}

inline PointSurfaceReference pointSurfaceReference( const bp::dict &store, const bp::dict &point )
{
	PointSurfaceReference result;
	result.targetPath = pathFromId( store, "scenePaths", dictValue<std::uint32_t>( point, "targetPathId", 0 ) );
	result.instanceSourcePath = pathFromId( store, "instanceSourcePaths", dictValue<std::uint32_t>( point, "instanceSourcePathId", 0 ) );
	result.instanceId = dictValue<std::uint32_t>( point, "instanceId", 0 );
	result.resolvedPath = !result.instanceSourcePath.empty() ? result.instanceSourcePath + "/" + std::to_string( result.instanceId ) : result.targetPath;
	return result;
}

inline void appendMatchedScenePaths(
	const ScenePlug *scene,
	const ScenePlug::ScenePath &path,
	const IECore::PathMatcher &matcher,
	std::vector<std::string> &paths
)
{
	if( !scene )
	{
		return;
	}

	const std::string pathString = ScenePlug::pathToString( path );
	const unsigned match = path.empty() ? IECore::PathMatcher::DescendantMatch : matcher.match( pathString );
	if( !path.empty() && ( match & IECore::PathMatcher::ExactMatch ) )
	{
		paths.push_back( pathString );
	}

	if( !path.empty() && !( match & ( IECore::PathMatcher::DescendantMatch | IECore::PathMatcher::AncestorMatch | IECore::PathMatcher::ExactMatch ) ) )
	{
		return;
	}

	IECore::ConstInternedStringVectorDataPtr childNamesData;
	try
	{
		childNamesData = scene->childNames( path );
	}
	catch( ... )
	{
		return;
	}

	if( !childNamesData )
	{
		return;
	}

	for( const IECore::InternedString &childName : childNamesData->readable() )
	{
		ScenePlug::ScenePath childPath = path;
		childPath.push_back( childName );
		appendMatchedScenePaths( scene, childPath, matcher, paths );
	}
}

inline void appendUniqueReference(
	std::vector<PointSurfaceReference> &references,
	std::set<std::string> &seenPaths,
	const PointSurfaceReference &reference
)
{
	if( reference.resolvedPath.empty() || seenPaths.count( reference.resolvedPath ) )
	{
		return;
	}
	seenPaths.insert( reference.resolvedPath );
	references.push_back( reference );
}

inline std::vector<PointSurfaceReference> filteredSurfaceReferences( const ScenePlug *scene, const PaintedPoints *node )
{
	std::vector<PointSurfaceReference> result;
	std::set<std::string> seenPaths;
	if( !scene || !node )
	{
		return result;
	}

	const std::string targetFilter = node->targetFilterPlug()->getValue();
	if( !targetFilter.empty() )
	{
		IECore::PathMatcher matcher;
		for( const std::string &token : splitFilterTokens( targetFilter ) )
		{
			matcher.addPath( token );
		}

		std::vector<std::string> matchedPaths;
		appendMatchedScenePaths( scene, ScenePlug::ScenePath(), matcher, matchedPaths );
		for( const std::string &path : matchedPaths )
		{
			appendUniqueReference( result, seenPaths, pointSurfaceReferenceFromResolvedPath( path ) );
		}
	}

	const std::string targetSetFilter = node->targetSetFilterPlug()->getValue();
	if( !targetSetFilter.empty() )
	{
		std::vector<std::string> matchedPaths;
		try
		{
			IECore::PathMatcher matcher = GafferScene::SetAlgo::evaluateSetExpression( targetSetFilter, scene );
			matcher.paths( matchedPaths );
		}
		catch( ... )
		{
			matchedPaths.clear();
		}

		for( const std::string &path : matchedPaths )
		{
			appendUniqueReference( result, seenPaths, pointSurfaceReferenceFromResolvedPath( path ) );
		}
	}

	return result;
}

inline std::set<std::string> selectedInstanceResolvedPaths( const bp::dict &store, const std::set<std::uint64_t> &pointIds )
{
	std::set<std::string> result;
	const bp::list points = bp::extract<bp::list>( store["points"] );
	for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
	{
		const bp::dict point = bp::extract<bp::dict>( *it );
		if( !pointIds.count( dictValue<std::uint64_t>( point, "pointId", 0 ) ) )
		{
			continue;
		}

		const PointSurfaceReference reference = pointSurfaceReference( store, point );
		if( !reference.instanceSourcePath.empty() )
		{
			result.insert( reference.resolvedPath );
		}
	}
	return result;
}

inline bool canReprojectToCandidate(
	const PointSurfaceReference &currentReference,
	const PointSurfaceReference &candidateReference,
	const std::set<std::string> &selectedInstancePaths
)
{
	if( candidateReference.resolvedPath == currentReference.resolvedPath )
	{
		return true;
	}

	if( candidateReference.instanceSourcePath.empty() )
	{
		return true;
	}

	return selectedInstancePaths.size() > 1 && selectedInstancePaths.count( candidateReference.resolvedPath );
}

inline bool reprojectSurfaceCandidate( const ScenePlug *scene, const PointSurfaceReference &reference, ReprojectSurfaceCandidate &candidate )
{
	if( !scene || reference.resolvedPath.empty() )
	{
		return false;
	}

	const ScenePlug::ScenePath scenePath = ScenePlug::stringToPath( reference.resolvedPath );
	if( !scene->exists( scenePath ) )
	{
		return false;
	}

	IECore::ConstObjectPtr object;
	Imath::M44f fullTransform;
	try
	{
		object = scene->object( scenePath );
		fullTransform = scene->fullTransform( scenePath );
	}
	catch( ... )
	{
		return false;
	}

	TriangleData triangleData;
	if( !meshTriangleData( object, triangleData ) )
	{
		return false;
	}

	candidate.resolvedPath = reference.resolvedPath;
	candidate.targetPath = reference.targetPath;
	candidate.instanceSourcePath = reference.instanceSourcePath;
	candidate.instanceId = reference.instanceId;
	candidate.scenePath = scenePath;
	candidate.triangleData = triangleData;
	candidate.fullTransform = fullTransform;
	candidate.inverseTransform = fullTransform.inverse();
	candidate.similarityTransform = similarityTransformScaleSquared( fullTransform, candidate.similarityScaleSquared );
	return true;
}

inline bool resolvedWorldReference( const ScenePlug *scene, const bp::dict &store, const bp::dict &point, Imath::V3f &referenceWorld )
{
	const PointSurfaceReference reference = pointSurfaceReference( store, point );
	if( scene && !reference.resolvedPath.empty() )
	{
		ReprojectSurfaceCandidate candidate;
		if( reprojectSurfaceCandidate( scene, reference, candidate ) )
		{
			const int triangleIndex = dictValue<int>( point, "triangleIndex", -1 );
			auto triangleIt = std::find_if(
				candidate.triangleData.triangles.begin(),
				candidate.triangleData.triangles.end(),
				[triangleIndex]( const std::array<int, 4> &triangle ) { return triangle[0] == triangleIndex; }
			);
			if( triangleIt != candidate.triangleData.triangles.end() )
			{
				const std::array<int, 4> &triangle = *triangleIt;
				if(
					triangle[1] >= 0 && triangle[2] >= 0 && triangle[3] >= 0 &&
					static_cast<size_t>( triangle[1] ) < candidate.triangleData.positions.size() &&
					static_cast<size_t>( triangle[2] ) < candidate.triangleData.positions.size() &&
					static_cast<size_t>( triangle[3] ) < candidate.triangleData.positions.size()
				)
				{
					const Imath::V3f barycentric = vectorFromValues( dictGet( point, "barycentric" ), Imath::V3f( 1.0f, 0.0f, 0.0f ) );
					const Imath::V3f a = candidate.triangleData.positions[triangle[1]];
					const Imath::V3f b = candidate.triangleData.positions[triangle[2]];
					const Imath::V3f c = candidate.triangleData.positions[triangle[3]];
					referenceWorld = trianglePoint( a, b, c, barycentric ) * candidate.fullTransform;
					return true;
				}
			}

			referenceWorld = vectorFromValues( dictGet( point, "restObjectP" ), Imath::V3f( 0.0f ) ) * candidate.fullTransform;
			return true;
		}
	}

	referenceWorld = vectorFromValues( dictGet( point, "restWorldP" ), Imath::V3f( 0.0f ) );
	return true;
}

inline bool closestReprojectHit( const ReprojectSurfaceCandidate &candidate, const Imath::V3f &referenceWorld, ReprojectHit &hit )
{
	const Imath::V3f referenceObject = referenceWorld * candidate.inverseTransform;
	bool found = false;
	for( const std::array<int, 4> &triangle : candidate.triangleData.triangles )
	{
		if(
			triangle[1] < 0 || triangle[2] < 0 || triangle[3] < 0 ||
			static_cast<size_t>( triangle[1] ) >= candidate.triangleData.positions.size() ||
			static_cast<size_t>( triangle[2] ) >= candidate.triangleData.positions.size() ||
			static_cast<size_t>( triangle[3] ) >= candidate.triangleData.positions.size()
		)
		{
			continue;
		}

		const Imath::V3f a = candidate.triangleData.positions[triangle[1]];
		const Imath::V3f b = candidate.triangleData.positions[triangle[2]];
		const Imath::V3f c = candidate.triangleData.positions[triangle[3]];
		Imath::V3f barycentric;
		const Imath::V3f objectPoint = closestPointOnTriangle( a, b, c, referenceObject, barycentric );
		const Imath::V3f worldPoint = objectPoint * candidate.fullTransform;
		const float distanceSquared = ( worldPoint - referenceWorld ).length2();
		if( found && distanceSquared >= hit.distanceSquared )
		{
			continue;
		}

		const Imath::V3f faceNormal = normalized( cross( b - a, c - a ), Imath::V3f( 0, 1, 0 ) );
		const Imath::V3f objectNormal = triangleNormal( candidate.triangleData.mesh.get(), triangle, barycentric, faceNormal );
		const Imath::V3f objectUp = triangleUp( a, b, c, objectNormal );
		Imath::V2f uv;
		triangleUV( candidate.triangleData.mesh.get(), triangle, barycentric, uv );

		hit.resolvedPath = candidate.resolvedPath;
		hit.targetPath = candidate.targetPath;
		hit.instanceSourcePath = candidate.instanceSourcePath;
		hit.instanceId = candidate.instanceId;
		hit.triangleIndex = triangle[0];
		hit.barycentric = barycentric;
		hit.objectPoint = objectPoint;
		hit.worldPoint = worldPoint;
		hit.objectNormal = objectNormal;
		hit.objectUp = objectUp;
		hit.uv = uv;
		hit.distanceSquared = distanceSquared;
		found = true;
	}

	return found;
}
