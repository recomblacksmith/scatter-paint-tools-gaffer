#include "GafferScatterPaint/StaticPoints.h"

#include "Gaffer/Context.h"
#include "Gaffer/StringPlug.h"
#include "Gaffer/TypedObjectPlug.h"

#include "IECore/CompoundObject.h"
#include "IECore/NullObject.h"
#include "IECore/SimpleTypedData.h"
#include "IECore/StringAlgo.h"
#include "IECore/VectorTypedData.h"

#include "IECoreScene/PointsPrimitive.h"
#include "IECoreScene/PrimitiveVariable.h"

#include "IECorePython/ScopedGILLock.h"

#include "boost/python.hpp"
#include "boost/python/stl_iterator.hpp"

#include "Imath/ImathMatrix.h"
#include "Imath/ImathQuat.h"
#include "Imath/ImathVec.h"

#include <algorithm>
#include <functional>
#include <cmath>
#include <set>
#include <string>
#include <vector>

using namespace Gaffer;
using namespace GafferScatterPaint;
using namespace IECore;
using namespace IECoreScene;

namespace bp = boost::python;

namespace
{

int currentFrame( const Context *context )
{
	return context ? static_cast<int>( std::lround( context->getFrame() ) ) : 0;
}

template<typename T>
T extractOr( const bp::object &value, const T &defaultValue )
{
	bp::extract<T> extractor( value );
	return extractor.check() ? extractor() : defaultValue;
}

bool isNone( const bp::object &value )
{
	return value.ptr() == Py_None;
}

bp::object dictGet( const bp::dict &dictionary, const char *key )
{
	return dictionary.has_key( key ) ? dictionary[key] : bp::object();
}

template<typename T>
T dictValue( const bp::dict &dictionary, const char *key, const T &defaultValue )
{
	return dictionary.has_key( key ) ? extractOr<T>( dictionary[key], defaultValue ) : defaultValue;
}

std::vector<float> floatVector( const bp::object &value, size_t expectedSize, const std::vector<float> &defaultValue )
{
	if( isNone( value ) )
	{
		return defaultValue;
	}

	std::vector<float> result;
	for( bp::stl_input_iterator<bp::object> it( value ), end; it != end; ++it )
	{
		result.push_back( extractOr<float>( *it, 0.0f ) );
	}

	if( result.size() != expectedSize )
	{
		return defaultValue;
	}

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
	std::vector<std::string> tokens;
	StringAlgo::tokenize( filter, ',', tokens );
	for( const std::string &token : tokens )
	{
		const std::string attribute = trimmed( token );
		if( !attribute.empty() )
		{
			result.insert( attribute );
		}
	}
	return result;
}

enum class ExportPreset
{
	Minimal = 0,
	Full = 1,
	Custom = 2,
};

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

V3fVectorDataPtr v3fVectorData( const std::vector<Imath::V3f> &values, GeometricData::Interpretation interpretation )
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

StringVectorDataPtr stringVectorData( const std::vector<std::string> &values )
{
	StringVectorDataPtr result = new StringVectorData();
	result->writable() = values;
	return result;
}

Imath::V3f vectorFromValues( const bp::object &value, const Imath::V3f &defaultValue )
{
	const std::vector<float> values = floatVector( value, 3, { defaultValue.x, defaultValue.y, defaultValue.z } );
	return Imath::V3f( values[0], values[1], values[2] );
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

struct OutputPointRecord
{
	Imath::V3f position = Imath::V3f( 0.0f );
	float width = 1.0f;
	float scale = 1.0f;
	Imath::Color3f color = Imath::Color3f( 1.0f, 1.0f, 1.0f );
	int64_t pointId = 0;
	int seed = 0;
	int64_t strokeId = 0;
	int triangleIndex = 0;
	Imath::V3f barycentric = Imath::V3f( 1.0f, 0.0f, 0.0f );
	bool attachmentResolved = false;
	std::string sourcePath;
	Imath::V3f normal = Imath::V3f( 0.0f, 1.0f, 0.0f );
	Imath::V3f up = Imath::V3f( 0.0f, 0.0f, 1.0f );
	Imath::Quatf orient = orientFromNormalUp( normal, up );
};

OutputPointRecord pointRecordFromPython( const bp::dict &point )
{
	OutputPointRecord result;
	result.position = vectorFromValues( dictGet( point, "P" ), Imath::V3f( 0.0f ) );
	result.width = dictValue<float>( point, "width", 1.0f );
	result.scale = dictValue<float>( point, "scale", dictValue<float>( point, "uniformScale", 1.0f ) );
	result.pointId = dictValue<int64_t>( point, "pointId", 0 );
	result.seed = dictValue<int>( point, "seed", 0 );
	result.strokeId = dictValue<int64_t>( point, "strokeId", 0 );
	result.color = Imath::Color3f(
		dictValue<float>( point, "colorR", dictValue<float>( point, "authoredColorR", 1.0f ) ),
		dictValue<float>( point, "colorG", dictValue<float>( point, "authoredColorG", 1.0f ) ),
		dictValue<float>( point, "colorB", dictValue<float>( point, "authoredColorB", 1.0f ) )
	);
	if( point.has_key( "scatterColor" ) )
	{
		const std::vector<float> authoredColor = floatVector( dictGet( point, "scatterColor" ), 3, { 1.0f, 1.0f, 1.0f } );
		result.color = Imath::Color3f( authoredColor[0], authoredColor[1], authoredColor[2] );
	}
	else if( point.has_key( "authoredColor" ) )
	{
		const std::vector<float> authoredColor = floatVector( dictGet( point, "authoredColor" ), 3, { 1.0f, 1.0f, 1.0f } );
		result.color = Imath::Color3f( authoredColor[0], authoredColor[1], authoredColor[2] );
	}
	result.triangleIndex = dictValue<int>( point, "triangleIndex", 0 );
	result.barycentric = vectorFromValues( dictGet( point, "barycentric" ), Imath::V3f( 1.0f, 0.0f, 0.0f ) );
	result.attachmentResolved = dictValue<bool>( point, "attachmentResolved", false );
	result.sourcePath = dictValue<std::string>( point, "sourcePath", "" );
	result.normal = vectorFromValues( dictGet( point, "N" ), Imath::V3f( 0.0f, 1.0f, 0.0f ) );
	result.up = vectorFromValues( dictGet( point, "up" ), Imath::V3f( 0.0f, 0.0f, 1.0f ) );
	result.orient = orientFromNormalUp( result.normal, result.up );
	if( point.has_key( "orient" ) )
	{
		const std::vector<float> orientValues = floatVector( dictGet( point, "orient" ), 4, {} );
		if( orientValues.size() == 4 )
		{
			result.orient = Imath::Quatf( orientValues[0], Imath::V3f( orientValues[1], orientValues[2], orientValues[3] ) );
		}
	}
	return result;
}

PointsPrimitivePtr pointsPrimitiveFromRecords(
	const std::vector<OutputPointRecord> &pointRecords,
	const std::string &pointType,
	ExportPreset exportPreset = ExportPreset::Full,
	const std::string &includeAttributes = ""
	,
	bool debugColor = false
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
		colors.push_back( debugColor ? ( point.attachmentResolved ? Imath::Color3f( 0.15f, 0.9f, 0.25f ) : Imath::Color3f( 1.0f, 0.45f, 0.0f ) ) : point.color );
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
	primitive->variables["N"] = PrimitiveVariable( PrimitiveVariable::Vertex, v3fVectorData( normals, GeometricData::Interpretation::Normal ) );
	primitive->variables["up"] = PrimitiveVariable( PrimitiveVariable::Vertex, v3fVectorData( ups, GeometricData::Interpretation::Vector ) );
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

ConstObjectPtr pointDataSample( ConstCompoundObjectPtr metadata, int frame )
{
	if( !metadata )
	{
		return nullptr;
	}

	const auto frameNumbersIt = metadata->members().find( "frameNumbers" );
	const auto frameSamplesIt = metadata->members().find( "frameSamples" );
	if( frameNumbersIt != metadata->members().end() && frameSamplesIt != metadata->members().end() )
	{
		ConstIntVectorDataPtr frameNumbers = runTimeCast<const IntVectorData>( frameNumbersIt->second.get() );
		ConstCompoundObjectPtr frameSamples = runTimeCast<const CompoundObject>( frameSamplesIt->second.get() );
		if( frameNumbers && frameSamples && !frameNumbers->readable().empty() )
		{
			const std::vector<int> &frames = frameNumbers->readable();
			int sampleFrame = frames.front();
			auto frameIt = std::lower_bound( frames.begin(), frames.end(), frame );
			if( frameIt == frames.end() )
			{
				sampleFrame = frames.back();
			}
			else
			{
				sampleFrame = *frameIt;
			}

			const auto sampleIt = frameSamples->members().find( std::to_string( sampleFrame ) );
			if( sampleIt != frameSamples->members().end() )
			{
				return sampleIt->second;
			}
		}
	}

	const auto primitiveIt = metadata->members().find( "pointsPrimitive" );
	if( primitiveIt != metadata->members().end() )
	{
		return primitiveIt->second;
	}

	return metadata;
}

} // namespace

GAFFER_NODE_DEFINE_TYPE( StaticPoints );

size_t StaticPoints::g_firstPlugIndex = 0;

StaticPoints::StaticPoints( const std::string &name )
	: ObjectSource( name, "scatter" )
{
	storeIndexOfNextChild( g_firstPlugIndex );
	addChild( new ObjectPlug( "pointData", Plug::In, new NullObject() ) );
	addChild( new StringPlug( "outputLocation", Plug::In, "/scatter" ) );
	addChild( new StringPlug( "pointType", Plug::In, "gl:point" ) );
	addChild( new BoolPlug( "debugColor", Plug::In, false ) );
	m_plugSetConnection = plugSetSignal().connect( std::bind( &StaticPoints::plugSet, this, std::placeholders::_1 ) );
	syncOutputLocation();
}

StaticPoints::~StaticPoints()
{
}

void StaticPoints::syncOutputLocation()
{
	std::string outputLocation = outputLocationPlug()->getValue();
	outputLocation = outputLocation.empty() ? "/scatter" : outputLocation;
	const size_t slash = outputLocation.find_last_of( '/' );
	std::string leafName = slash == std::string::npos ? outputLocation : outputLocation.substr( slash + 1 );
	if( leafName.empty() )
	{
		leafName = "scatter";
	}

	if( namePlug()->getValue() != leafName )
	{
		namePlug()->setValue( leafName );
	}
}

void StaticPoints::plugSet( Plug *plug )
{
	if( plug == outputLocationPlug() )
	{
		syncOutputLocation();
	}
}

void StaticPoints::setPointRecords( const bp::object &records )
{
	IECorePython::ScopedGILLock gilLock;

	std::vector<OutputPointRecord> pointRecords;
	for( bp::stl_input_iterator<bp::object> it( records ), end; it != end; ++it )
	{
		pointRecords.push_back( pointRecordFromPython( bp::extract<bp::dict>( *it ) ) );
	}

	CompoundObjectPtr metadata = new CompoundObject();
	metadata->members()["pointCount"] = new IntData( static_cast<int>( pointRecords.size() ) );
	metadata->members()["pointsPrimitive"] = pointsPrimitiveFromRecords( pointRecords, pointTypePlug()->getValue(), ExportPreset::Full, "", debugColorPlug()->getValue() );
	pointDataPlug()->setValue( metadata );
}

void StaticPoints::setFramePointRecords( const bp::object &frameRecords )
{
	IECorePython::ScopedGILLock gilLock;

	CompoundObjectPtr metadata = new CompoundObject();
	CompoundObjectPtr frameSamples = new CompoundObject();
	std::vector<int> frameNumbers;
	ObjectPtr firstPrimitive;
	int firstPointCount = 0;

	for( bp::stl_input_iterator<bp::object> it( frameRecords ), end; it != end; ++it )
	{
		const bp::dict frameRecord = bp::extract<bp::dict>( *it );
		const int frame = dictValue<int>( frameRecord, "frame", 0 );
		std::vector<OutputPointRecord> pointRecords;
		const bp::object recordsObject = dictGet( frameRecord, "records" );
		for( bp::stl_input_iterator<bp::object> recordIt( recordsObject ), recordEnd; recordIt != recordEnd; ++recordIt )
		{
			pointRecords.push_back( pointRecordFromPython( bp::extract<bp::dict>( *recordIt ) ) );
		}

		PointsPrimitivePtr primitive = pointsPrimitiveFromRecords( pointRecords, pointTypePlug()->getValue(), ExportPreset::Full, "", debugColorPlug()->getValue() );
		if( !firstPrimitive )
		{
			firstPrimitive = primitive;
			firstPointCount = static_cast<int>( pointRecords.size() );
		}

		frameNumbers.push_back( frame );
		frameSamples->members()[std::to_string( frame )] = primitive;
	}

	metadata->members()["pointCount"] = new IntData( firstPointCount );
	metadata->members()["pointsPrimitive"] = firstPrimitive ? firstPrimitive : static_cast<ObjectPtr>( new NullObject() );
	metadata->members()["frameNumbers"] = new IntVectorData( frameNumbers );
	metadata->members()["frameSamples"] = frameSamples;
	pointDataPlug()->setValue( metadata );
}

void StaticPoints::setFramePointData( const bp::object &frameData )
{
	IECorePython::ScopedGILLock gilLock;

	CompoundObjectPtr metadata = new CompoundObject();
	CompoundObjectPtr frameSamples = new CompoundObject();
	std::vector<int> frameNumbers;
	ObjectPtr firstPrimitive;
	int firstPointCount = 0;

	for( bp::stl_input_iterator<bp::object> it( frameData ), end; it != end; ++it )
	{
		const bp::dict sample = bp::extract<bp::dict>( *it );
		const int frame = dictValue<int>( sample, "frame", 0 );
		ConstPointsPrimitivePtr primitive = bp::extract<ConstPointsPrimitivePtr>( dictGet( sample, "pointsPrimitive" ) );
		if( !primitive )
		{
			primitive = new PointsPrimitive( new V3fVectorData() );
		}

		if( !firstPrimitive )
		{
			firstPrimitive = primitive->copy();
			firstPointCount = dictValue<int>( sample, "pointCount", static_cast<int>( primitive->getNumPoints() ) );
		}

		frameNumbers.push_back( frame );
		frameSamples->members()[std::to_string( frame )] = primitive->copy();
	}

	metadata->members()["pointCount"] = new IntData( firstPointCount );
	metadata->members()["pointsPrimitive"] = firstPrimitive ? firstPrimitive : static_cast<ObjectPtr>( new PointsPrimitive( new V3fVectorData() ) );
	metadata->members()["frameNumbers"] = new IntVectorData( frameNumbers );
	metadata->members()["frameSamples"] = frameSamples;
	pointDataPlug()->setValue( metadata );
}

ObjectPlug *StaticPoints::pointDataPlug() { return getChild<ObjectPlug>( g_firstPlugIndex ); }
const ObjectPlug *StaticPoints::pointDataPlug() const { return getChild<ObjectPlug>( g_firstPlugIndex ); }
StringPlug *StaticPoints::outputLocationPlug() { return getChild<StringPlug>( g_firstPlugIndex + 1 ); }
const StringPlug *StaticPoints::outputLocationPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 1 ); }
StringPlug *StaticPoints::pointTypePlug() { return getChild<StringPlug>( g_firstPlugIndex + 2 ); }
const StringPlug *StaticPoints::pointTypePlug() const { return getChild<StringPlug>( g_firstPlugIndex + 2 ); }
BoolPlug *StaticPoints::debugColorPlug() { return getChild<BoolPlug>( g_firstPlugIndex + 3 ); }
const BoolPlug *StaticPoints::debugColorPlug() const { return getChild<BoolPlug>( g_firstPlugIndex + 3 ); }

