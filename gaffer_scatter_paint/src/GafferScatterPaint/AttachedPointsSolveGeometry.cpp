#include "AttachedPointsPrivate.h"

using namespace Gaffer;
using namespace GafferScatterPaint;
using namespace GafferScatterPaint::AttachedPointsPrivate;
using namespace GafferScene;
using namespace IECore;
using namespace IECoreScene;

namespace GafferScatterPaint
{

namespace AttachedPointsPrivate
{

StringVectorDataPtr stringVectorData( const std::vector<std::string> &values )
{
	StringVectorDataPtr result = new StringVectorData();
	result->writable() = values;
	return result;
}

V3fVectorDataPtr v3fVectorData( const std::vector<Imath::V3f> &values, IECore::GeometricData::Interpretation interpretation )
{
	V3fVectorDataPtr result = new V3fVectorData();
	result->writable() = values;
	result->setInterpretation( interpretation );
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

FloatVectorDataPtr floatVectorData( const std::vector<float> &values )
{
	FloatVectorDataPtr result = new FloatVectorData();
	result->writable() = values;
	return result;
}

IntVectorDataPtr intVectorData( const std::vector<int> &values )
{
	IntVectorDataPtr result = new IntVectorData();
	result->writable() = values;
	return result;
}

Int64VectorDataPtr int64VectorData( const std::vector<int64_t> &values )
{
	Int64VectorDataPtr result = new Int64VectorData();
	result->writable() = values;
	return result;
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

std::set<std::string> parseAttributeFilter( const std::string &filter )
{
	std::set<std::string> result;
	std::stringstream stream( filter );
	std::string token;
	while( std::getline( stream, token, ',' ) )
	{
		token = trimmed( token );
		if( !token.empty() )
		{
			result.insert( token );
		}
	}
	return result;
}

ExportPreset exportPresetValue( int value )
{
	switch( value )
	{
		case 1:
			return ExportPreset::Full;
		case 2:
			return ExportPreset::Custom;
		case 0:
		default:
			return ExportPreset::Minimal;
	}
}

bool includeOptionalAttribute( ExportPreset preset, const std::set<std::string> &customAttributes, const std::string &attributeName )
{
	if( preset == ExportPreset::Full )
	{
		return true;
	}
	if( preset == ExportPreset::Custom )
	{
		return customAttributes.find( attributeName ) != customAttributes.end();
	}
	return false;
}

Imath::V3f cross( const Imath::V3f &a, const Imath::V3f &b )
{
	return Imath::V3f(
		( a.y * b.z ) - ( a.z * b.y ),
		( a.z * b.x ) - ( a.x * b.z ),
		( a.x * b.y ) - ( a.y * b.x )
	);
}

Imath::V3f normalized( const Imath::V3f &vector, const Imath::V3f &defaultValue )
{
	const float lengthSquared = vector.dot( vector );
	if( lengthSquared <= 0.0f )
	{
		return defaultValue;
	}
	return vector / std::sqrt( lengthSquared );
}

Imath::V3f orthogonalized( const Imath::V3f &vector, const Imath::V3f &normal, const Imath::V3f &defaultValue )
{
	const Imath::V3f projected = vector - ( normal * vector.dot( normal ) );
	return normalized( projected, defaultValue );
}

Imath::V3f trianglePoint( const Imath::V3f &a, const Imath::V3f &b, const Imath::V3f &c, const Imath::V3f &barycentric )
{
	return ( a * barycentric.x ) + ( b * barycentric.y ) + ( c * barycentric.z );
}

Imath::V3f triangleUp( const Imath::V3f &a, const Imath::V3f &b, const Imath::V3f &c, const Imath::V3f &normal )
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

Imath::Quatf quatFromAxes( const Imath::V3f &xAxis, const Imath::V3f &yAxis, const Imath::V3f &zAxis )
{
	const float m00 = xAxis.x;
	const float m01 = yAxis.x;
	const float m02 = zAxis.x;
	const float m10 = xAxis.y;
	const float m11 = yAxis.y;
	const float m12 = zAxis.y;
	const float m20 = xAxis.z;
	const float m21 = yAxis.z;
	const float m22 = zAxis.z;

	const float trace = m00 + m11 + m22;
	if( trace > 0.0f )
	{
		const float s = std::sqrt( trace + 1.0f ) * 2.0f;
		return Imath::Quatf(
			0.25f * s,
			Imath::V3f(
				( m21 - m12 ) / s,
				( m02 - m20 ) / s,
				( m10 - m01 ) / s
			)
		);
	}

	if( m00 > m11 && m00 > m22 )
	{
		const float s = std::sqrt( 1.0f + m00 - m11 - m22 ) * 2.0f;
		return Imath::Quatf(
			( m21 - m12 ) / s,
			Imath::V3f(
				0.25f * s,
				( m01 + m10 ) / s,
				( m02 + m20 ) / s
			)
		);
	}

	if( m11 > m22 )
	{
		const float s = std::sqrt( 1.0f + m11 - m00 - m22 ) * 2.0f;
		return Imath::Quatf(
			( m02 - m20 ) / s,
			Imath::V3f(
				( m01 + m10 ) / s,
				0.25f * s,
				( m12 + m21 ) / s
			)
		);
	}

	const float s = std::sqrt( 1.0f + m22 - m00 - m11 ) * 2.0f;
	return Imath::Quatf(
		( m10 - m01 ) / s,
		Imath::V3f(
			( m02 + m20 ) / s,
			( m12 + m21 ) / s,
			0.25f * s
		)
	);
}

Imath::Quatf orientFromNormalUp( const Imath::V3f &normal, const Imath::V3f &up )
{
	const Imath::V3f zAxis = normalized( normal, Imath::V3f( 0, 0, 1 ) );
	Imath::V3f yAxis = orthogonalized( up, zAxis, Imath::V3f( 0, 1, 0 ) );
	const Imath::V3f xAxis = normalized( cross( yAxis, zAxis ), Imath::V3f( 1, 0, 0 ) );
	yAxis = normalized( cross( zAxis, xAxis ), Imath::V3f( 0, 1, 0 ) );
	return quatFromAxes( xAxis, yAxis, zAxis );
}

Imath::Quatf axisAngleOrient( const Imath::V3f &axis, float angle )
{
	const Imath::V3f safeAxis = normalized( axis, Imath::V3f( 0, 0, 1 ) );
	const float halfAngle = angle * 0.5f;
	return Imath::Quatf( std::cos( halfAngle ), safeAxis * std::sin( halfAngle ) );
}

Imath::Quatf composedOrient( const Imath::V3f &normal, const Imath::V3f &up, float normalSpin, const Imath::V2f &tangentRotation )
{
	const Imath::V3f safeNormal = normalized( normal, Imath::V3f( 0, 0, 1 ) );
	const Imath::V3f safeUp = orthogonalized( up, safeNormal, Imath::V3f( 0, 1, 0 ) );
	const Imath::V3f tangent = normalized( cross( safeUp, safeNormal ), Imath::V3f( 1, 0, 0 ) );
	Imath::Quatf result = orientFromNormalUp( safeNormal, safeUp );
	result *= axisAngleOrient( safeNormal, normalSpin );
	result *= axisAngleOrient( tangent, tangentRotation.x );
	result *= axisAngleOrient( safeUp, tangentRotation.y );
	result.normalize();
	return result;
}


bool meshTriangleData( ConstObjectPtr object, TriangleData &result )
{
	result = TriangleData();
	result.mesh = runTimeCast<const MeshPrimitive>( object.get() );
	if( !result.mesh )
	{
		return false;
	}

	const auto *positionsData = result.mesh->variableData<V3fVectorData>( "P", PrimitiveVariable::Vertex );
	const IntVectorData *verticesPerFace = result.mesh->verticesPerFace();
	const IntVectorData *vertexIds = result.mesh->vertexIds();
	if( !positionsData || !verticesPerFace || !vertexIds )
	{
		return false;
	}

	result.positions = positionsData->readable();
	const auto &verticesPerFaceReadable = verticesPerFace->readable();
	const auto &vertexIdsReadable = vertexIds->readable();

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
			result.triangleOffsets.push_back( static_cast<int>( result.triangles.size() ) );
			result.triangles.push_back( { triangleIndex, anchor, faceIndices[faceOffset], faceIndices[faceOffset + 1] } );
			++triangleIndex;
		}

		offset += faceVertexCount;
	}

	return true;
}

bool supportsCurrentSolveMesh( const TriangleData &triangleData )
{
	return
		triangleData.mesh &&
		!triangleData.positions.empty() &&
		!triangleData.triangles.empty();
}

const std::array<int, 4> *triangleForIndex( const TriangleData &triangleData, int triangleIndex )
{
	if( triangleIndex < 0 || static_cast<size_t>( triangleIndex ) >= triangleData.triangleOffsets.size() )
	{
		return nullptr;
	}

	const int triangleOffset = triangleData.triangleOffsets[triangleIndex];
	if( triangleOffset < 0 || static_cast<size_t>( triangleOffset ) >= triangleData.triangles.size() )
	{
		return nullptr;
	}

	const std::array<int, 4> &triangle = triangleData.triangles[triangleOffset];
	return triangle[0] == triangleIndex ? &triangle : nullptr;
}

template<typename T>
bool indexedValue( const std::vector<T> &data, int index, const T &defaultValue, T &result )
{
	if( index < 0 || static_cast<size_t>( index ) >= data.size() )
	{
		result = defaultValue;
		return false;
	}
	result = data[index];
	return true;
}

Imath::V3f triangleNormal( const MeshPrimitive *mesh, const std::array<int, 4> &triangle, const Imath::V3f &barycentric, const Imath::V3f &fallbackNormal )
{
	const auto it = mesh->variables.find( "N" );
	if( it == mesh->variables.end() )
	{
		return fallbackNormal;
	}

	const PrimitiveVariable &normalPrimvar = it->second;
	const auto *normalData = runTimeCast<const V3fVectorData>( normalPrimvar.data.get() );
	if( !normalData )
	{
		return fallbackNormal;
	}

	const std::vector<Imath::V3f> &normals = normalData->readable();
	const IntVectorData *normalIndicesData = normalPrimvar.indices.get();
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

	if( normalPrimvar.interpolation == PrimitiveVariable::Vertex || normalPrimvar.interpolation == PrimitiveVariable::Varying )
	{
		const Imath::V3f a = fetchNormal( triangle[1], fallbackNormal );
		const Imath::V3f b = fetchNormal( triangle[2], fallbackNormal );
		const Imath::V3f c = fetchNormal( triangle[3], fallbackNormal );
		return normalized( trianglePoint( a, b, c, barycentric ), fallbackNormal );
	}

	if( normalPrimvar.interpolation == PrimitiveVariable::FaceVarying )
	{
		const DataPtr expanded = normalPrimvar.expandedData();
		const V3fVectorData *expandedData = runTimeCast<const V3fVectorData>( expanded.get() );
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

	if( normalPrimvar.interpolation == PrimitiveVariable::Uniform )
	{
		Imath::V3f value;
		indexedValue<Imath::V3f>( normals, triangle[0], fallbackNormal, value );
		return normalized( value, fallbackNormal );
	}

	return fallbackNormal;
}

PointsPrimitivePtr pointsPrimitiveFromRecords(
	const std::vector<OutputPointRecord> &pointRecords,
	const std::string &pointType,
	ExportPreset exportPreset,
	const std::string &includeAttributes,
	bool debugColor
)
{
	std::vector<Imath::V3f> positions;
	std::vector<float> widths;
	std::vector<float> scales;
	std::vector<int64_t> ids;
	std::vector<int> seeds;
	std::vector<int64_t> strokeIds;
	std::vector<int> triangleIndices;
	std::vector<Imath::V3f> barycentrics;
	std::vector<int> attachmentResolved;
	std::vector<Imath::Color3f> authoredColors;
	std::vector<Imath::Color3f> colors;
	std::vector<std::string> sourcePaths;
	std::vector<Imath::V3f> normals;
	std::vector<Imath::V3f> ups;
	std::vector<Imath::Quatf> orients;

	positions.reserve( pointRecords.size() );
	widths.reserve( pointRecords.size() );
	scales.reserve( pointRecords.size() );
	ids.reserve( pointRecords.size() );
	seeds.reserve( pointRecords.size() );
	strokeIds.reserve( pointRecords.size() );
	triangleIndices.reserve( pointRecords.size() );
	barycentrics.reserve( pointRecords.size() );
	attachmentResolved.reserve( pointRecords.size() );
	authoredColors.reserve( pointRecords.size() );
	colors.reserve( pointRecords.size() );
	sourcePaths.reserve( pointRecords.size() );
	normals.reserve( pointRecords.size() );
	ups.reserve( pointRecords.size() );
	orients.reserve( pointRecords.size() );

	for( const OutputPointRecord &point : pointRecords )
	{
		positions.push_back( point.position );
		widths.push_back( point.width );
		scales.push_back( point.scale );
		ids.push_back( point.pointId );
		seeds.push_back( point.seed );
		strokeIds.push_back( point.strokeId );
		triangleIndices.push_back( point.triangleIndex );
		barycentrics.push_back( point.barycentric );
		attachmentResolved.push_back( point.attachmentResolved ? 1 : 0 );
		authoredColors.push_back( point.color );
		colors.push_back( debugColor ? ( point.attachmentResolved ? g_debugResolvedColor : g_debugUnresolvedColor ) : point.color );
		sourcePaths.push_back( point.sourcePath );
		normals.push_back( point.normal );
		ups.push_back( point.up );
		orients.push_back( point.orient );
	}

	PointsPrimitivePtr primitive = new PointsPrimitive(
		v3fVectorData( positions, GeometricData::Interpretation::Point ),
		floatVectorData( widths )
	);

	primitive->variables["type"] = PrimitiveVariable( PrimitiveVariable::Constant, new StringData( pointType ) );
	primitive->variables["width"] = PrimitiveVariable( PrimitiveVariable::Vertex, floatVectorData( widths ) );
	primitive->variables["scale"] = PrimitiveVariable( PrimitiveVariable::Vertex, floatVectorData( scales ) );
	primitive->variables["id"] = PrimitiveVariable( PrimitiveVariable::Vertex, int64VectorData( ids ) );
	primitive->variables["seed"] = PrimitiveVariable( PrimitiveVariable::Vertex, intVectorData( seeds ) );
	primitive->variables["strokeId"] = PrimitiveVariable( PrimitiveVariable::Vertex, int64VectorData( strokeIds ) );
	primitive->variables["scatterColor"] = PrimitiveVariable( PrimitiveVariable::Vertex, color3fVectorData( authoredColors ) );
	primitive->variables["Cs"] = PrimitiveVariable( PrimitiveVariable::Vertex, color3fVectorData( colors ) );
	primitive->variables["sourcePath"] = PrimitiveVariable( PrimitiveVariable::Vertex, stringVectorData( sourcePaths ) );
	primitive->variables["N"] = PrimitiveVariable(
		PrimitiveVariable::Vertex,
		v3fVectorData( normals, GeometricData::Interpretation::Normal )
	);
	primitive->variables["up"] = PrimitiveVariable(
		PrimitiveVariable::Vertex,
		v3fVectorData( ups, GeometricData::Interpretation::Vector )
	);
	primitive->variables["orient"] = PrimitiveVariable( PrimitiveVariable::Vertex, quatfVectorData( orients ) );

	const std::set<std::string> customAttributes = parseAttributeFilter( includeAttributes );
	if( includeOptionalAttribute( exportPreset, customAttributes, "triangleIndex" ) )
	{
		primitive->variables["triangleIndex"] = PrimitiveVariable( PrimitiveVariable::Vertex, intVectorData( triangleIndices ) );
	}
	if( includeOptionalAttribute( exportPreset, customAttributes, "barycentric" ) )
	{
		primitive->variables["barycentric"] = PrimitiveVariable(
			PrimitiveVariable::Vertex,
			v3fVectorData( barycentrics, GeometricData::Interpretation::Vector )
		);
	}
	if( includeOptionalAttribute( exportPreset, customAttributes, "attachmentResolved" ) )
	{
		primitive->variables["attachmentResolved"] = PrimitiveVariable( PrimitiveVariable::Vertex, intVectorData( attachmentResolved ) );
	}

	return primitive;
}

} // namespace AttachedPointsPrivate

} // namespace GafferScatterPaint
