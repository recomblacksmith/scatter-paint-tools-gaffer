#pragma once

#include "GafferScatterPaint/AttachedPoints.h"
#include "GafferScatterPaint/PaintedPoints.h"

#include "Gaffer/NumericPlug.h"
#include "Gaffer/StringPlug.h"
#include "Gaffer/TypedObjectPlug.h"

#include "GafferScene/ScenePlug.h"

#include "IECore/CompoundObject.h"
#include "IECore/Exception.h"
#include "IECore/PathMatcherData.h"
#include "IECore/RunTimeTyped.h"
#include "IECore/SimpleTypedData.h"
#include "IECore/VectorTypedData.h"
#include "IECorePython/ScopedGILLock.h"
#include "IECoreScene/MeshPrimitive.h"
#include "IECoreScene/PointsPrimitive.h"
#include "IECoreScene/PrimitiveVariable.h"

#include "boost/python.hpp"
#include "boost/python/stl_iterator.hpp"

#include "Imath/ImathMatrix.h"
#include "Imath/ImathQuat.h"
#include "Imath/ImathVec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace GafferScatterPaint
{

namespace AttachedPointsPrivate
{

namespace bp = boost::python;

inline constexpr int g_schemaVersion = 1;
inline const Imath::Color3f g_defaultAuthoredColor( 1.0f, 1.0f, 1.0f );
inline const Imath::Color3f g_debugResolvedColor( 0.15f, 0.9f, 0.25f );
inline const Imath::Color3f g_debugUnresolvedColor( 1.0f, 0.45f, 0.0f );

template<typename T>
T extractOr( const bp::object &value, const T &defaultValue )
{
	bp::extract<T> extractor( value );
	return extractor.check() ? extractor() : defaultValue;
}

template<typename T>
T dictValue( const bp::dict &dictionary, const char *key, const T &defaultValue )
{
	return dictionary.has_key( key ) ? extractOr<T>( dictionary[key], defaultValue ) : defaultValue;
}

enum class ExportPreset
{
	Minimal = 0,
	Full = 1,
	Custom = 2,
};

struct TriangleData
{
	std::vector<Imath::V3f> positions;
	std::vector<std::array<int, 4>> triangles;
	std::vector<int> triangleOffsets;
	IECoreScene::ConstMeshPrimitivePtr mesh;
};

struct PathSolveCache
{
	std::string sourcePath;
	GafferScene::ScenePlug::ScenePath scenePath;
	bool scenePathParsed = false;
	bool existsChecked = false;
	bool exists = false;
	bool transformChecked = false;
	bool hasTransform = false;
	bool geometryChecked = false;
	bool geometryLoadFailed = false;
	bool geometrySupported = false;
	IECore::ConstObjectPtr object;
	Imath::M44f fullTransform = Imath::M44f( 1.0f );
	TriangleData triangleData;
};

Imath::Quatf orientFromNormalUp( const Imath::V3f &normal, const Imath::V3f &up );

struct OutputPointRecord
{
	Imath::V3f position = Imath::V3f( 0.0f );
	float width = 1.0f;
	float scale = 1.0f;
	Imath::Color3f color = g_defaultAuthoredColor;
	int64_t pointId = 0;
	int seed = 0;
	int64_t strokeId = 0;
	int triangleIndex = 0;
	Imath::V3f barycentric = Imath::V3f( 1.0f, 0.0f, 0.0f );
	bool attachmentResolved = false;
	std::string sourcePath;
	Imath::V3f normal = Imath::V3f( 0.0f, 1.0f, 0.0f );
	Imath::V3f up = Imath::V3f( 0.0f, 0.0f, 1.0f );
	Imath::V2f tangentRotation = Imath::V2f( 0.0f );
	float normalSpin = 0.0f;
	Imath::Quatf orient = orientFromNormalUp( normal, up );
};

struct AuthoredLayerRecord
{
	int layerId = 0;
	int order = 0;
	bool enabled = true;
	bool visible = true;
	bool mute = false;
	bool solo = false;
	int mode = 0;
	int frameStart = 0;
	int frameEnd = 0;
	bool holdOutsideRange = true;
	bool colorEnabled = false;
	Imath::Color3f color = g_defaultAuthoredColor;
};

struct AuthoredStrokeRecord
{
	int strokeId = 0;
	int layerId = 0;
	int order = 0;
	int mode = 0;
	int frameStart = 0;
	int frameEnd = 0;
	bool colorEnabled = false;
	Imath::Color3f color = g_defaultAuthoredColor;
};

struct AuthoredPointRecord
{
	int pointId = 0;
	int strokeId = 0;
	int layerId = 0;
	bool valid = true;
	int targetPathId = 0;
	int instanceId = 0;
	int instanceSourcePathId = 0;
	int triangleIndex = -1;
	Imath::V3f barycentric = Imath::V3f( 1.0f, 0.0f, 0.0f );
	Imath::V3f restObjectP = Imath::V3f( 0.0f );
	Imath::V3f restWorldP = Imath::V3f( 0.0f );
	Imath::V2f restUV = Imath::V2f( 0.0f );
	Imath::V3f restNormal = Imath::V3f( 0.0f, 1.0f, 0.0f );
	Imath::V3f restUp = Imath::V3f( 0.0f, 0.0f, 1.0f );
	float uniformScale = 1.0f;
	float width = 1.0f;
	float normalSpin = 0.0f;
	Imath::V2f tangentRotation = Imath::V2f( 0.0f );
	int seed = 0;
	int anchorModeUsed = 3;
	int topologyGeneration = 0;
	bool colorEnabled = false;
	Imath::Color3f color = g_defaultAuthoredColor;
};

struct StoreSnapshot
{
	bool available = false;
	int schemaVersion = g_schemaVersion;
	int invalidPointCount = 0;
	int invalidStrokeCount = 0;
	int failingFrame = 0;
	Imath::Color3f defaultColor = g_defaultAuthoredColor;
	std::vector<std::string> failingTargetPaths;
	std::vector<std::string> validationCategories;
	std::vector<std::string> scenePaths;
	std::vector<std::string> instanceSourcePaths;
	std::vector<AuthoredLayerRecord> layers;
	std::vector<AuthoredStrokeRecord> strokes;
	std::vector<AuthoredPointRecord> points;
};

bool isNone( const bp::object &value );
std::string pyString( const bp::object &value );
bp::object dictGet( const bp::dict &dictionary, const char *key );
std::vector<std::string> stringVector( const bp::object &value );
std::vector<float> floatVector( const bp::object &value, const std::vector<float> &defaultValue );
int currentFrame( const Gaffer::Context *context );
IECore::StringVectorDataPtr stringVectorData( const std::vector<std::string> &values );
ExportPreset exportPresetValue( int value );
Imath::V3f vectorFromValues( const bp::object &value, const Imath::V3f &defaultValue );
Imath::V2f vector2FromValues( const bp::object &value, const Imath::V2f &defaultValue );
Imath::Quatf quatFromValues( const bp::object &value, const Imath::Quatf &defaultValue );
Imath::V3f cross( const Imath::V3f &a, const Imath::V3f &b );
Imath::V3f normalized( const Imath::V3f &vector, const Imath::V3f &defaultValue );
Imath::V3f orthogonalized( const Imath::V3f &vector, const Imath::V3f &normal, const Imath::V3f &defaultValue );
Imath::V3f trianglePoint( const Imath::V3f &a, const Imath::V3f &b, const Imath::V3f &c, const Imath::V3f &barycentric );
Imath::V3f triangleUp( const Imath::V3f &a, const Imath::V3f &b, const Imath::V3f &c, const Imath::V3f &normal );
Imath::Quatf composedOrient( const Imath::V3f &normal, const Imath::V3f &up, float normalSpin, const Imath::V2f &tangentRotation );
bool meshTriangleData( IECore::ConstObjectPtr object, TriangleData &result );
bool supportsCurrentSolveMesh( const TriangleData &triangleData );
const std::array<int, 4> *triangleForIndex( const TriangleData &triangleData, int triangleIndex );
Imath::V3f triangleNormal(
	const IECoreScene::MeshPrimitive *mesh,
	const std::array<int, 4> &triangle,
	const Imath::V3f &barycentric,
	const Imath::V3f &fallbackNormal
);
StoreSnapshot loadStoreSnapshot( const AttachedPoints *node, std::string &loadError );
void clearLastValidState( const AttachedPoints *node );
void hashAuthoredStoreSource( const AttachedPoints *node, IECore::MurmurHash &h );
IECoreScene::PointsPrimitivePtr pointsPrimitiveFromRecords(
	const std::vector<OutputPointRecord> &pointRecords,
	const std::string &pointType,
	ExportPreset exportPreset,
	const std::string &includeAttributes,
	bool debugColor
);
IECore::ConstObjectPtr computeOutputObject(
	const AttachedPoints *node,
	const GafferScene::ScenePlug::ScenePath &path,
	const Gaffer::Context *context
);

extern const std::vector<std::string> g_validationCategoryNames;

} // namespace AttachedPointsPrivate

} // namespace GafferScatterPaint