void StaticPoints::affects( const Plug *input, AffectedPlugsContainer &outputs ) const
{
	ObjectSource::affects( input, outputs );
	if( input == pointDataPlug() )
	{
		outputs.push_back( sourcePlug() );
	}
	else if( input == pointTypePlug() )
	{
		outputs.push_back( sourcePlug() );
	}
	else if( input == debugColorPlug() )
	{
		outputs.push_back( sourcePlug() );
	}
}

void StaticPoints::hashSource( const Context *context, MurmurHash &h ) const
{
	pointDataPlug()->hash( h );
	pointTypePlug()->hash( h );
	debugColorPlug()->hash( h );
	h.append( currentFrame( context ) );
}

ConstObjectPtr StaticPoints::computeSource( const Context *context ) const
{
	ConstObjectPtr value = pointDataPlug()->getValue();
	if( !value )
	{
		return NullObject::defaultNullObject();
	}

	ConstCompoundObjectPtr metadata = runTimeCast<const CompoundObject>( value.get() );
	if( metadata )
	{
		ConstObjectPtr sampledValue = pointDataSample( metadata, currentFrame( context ) );
		if( sampledValue )
		{
			value = sampledValue;
		}
	}

	ConstPointsPrimitivePtr points = runTimeCast<const PointsPrimitive>( value.get() );
	if( !points )
	{
		return value;
	}

	PointsPrimitivePtr result = runTimeCast<PointsPrimitive>( points->copy() );
	result->variables["type"] = PrimitiveVariable( PrimitiveVariable::Constant, new StringData( pointTypePlug()->getValue() ) );
	const auto scatterIt = result->variables.find( "scatterColor" );
	if( scatterIt != result->variables.end() )
	{
		PrimitiveVariable displayColor = scatterIt->second;
		if( debugColorPlug()->getValue() )
		{
			std::vector<Imath::Color3f> debugColors;
			const auto attachmentIt = result->variables.find( "attachmentResolved" );
			if( attachmentIt != result->variables.end() )
			{
				ConstIntVectorDataPtr attachmentData = runTimeCast<const IntVectorData>( attachmentIt->second.data.get() );
				const std::vector<int> values = attachmentData ? attachmentData->readable() : std::vector<int>();
				debugColors.reserve( result->getNumPoints() );
				for( size_t i = 0; i < result->getNumPoints(); ++i )
				{
					const bool resolved = i < values.size() && values[i] != 0;
					debugColors.push_back( resolved ? Imath::Color3f( 0.15f, 0.9f, 0.25f ) : Imath::Color3f( 1.0f, 0.45f, 0.0f ) );
				}
			}
			displayColor.data = color3fVectorData( debugColors );
		}
		result->variables["Cs"] = displayColor;
	}
	return result;
}
