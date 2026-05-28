#include "PaintedPointsPrivate.h"

#include "BrushSimdDispatch.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <map>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace
{

using Clock = std::chrono::steady_clock;

struct SampleGroupKey
{
	std::uint32_t targetPathId = 0;
	std::uint32_t instanceId = 0;
	std::uint32_t instanceSourcePathId = 0;

	bool operator==( const SampleGroupKey &other ) const
	{
		return
			targetPathId == other.targetPathId &&
			instanceId == other.instanceId &&
			instanceSourcePathId == other.instanceSourcePathId;
	}
};

struct SampleGroupKeyHash
{
	std::size_t operator()( const SampleGroupKey &key ) const
	{
		const std::size_t h1 = std::hash<std::uint32_t>()( key.targetPathId );
		const std::size_t h2 = std::hash<std::uint32_t>()( key.instanceId );
		const std::size_t h3 = std::hash<std::uint32_t>()( key.instanceSourcePathId );
		return h1 ^ ( h2 << 1 ) ^ ( h3 << 2 );
	}
};

struct SampleCellKey
{
	int x = 0;
	int y = 0;
	int z = 0;

	bool operator==( const SampleCellKey &other ) const
	{
		return x == other.x && y == other.y && z == other.z;
	}
};

struct SampleCellKeyHash
{
	std::size_t operator()( const SampleCellKey &key ) const
	{
		const std::size_t h1 = std::hash<int>()( key.x );
		const std::size_t h2 = std::hash<int>()( key.y );
		const std::size_t h3 = std::hash<int>()( key.z );
		return h1 ^ ( h2 << 1 ) ^ ( h3 << 2 );
	}
};

struct SampleGroupData
{
	std::unordered_map<SampleCellKey, std::vector<Imath::V3f>, SampleCellKeyHash> buckets;
	Imath::V3f min;
	Imath::V3f max;
	bool hasBounds = false;
};

using SampleGroups = std::unordered_map<SampleGroupKey, SampleGroupData, SampleGroupKeyHash>;

struct SurfacePointKey
{
	std::string sourcePath;
	std::uint32_t instanceId = 0;
	std::string instanceSourcePath;
	int triangleIndex = -1;
	std::array<int, 3> barycentric = { 1000000, 0, 0 };

	bool operator==( const SurfacePointKey &other ) const
	{
		return
			sourcePath == other.sourcePath &&
			instanceId == other.instanceId &&
			instanceSourcePath == other.instanceSourcePath &&
			triangleIndex == other.triangleIndex &&
			barycentric == other.barycentric;
	}
};

struct SurfacePointKeyHash
{
	std::size_t operator()( const SurfacePointKey &key ) const
	{
		std::size_t result = std::hash<std::string>()( key.sourcePath );
		result ^= std::hash<std::uint32_t>()( key.instanceId ) << 1;
		result ^= std::hash<std::string>()( key.instanceSourcePath ) << 2;
		result ^= std::hash<int>()( key.triangleIndex ) << 3;
		result ^= std::hash<int>()( key.barycentric[0] ) << 4;
		result ^= std::hash<int>()( key.barycentric[1] ) << 5;
		result ^= std::hash<int>()( key.barycentric[2] ) << 6;
		return result;
	}
};

struct CompiledSurfacePoint
{
	std::string sourcePath;
	std::uint32_t instanceId = 0;
	std::string instanceSourcePath;
	std::uint32_t triangleIndex = 0;
	Imath::V3f barycentric = Imath::V3f( 1.0f, 0.0f, 0.0f );
	Imath::V3f objectPoint = Imath::V3f( 0.0f );
	Imath::V3f worldPoint = Imath::V3f( 0.0f );
	Imath::V2f restUV = Imath::V2f( 0.0f );
	Imath::V3f normal = Imath::V3f( 0.0f, 1.0f, 0.0f );
	Imath::V3f up = Imath::V3f( 0.0f, 0.0f, 1.0f );
};

using DirectCandidateCache = std::unordered_map<std::string, ReprojectSurfaceCandidate>;
using PathIdCache = std::unordered_map<std::string, std::uint32_t>;

struct ResolvedBuildContext
{
	std::array<float, 3> inheritedColor;
	PathIdCache scenePathIds;
	PathIdCache instanceSourcePathIds;
	float width = 1.0f;
	float uniformScale = 1.0f;
	Imath::V2f tangentRotation = Imath::V2f( 0.0f );
	float normalSpin = 0.0f;
	float pressureDensity = 1.0f;
	float pressureSoftness = 1.0f;
	int baseSeed = 0;
	bool valid = true;
};

struct CompiledBrushSample
{
	std::string sourcePath;
	std::uint32_t instanceId = 0;
	std::string instanceSourcePath;
	int preferredTriangleIndex = -1;
	Imath::V3f worldPosition = Imath::V3f( 0.0f );
	float width = 1.0f;
	float uniformScale = 1.0f;
	Imath::V2f tangentRotation = Imath::V2f( 0.0f );
	float normalSpin = 0.0f;
	float pressureDensity = 1.0f;
	float pressureSoftness = 1.0f;
	int baseSeed = 0;
	bool valid = true;
};

struct SurfaceResolveTimings
{
	double candidateMs = 0.0;
	double hitSearchMs = 0.0;
	double hitBuildMs = 0.0;
};

struct SurfaceCandidateView
{
	const ReprojectSurfaceCandidate *candidate = nullptr;
	std::string targetPath;
	std::string resolvedPath;
	std::string instanceSourcePath;
	std::uint32_t instanceId = 0;
};

struct SurfaceResolveCacheKey
{
	std::string sourcePath;
	std::uint32_t instanceId = 0;
	std::string instanceSourcePath;

	bool operator==( const SurfaceResolveCacheKey &other ) const
	{
		return
			sourcePath == other.sourcePath &&
			instanceId == other.instanceId &&
			instanceSourcePath == other.instanceSourcePath;
	}
};

struct SurfaceResolveCacheKeyHash
{
	std::size_t operator()( const SurfaceResolveCacheKey &key ) const
	{
		std::size_t result = std::hash<std::string>()( key.sourcePath );
		result ^= std::hash<std::uint32_t>()( key.instanceId ) << 1;
		result ^= std::hash<std::string>()( key.instanceSourcePath ) << 2;
		return result;
	}
};

struct SurfaceResolveCacheValue
{
	int triangleIndex = -1;
	Imath::V3f worldPoint = Imath::V3f( 0.0f );
	bool hasWorldPoint = false;
};

using SurfaceResolveCache = std::unordered_map<SurfaceResolveCacheKey, SurfaceResolveCacheValue, SurfaceResolveCacheKeyHash>;

struct BestSurfaceHit
{
	std::array<int, 4> triangle = { -1, -1, -1, -1 };
	Imath::V3f barycentric = Imath::V3f( 1.0f, 0.0f, 0.0f );
	Imath::V3f objectPoint = Imath::V3f( 0.0f );
	Imath::V3f worldPoint = Imath::V3f( 0.0f );
	float distanceSquared = std::numeric_limits<float>::max();
	bool hasDistanceBound = false;
	bool found = false;
	bool preferred = false;
};

struct CandidateBlockHit
{
	int lane = -1;
	float distanceSquared = std::numeric_limits<float>::max();
	std::array<float, 3> point = { 0.0f, 0.0f, 0.0f };
	std::array<float, 3> barycentric = { 1.0f, 0.0f, 0.0f };
};

struct BvhTraversalEntry
{
	int nodeIndex = -1;
	float distanceSquared = std::numeric_limits<float>::max();
};

struct CompiledInputSample
{
	PyObject *original = nullptr;
	CompiledBrushSample compiled;

	CompiledInputSample() = default;

	CompiledInputSample( const CompiledInputSample &other )
		: original( other.original ), compiled( other.compiled )
	{
		Py_XINCREF( original );
	}

	CompiledInputSample( CompiledInputSample &&other ) noexcept
		: original( other.original ), compiled( std::move( other.compiled ) )
	{
		other.original = nullptr;
	}

	CompiledInputSample &operator=( const CompiledInputSample &other )
	{
		if( this == &other )
		{
			return *this;
		}
		Py_XINCREF( other.original );
		Py_XDECREF( original );
		original = other.original;
		compiled = other.compiled;
		return *this;
	}

	CompiledInputSample &operator=( CompiledInputSample &&other ) noexcept
	{
		if( this == &other )
		{
			return *this;
		}
		Py_XDECREF( original );
		original = other.original;
		compiled = std::move( other.compiled );
		other.original = nullptr;
		return *this;
	}

	~CompiledInputSample()
	{
		Py_XDECREF( original );
	}
};

bp::list pythonVectorList( const Imath::V3f &value )
{
	bp::list result;
	result.append( value.x );
	result.append( value.y );
	result.append( value.z );
	return result;
}

bp::dict pythonBrushSampleFromData( const BrushSampleData &sample )
{
	bp::dict result;
	result["point"] = pythonVectorList( sample.point );
	bp::list tangentRotation;
	tangentRotation.append( sample.tangentRotation.x );
	tangentRotation.append( sample.tangentRotation.y );
	result["tangentRotation"] = tangentRotation;
	result["width"] = sample.width;
	result["scale"] = sample.scale;
	result["normalSpin"] = sample.normalSpin;
	result["pressureDensity"] = sample.pressureDensity;
	result["pressureSoftness"] = sample.pressureSoftness;
	result["seed"] = sample.seed;
	result["sourcePath"] = sample.sourcePath;
	result["instanceId"] = sample.instanceId;
	result["instanceSourcePath"] = sample.instanceSourcePath;
	result["triangleIndex"] = sample.triangleIndex;
	result["barycentric"] = pythonVectorList( sample.barycentric );
	result["attachmentResolved"] = sample.attachmentResolved;
	result["sampleOrigin"] = sample.sampleOrigin;
	result["sampleExpanded"] = sample.sampleExpanded;
	result["sampleSourceIndex"] = sample.sampleSourceIndex;
	result["sampleExpansionIndex"] = sample.sampleExpansionIndex;
	result["N"] = pythonVectorList( sample.normal );
	result["valid"] = sample.valid;
	return result;
}

struct SampleCompileCache
{
	PyObject *sourcePathObject = nullptr;
	std::string sourcePath;
	PyObject *instanceIdObject = nullptr;
	std::uint32_t instanceId = 0;
	PyObject *instanceSourcePathObject = nullptr;
	std::string instanceSourcePath;
	PyObject *triangleIndexObject = nullptr;
	int triangleIndex = -1;
	PyObject *normalSpinObject = nullptr;
	float normalSpin = 0.0f;
	PyObject *widthObject = nullptr;
	float width = 1.0f;
	PyObject *scaleObject = nullptr;
	float scale = 1.0f;
	PyObject *uniformScaleObject = nullptr;
	float uniformScale = 1.0f;
	PyObject *pressureDensityObject = nullptr;
	float pressureDensity = 1.0f;
	PyObject *pressureSoftnessObject = nullptr;
	float pressureSoftness = 1.0f;
	PyObject *validObject = nullptr;
	bool valid = true;
};

Imath::V2f pyV2fFast( PyObject *value, const Imath::V2f &defaultValue )
{
	if( !value || value == Py_None )
	{
		return defaultValue;
	}
	if( PyList_CheckExact( value ) )
	{
		if( PyList_GET_SIZE( value ) != 2 )
		{
			return defaultValue;
		}
		return Imath::V2f(
			pyFloatFast( PyList_GET_ITEM( value, 0 ), defaultValue.x ),
			pyFloatFast( PyList_GET_ITEM( value, 1 ), defaultValue.y )
		);
	}
	if( PyTuple_CheckExact( value ) )
	{
		if( PyTuple_GET_SIZE( value ) != 2 )
		{
			return defaultValue;
		}
		return Imath::V2f(
			pyFloatFast( PyTuple_GET_ITEM( value, 0 ), defaultValue.x ),
			pyFloatFast( PyTuple_GET_ITEM( value, 1 ), defaultValue.y )
		);
	}
	PyObject *sequence = PySequence_Fast( value, nullptr );
	if( !sequence )
	{
		PyErr_Clear();
		return defaultValue;
	}
	if( PySequence_Fast_GET_SIZE( sequence ) != 2 )
	{
		Py_DECREF( sequence );
		return defaultValue;
	}
	PyObject **items = PySequence_Fast_ITEMS( sequence );
	const Imath::V2f result(
		pyFloatFast( items[0], defaultValue.x ),
		pyFloatFast( items[1], defaultValue.y )
	);
	Py_DECREF( sequence );
	return result;
}

float cachedPyFloatFast( PyObject *value, PyObject *&cachedObject, float &cachedValue, float defaultValue )
{
	if( value == cachedObject )
	{
		return cachedValue;
	}
	cachedObject = value;
	cachedValue = pyFloatFast( value, defaultValue );
	return cachedValue;
}

int cachedPyIntFast( PyObject *value, PyObject *&cachedObject, int &cachedValue, int defaultValue )
{
	if( value == cachedObject )
	{
		return cachedValue;
	}
	cachedObject = value;
	cachedValue = pyIntFast( value, defaultValue );
	return cachedValue;
}

std::uint32_t cachedPyUInt32Fast( PyObject *value, PyObject *&cachedObject, std::uint32_t &cachedValue, std::uint32_t defaultValue )
{
	if( value == cachedObject )
	{
		return cachedValue;
	}
	cachedObject = value;
	cachedValue = pyUInt32Fast( value, defaultValue );
	return cachedValue;
}

bool cachedPyBoolFast( PyObject *value, PyObject *&cachedObject, bool &cachedValue, bool defaultValue )
{
	if( value == cachedObject )
	{
		return cachedValue;
	}
	cachedObject = value;
	cachedValue = pyBoolFast( value, defaultValue );
	return cachedValue;
}

const std::string &cachedPyStringFast( PyObject *value, PyObject *&cachedObject, std::string &cachedValue, const std::string &defaultValue )
{
	if( value == cachedObject )
	{
		return cachedValue;
	}
	cachedObject = value;
	cachedValue = pyStringFast( value, defaultValue );
	return cachedValue;
}

double elapsedMilliseconds( const Clock::time_point &start )
{
	return std::chrono::duration<double, std::milli>( Clock::now() - start ).count();
}

int triangleOffsetForIndex( const TriangleData &triangleData, int triangleIndex )
{
	if( triangleIndex < 0 || static_cast<size_t>( triangleIndex ) >= triangleData.triangleLookupOffsets.size() )
	{
		return -1;
	}

	const int triangleOffset = triangleData.triangleLookupOffsets[triangleIndex];
	if( triangleOffset < 0 || static_cast<size_t>( triangleOffset ) >= triangleData.triangles.size() )
	{
		return -1;
	}

	return triangleData.triangles[triangleOffset][0] == triangleIndex ? triangleOffset : -1;
}

bool updateClosestPaintHitCandidate(
	const SurfaceCandidateView &candidateView,
	const Imath::V3f &referenceWorld,
	const Imath::V3f &referenceObject,
	std::size_t triangleOffset,
	bool trianglePreferred,
	BestSurfaceHit &bestHit
)
{
	const ReprojectSurfaceCandidate &candidate = *candidateView.candidate;
	if(
		triangleOffset >= candidate.triangleData.triangles.size() ||
		triangleOffset >= candidate.triangleData.ax.size() ||
		triangleOffset >= candidate.triangleData.ay.size() ||
		triangleOffset >= candidate.triangleData.az.size() ||
		triangleOffset >= candidate.triangleData.bx.size() ||
		triangleOffset >= candidate.triangleData.by.size() ||
		triangleOffset >= candidate.triangleData.bz.size() ||
		triangleOffset >= candidate.triangleData.cx.size() ||
		triangleOffset >= candidate.triangleData.cy.size() ||
		triangleOffset >= candidate.triangleData.cz.size()
	)
	{
		return false;
	}

	const std::array<int, 4> &triangle = candidate.triangleData.triangles[triangleOffset];
	ClosestPointTriangleInput triangleInput;
	triangleInput.a = {
		candidate.triangleData.ax[triangleOffset],
		candidate.triangleData.ay[triangleOffset],
		candidate.triangleData.az[triangleOffset]
	};
	triangleInput.b = {
		candidate.triangleData.bx[triangleOffset],
		candidate.triangleData.by[triangleOffset],
		candidate.triangleData.bz[triangleOffset]
	};
	triangleInput.c = {
		candidate.triangleData.cx[triangleOffset],
		candidate.triangleData.cy[triangleOffset],
		candidate.triangleData.cz[triangleOffset]
	};
	triangleInput.point = {referenceObject.x, referenceObject.y, referenceObject.z};
	ClosestPointTriangleOutput triangleOutput;
	brushResolveKernels().closestPointOnTriangle( triangleInput, triangleOutput );
	const float objectPointX = triangleOutput.point[0];
	const float objectPointY = triangleOutput.point[1];
	const float objectPointZ = triangleOutput.point[2];
	const float distanceSquared = candidate.similarityTransform ?
		triangleOutput.distanceSquared * candidate.similarityScaleSquared :
		( ( Imath::V3f( objectPointX, objectPointY, objectPointZ ) * candidate.fullTransform ) - referenceWorld ).length2();

	bool shouldReplace = false;
	if( !bestHit.found )
	{
		shouldReplace = !bestHit.hasDistanceBound || distanceSquared <= bestHit.distanceSquared + 1e-9f;
	}
	else if( distanceSquared < bestHit.distanceSquared - 1e-9f )
	{
		shouldReplace = true;
	}
	else if( std::abs( distanceSquared - bestHit.distanceSquared ) <= 1e-9f && trianglePreferred && !bestHit.preferred )
	{
		shouldReplace = true;
	}

	if( !shouldReplace )
	{
		return false;
	}

	bestHit.triangle = triangle;
	bestHit.barycentric = Imath::V3f( triangleOutput.barycentric[0], triangleOutput.barycentric[1], triangleOutput.barycentric[2] );
	bestHit.objectPoint = Imath::V3f( objectPointX, objectPointY, objectPointZ );
	bestHit.worldPoint = bestHit.objectPoint * candidate.fullTransform;
	bestHit.distanceSquared = distanceSquared;
	bestHit.hasDistanceBound = true;
	bestHit.found = true;
	bestHit.preferred = trianglePreferred;
	return true;
}

bool updateClosestPaintHitCandidateBlock(
	const SurfaceCandidateView &candidateView,
	const Imath::V3f &referenceWorld,
	const Imath::V3f &referenceObject,
	std::size_t triangleOffset,
	std::size_t blockSize,
	int preferredTriangleIndex,
	bool preferTriangle,
	BestSurfaceHit &bestHit
)
{
	const ReprojectSurfaceCandidate &candidate = *candidateView.candidate;
	const BrushResolveKernelSet &kernels = brushResolveKernels();
	if( !kernels.closestPointOnTriangleBlock || blockSize == 0 )
	{
		return false;
	}

	alignas( 32 ) float ax[8] = { 0.0f };
	alignas( 32 ) float ay[8] = { 0.0f };
	alignas( 32 ) float az[8] = { 0.0f };
	alignas( 32 ) float bx[8] = { 0.0f };
	alignas( 32 ) float by[8] = { 0.0f };
	alignas( 32 ) float bz[8] = { 0.0f };
	alignas( 32 ) float cx[8] = { 0.0f };
	alignas( 32 ) float cy[8] = { 0.0f };
	alignas( 32 ) float cz[8] = { 0.0f };
	for( std::size_t i = 0; i < blockSize; ++i )
	{
		const std::size_t src = triangleOffset + i;
		ax[i] = candidate.triangleData.ax[src];
		ay[i] = candidate.triangleData.ay[src];
		az[i] = candidate.triangleData.az[src];
		bx[i] = candidate.triangleData.bx[src];
		by[i] = candidate.triangleData.by[src];
		bz[i] = candidate.triangleData.bz[src];
		cx[i] = candidate.triangleData.cx[src];
		cy[i] = candidate.triangleData.cy[src];
		cz[i] = candidate.triangleData.cz[src];
	}

	ClosestPointTriangleBlockInput blockInput;
	blockInput.ax = ax;
	blockInput.ay = ay;
	blockInput.az = az;
	blockInput.bx = bx;
	blockInput.by = by;
	blockInput.bz = bz;
	blockInput.cx = cx;
	blockInput.cy = cy;
	blockInput.cz = cz;
	blockInput.point = { referenceObject.x, referenceObject.y, referenceObject.z };
	ClosestPointTriangleBlockOutput blockOutput;
	kernels.closestPointOnTriangleBlock( blockInput, blockOutput );

	CandidateBlockHit candidateHit;
	for( std::size_t i = 0; i < blockSize; ++i )
	{
		const float px = blockOutput.pointX[i];
		const float py = blockOutput.pointY[i];
		const float pz = blockOutput.pointZ[i];
		const float distanceSquared = candidate.similarityTransform ?
			blockOutput.distanceSquared[i] * candidate.similarityScaleSquared :
			( ( Imath::V3f( px, py, pz ) * candidate.fullTransform ) - referenceWorld ).length2();
		if( distanceSquared < candidateHit.distanceSquared )
		{
			candidateHit.lane = static_cast<int>( i );
			candidateHit.distanceSquared = distanceSquared;
			candidateHit.point = { px, py, pz };
			candidateHit.barycentric = { blockOutput.baryX[i], blockOutput.baryY[i], blockOutput.baryZ[i] };
		}
	}

	if( candidateHit.lane < 0 )
	{
		return false;
	}

	const std::array<int, 4> &triangle = candidate.triangleData.triangles[triangleOffset + static_cast<std::size_t>( candidateHit.lane )];
	const bool trianglePreferred = preferTriangle && triangle[0] == preferredTriangleIndex;
	bool shouldReplace = false;
	if( !bestHit.found )
	{
		shouldReplace = !bestHit.hasDistanceBound || candidateHit.distanceSquared <= bestHit.distanceSquared + 1e-9f;
	}
	else if( candidateHit.distanceSquared < bestHit.distanceSquared - 1e-9f )
	{
		shouldReplace = true;
	}
	else if( std::abs( candidateHit.distanceSquared - bestHit.distanceSquared ) <= 1e-9f && trianglePreferred && !bestHit.preferred )
	{
		shouldReplace = true;
	}
	if( !shouldReplace )
	{
		return false;
	}

	bestHit.triangle = triangle;
	bestHit.barycentric = Imath::V3f( candidateHit.barycentric[0], candidateHit.barycentric[1], candidateHit.barycentric[2] );
	bestHit.objectPoint = Imath::V3f( candidateHit.point[0], candidateHit.point[1], candidateHit.point[2] );
	bestHit.worldPoint = bestHit.objectPoint * candidate.fullTransform;
	bestHit.distanceSquared = candidateHit.distanceSquared;
	bestHit.hasDistanceBound = true;
	bestHit.found = true;
	bestHit.preferred = trianglePreferred;
	return true;
}

float distanceSquaredToBounds( const Imath::V3f &point, const Imath::V3f &min, const Imath::V3f &max )
{
	const float dx = point.x < min.x ? ( min.x - point.x ) : ( point.x > max.x ? point.x - max.x : 0.0f );
	const float dy = point.y < min.y ? ( min.y - point.y ) : ( point.y > max.y ? point.y - max.y : 0.0f );
	const float dz = point.z < min.z ? ( min.z - point.z ) : ( point.z > max.z ? point.z - max.z : 0.0f );
	return dx * dx + dy * dy + dz * dz;
}

Imath::V3f transformedBoundCorner( const Imath::M44f &transform, const Imath::V3f &min, const Imath::V3f &max, int cornerIndex )
{
	return Imath::V3f(
		(cornerIndex & 1) ? max.x : min.x,
		(cornerIndex & 2) ? max.y : min.y,
		(cornerIndex & 4) ? max.z : min.z
	) * transform;
}

float worldDistanceSquaredToBounds(
	const Imath::V3f &referenceWorld,
	const Imath::M44f &fullTransform,
	const Imath::V3f &min,
	const Imath::V3f &max
)
{
	Imath::V3f worldMin = transformedBoundCorner( fullTransform, min, max, 0 );
	Imath::V3f worldMax = worldMin;
	for( int i = 1; i < 8; ++i )
	{
		const Imath::V3f corner = transformedBoundCorner( fullTransform, min, max, i );
		worldMin = Imath::V3f(
			std::min( worldMin.x, corner.x ),
			std::min( worldMin.y, corner.y ),
			std::min( worldMin.z, corner.z )
		);
		worldMax = Imath::V3f(
			std::max( worldMax.x, corner.x ),
			std::max( worldMax.y, corner.y ),
			std::max( worldMax.z, corner.z )
		);
	}
	return distanceSquaredToBounds( referenceWorld, worldMin, worldMax );
}

float candidateDistanceSquaredToBounds(
	const ReprojectSurfaceCandidate &candidate,
	const Imath::V3f &referenceWorld,
	const Imath::V3f &referenceObject,
	const Imath::V3f &min,
	const Imath::V3f &max
)
{
	if( candidate.similarityTransform )
	{
		return distanceSquaredToBounds( referenceObject, min, max ) * candidate.similarityScaleSquared;
	}
	return worldDistanceSquaredToBounds( referenceWorld, candidate.fullTransform, min, max );
}

bool triangleOffsetSkipped( std::size_t triangleOffset, int warmStartTriangleOffset, int preferredTriangleOffset )
{
	return
		static_cast<int>( triangleOffset ) == warmStartTriangleOffset ||
		static_cast<int>( triangleOffset ) == preferredTriangleOffset;
}

void scanTriangleRange(
	const SurfaceCandidateView &candidateView,
	const Imath::V3f &referenceWorld,
	const Imath::V3f &referenceObject,
	std::size_t firstTriangle,
	std::size_t triangleCount,
	int preferredTriangleIndex,
	bool preferTriangle,
	int warmStartTriangleOffset,
	int preferredTriangleOffset,
	BestSurfaceHit &bestHit
)
{
	const ReprojectSurfaceCandidate &candidate = *candidateView.candidate;
	const BrushResolveKernelSet &kernels = brushResolveKernels();
	const int blockWidth = std::max( 1, kernels.blockWidth );
	std::size_t triangleOffset = firstTriangle;
	const std::size_t lastTriangle = firstTriangle + triangleCount;
	while( triangleOffset < lastTriangle )
	{
		const std::size_t remaining = lastTriangle - triangleOffset;
		if( kernels.closestPointOnTriangleBlock && blockWidth > 1 && remaining >= static_cast<std::size_t>( blockWidth ) )
		{
			bool contiguousBlock = true;
			bool containsSkippedTriangle = false;
			for( int i = 0; i < blockWidth; ++i )
			{
				const std::size_t blockTriangleOffset = candidate.triangleData.bvhTriangleOffsets[triangleOffset + static_cast<std::size_t>( i )];
				if( i > 0 )
				{
					const std::size_t previousTriangleOffset = candidate.triangleData.bvhTriangleOffsets[triangleOffset + static_cast<std::size_t>( i - 1 )];
					if( blockTriangleOffset != previousTriangleOffset + 1 )
					{
						contiguousBlock = false;
						break;
					}
				}
				if( triangleOffsetSkipped( blockTriangleOffset, warmStartTriangleOffset, preferredTriangleOffset ) )
				{
					containsSkippedTriangle = true;
					break;
				}
			}
			if( contiguousBlock && !containsSkippedTriangle )
			{
				updateClosestPaintHitCandidateBlock(
					candidateView,
					referenceWorld,
					referenceObject,
					candidate.triangleData.bvhTriangleOffsets[triangleOffset],
					static_cast<std::size_t>( blockWidth ),
					preferredTriangleIndex,
					preferTriangle,
					bestHit
				);
				triangleOffset += static_cast<std::size_t>( blockWidth );
				continue;
			}
		}

		const std::size_t resolvedTriangleOffset = candidate.triangleData.bvhTriangleOffsets[triangleOffset];
		if( !triangleOffsetSkipped( resolvedTriangleOffset, warmStartTriangleOffset, preferredTriangleOffset ) )
		{
			const std::array<int, 4> &triangle = candidate.triangleData.triangles[resolvedTriangleOffset];
			const bool trianglePreferred = preferTriangle && triangle[0] == preferredTriangleIndex;
			updateClosestPaintHitCandidate(
				candidateView,
				referenceWorld,
				referenceObject,
				resolvedTriangleOffset,
				trianglePreferred,
				bestHit
			);
		}
		++triangleOffset;
	}
}

void traverseTriangleBvh(
	const SurfaceCandidateView &candidateView,
	const Imath::V3f &referenceWorld,
	const Imath::V3f &referenceObject,
	int preferredTriangleIndex,
	bool preferTriangle,
	int warmStartTriangleOffset,
	int preferredTriangleOffset,
	BestSurfaceHit &bestHit
)
{
	const ReprojectSurfaceCandidate &candidate = *candidateView.candidate;
	if( candidate.triangleData.bvhNodes.empty() || candidate.triangleData.bvhTriangleOffsets.empty() )
	{
		scanTriangleRange(
			candidateView,
			referenceWorld,
			referenceObject,
			0,
			candidate.triangleData.triangles.size(),
			preferredTriangleIndex,
			preferTriangle,
			warmStartTriangleOffset,
			preferredTriangleOffset,
			bestHit
		);
		return;
	}

	std::array<BvhTraversalEntry, 64> stack;
	std::size_t stackSize = 0;
	stack[stackSize++] = { 0, 0.0f };
	while( stackSize > 0 )
	{
		const BvhTraversalEntry entry = stack[--stackSize];
		if( bestHit.hasDistanceBound && entry.distanceSquared > bestHit.distanceSquared )
		{
			continue;
		}

		const TriangleBvhNode &node = candidate.triangleData.bvhNodes[entry.nodeIndex];
		if( node.isLeaf() )
		{
			scanTriangleRange(
				candidateView,
				referenceWorld,
				referenceObject,
				static_cast<std::size_t>( std::max( node.firstTriangle, 0 ) ),
				static_cast<std::size_t>( std::max( node.triangleCount, 0 ) ),
				preferredTriangleIndex,
				preferTriangle,
				warmStartTriangleOffset,
				preferredTriangleOffset,
				bestHit
			);
			continue;
		}

		BvhTraversalEntry children[2];
		int childCount = 0;
		for( const int childIndex : { node.leftChild, node.rightChild } )
		{
			if( childIndex < 0 )
			{
				continue;
			}
			const TriangleBvhNode &child = candidate.triangleData.bvhNodes[childIndex];
			const float childDistanceSquared = candidateDistanceSquaredToBounds(
				candidate,
				referenceWorld,
				referenceObject,
				child.min,
				child.max
			);
			if( bestHit.hasDistanceBound && childDistanceSquared > bestHit.distanceSquared )
			{
				continue;
			}
			children[childCount++] = { childIndex, childDistanceSquared };
		}
		if( childCount == 2 && children[0].distanceSquared < children[1].distanceSquared )
		{
			std::swap( children[0], children[1] );
		}
		for( int i = 0; i < childCount; ++i )
		{
			if( stackSize < stack.size() )
			{
				stack[stackSize++] = children[i];
			}
			else
			{
				scanTriangleRange(
					candidateView,
					referenceWorld,
					referenceObject,
					0,
					candidate.triangleData.triangles.size(),
					preferredTriangleIndex,
					preferTriangle,
					warmStartTriangleOffset,
					preferredTriangleOffset,
					bestHit
				);
				return;
			}
		}
	}
}

bool finalizeClosestPaintHit(
	const SurfaceCandidateView &candidateView,
	const BestSurfaceHit &bestHit,
	ReprojectHit &hit
);

bool barycentricStrictlyInterior( const Imath::V3f &barycentric, float epsilon = 1e-4f )
{
	return
		barycentric.x > epsilon && barycentric.x < 1.0f - epsilon &&
		barycentric.y > epsilon && barycentric.y < 1.0f - epsilon &&
		barycentric.z > epsilon && barycentric.z < 1.0f - epsilon;
}

bool tryFastAcceptTriangleHit(
	const SurfaceCandidateView &candidateView,
	const Imath::V3f &referenceWorld,
	const Imath::V3f &referenceObject,
	int triangleOffset,
	bool trianglePreferred,
	ReprojectHit &hit
)
{
	BestSurfaceHit bestHit;
	if( triangleOffset < 0 || !updateClosestPaintHitCandidate( candidateView, referenceWorld, referenceObject, static_cast<std::size_t>( triangleOffset ), trianglePreferred, bestHit ) )
	{
		return false;
	}
	if( !barycentricStrictlyInterior( bestHit.barycentric ) )
	{
		return false;
	}
	return finalizeClosestPaintHit( candidateView, bestHit, hit );
}

bool finalizeClosestPaintHit(
	const SurfaceCandidateView &candidateView,
	const BestSurfaceHit &bestHit,
	ReprojectHit &hit
)
{
	if( !bestHit.found )
	{
		return false;
	}

	const ReprojectSurfaceCandidate &candidate = *candidateView.candidate;
	const std::array<int, 4> &triangle = bestHit.triangle;
	const Imath::V3f a = candidate.triangleData.positions[triangle[1]];
	const Imath::V3f b = candidate.triangleData.positions[triangle[2]];
	const Imath::V3f c = candidate.triangleData.positions[triangle[3]];
	const Imath::V3f faceNormal = normalized( cross( b - a, c - a ), Imath::V3f( 0.0f, 1.0f, 0.0f ) );
	const Imath::V3f objectNormal = triangleNormal( candidate.triangleData.mesh.get(), triangle, bestHit.barycentric, faceNormal );
	const Imath::V3f objectUp = triangleUp( a, b, c, objectNormal );
	Imath::V2f uv;
	triangleUV( candidate.triangleData.mesh.get(), triangle, bestHit.barycentric, uv );

	hit.resolvedPath = candidateView.resolvedPath;
	hit.targetPath = candidateView.targetPath;
	hit.instanceSourcePath = candidateView.instanceSourcePath;
	hit.instanceId = candidateView.instanceId;
	hit.triangleIndex = triangle[0];
	hit.barycentric = bestHit.barycentric;
	hit.objectPoint = bestHit.objectPoint;
	hit.worldPoint = bestHit.worldPoint;
	hit.objectNormal = objectNormal;
	hit.objectUp = objectUp;
	hit.uv = uv;
	hit.distanceSquared = bestHit.distanceSquared;
	return true;
}

bool diagnosticsEnabled()
{
	static const bool g_enabled = []() {
		const char *value = std::getenv( "GAFFER_SCATTER_PAINT_DIAGNOSTICS" );
		if( !value || !value[0] )
		{
			return false;
		}

		const std::string normalized = value;
		return normalized != "0" && normalized != "false" && normalized != "FALSE";
	}();

	return g_enabled;
}

std::unordered_map<std::string, std::uint32_t> pathIdLookup( const std::vector<std::string> &paths )
{
	std::unordered_map<std::string, std::uint32_t> result;
	result.reserve( paths.size() );
	for( std::size_t i = 0; i < paths.size(); ++i )
	{
		result.emplace( paths[i], static_cast<std::uint32_t>( i + 1 ) );
	}
	return result;
}

std::string trimmedName( const std::string &value )
{
	const std::string whitespace = " \t\n\r\f\v";
	const std::size_t start = value.find_first_not_of( whitespace );
	if( start == std::string::npos )
	{
		return std::string();
	}
	const std::size_t end = value.find_last_not_of( whitespace );
	return value.substr( start, end - start + 1 );
}

std::uint32_t internPath( std::vector<std::string> &paths, const std::string &path )
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

std::array<float, 3> inheritedColor( const CacheSchema &schema, const LayerRecord &layer, const StrokeRecord &stroke )
{
	if( stroke.colorEnabled )
	{
		return stroke.color;
	}
	if( layer.colorEnabled )
	{
		return layer.color;
	}
	return schema.node.defaultColor;
}

std::array<float, 3> colorFromObject( const bp::dict &dictionary, const char *key, const std::array<float, 3> &defaultValue )
{
	const std::vector<float> values = floatVector( dictGet( dictionary, key ), 3, { defaultValue[0], defaultValue[1], defaultValue[2] } );
	return { values[0], values[1], values[2] };
}

PointRecord pointRecordFromAuthored( CacheSchema &schema, const LayerRecord &layer, const StrokeRecord &stroke, const bp::dict &pointData )
{
	const std::array<float, 3> inherited = inheritedColor( schema, layer, stroke );
	const std::vector<float> barycentric = floatVector( dictGet( pointData, "barycentric" ), 3, { 1.0f, 0.0f, 0.0f } );
	const std::vector<float> worldP = floatVector( pointData.has_key( "P" ) ? pointData["P"] : dictGet( pointData, "restWorldP" ), 3, { 0.0f, 0.0f, 0.0f } );
	const std::vector<float> objectP = floatVector( pointData.has_key( "restObjectP" ) ? pointData["restObjectP"] : bp::object(), 3, worldP );
	const std::vector<float> restUV = floatVector( dictGet( pointData, "restUV" ), 2, { 0.0f, 0.0f } );
	const std::vector<float> normal = floatVector( pointData.has_key( "N" ) ? pointData["N"] : dictGet( pointData, "restNormal" ), 3, { 0.0f, 1.0f, 0.0f } );
	const std::vector<float> up = floatVector( pointData.has_key( "up" ) ? pointData["up"] : dictGet( pointData, "restUp" ), 3, { 0.0f, 0.0f, 1.0f } );
	const std::vector<float> tangentRotation = floatVector( dictGet( pointData, "tangentRotation" ), 2, { 0.0f, 0.0f } );

	PointRecord point;
	point.pointId = schema.nextIds.point++;
	point.strokeId = stroke.strokeId;
	point.layerId = layer.layerId;
	point.targetPathId = internPath( schema.scenePaths, dictValue<std::string>( pointData, "sourcePath", "" ) );
	point.instanceId = dictValue<std::uint32_t>( pointData, "instanceId", 0 );
	point.instanceSourcePathId = internPath( schema.instanceSourcePaths, dictValue<std::string>( pointData, "instanceSourcePath", "" ) );
	point.triangleIndex = dictValue<std::uint32_t>( pointData, "triangleIndex", 0 );
	point.barycentric = { barycentric[0], barycentric[1], barycentric[2] };
	point.restObjectP = { objectP[0], objectP[1], objectP[2] };
	point.restWorldP = { worldP[0], worldP[1], worldP[2] };
	point.restUV = { restUV[0], restUV[1] };
	point.restNormal = { normal[0], normal[1], normal[2] };
	point.restUp = { up[0], up[1], up[2] };
	point.width = dictValue<float>( pointData, "width", 1.0f );
	point.uniformScale = pointData.has_key( "scale" ) ? extractOr<float>( pointData["scale"], 1.0f ) : dictValue<float>( pointData, "uniformScale", 1.0f );
	point.seed = dictValue<std::uint32_t>( pointData, "seed", 0 );
	point.normalSpin = dictValue<float>( pointData, "normalSpin", 0.0f );
	point.tangentRotation = { tangentRotation[0], tangentRotation[1] };
	point.pressureDensity = dictValue<float>( pointData, "pressureDensity", 1.0f );
	point.pressureSoftness = dictValue<float>( pointData, "pressureSoftness", 1.0f );
	point.valid = dictValue<bool>( pointData, "valid", true );
	point.lastValidFrame = dictValue<int>( pointData, "lastValidFrame", 0 );
	point.anchorModeUsed = dictValue<bool>( pointData, "attachmentResolved", false ) ? AnchorMode::Barycentric : AnchorMode::Reprojected;
	point.topologyGeneration = dictValue<std::uint32_t>( pointData, "topologyGeneration", 0 );
	point.colorEnabled = dictValue<bool>( pointData, "colorEnabled", pointData.has_key( "color" ) );
	point.color = colorFromObject( pointData, "color", inherited );
	return point;
}

std::uint32_t internPathCached( std::vector<std::string> &paths, PathIdCache &cache, const std::string &path )
{
	if( path.empty() )
	{
		return 0;
	}
	auto it = cache.find( path );
	if( it != cache.end() )
	{
		return it->second;
	}
	const std::uint32_t id = internPath( paths, path );
	cache.emplace( path, id );
	return id;
}

CompiledBrushSample compileBrushSample( PyObject *sample, SampleCompileCache &cache )
{
	CompiledBrushSample result;
	PyObject *pointValue = dictItemBorrowed( sample, "point" );
	PyObject *scaleValue = dictItemBorrowed( sample, "scale" );
	PyObject *uniformScaleValue = dictItemBorrowed( sample, "uniformScale" );
	PyObject *sourcePathValue = dictItemBorrowed( sample, "sourcePath" );
	PyObject *instanceIdValue = dictItemBorrowed( sample, "instanceId" );
	PyObject *instanceSourcePathValue = dictItemBorrowed( sample, "instanceSourcePath" );
	PyObject *triangleIndexValue = dictItemBorrowed( sample, "triangleIndex" );
	PyObject *normalSpinValue = dictItemBorrowed( sample, "normalSpin" );
	PyObject *tangentRotationValue = dictItemBorrowed( sample, "tangentRotation" );
	PyObject *widthValue = dictItemBorrowed( sample, "width" );
	PyObject *pressureDensityValue = dictItemBorrowed( sample, "pressureDensity" );
	PyObject *pressureSoftnessValue = dictItemBorrowed( sample, "pressureSoftness" );
	PyObject *validValue = dictItemBorrowed( sample, "valid" );
	result.worldPosition = pyV3fFast( pointValue, Imath::V3f( 0.0f ) );
	result.sourcePath = cachedPyStringFast( sourcePathValue, cache.sourcePathObject, cache.sourcePath, std::string() );
	result.instanceId = cachedPyUInt32Fast( instanceIdValue, cache.instanceIdObject, cache.instanceId, 0 );
	result.instanceSourcePath = cachedPyStringFast( instanceSourcePathValue, cache.instanceSourcePathObject, cache.instanceSourcePath, std::string() );
	result.preferredTriangleIndex = cachedPyIntFast( triangleIndexValue, cache.triangleIndexObject, cache.triangleIndex, -1 );
	result.normalSpin = cachedPyFloatFast( normalSpinValue, cache.normalSpinObject, cache.normalSpin, 0.0f );
	result.tangentRotation = pyV2fFast( tangentRotationValue, Imath::V2f( 0.0f ) );
	result.width = cachedPyFloatFast( widthValue, cache.widthObject, cache.width, 1.0f );
	result.uniformScale = scaleValue ? cachedPyFloatFast( scaleValue, cache.scaleObject, cache.scale, 1.0f ) : cachedPyFloatFast( uniformScaleValue, cache.uniformScaleObject, cache.uniformScale, 1.0f );
	result.pressureDensity = cachedPyFloatFast( pressureDensityValue, cache.pressureDensityObject, cache.pressureDensity, 1.0f );
	result.pressureSoftness = cachedPyFloatFast( pressureSoftnessValue, cache.pressureSoftnessObject, cache.pressureSoftness, 1.0f );
	result.baseSeed = pyIntFast( dictItemBorrowed( sample, "seed" ), 0 );
	result.valid = cachedPyBoolFast( validValue, cache.validObject, cache.valid, true );
	return result;
}

PointRecord pointRecordFromResolvedSample(
	CacheSchema &schema,
	const LayerRecord &layer,
	const StrokeRecord &stroke,
	const ResolvedBuildContext &context,
	const CompiledSurfacePoint &surfacePoint,
	int seedOffset
)
{
	const std::uint32_t seed = static_cast<std::uint32_t>( std::max( 0, context.baseSeed + seedOffset ) );
	const float displayLift = std::max( context.width * 0.1f, 0.01f );
	const Imath::V3f liftedWorldPoint = surfacePoint.worldPoint + ( surfacePoint.normal * displayLift );

	PointRecord point;
	point.pointId = schema.nextIds.point++;
	point.strokeId = stroke.strokeId;
	point.layerId = layer.layerId;
	point.targetPathId = internPathCached( schema.scenePaths, const_cast<PathIdCache &>( context.scenePathIds ), surfacePoint.sourcePath );
	point.instanceId = surfacePoint.instanceId;
	point.instanceSourcePathId = internPathCached( schema.instanceSourcePaths, const_cast<PathIdCache &>( context.instanceSourcePathIds ), surfacePoint.instanceSourcePath );
	point.triangleIndex = surfacePoint.triangleIndex;
	point.barycentric = { surfacePoint.barycentric.x, surfacePoint.barycentric.y, surfacePoint.barycentric.z };
	point.restObjectP = { surfacePoint.objectPoint.x, surfacePoint.objectPoint.y, surfacePoint.objectPoint.z };
	point.restWorldP = { liftedWorldPoint.x, liftedWorldPoint.y, liftedWorldPoint.z };
	point.restUV = { surfacePoint.restUV.x, surfacePoint.restUV.y };
	point.restNormal = { surfacePoint.normal.x, surfacePoint.normal.y, surfacePoint.normal.z };
	point.restUp = { surfacePoint.up.x, surfacePoint.up.y, surfacePoint.up.z };
	point.width = context.width;
	point.uniformScale = context.uniformScale;
	point.seed = seed;
	point.normalSpin = context.normalSpin;
	point.tangentRotation = { context.tangentRotation.x, context.tangentRotation.y };
	point.pressureDensity = context.pressureDensity;
	point.pressureSoftness = context.pressureSoftness;
	point.valid = context.valid;
	point.lastValidFrame = 0;
	point.anchorModeUsed = AnchorMode::Barycentric;
	point.topologyGeneration = 0;
	point.colorEnabled = false;
	point.color = context.inheritedColor;
	return point;
}

StrokeRecord *resolveStroke( CacheSchema &schema, const bp::object &identifier )
{
	const bool hasInt = bp::extract<std::uint64_t>( identifier ).check();
	const std::uint64_t idValue = hasInt ? bp::extract<std::uint64_t>( identifier )() : 0;
	const bool hasString = bp::extract<std::string>( identifier ).check();
	const std::string nameValue = hasString ? bp::extract<std::string>( identifier )() : std::string();
	for( StrokeRecord &stroke : schema.strokes )
	{
		if( ( hasInt && stroke.strokeId == idValue ) || ( hasString && stroke.name == nameValue ) )
		{
			return &stroke;
		}
	}
	throw std::runtime_error( "Unknown stroke" );
}

LayerRecord *findLayer( CacheSchema &schema, std::uint64_t layerId )
{
	for( LayerRecord &layer : schema.layers )
	{
		if( layer.layerId == layerId )
		{
			return &layer;
		}
	}
	return nullptr;
}

LayerRecord *findLayerByName( CacheSchema &schema, const std::string &name )
{
	for( LayerRecord &layer : schema.layers )
	{
		if( layer.name == name )
		{
			return &layer;
		}
	}
	return nullptr;
}

StrokeRecord *findStrokeByName( CacheSchema &schema, std::uint64_t layerId, const std::string &name )
{
	for( StrokeRecord &stroke : schema.strokes )
	{
		if( stroke.layerId == layerId && stroke.name == name )
		{
			return &stroke;
		}
	}
	return nullptr;
}

void reindexLayerOrder( CacheSchema &schema )
{
	std::stable_sort(
		schema.layers.begin(), schema.layers.end(),
		[]( const LayerRecord &a, const LayerRecord &b ) {
			return a.order < b.order;
		}
	);
	for( std::size_t i = 0; i < schema.layers.size(); ++i )
	{
		schema.layers[i].order = static_cast<std::int32_t>( i );
	}
}

void reindexStrokeOrder( CacheSchema &schema, std::uint64_t layerId )
{
	std::vector<StrokeRecord *> layerStrokes;
	for( StrokeRecord &stroke : schema.strokes )
	{
		if( stroke.layerId == layerId )
		{
			layerStrokes.push_back( &stroke );
		}
	}
	std::stable_sort(
		layerStrokes.begin(), layerStrokes.end(),
		[]( const StrokeRecord *a, const StrokeRecord *b ) {
			return a->order < b->order;
		}
	);
	for( std::size_t i = 0; i < layerStrokes.size(); ++i )
	{
		layerStrokes[i]->order = static_cast<std::int32_t>( i );
	}
}

void rebuildChunksAndCounts( CacheSchema &schema, const std::set<std::uint64_t> &changedStrokeIds )
{
	rebuildSchemaChunksAndCounts( schema, changedStrokeIds );
}

bp::dict brushPaintResult( std::size_t committed, int resolved, int fallback, double authoredBuildMs )
{
	bp::dict result;
	result["committed"] = committed;
	result["resolved"] = resolved;
	result["fallback"] = fallback;
	result["authoredBuildMs"] = authoredBuildMs;
	return result;
}

std::string sampleOriginLabel( const bp::dict &sample )
{
	std::string origin = trimmedName( dictValue<std::string>( sample, "sampleOrigin", "unknown" ) );
	if( origin.empty() )
	{
		origin = "unknown";
	}
	if( dictValue<bool>( sample, "sampleExpanded", false ) )
	{
		origin += "+jittered";
	}
	return origin;
}

std::string sampleOriginLabel( const BrushSampleData &sample )
{
	std::string origin = trimmedName( sample.sampleOrigin );
	if( origin.empty() )
	{
		origin = "unknown";
	}
	if( sample.sampleExpanded )
	{
		origin += "+jittered";
	}
	return origin;
}

std::string sampleSummary( const bp::dict &sample )
{
	const std::vector<float> point = floatVector( dictGet( sample, "point" ), 3, { 0.0f, 0.0f, 0.0f } );
	const std::vector<float> barycentric = floatVector( dictGet( sample, "barycentric" ), 3, { 1.0f, 0.0f, 0.0f } );
	std::ostringstream stream;
	stream
		<< sampleOriginLabel( sample )
		<< " sourceIndex=" << dictValue<int>( sample, "sampleSourceIndex", -1 )
		<< " expansionIndex=" << dictValue<int>( sample, "sampleExpansionIndex", 0 )
		<< " path=" << dictValue<std::string>( sample, "sourcePath", std::string() )
		<< " triangle=" << dictValue<int>( sample, "triangleIndex", 0 )
		<< " point=(" << point[0] << ", " << point[1] << ", " << point[2] << ")"
		<< " barycentric=(" << barycentric[0] << ", " << barycentric[1] << ", " << barycentric[2] << ")"
		<< " attachmentResolved=" << ( dictValue<bool>( sample, "attachmentResolved", false ) ? "true" : "false" );
	return stream.str();
}

std::string sampleSummary( const BrushSampleData &sample )
{
	std::ostringstream stream;
	stream
		<< sampleOriginLabel( sample )
		<< " sourceIndex=" << sample.sampleSourceIndex
		<< " expansionIndex=" << sample.sampleExpansionIndex
		<< " path=" << sample.sourcePath
		<< " triangle=" << sample.triangleIndex
		<< " point=(" << sample.point.x << ", " << sample.point.y << ", " << sample.point.z << ")"
		<< " barycentric=(" << sample.barycentric.x << ", " << sample.barycentric.y << ", " << sample.barycentric.z << ")"
		<< " attachmentResolved=" << ( sample.attachmentResolved ? "true" : "false" );
	return stream.str();
}

std::string fallbackDiagnosticsSummary(
	const std::map<std::string, int> &fallbackCountsByOrigin,
	const std::vector<std::string> &fallbackExamples
)
{
	if( fallbackCountsByOrigin.empty() )
	{
		return std::string();
	}

	std::ostringstream stream;
	stream << "fallback authored samples by origin=";
	bool first = true;
	for( const auto &[origin, count] : fallbackCountsByOrigin )
	{
		if( !first )
		{
			stream << ", ";
		}
		first = false;
		stream << origin << ":" << count;
	}
	if( !fallbackExamples.empty() )
	{
		stream << " | examples: ";
		for( std::size_t i = 0; i < fallbackExamples.size(); ++i )
		{
			if( i )
			{
				stream << " ; ";
			}
			stream << fallbackExamples[i];
		}
	}
	return stream.str();
}

bp::dict brushEraseResult( std::size_t removedCount, std::size_t strokeCount, int eraseSpace = 0 )
{
	bp::dict result;
	result["removed"] = removedCount;
	result["removedCount"] = removedCount;
	result["strokeCount"] = strokeCount;
	result["eraseSpace"] = eraseSpace;
	return result;
}

bp::dict ensureLayerStrokeResult(
	std::uint64_t layerId,
	std::uint64_t strokeId,
	bool layerCreated,
	bool strokeCreated,
	double loadStoreMs,
	double layerLookupMs,
	double strokeLookupMs,
	double writeMs,
	double loadResolvePathMs,
	double loadReadBytesMs,
	double loadBlobExtractMs,
	double loadUnpackMs,
	double loadPopulateMetadataMs,
	double unpackChecksumMs,
	double unpackHeaderMs,
	double unpackNodeMs,
	double unpackLockMs,
	double unpackScenePathsMs,
	double unpackLayersMs,
	double unpackStrokesMs,
	double unpackChunksMs,
	double unpackPointsMs,
	double unpackSelectionSetsMs,
	double unpackDiagnosticsMs,
	double unpackUpgradesMs,
	double unpackPointBackupsMs,
	double writePopulateMetadataMs,
	double writeLockMs,
	double writePackMs,
	double packHeaderMs,
	double packNodeMs,
	double packLockMs,
	double packScenePathsMs,
	double packLayersMs,
	double packStrokesMs,
	double packChunksMs,
	double packPointsMs,
	double packPointsReserveMs,
	double packPointsReserveReallocated,
	double packPointsCapacityBeforeBytes,
	double packPointsCapacityAfterBytes,
	double packPointsResizeMs,
	double packPointsFillMs,
	double packPointsCopyMs,
	double packPointsChecksumInlineMs,
	double packPointsChecksumMs,
	double packSelectionSetsMs,
	double packDiagnosticsMs,
	double packUpgradesMs,
	double packPointBackupsMs,
	double packFinalChecksumMs,
	double packFinalHeaderWriteMs,
	double packFinalBufferMs,
	double writeResolvePathMs,
	double writeBackupMs,
	double writeBytesMs,
	double writeClearBlobMs,
	double writeReleaseLockMs,
	double writeBlobObjectMs,
	double writeBlobObjectResizeMs,
	double writeBlobObjectCopyMs,
	double writeBlobPlugSetMs,
	double writeSetBlobMs,
	double totalMs
)
{
	bp::dict result;
	result["layerId"] = layerId;
	result["strokeId"] = strokeId;
	result["layerCreated"] = layerCreated;
	result["strokeCreated"] = strokeCreated;
	result["loadStoreMs"] = loadStoreMs;
	result["layerLookupMs"] = layerLookupMs;
	result["strokeLookupMs"] = strokeLookupMs;
	result["writeMs"] = writeMs;
	result["loadResolvePathMs"] = loadResolvePathMs;
	result["loadReadBytesMs"] = loadReadBytesMs;
	result["loadBlobExtractMs"] = loadBlobExtractMs;
	result["loadUnpackMs"] = loadUnpackMs;
	result["loadPopulateMetadataMs"] = loadPopulateMetadataMs;
	result["unpackChecksumMs"] = unpackChecksumMs;
	result["unpackHeaderMs"] = unpackHeaderMs;
	result["unpackNodeMs"] = unpackNodeMs;
	result["unpackLockMs"] = unpackLockMs;
	result["unpackScenePathsMs"] = unpackScenePathsMs;
	result["unpackLayersMs"] = unpackLayersMs;
	result["unpackStrokesMs"] = unpackStrokesMs;
	result["unpackChunksMs"] = unpackChunksMs;
	result["unpackPointsMs"] = unpackPointsMs;
	result["unpackSelectionSetsMs"] = unpackSelectionSetsMs;
	result["unpackDiagnosticsMs"] = unpackDiagnosticsMs;
	result["unpackUpgradesMs"] = unpackUpgradesMs;
	result["unpackPointBackupsMs"] = unpackPointBackupsMs;
	result["writePopulateMetadataMs"] = writePopulateMetadataMs;
	result["writeLockMs"] = writeLockMs;
	result["writePackMs"] = writePackMs;
	result["packHeaderMs"] = packHeaderMs;
	result["packNodeMs"] = packNodeMs;
	result["packLockMs"] = packLockMs;
	result["packScenePathsMs"] = packScenePathsMs;
	result["packLayersMs"] = packLayersMs;
	result["packStrokesMs"] = packStrokesMs;
	result["packChunksMs"] = packChunksMs;
	result["packPointsMs"] = packPointsMs;
	result["packPointsReserveMs"] = packPointsReserveMs;
	result["packPointsReserveReallocated"] = packPointsReserveReallocated;
	result["packPointsCapacityBeforeBytes"] = packPointsCapacityBeforeBytes;
	result["packPointsCapacityAfterBytes"] = packPointsCapacityAfterBytes;
	result["packPointsResizeMs"] = packPointsResizeMs;
	result["packPointsFillMs"] = packPointsFillMs;
	result["packPointsCopyMs"] = packPointsCopyMs;
	result["packPointsChecksumInlineMs"] = packPointsChecksumInlineMs;
	result["packPointsChecksumMs"] = packPointsChecksumMs;
	result["packSelectionSetsMs"] = packSelectionSetsMs;
	result["packDiagnosticsMs"] = packDiagnosticsMs;
	result["packUpgradesMs"] = packUpgradesMs;
	result["packPointBackupsMs"] = packPointBackupsMs;
	result["packFinalChecksumMs"] = packFinalChecksumMs;
	result["packFinalHeaderWriteMs"] = packFinalHeaderWriteMs;
	result["packFinalBufferMs"] = packFinalBufferMs;
	result["writeResolvePathMs"] = writeResolvePathMs;
	result["writeBackupMs"] = writeBackupMs;
	result["writeBytesMs"] = writeBytesMs;
	result["writeClearBlobMs"] = writeClearBlobMs;
	result["writeReleaseLockMs"] = writeReleaseLockMs;
	result["writeBlobObjectMs"] = writeBlobObjectMs;
	result["writeBlobObjectResizeMs"] = writeBlobObjectResizeMs;
	result["writeBlobObjectCopyMs"] = writeBlobObjectCopyMs;
	result["writeBlobPlugSetMs"] = writeBlobPlugSetMs;
	result["writeSetBlobMs"] = writeSetBlobMs;
	result["totalMs"] = totalMs;
	return result;
}

SampleCellKey sampleCellKey( const Imath::V3f &point, float cellSize )
{
	return {
		static_cast<int>( std::floor( point.x / cellSize ) ),
		static_cast<int>( std::floor( point.y / cellSize ) ),
		static_cast<int>( std::floor( point.z / cellSize ) )
	};
}

void addSamplePosition( SampleGroupData &group, const Imath::V3f &point, float cellSize )
{
	if( !group.hasBounds )
	{
		group.min = point;
		group.max = point;
		group.hasBounds = true;
	}
	else
	{
		group.min.x = std::min( group.min.x, point.x );
		group.min.y = std::min( group.min.y, point.y );
		group.min.z = std::min( group.min.z, point.z );
		group.max.x = std::max( group.max.x, point.x );
		group.max.y = std::max( group.max.y, point.y );
		group.max.z = std::max( group.max.z, point.z );
	}
	group.buckets[sampleCellKey( point, cellSize )].push_back( point );
}

bool pointWithinExpandedBounds( const SampleGroupData &group, const Imath::V3f &point, float radius )
{
	if( !group.hasBounds )
	{
		return false;
	}
	return
		point.x >= group.min.x - radius && point.x <= group.max.x + radius &&
		point.y >= group.min.y - radius && point.y <= group.max.y + radius &&
		point.z >= group.min.z - radius && point.z <= group.max.z + radius;
}

bool pointMatchesSampleGroup( const SampleGroupData &group, const Imath::V3f &point, float radius, float radiusSquared )
{
	if( !pointWithinExpandedBounds( group, point, radius ) )
	{
		return false;
	}

	const SampleCellKey center = sampleCellKey( point, radius );
	for( int dz = -1; dz <= 1; ++dz )
	{
		for( int dy = -1; dy <= 1; ++dy )
		{
			for( int dx = -1; dx <= 1; ++dx )
			{
				auto bucketIt = group.buckets.find( { center.x + dx, center.y + dy, center.z + dz } );
				if( bucketIt == group.buckets.end() )
				{
					continue;
				}

				for( const Imath::V3f &samplePosition : bucketIt->second )
				{
					const Imath::V3f delta = point - samplePosition;
					if( delta.dot( delta ) <= radiusSquared )
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

SampleGroups buildSampleGroups( const CacheSchema &schema, const bp::object &samples, float cellSize )
{
	SampleGroups result;
	const std::unordered_map<std::string, std::uint32_t> scenePathIds = pathIdLookup( schema.scenePaths );
	const std::unordered_map<std::string, std::uint32_t> instanceSourcePathIds = pathIdLookup( schema.instanceSourcePaths );
	for( bp::stl_input_iterator<bp::object> it( samples ), end; it != end; ++it )
	{
		const bp::dict sample = bp::extract<bp::dict>( *it );
		const std::string sourcePath = dictValue<std::string>( sample, "sourcePath", "" );
		const std::string instanceSourcePath = dictValue<std::string>( sample, "instanceSourcePath", "" );
		const auto sourceIt = scenePathIds.find( sourcePath );
		if( sourceIt == scenePathIds.end() )
		{
			continue;
		}

		const auto instanceSourceIt = instanceSourcePath.empty() ? instanceSourcePathIds.end() : instanceSourcePathIds.find( instanceSourcePath );
		SampleGroupKey key;
		key.targetPathId = sourceIt->second;
		key.instanceId = dictValue<std::uint32_t>( sample, "instanceId", 0 );
		key.instanceSourcePathId = instanceSourceIt == instanceSourcePathIds.end() ? 0 : instanceSourceIt->second;
		addSamplePosition( result[key], vectorFromValues( dictGet( sample, "point" ), Imath::V3f( 0.0f ) ), cellSize );
	}
	return result;
}

std::vector<float> samplePositionValues( const bp::dict &sample )
{
	return floatVector( dictGet( sample, "point" ), 3, { 0.0f, 0.0f, 0.0f } );
}

Imath::V3f sampleWorldPosition( const bp::dict &sample )
{
	const std::vector<float> point = samplePositionValues( sample );
	return Imath::V3f( point[0], point[1], point[2] );
}

std::array<int, 3> roundedBarycentricKey( const Imath::V3f &barycentric )
{
	return {
		static_cast<int>( std::lround( barycentric.x * 1000000.0f ) ),
		static_cast<int>( std::lround( barycentric.y * 1000000.0f ) ),
		static_cast<int>( std::lround( barycentric.z * 1000000.0f ) )
	};
}

bp::list vectorList( const Imath::V3f &value )
{
	bp::list result;
	result.append( value.x );
	result.append( value.y );
	result.append( value.z );
	return result;
}

bp::list vectorList( const Imath::V2f &value )
{
	bp::list result;
	result.append( value.x );
	result.append( value.y );
	return result;
}

CompiledSurfacePoint surfacePointFromHit( const ReprojectHit &hit, const Imath::M44f &fullTransform )
{
	Imath::V3f worldNormal;
	fullTransform.multDirMatrix( hit.objectNormal, worldNormal );
	Imath::V3f worldUp;
	fullTransform.multDirMatrix( hit.objectUp, worldUp );
	CompiledSurfacePoint result;
	const Imath::V3f normal = normalized( worldNormal, Imath::V3f( 0.0f, 1.0f, 0.0f ) );
	const Imath::V3f up = orthogonalized( worldUp, normal, Imath::V3f( 0.0f, 0.0f, 1.0f ) );
	result.sourcePath = hit.targetPath;
	result.instanceId = hit.instanceId;
	result.instanceSourcePath = hit.instanceSourcePath;
	result.triangleIndex = static_cast<std::uint32_t>( std::max( 0, hit.triangleIndex ) );
	result.barycentric = hit.barycentric;
	result.objectPoint = hit.objectPoint;
	result.worldPoint = hit.worldPoint;
	result.restUV = hit.uv;
	result.normal = normal;
	result.up = up;
	return result;
}

bool closestPaintHit(
	const SurfaceCandidateView &candidateView,
	const Imath::V3f &referenceWorld,
	int preferredTriangleIndex,
	int warmStartTriangleIndex,
	const Imath::V3f *warmStartWorldPoint,
	bool preferTriangle,
	ReprojectHit &hit
)
{
	const ReprojectSurfaceCandidate &candidate = *candidateView.candidate;
	const Imath::V3f referenceObject = referenceWorld * candidate.inverseTransform;
	const int warmStartTriangleOffset = triangleOffsetForIndex( candidate.triangleData, warmStartTriangleIndex );
	const int preferredTriangleOffset = preferredTriangleIndex >= 0 ? triangleOffsetForIndex( candidate.triangleData, preferredTriangleIndex ) : -1;
	if( warmStartTriangleOffset >= 0 )
	{
		if( tryFastAcceptTriangleHit( candidateView, referenceWorld, referenceObject, warmStartTriangleOffset, preferTriangle && warmStartTriangleIndex == preferredTriangleIndex, hit ) )
		{
			return true;
		}
	}
	if( preferredTriangleOffset >= 0 && preferredTriangleOffset != warmStartTriangleOffset )
	{
		if( tryFastAcceptTriangleHit( candidateView, referenceWorld, referenceObject, preferredTriangleOffset, preferTriangle, hit ) )
		{
			return true;
		}
	}

	BestSurfaceHit bestHit;
	if( warmStartWorldPoint )
	{
		bestHit.distanceSquared = ( *warmStartWorldPoint - referenceWorld ).length2();
		bestHit.hasDistanceBound = true;
	}
	if( warmStartTriangleOffset >= 0 )
	{
		updateClosestPaintHitCandidate(
			candidateView,
			referenceWorld,
			referenceObject,
			static_cast<std::size_t>( warmStartTriangleOffset ),
			preferTriangle && warmStartTriangleIndex == preferredTriangleIndex,
			bestHit
		);
	}
	if( preferredTriangleOffset >= 0 && preferredTriangleOffset != warmStartTriangleOffset )
	{
		updateClosestPaintHitCandidate(
			candidateView,
			referenceWorld,
			referenceObject,
			static_cast<std::size_t>( preferredTriangleOffset ),
			preferTriangle,
			bestHit
		);
	}
	traverseTriangleBvh(
		candidateView,
		referenceWorld,
		referenceObject,
		preferredTriangleIndex,
		preferTriangle,
		warmStartTriangleOffset,
		preferredTriangleOffset,
		bestHit
	);

	return finalizeClosestPaintHit( candidateView, bestHit, hit );
}

bool directSurfaceCandidate(
	const ScenePlug *scene,
	const CompiledBrushSample &sample,
	DirectCandidateCache &cache,
	std::unordered_set<std::string> &missingPaths,
	SurfaceCandidateView &candidate
)
{
	const std::string &sourcePath = sample.sourcePath;
	if( sourcePath.empty() )
	{
		return false;
	}

	auto cacheIt = cache.find( sourcePath );
	if( cacheIt == cache.end() )
	{
		if( missingPaths.count( sourcePath ) )
		{
			return false;
		}

		PointSurfaceReference reference;
		reference.targetPath = sourcePath;
		reference.resolvedPath = sourcePath;
		ReprojectSurfaceCandidate builtCandidate;
		if( !reprojectSurfaceCandidate( scene, reference, builtCandidate ) )
		{
			missingPaths.insert( sourcePath );
			return false;
		}
		cacheIt = cache.emplace( sourcePath, std::move( builtCandidate ) ).first;
	}

	candidate.candidate = &cacheIt->second;
	candidate.targetPath = sourcePath;
	candidate.resolvedPath = sourcePath;
	candidate.instanceId = sample.instanceId;
	candidate.instanceSourcePath = sample.instanceSourcePath;
	return true;
}

bool directSurfaceCandidate(
	const ScenePlug *scene,
	const BrushSampleData &sample,
	DirectCandidateCache &cache,
	std::unordered_set<std::string> &missingPaths,
	SurfaceCandidateView &candidate
)
{
	const std::string &sourcePath = sample.sourcePath;
	if( sourcePath.empty() )
	{
		return false;
	}

	auto cacheIt = cache.find( sourcePath );
	if( cacheIt == cache.end() )
	{
		if( missingPaths.count( sourcePath ) )
		{
			return false;
		}

		PointSurfaceReference reference;
		reference.targetPath = sourcePath;
		reference.resolvedPath = sourcePath;
		ReprojectSurfaceCandidate builtCandidate;
		if( !reprojectSurfaceCandidate( scene, reference, builtCandidate ) )
		{
			missingPaths.insert( sourcePath );
			return false;
		}
		cacheIt = cache.emplace( sourcePath, std::move( builtCandidate ) ).first;
	}

	candidate.candidate = &cacheIt->second;
	candidate.targetPath = sourcePath;
	candidate.resolvedPath = sourcePath;
	candidate.instanceId = sample.instanceId;
	candidate.instanceSourcePath = sample.instanceSourcePath;
	return true;
}

std::vector<ReprojectSurfaceCandidate> compiledFilteredSurfaceCandidates( const PaintedPoints *node )
{
	std::vector<ReprojectSurfaceCandidate> result;
	const ScenePlug *scene = node ? node->inPlug() : nullptr;
	if( !scene || !node )
	{
		return result;
	}

	for( const PointSurfaceReference &reference : filteredSurfaceReferences( scene, node ) )
	{
		ReprojectSurfaceCandidate candidate;
		if( reprojectSurfaceCandidate( scene, reference, candidate ) )
		{
			result.push_back( std::move( candidate ) );
		}
	}

	return result;
}

std::vector<CompiledSurfacePoint> compiledBrushSampleSurfacePoints(
	const ScenePlug *scene,
	const CompiledBrushSample &sample,
	const std::vector<ReprojectSurfaceCandidate> *filteredCandidates,
	DirectCandidateCache &directCandidateCache,
	std::unordered_set<std::string> &missingDirectPaths,
	SurfaceResolveCache *surfaceResolveCache,
	SurfaceResolveTimings *timings = nullptr
)
{
	std::vector<CompiledSurfacePoint> results;
	if( !scene )
	{
		return results;
	}

	const std::string &preferredSourcePath = sample.sourcePath;
	const int preferredTriangleIndex = sample.preferredTriangleIndex;
	const Imath::V3f &worldPosition = sample.worldPosition;
	SurfaceResolveCacheKey cacheKey;
	cacheKey.sourcePath = sample.sourcePath;
	cacheKey.instanceId = sample.instanceId;
	cacheKey.instanceSourcePath = sample.instanceSourcePath;
	const bool directMode = !filteredCandidates;
	int warmStartTriangleIndex = -1;
	const Imath::V3f *warmStartWorldPoint = nullptr;
	if( directMode && surfaceResolveCache )
	{
		auto cacheIt = surfaceResolveCache->find( cacheKey );
		if( cacheIt != surfaceResolveCache->end() )
		{
			warmStartTriangleIndex = cacheIt->second.triangleIndex;
			if( cacheIt->second.hasWorldPoint )
			{
				warmStartWorldPoint = &cacheIt->second.worldPoint;
			}
		}
	}
	std::unordered_set<SurfacePointKey, SurfacePointKeyHash> seen;

		auto appendHit = [&]( const SurfaceCandidateView &candidate ) {
			ReprojectHit hit;
			const bool preferTriangle = preferredTriangleIndex >= 0 && candidate.targetPath == preferredSourcePath;
			const auto hitSearchStart = Clock::now();
			if( !closestPaintHit( candidate, worldPosition, preferredTriangleIndex, warmStartTriangleIndex, warmStartWorldPoint, preferTriangle, hit ) )
			{
				if( timings )
				{
					timings->hitSearchMs += elapsedMilliseconds( hitSearchStart );
			}
			return;
		}
		if( timings )
		{
			timings->hitSearchMs += elapsedMilliseconds( hitSearchStart );
		}

		SurfacePointKey key;
		key.sourcePath = hit.targetPath;
		key.instanceId = hit.instanceId;
		key.instanceSourcePath = hit.instanceSourcePath;
		key.triangleIndex = hit.triangleIndex;
		key.barycentric = roundedBarycentricKey( hit.barycentric );
		if( !seen.insert( key ).second )
		{
			return;
		}
		if( directMode && surfaceResolveCache )
		{
			SurfaceResolveCacheValue &cached = ( *surfaceResolveCache )[cacheKey];
			cached.triangleIndex = hit.triangleIndex;
			cached.worldPoint = hit.worldPoint;
			cached.hasWorldPoint = true;
			warmStartTriangleIndex = hit.triangleIndex;
			warmStartWorldPoint = &cached.worldPoint;
		}

		const auto hitBuildStart = Clock::now();
		results.push_back( surfacePointFromHit( hit, candidate.candidate->fullTransform ) );
		if( timings )
		{
			timings->hitBuildMs += elapsedMilliseconds( hitBuildStart );
		}
	};

	if( filteredCandidates )
	{
		for( const ReprojectSurfaceCandidate &candidate : *filteredCandidates )
		{
			SurfaceCandidateView candidateView;
			candidateView.candidate = &candidate;
			candidateView.targetPath = candidate.targetPath;
			candidateView.resolvedPath = candidate.resolvedPath;
			candidateView.instanceId = candidate.instanceId;
			candidateView.instanceSourcePath = candidate.instanceSourcePath;
			appendHit( candidateView );
		}
		return results;
	}

	SurfaceCandidateView candidate;
	const auto candidateStart = Clock::now();
	if( directSurfaceCandidate( scene, sample, directCandidateCache, missingDirectPaths, candidate ) )
	{
		if( timings )
		{
			timings->candidateMs += elapsedMilliseconds( candidateStart );
		}
		appendHit( candidate );
	}
	else if( timings )
	{
		timings->candidateMs += elapsedMilliseconds( candidateStart );
	}

	return results;
}

bool compiledDirectBrushSampleSurfacePoint(
	const ScenePlug *scene,
	const CompiledBrushSample &sample,
	DirectCandidateCache &directCandidateCache,
	std::unordered_set<std::string> &missingDirectPaths,
	SurfaceResolveCache *surfaceResolveCache,
	CompiledSurfacePoint &result,
	SurfaceResolveTimings *timings = nullptr
)
{
	if( !scene )
	{
		return false;
	}

	SurfaceResolveCacheKey cacheKey;
	cacheKey.sourcePath = sample.sourcePath;
	cacheKey.instanceId = sample.instanceId;
	cacheKey.instanceSourcePath = sample.instanceSourcePath;

	int warmStartTriangleIndex = -1;
	const Imath::V3f *warmStartWorldPoint = nullptr;
	if( surfaceResolveCache )
	{
		auto cacheIt = surfaceResolveCache->find( cacheKey );
		if( cacheIt != surfaceResolveCache->end() )
		{
			warmStartTriangleIndex = cacheIt->second.triangleIndex;
			if( cacheIt->second.hasWorldPoint )
			{
				warmStartWorldPoint = &cacheIt->second.worldPoint;
			}
		}
	}

	SurfaceCandidateView candidate;
	const auto candidateStart = Clock::now();
	if( !directSurfaceCandidate( scene, sample, directCandidateCache, missingDirectPaths, candidate ) )
	{
		if( timings )
		{
			timings->candidateMs += elapsedMilliseconds( candidateStart );
		}
		return false;
	}
	if( timings )
	{
		timings->candidateMs += elapsedMilliseconds( candidateStart );
	}

	ReprojectHit hit;
	const bool preferTriangle = sample.preferredTriangleIndex >= 0 && candidate.targetPath == sample.sourcePath;
	const auto hitSearchStart = Clock::now();
	if( !closestPaintHit( candidate, sample.worldPosition, sample.preferredTriangleIndex, warmStartTriangleIndex, warmStartWorldPoint, preferTriangle, hit ) )
	{
		if( timings )
		{
			timings->hitSearchMs += elapsedMilliseconds( hitSearchStart );
		}
		return false;
	}
	if( timings )
	{
		timings->hitSearchMs += elapsedMilliseconds( hitSearchStart );
	}

	if( surfaceResolveCache )
	{
		SurfaceResolveCacheValue &cached = ( *surfaceResolveCache )[cacheKey];
		cached.triangleIndex = hit.triangleIndex;
		cached.worldPoint = hit.worldPoint;
		cached.hasWorldPoint = true;
	}

	const auto hitBuildStart = Clock::now();
	result = surfacePointFromHit( hit, candidate.candidate->fullTransform );
	if( timings )
	{
		timings->hitBuildMs += elapsedMilliseconds( hitBuildStart );
	}
	return true;
}

bool compiledDirectBrushSampleSurfacePoint(
	const ScenePlug *scene,
	const BrushSampleData &sample,
	DirectCandidateCache &directCandidateCache,
	std::unordered_set<std::string> &missingDirectPaths,
	SurfaceResolveCache *surfaceResolveCache,
	CompiledSurfacePoint &result,
	SurfaceResolveTimings *timings = nullptr
)
{
	if( !scene )
	{
		return false;
	}

	SurfaceResolveCacheKey cacheKey;
	cacheKey.sourcePath = sample.sourcePath;
	cacheKey.instanceId = sample.instanceId;
	cacheKey.instanceSourcePath = sample.instanceSourcePath;

	int warmStartTriangleIndex = -1;
	const Imath::V3f *warmStartWorldPoint = nullptr;
	if( surfaceResolveCache )
	{
		auto cacheIt = surfaceResolveCache->find( cacheKey );
		if( cacheIt != surfaceResolveCache->end() )
		{
			warmStartTriangleIndex = cacheIt->second.triangleIndex;
			if( cacheIt->second.hasWorldPoint )
			{
				warmStartWorldPoint = &cacheIt->second.worldPoint;
			}
		}
	}

	SurfaceCandidateView candidate;
	const auto candidateStart = Clock::now();
	if( !directSurfaceCandidate( scene, sample, directCandidateCache, missingDirectPaths, candidate ) )
	{
		if( timings )
		{
			timings->candidateMs += elapsedMilliseconds( candidateStart );
		}
		return false;
	}
	if( timings )
	{
		timings->candidateMs += elapsedMilliseconds( candidateStart );
	}

	ReprojectHit hit;
	const bool preferTriangle = sample.triangleIndex >= 0 && candidate.targetPath == sample.sourcePath;
	const auto hitSearchStart = Clock::now();
	if( !closestPaintHit( candidate, sample.point, sample.triangleIndex, warmStartTriangleIndex, warmStartWorldPoint, preferTriangle, hit ) )
	{
		if( timings )
		{
			timings->hitSearchMs += elapsedMilliseconds( hitSearchStart );
		}
		return false;
	}
	if( timings )
	{
		timings->hitSearchMs += elapsedMilliseconds( hitSearchStart );
	}

	if( surfaceResolveCache )
	{
		SurfaceResolveCacheValue &cached = ( *surfaceResolveCache )[cacheKey];
		cached.triangleIndex = hit.triangleIndex;
		cached.worldPoint = hit.worldPoint;
		cached.hasWorldPoint = true;
	}

	const auto hitBuildStart = Clock::now();
	result = surfacePointFromHit( hit, candidate.candidate->fullTransform );
	if( timings )
	{
		timings->hitBuildMs += elapsedMilliseconds( hitBuildStart );
	}
	return true;
}

} // namespace

bp::object PaintedPoints::brushPaintCommit( const bp::object &strokeIdentifier, const bp::object &samples, bool append )
{
	const auto loadSchemaStart = Clock::now();
	std::string loadError;
	bp::dict loadTimings;
	CacheSchema schema = loadCacheSchemaForNode( this, &loadError, &loadTimings );
	if( !loadError.empty() )
	{
		throw std::runtime_error( loadError );
	}
	std::string pendingLoadError;
	const CacheSchema pendingSchema = loadPendingPaintSchemaForNode( this, &pendingLoadError );
	if( !pendingLoadError.empty() )
	{
		throw std::runtime_error( pendingLoadError );
	}
	mergePendingPaintSchemaInto( schema, pendingSchema );
	applyInteractiveOverlayToSchema( this, schema );
	const double loadSchemaMs = elapsedMilliseconds( loadSchemaStart );

	StrokeRecord *stroke = resolveStroke( schema, strokeIdentifier );
	LayerRecord *layer = findLayer( schema, stroke->layerId );
	if( !layer )
	{
		throw std::runtime_error( "Unknown layer" );
	}

	double schemaDictMs = 0.0;
	double nodeMetadataMs = 0.0;
	const auto pythonSetupStart = Clock::now();
	// Paint-time surface reprojection only needs incoming samples and scene plugs.
	// Avoid materializing the full authored cache store for every stroke.
	bp::dict store;
	const double pythonSetupMs = elapsedMilliseconds( pythonSetupStart );
	const double storeBuildMs = schemaDictMs + nodeMetadataMs + pythonSetupMs;
	bp::object pythonNodeObject( bp::ptr( this ) );
	const ScenePlug *scene = inPlug();
	const bool filteredMode = paintThroughModePlug()->getValue() == 1;
	std::vector<ReprojectSurfaceCandidate> filteredCandidates;
	DirectCandidateCache directCandidateCache;
	std::unordered_set<std::string> missingDirectPaths;
	SurfaceResolveCache surfaceResolveCache;
	double surfaceCandidatesMs = 0.0;
	int surfaceCandidateCount = 0;
	if( filteredMode )
	{
		const auto surfaceCandidatesStart = Clock::now();
		filteredCandidates = compiledFilteredSurfaceCandidates( this );
		surfaceCandidatesMs = elapsedMilliseconds( surfaceCandidatesStart );
		surfaceCandidateCount = static_cast<int>( filteredCandidates.size() );
	}

	std::vector<PointRecord> authoredPoints;
	authoredPoints.reserve( bp::len( samples ) );
	double sampleDictExtractMs = 0.0;
	double sampleCompileMs = 0.0;
	int resolvedCount = 0;
	int fallbackCount = 0;
	int inputSampleCount = 0;
	int resolvedSampleCount = 0;
	int fallbackSampleCount = 0;
	int surfacePointCountTotal = 0;
	int surfacePointCountMax = 0;
	int surfaceSamplesZeroHits = 0;
	int surfaceSamplesOneHit = 0;
	int surfaceSamplesManyHits = 0;
	double sampleExtractMs = 0.0;
	double sampleBuildBaseDecodeMs = 0.0;
	double sampleBuildJitterMs = 0.0;
	double sampleBuildConstructMs = 0.0;
	double surfaceResolveMs = 0.0;
	double surfaceResolveListMs = 0.0;
	double surfaceResolveCountMs = 0.0;
	double surfaceResolveCandidateMs = 0.0;
	double surfaceResolveHitSearchMs = 0.0;
	double surfaceResolveHitBuildMs = 0.0;
	double fallbackAuthorMs = 0.0;
	double fallbackPythonAuthorMs = 0.0;
	double fallbackRecordConvertMs = 0.0;
	double recordBuildMs = 0.0;
	double recordBuildPythonAuthorMs = 0.0;
	double recordBuildConvertMs = 0.0;
	double resultPackMs = 0.0;
	std::map<std::string, int> fallbackCountsByOrigin;
	std::vector<std::string> fallbackExamples;
	ResolvedBuildContext resolvedBuildContext;
	resolvedBuildContext.inheritedColor = inheritedColor( schema, *layer, *stroke );
	resolvedBuildContext.scenePathIds.reserve( 8 );
	resolvedBuildContext.instanceSourcePathIds.reserve( 8 );
	SampleCompileCache sampleCompileCache;
	std::vector<CompiledInputSample> compiledSamples;
	compiledSamples.reserve( bp::len( samples ) );
	for( bp::stl_input_iterator<bp::object> it( samples ), end; it != end; ++it )
	{
		PyObject *sampleObject = ( *it ).ptr();
		const auto sampleCompileStart = Clock::now();
		CompiledInputSample compiledInput;
		compiledInput.compiled = compileBrushSample( sampleObject, sampleCompileCache );
		compiledInput.original = compiledInput.compiled.valid ? nullptr : sampleObject;
		Py_XINCREF( compiledInput.original );
		sampleCompileMs += elapsedMilliseconds( sampleCompileStart );
		compiledSamples.push_back( std::move( compiledInput ) );
	}
	sampleExtractMs = sampleDictExtractMs + sampleCompileMs;
	for( const CompiledInputSample &compiledInput : compiledSamples )
	{
		PyObject *sampleObject = compiledInput.original;
		const CompiledBrushSample &compiledSample = compiledInput.compiled;
		++inputSampleCount;
		resolvedBuildContext.width = compiledSample.width;
		resolvedBuildContext.uniformScale = compiledSample.uniformScale;
		resolvedBuildContext.tangentRotation = compiledSample.tangentRotation;
		resolvedBuildContext.normalSpin = compiledSample.normalSpin;
		resolvedBuildContext.pressureDensity = compiledSample.pressureDensity;
		resolvedBuildContext.pressureSoftness = compiledSample.pressureSoftness;
		resolvedBuildContext.baseSeed = compiledSample.baseSeed;
		resolvedBuildContext.valid = compiledSample.valid;

		SurfaceResolveTimings surfaceResolveTimings;
		std::vector<CompiledSurfacePoint> surfacePoints;
		CompiledSurfacePoint directSurfacePoint;
		bool hasDirectSurfacePoint = false;
		int surfacePointCount = 0;
		const auto surfaceResolveStart = Clock::now();
		if( filteredMode )
		{
			surfacePoints = compiledBrushSampleSurfacePoints(
				scene,
				compiledSample,
				&filteredCandidates,
				directCandidateCache,
				missingDirectPaths,
				nullptr,
				&surfaceResolveTimings
			);
			surfacePointCount = static_cast<int>( surfacePoints.size() );
		}
		else
		{
			hasDirectSurfacePoint = compiledDirectBrushSampleSurfacePoint(
				scene,
				compiledSample,
				directCandidateCache,
				missingDirectPaths,
				&surfaceResolveCache,
				directSurfacePoint,
				&surfaceResolveTimings
			);
			surfacePointCount = hasDirectSurfacePoint ? 1 : 0;
		}
		surfaceResolveListMs += elapsedMilliseconds( surfaceResolveStart );
		surfaceResolveCandidateMs += surfaceResolveTimings.candidateMs;
		surfaceResolveHitSearchMs += surfaceResolveTimings.hitSearchMs;
		surfaceResolveHitBuildMs += surfaceResolveTimings.hitBuildMs;
		surfaceResolveMs = surfaceResolveListMs + surfaceResolveCountMs;
		surfacePointCountTotal += surfacePointCount;
		surfacePointCountMax = std::max( surfacePointCountMax, surfacePointCount );
		if( surfacePointCount == 0 )
		{
			++surfaceSamplesZeroHits;
		}
		else if( surfacePointCount == 1 )
		{
			++surfaceSamplesOneHit;
		}
		else
		{
			++surfaceSamplesManyHits;
		}

		if( surfacePointCount == 0 )
		{
			if( !sampleObject )
			{
				continue;
			}
			bp::dict sample( bp::handle<>( bp::borrowed( sampleObject ) ) );
			const auto fallbackAuthorStart = Clock::now();
			const auto fallbackPythonAuthorStart = Clock::now();
			const bp::dict authoredSample = bp::extract<bp::dict>( pythonNodeObject.attr( "_PaintedPoints__authoredBrushPoint" )( sample ) );
			fallbackPythonAuthorMs += elapsedMilliseconds( fallbackPythonAuthorStart );
			const auto fallbackRecordConvertStart = Clock::now();
			authoredPoints.push_back( pointRecordFromAuthored( schema, *layer, *stroke, authoredSample ) );
			fallbackRecordConvertMs += elapsedMilliseconds( fallbackRecordConvertStart );
			fallbackAuthorMs += elapsedMilliseconds( fallbackAuthorStart );
			fallbackCount += 1;
			fallbackSampleCount += 1;
			fallbackCountsByOrigin[sampleOriginLabel( sample )] += 1;
			if( fallbackExamples.size() < 4 )
			{
				fallbackExamples.push_back( sampleSummary( sample ) );
			}
			continue;
		}

		++resolvedSampleCount;
		int surfaceIndex = 0;
		if( hasDirectSurfacePoint )
		{
			const auto recordBuildStart = Clock::now();
			const auto recordBuildConvertStart = Clock::now();
			authoredPoints.push_back( pointRecordFromResolvedSample( schema, *layer, *stroke, resolvedBuildContext, directSurfacePoint, 0 ) );
			recordBuildConvertMs += elapsedMilliseconds( recordBuildConvertStart );
			recordBuildMs += elapsedMilliseconds( recordBuildStart );
			resolvedCount += 1;
		}
		else
		{
			for( const CompiledSurfacePoint &surfacePoint : surfacePoints )
			{
				const auto recordBuildStart = Clock::now();
				const auto recordBuildConvertStart = Clock::now();
				authoredPoints.push_back( pointRecordFromResolvedSample( schema, *layer, *stroke, resolvedBuildContext, surfacePoint, surfaceIndex ) );
				recordBuildConvertMs += elapsedMilliseconds( recordBuildConvertStart );
				recordBuildMs += elapsedMilliseconds( recordBuildStart );
				resolvedCount += 1;
				++surfaceIndex;
			}
		}
	}
	const auto resultPackStart = Clock::now();
	const std::string fallbackDiagnostics = fallbackDiagnosticsSummary( fallbackCountsByOrigin, fallbackExamples );
	if( diagnosticsEnabled() && !fallbackDiagnostics.empty() )
	{
		IECore::msg( IECore::Msg::Level::Info, "PaintedPoints.brushPaintCommit", fallbackDiagnostics );
	}
	bp::dict result = brushPaintResult( authoredPoints.size(), resolvedCount, fallbackCount, 0.0 );
	resultPackMs += elapsedMilliseconds( resultPackStart );
	const double authoredBuildMs = sampleExtractMs + surfaceResolveMs + fallbackAuthorMs + recordBuildMs + resultPackMs;

	if( !append )
	{
		schema.points.erase(
			std::remove_if(
				schema.points.begin(), schema.points.end(),
				[stroke]( const PointRecord &point ) {
					return point.strokeId == stroke->strokeId;
				}
			),
			schema.points.end()
		);
	}
	for( const PointRecord &point : authoredPoints )
	{
		schema.points.push_back( point );
	}

	const auto rebuildStart = Clock::now();
	rebuildChunksAndCounts( schema, { stroke->strokeId } );
	const double rebuildMs = elapsedMilliseconds( rebuildStart );
	const auto diagnosticsStart = Clock::now();
	applyTrustedDiagnostics( this, schema );
	const double diagnosticsMs = elapsedMilliseconds( diagnosticsStart );
	const auto writeStart = Clock::now();
	bp::dict writeTimings;
	writeCacheSchemaDirect( this, schema, &writeTimings );
	clearInteractiveOverlay( this );
	const double writeMs = elapsedMilliseconds( writeStart );
	const auto syncStart = Clock::now();
	syncStateFromSchemaTrusted( this, schema );
	const double syncMs = elapsedMilliseconds( syncStart );
	result["authoredBuildMs"] = authoredBuildMs;
	result["loadSchemaMs"] = loadSchemaMs;
	result["loadResolvePathMs"] = loadTimings.get( "loadResolvePathMs", bp::object( 0.0 ) );
	result["loadReadBytesMs"] = loadTimings.get( "loadReadBytesMs", bp::object( 0.0 ) );
	result["loadBlobExtractMs"] = loadTimings.get( "loadBlobExtractMs", bp::object( 0.0 ) );
	result["loadUnpackMs"] = loadTimings.get( "loadUnpackMs", bp::object( 0.0 ) );
	result["loadPopulateMetadataMs"] = loadTimings.get( "loadPopulateMetadataMs", bp::object( 0.0 ) );
	result["unpackChecksumMs"] = loadTimings.get( "unpackChecksumMs", bp::object( 0.0 ) );
	result["unpackHeaderMs"] = loadTimings.get( "unpackHeaderMs", bp::object( 0.0 ) );
	result["unpackNodeMs"] = loadTimings.get( "unpackNodeMs", bp::object( 0.0 ) );
	result["unpackLockMs"] = loadTimings.get( "unpackLockMs", bp::object( 0.0 ) );
	result["unpackScenePathsMs"] = loadTimings.get( "unpackScenePathsMs", bp::object( 0.0 ) );
	result["unpackLayersMs"] = loadTimings.get( "unpackLayersMs", bp::object( 0.0 ) );
	result["unpackStrokesMs"] = loadTimings.get( "unpackStrokesMs", bp::object( 0.0 ) );
	result["unpackChunksMs"] = loadTimings.get( "unpackChunksMs", bp::object( 0.0 ) );
	result["unpackPointsMs"] = loadTimings.get( "unpackPointsMs", bp::object( 0.0 ) );
	result["unpackSelectionSetsMs"] = loadTimings.get( "unpackSelectionSetsMs", bp::object( 0.0 ) );
	result["unpackDiagnosticsMs"] = loadTimings.get( "unpackDiagnosticsMs", bp::object( 0.0 ) );
	result["unpackUpgradesMs"] = loadTimings.get( "unpackUpgradesMs", bp::object( 0.0 ) );
	result["unpackPointBackupsMs"] = loadTimings.get( "unpackPointBackupsMs", bp::object( 0.0 ) );
	result["storeBuildMs"] = storeBuildMs;
	result["schemaDictMs"] = schemaDictMs;
	result["nodeMetadataMs"] = nodeMetadataMs;
	result["pythonSetupMs"] = pythonSetupMs;
	result["surfaceCandidatesMs"] = surfaceCandidatesMs;
	result["surfaceCandidateCount"] = surfaceCandidateCount;
	result["filteredMode"] = filteredMode;
	result["rebuildMs"] = rebuildMs;
	result["diagnosticsMs"] = diagnosticsMs;
	result["writeMs"] = writeMs;
	result["writePopulateMetadataMs"] = writeTimings.get( "writePopulateMetadataMs", bp::object( 0.0 ) );
	result["writeLockMs"] = writeTimings.get( "writeLockMs", bp::object( 0.0 ) );
	result["writePackMs"] = writeTimings.get( "writePackMs", bp::object( 0.0 ) );
	result["packHeaderMs"] = writeTimings.get( "packHeaderMs", bp::object( 0.0 ) );
	result["packNodeMs"] = writeTimings.get( "packNodeMs", bp::object( 0.0 ) );
	result["packLockMs"] = writeTimings.get( "packLockMs", bp::object( 0.0 ) );
	result["packScenePathsMs"] = writeTimings.get( "packScenePathsMs", bp::object( 0.0 ) );
	result["packLayersMs"] = writeTimings.get( "packLayersMs", bp::object( 0.0 ) );
	result["packStrokesMs"] = writeTimings.get( "packStrokesMs", bp::object( 0.0 ) );
	result["packChunksMs"] = writeTimings.get( "packChunksMs", bp::object( 0.0 ) );
	result["packPointsMs"] = writeTimings.get( "packPointsMs", bp::object( 0.0 ) );
	result["packPointsReserveMs"] = writeTimings.get( "packPointsReserveMs", bp::object( 0.0 ) );
	result["packPointsReserveReallocated"] = writeTimings.get( "packPointsReserveReallocated", bp::object( 0.0 ) );
	result["packPointsCapacityBeforeBytes"] = writeTimings.get( "packPointsCapacityBeforeBytes", bp::object( 0.0 ) );
	result["packPointsCapacityAfterBytes"] = writeTimings.get( "packPointsCapacityAfterBytes", bp::object( 0.0 ) );
	result["packPointsResizeMs"] = writeTimings.get( "packPointsResizeMs", bp::object( 0.0 ) );
	result["packPointsFillMs"] = writeTimings.get( "packPointsFillMs", bp::object( 0.0 ) );
	result["packPointsCopyMs"] = writeTimings.get( "packPointsCopyMs", bp::object( 0.0 ) );
	result["packPointsChecksumInlineMs"] = writeTimings.get( "packPointsChecksumInlineMs", bp::object( 0.0 ) );
	result["packPointsChecksumMs"] = writeTimings.get( "packPointsChecksumMs", bp::object( 0.0 ) );
	result["packSelectionSetsMs"] = writeTimings.get( "packSelectionSetsMs", bp::object( 0.0 ) );
	result["packDiagnosticsMs"] = writeTimings.get( "packDiagnosticsMs", bp::object( 0.0 ) );
	result["packUpgradesMs"] = writeTimings.get( "packUpgradesMs", bp::object( 0.0 ) );
	result["packPointBackupsMs"] = writeTimings.get( "packPointBackupsMs", bp::object( 0.0 ) );
	result["packFinalChecksumMs"] = writeTimings.get( "packFinalChecksumMs", bp::object( 0.0 ) );
	result["packFinalHeaderWriteMs"] = writeTimings.get( "packFinalHeaderWriteMs", bp::object( 0.0 ) );
	result["packFinalBufferMs"] = writeTimings.get( "packFinalBufferMs", bp::object( 0.0 ) );
	result["writeResolvePathMs"] = writeTimings.get( "writeResolvePathMs", bp::object( 0.0 ) );
	result["writeBackupMs"] = writeTimings.get( "writeBackupMs", bp::object( 0.0 ) );
	result["writeBytesMs"] = writeTimings.get( "writeBytesMs", bp::object( 0.0 ) );
	result["writeClearBlobMs"] = writeTimings.get( "writeClearBlobMs", bp::object( 0.0 ) );
	result["writeReleaseLockMs"] = writeTimings.get( "writeReleaseLockMs", bp::object( 0.0 ) );
	result["writeBlobObjectMs"] = writeTimings.get( "writeBlobObjectMs", bp::object( 0.0 ) );
	result["writeBlobObjectResizeMs"] = writeTimings.get( "writeBlobObjectResizeMs", bp::object( 0.0 ) );
	result["writeBlobObjectCopyMs"] = writeTimings.get( "writeBlobObjectCopyMs", bp::object( 0.0 ) );
	result["writeBlobPlugSetMs"] = writeTimings.get( "writeBlobPlugSetMs", bp::object( 0.0 ) );
	result["writeSetBlobMs"] = writeTimings.get( "writeSetBlobMs", bp::object( 0.0 ) );
	result["syncMs"] = syncMs;
	result["sampleBuildBaseDecodeMs"] = sampleBuildBaseDecodeMs;
	result["sampleBuildJitterMs"] = sampleBuildJitterMs;
	result["sampleBuildConstructMs"] = sampleBuildConstructMs;
	result["sampleDictExtractMs"] = sampleDictExtractMs;
	result["sampleCompileMs"] = sampleCompileMs;
	result["sampleExtractMs"] = sampleExtractMs;
	result["surfaceResolveMs"] = surfaceResolveMs;
	result["surfaceResolveListMs"] = surfaceResolveListMs;
	result["surfaceResolveCountMs"] = surfaceResolveCountMs;
	result["surfaceResolveCandidateMs"] = surfaceResolveCandidateMs;
	result["surfaceResolveHitSearchMs"] = surfaceResolveHitSearchMs;
	result["surfaceResolveHitBuildMs"] = surfaceResolveHitBuildMs;
	result["simdBackend"] = brushResolveBackendName();
	result["fallbackAuthorMs"] = fallbackAuthorMs;
	result["fallbackPythonAuthorMs"] = fallbackPythonAuthorMs;
	result["fallbackRecordConvertMs"] = fallbackRecordConvertMs;
	result["recordBuildMs"] = recordBuildMs;
	result["recordBuildPythonAuthorMs"] = recordBuildPythonAuthorMs;
	result["recordBuildConvertMs"] = recordBuildConvertMs;
	result["resultPackMs"] = resultPackMs;
	result["inputSampleCount"] = inputSampleCount;
	result["resolvedSampleCount"] = resolvedSampleCount;
	result["fallbackSampleCount"] = fallbackSampleCount;
	result["surfacePointCountTotal"] = surfacePointCountTotal;
	result["surfacePointCountMax"] = surfacePointCountMax;
	result["surfaceSamplesZeroHits"] = surfaceSamplesZeroHits;
	result["surfaceSamplesOneHit"] = surfaceSamplesOneHit;
	result["surfaceSamplesManyHits"] = surfaceSamplesManyHits;
	if( !fallbackDiagnostics.empty() )
	{
		result["fallbackDiagnostics"] = fallbackDiagnostics;
	}
	return result;
}

bp::object PaintedPoints::brushPaintCommitNative( const bp::object &strokeIdentifier, const bp::object &samples, bool append )
{
	SampleCompileCache sampleCompileCache;
	BrushSampleDataList typedSamples;
	typedSamples.reserve( bp::len( samples ) );
	for( bp::stl_input_iterator<bp::object> it( samples ), end; it != end; ++it )
	{
		PyObject *sampleObject = ( *it ).ptr();
		const CompiledBrushSample compiledSample = compileBrushSample( sampleObject, sampleCompileCache );
		BrushSampleData typedSample;
		typedSample.point = compiledSample.worldPosition;
		typedSample.width = compiledSample.width;
		typedSample.scale = compiledSample.uniformScale;
		typedSample.tangentRotation = compiledSample.tangentRotation;
		typedSample.normalSpin = compiledSample.normalSpin;
		typedSample.pressureDensity = compiledSample.pressureDensity;
		typedSample.pressureSoftness = compiledSample.pressureSoftness;
		typedSample.seed = static_cast<std::uint64_t>( std::max( compiledSample.baseSeed, 0 ) );
		typedSample.sourcePath = compiledSample.sourcePath;
		typedSample.instanceId = compiledSample.instanceId;
		typedSample.instanceSourcePath = compiledSample.instanceSourcePath;
		typedSample.triangleIndex = compiledSample.preferredTriangleIndex;
		typedSample.valid = compiledSample.valid;
		typedSample.attachmentResolved = pyBoolFast( dictItemBorrowed( sampleObject, "attachmentResolved" ), false );
		typedSample.sampleExpanded = pyBoolFast( dictItemBorrowed( sampleObject, "sampleExpanded" ), false );
		typedSample.sampleOrigin = pyStringFast( dictItemBorrowed( sampleObject, "sampleOrigin" ), std::string() );
		typedSample.sampleSourceIndex = pyIntFast( dictItemBorrowed( sampleObject, "sampleSourceIndex" ), 0 );
		typedSample.sampleExpansionIndex = pyIntFast( dictItemBorrowed( sampleObject, "sampleExpansionIndex" ), 0 );
		typedSample.normal = pyV3fFast( dictItemBorrowed( sampleObject, "N" ), typedSample.normal );
		typedSample.barycentric = pyV3fFast( dictItemBorrowed( sampleObject, "barycentric" ), typedSample.barycentric );
		typedSamples.push_back( std::move( typedSample ) );
	}
	return brushPaintCommitTyped( strokeIdentifier, typedSamples, append );
}

bp::object PaintedPoints::brushPaintCommitTyped( const bp::object &strokeIdentifier, const BrushSampleDataList &samples, bool append )
{
	const auto loadSchemaStart = Clock::now();
	std::string loadError;
	bp::dict loadTimings;
	CacheSchema schema = loadCacheSchemaForNode( this, &loadError, &loadTimings );
	if( !loadError.empty() )
	{
		throw std::runtime_error( loadError );
	}
	std::string pendingLoadError;
	const CacheSchema pendingSchema = loadPendingPaintSchemaForNode( this, &pendingLoadError );
	if( !pendingLoadError.empty() )
	{
		throw std::runtime_error( pendingLoadError );
	}
	mergePendingPaintSchemaInto( schema, pendingSchema );
	applyInteractiveOverlayToSchema( this, schema );
	const double loadSchemaMs = elapsedMilliseconds( loadSchemaStart );

	StrokeRecord *stroke = resolveStroke( schema, strokeIdentifier );
	LayerRecord *layer = findLayer( schema, stroke->layerId );
	if( !layer )
	{
		throw std::runtime_error( "Unknown layer" );
	}

	double schemaDictMs = 0.0;
	double nodeMetadataMs = 0.0;
	const auto pythonSetupStart = Clock::now();
	bp::dict store;
	bp::object pythonNodeObject( bp::ptr( this ) );
	const double pythonSetupMs = elapsedMilliseconds( pythonSetupStart );
	const double storeBuildMs = schemaDictMs + nodeMetadataMs + pythonSetupMs;
	const ScenePlug *scene = inPlug();
	const bool filteredMode = paintThroughModePlug()->getValue() == 1;
	std::vector<ReprojectSurfaceCandidate> filteredCandidates;
	DirectCandidateCache directCandidateCache;
	std::unordered_set<std::string> missingDirectPaths;
	SurfaceResolveCache surfaceResolveCache;
	double surfaceCandidatesMs = 0.0;
	int surfaceCandidateCount = 0;
	if( filteredMode )
	{
		const auto surfaceCandidatesStart = Clock::now();
		filteredCandidates = compiledFilteredSurfaceCandidates( this );
		surfaceCandidatesMs = elapsedMilliseconds( surfaceCandidatesStart );
		surfaceCandidateCount = static_cast<int>( filteredCandidates.size() );
	}

	std::vector<PointRecord> authoredPoints;
	authoredPoints.reserve( samples.size() );
	double sampleDictExtractMs = 0.0;
	double sampleCompileMs = 0.0;
	int resolvedCount = 0;
	int fallbackCount = 0;
	int inputSampleCount = 0;
	int resolvedSampleCount = 0;
	int fallbackSampleCount = 0;
	int surfacePointCountTotal = 0;
	int surfacePointCountMax = 0;
	int surfaceSamplesZeroHits = 0;
	int surfaceSamplesOneHit = 0;
	int surfaceSamplesManyHits = 0;
	double sampleExtractMs = 0.0;
	double sampleBuildBaseDecodeMs = 0.0;
	double sampleBuildJitterMs = 0.0;
	double sampleBuildConstructMs = 0.0;
	double surfaceResolveMs = 0.0;
	double surfaceResolveListMs = 0.0;
	double surfaceResolveCountMs = 0.0;
	double surfaceResolveCandidateMs = 0.0;
	double surfaceResolveHitSearchMs = 0.0;
	double surfaceResolveHitBuildMs = 0.0;
	double fallbackAuthorMs = 0.0;
	double fallbackPythonAuthorMs = 0.0;
	double fallbackRecordConvertMs = 0.0;
	double recordBuildMs = 0.0;
	double recordBuildPythonAuthorMs = 0.0;
	double recordBuildConvertMs = 0.0;
	double resultPackMs = 0.0;
	std::map<std::string, int> fallbackCountsByOrigin;
	std::vector<std::string> fallbackExamples;
	ResolvedBuildContext resolvedBuildContext;
	resolvedBuildContext.inheritedColor = inheritedColor( schema, *layer, *stroke );
	resolvedBuildContext.scenePathIds.reserve( 8 );
	resolvedBuildContext.instanceSourcePathIds.reserve( 8 );
	sampleExtractMs = sampleDictExtractMs + sampleCompileMs;
	for( std::size_t i = 0; i < samples.size(); ++i )
	{
		const BrushSampleData &sample = samples[i];
		++inputSampleCount;
		resolvedBuildContext.width = sample.width;
		resolvedBuildContext.uniformScale = sample.scale;
		resolvedBuildContext.tangentRotation = sample.tangentRotation;
		resolvedBuildContext.normalSpin = sample.normalSpin;
		resolvedBuildContext.pressureDensity = sample.pressureDensity;
		resolvedBuildContext.pressureSoftness = sample.pressureSoftness;
		resolvedBuildContext.baseSeed = static_cast<int>( std::min<std::uint64_t>( sample.seed, static_cast<std::uint64_t>( std::numeric_limits<int>::max() ) ) );
		resolvedBuildContext.valid = sample.valid;

		SurfaceResolveTimings surfaceResolveTimings;
		std::vector<CompiledSurfacePoint> surfacePoints;
		CompiledSurfacePoint directSurfacePoint;
		bool hasDirectSurfacePoint = false;
		int surfacePointCount = 0;
		const auto surfaceResolveStart = Clock::now();
		if( filteredMode )
		{
			CompiledBrushSample compiledSample;
			compiledSample.worldPosition = sample.point;
			compiledSample.sourcePath = sample.sourcePath;
			compiledSample.instanceId = sample.instanceId;
			compiledSample.instanceSourcePath = sample.instanceSourcePath;
			compiledSample.preferredTriangleIndex = sample.triangleIndex;
			compiledSample.width = sample.width;
			compiledSample.uniformScale = sample.scale;
			compiledSample.tangentRotation = sample.tangentRotation;
			compiledSample.normalSpin = sample.normalSpin;
			surfacePoints = compiledBrushSampleSurfacePoints(
				scene,
				compiledSample,
				&filteredCandidates,
				directCandidateCache,
				missingDirectPaths,
				nullptr,
				&surfaceResolveTimings
			);
			surfacePointCount = static_cast<int>( surfacePoints.size() );
		}
		else
		{
			hasDirectSurfacePoint = compiledDirectBrushSampleSurfacePoint(
				scene,
				sample,
				directCandidateCache,
				missingDirectPaths,
				&surfaceResolveCache,
				directSurfacePoint,
				&surfaceResolveTimings
			);
			surfacePointCount = hasDirectSurfacePoint ? 1 : 0;
		}
		surfaceResolveListMs += elapsedMilliseconds( surfaceResolveStart );
		surfaceResolveCandidateMs += surfaceResolveTimings.candidateMs;
		surfaceResolveHitSearchMs += surfaceResolveTimings.hitSearchMs;
		surfaceResolveHitBuildMs += surfaceResolveTimings.hitBuildMs;
		surfaceResolveMs = surfaceResolveListMs + surfaceResolveCountMs;
		surfacePointCountTotal += surfacePointCount;
		surfacePointCountMax = std::max( surfacePointCountMax, surfacePointCount );
		if( surfacePointCount == 0 )
		{
			++surfaceSamplesZeroHits;
		}
		else if( surfacePointCount == 1 )
		{
			++surfaceSamplesOneHit;
		}
		else
		{
			++surfaceSamplesManyHits;
		}

		if( surfacePointCount == 0 )
		{
			if( sample.valid )
			{
				continue;
			}
			const auto fallbackAuthorStart = Clock::now();
			const auto fallbackPythonAuthorStart = Clock::now();
			const bp::dict authoredSample = bp::extract<bp::dict>( pythonNodeObject.attr( "_PaintedPoints__authoredBrushPoint" )( pythonBrushSampleFromData( sample ) ) );
			fallbackPythonAuthorMs += elapsedMilliseconds( fallbackPythonAuthorStart );
			const auto fallbackRecordConvertStart = Clock::now();
			authoredPoints.push_back( pointRecordFromAuthored( schema, *layer, *stroke, authoredSample ) );
			fallbackRecordConvertMs += elapsedMilliseconds( fallbackRecordConvertStart );
			fallbackAuthorMs += elapsedMilliseconds( fallbackAuthorStart );
			fallbackCount += 1;
			fallbackSampleCount += 1;
			fallbackCountsByOrigin[sampleOriginLabel( sample )] += 1;
			if( fallbackExamples.size() < 4 )
			{
				fallbackExamples.push_back( sampleSummary( sample ) );
			}
			continue;
		}

		++resolvedSampleCount;
		int surfaceIndex = 0;
		if( hasDirectSurfacePoint )
		{
			const auto recordBuildStart = Clock::now();
			const auto recordBuildConvertStart = Clock::now();
			authoredPoints.push_back( pointRecordFromResolvedSample( schema, *layer, *stroke, resolvedBuildContext, directSurfacePoint, 0 ) );
			recordBuildConvertMs += elapsedMilliseconds( recordBuildConvertStart );
			recordBuildMs += elapsedMilliseconds( recordBuildStart );
			resolvedCount += 1;
		}
		else
		{
			for( const CompiledSurfacePoint &surfacePoint : surfacePoints )
			{
				const auto recordBuildStart = Clock::now();
				const auto recordBuildConvertStart = Clock::now();
				authoredPoints.push_back( pointRecordFromResolvedSample( schema, *layer, *stroke, resolvedBuildContext, surfacePoint, surfaceIndex ) );
				recordBuildConvertMs += elapsedMilliseconds( recordBuildConvertStart );
				recordBuildMs += elapsedMilliseconds( recordBuildStart );
				resolvedCount += 1;
				++surfaceIndex;
			}
		}
	}
	const auto resultPackStart = Clock::now();
	const std::string fallbackDiagnostics = fallbackDiagnosticsSummary( fallbackCountsByOrigin, fallbackExamples );
	if( diagnosticsEnabled() && !fallbackDiagnostics.empty() )
	{
		IECore::msg( IECore::Msg::Level::Info, "PaintedPoints.brushPaintCommitTyped", fallbackDiagnostics );
	}
	bp::dict result = brushPaintResult( authoredPoints.size(), resolvedCount, fallbackCount, 0.0 );
	resultPackMs += elapsedMilliseconds( resultPackStart );
	const double authoredBuildMs = sampleExtractMs + surfaceResolveMs + fallbackAuthorMs + recordBuildMs + resultPackMs;

	if( !append )
	{
		schema.points.erase(
			std::remove_if(
				schema.points.begin(), schema.points.end(),
				[stroke]( const PointRecord &point ) {
					return point.strokeId == stroke->strokeId;
				}
			),
			schema.points.end()
		);
	}
	for( const PointRecord &point : authoredPoints )
	{
		schema.points.push_back( point );
	}

	const auto rebuildStart = Clock::now();
	rebuildChunksAndCounts( schema, { stroke->strokeId } );
	const double rebuildMs = elapsedMilliseconds( rebuildStart );
	const auto diagnosticsStart = Clock::now();
	applyTrustedDiagnostics( this, schema );
	const double diagnosticsMs = elapsedMilliseconds( diagnosticsStart );
	const auto writeStart = Clock::now();
	bp::dict writeTimings;
	if( append )
	{
		appendInteractivePoints( this, schema, authoredPoints );
	}
	else
	{
		writeCacheSchemaDirect( this, schema, &writeTimings );
		clearInteractiveOverlay( this );
	}
	const double writeMs = elapsedMilliseconds( writeStart );
	const auto syncStart = Clock::now();
	syncStateFromSchemaTrusted( this, schema );
	const double syncMs = elapsedMilliseconds( syncStart );
	result["authoredBuildMs"] = authoredBuildMs;
	result["loadSchemaMs"] = loadSchemaMs;
	result["loadResolvePathMs"] = loadTimings.get( "loadResolvePathMs", bp::object( 0.0 ) );
	result["loadReadBytesMs"] = loadTimings.get( "loadReadBytesMs", bp::object( 0.0 ) );
	result["loadBlobExtractMs"] = loadTimings.get( "loadBlobExtractMs", bp::object( 0.0 ) );
	result["loadUnpackMs"] = loadTimings.get( "loadUnpackMs", bp::object( 0.0 ) );
	result["loadPopulateMetadataMs"] = loadTimings.get( "loadPopulateMetadataMs", bp::object( 0.0 ) );
	result["unpackChecksumMs"] = loadTimings.get( "unpackChecksumMs", bp::object( 0.0 ) );
	result["unpackHeaderMs"] = loadTimings.get( "unpackHeaderMs", bp::object( 0.0 ) );
	result["unpackNodeMs"] = loadTimings.get( "unpackNodeMs", bp::object( 0.0 ) );
	result["unpackLockMs"] = loadTimings.get( "unpackLockMs", bp::object( 0.0 ) );
	result["unpackScenePathsMs"] = loadTimings.get( "unpackScenePathsMs", bp::object( 0.0 ) );
	result["unpackLayersMs"] = loadTimings.get( "unpackLayersMs", bp::object( 0.0 ) );
	result["unpackStrokesMs"] = loadTimings.get( "unpackStrokesMs", bp::object( 0.0 ) );
	result["unpackChunksMs"] = loadTimings.get( "unpackChunksMs", bp::object( 0.0 ) );
	result["unpackPointsMs"] = loadTimings.get( "unpackPointsMs", bp::object( 0.0 ) );
	result["unpackSelectionSetsMs"] = loadTimings.get( "unpackSelectionSetsMs", bp::object( 0.0 ) );
	result["unpackDiagnosticsMs"] = loadTimings.get( "unpackDiagnosticsMs", bp::object( 0.0 ) );
	result["unpackUpgradesMs"] = loadTimings.get( "unpackUpgradesMs", bp::object( 0.0 ) );
	result["unpackPointBackupsMs"] = loadTimings.get( "unpackPointBackupsMs", bp::object( 0.0 ) );
	result["storeBuildMs"] = storeBuildMs;
	result["schemaDictMs"] = schemaDictMs;
	result["nodeMetadataMs"] = nodeMetadataMs;
	result["pythonSetupMs"] = pythonSetupMs;
	result["surfaceCandidatesMs"] = surfaceCandidatesMs;
	result["surfaceCandidateCount"] = surfaceCandidateCount;
	result["filteredMode"] = filteredMode;
	result["rebuildMs"] = rebuildMs;
	result["diagnosticsMs"] = diagnosticsMs;
	result["writeMs"] = writeMs;
	result["writePopulateMetadataMs"] = writeTimings.get( "writePopulateMetadataMs", bp::object( 0.0 ) );
	result["writeLockMs"] = writeTimings.get( "writeLockMs", bp::object( 0.0 ) );
	result["writePackMs"] = writeTimings.get( "writePackMs", bp::object( 0.0 ) );
	result["packHeaderMs"] = writeTimings.get( "packHeaderMs", bp::object( 0.0 ) );
	result["packNodeMs"] = writeTimings.get( "packNodeMs", bp::object( 0.0 ) );
	result["packLockMs"] = writeTimings.get( "packLockMs", bp::object( 0.0 ) );
	result["packScenePathsMs"] = writeTimings.get( "packScenePathsMs", bp::object( 0.0 ) );
	result["packLayersMs"] = writeTimings.get( "packLayersMs", bp::object( 0.0 ) );
	result["packStrokesMs"] = writeTimings.get( "packStrokesMs", bp::object( 0.0 ) );
	result["packChunksMs"] = writeTimings.get( "packChunksMs", bp::object( 0.0 ) );
	result["packPointsMs"] = writeTimings.get( "packPointsMs", bp::object( 0.0 ) );
	result["packPointsReserveMs"] = writeTimings.get( "packPointsReserveMs", bp::object( 0.0 ) );
	result["packPointsReserveReallocated"] = writeTimings.get( "packPointsReserveReallocated", bp::object( 0.0 ) );
	result["packPointsCapacityBeforeBytes"] = writeTimings.get( "packPointsCapacityBeforeBytes", bp::object( 0.0 ) );
	result["packPointsCapacityAfterBytes"] = writeTimings.get( "packPointsCapacityAfterBytes", bp::object( 0.0 ) );
	result["packPointsResizeMs"] = writeTimings.get( "packPointsResizeMs", bp::object( 0.0 ) );
	result["packPointsFillMs"] = writeTimings.get( "packPointsFillMs", bp::object( 0.0 ) );
	result["packPointsCopyMs"] = writeTimings.get( "packPointsCopyMs", bp::object( 0.0 ) );
	result["packPointsChecksumInlineMs"] = writeTimings.get( "packPointsChecksumInlineMs", bp::object( 0.0 ) );
	result["packPointsChecksumMs"] = writeTimings.get( "packPointsChecksumMs", bp::object( 0.0 ) );
	result["packSelectionSetsMs"] = writeTimings.get( "packSelectionSetsMs", bp::object( 0.0 ) );
	result["packDiagnosticsMs"] = writeTimings.get( "packDiagnosticsMs", bp::object( 0.0 ) );
	result["packUpgradesMs"] = writeTimings.get( "packUpgradesMs", bp::object( 0.0 ) );
	result["packPointBackupsMs"] = writeTimings.get( "packPointBackupsMs", bp::object( 0.0 ) );
	result["packFinalChecksumMs"] = writeTimings.get( "packFinalChecksumMs", bp::object( 0.0 ) );
	result["packFinalHeaderWriteMs"] = writeTimings.get( "packFinalHeaderWriteMs", bp::object( 0.0 ) );
	result["packFinalBufferMs"] = writeTimings.get( "packFinalBufferMs", bp::object( 0.0 ) );
	result["writeResolvePathMs"] = writeTimings.get( "writeResolvePathMs", bp::object( 0.0 ) );
	result["writeBackupMs"] = writeTimings.get( "writeBackupMs", bp::object( 0.0 ) );
	result["writeBytesMs"] = writeTimings.get( "writeBytesMs", bp::object( 0.0 ) );
	result["writeClearBlobMs"] = writeTimings.get( "writeClearBlobMs", bp::object( 0.0 ) );
	result["writeReleaseLockMs"] = writeTimings.get( "writeReleaseLockMs", bp::object( 0.0 ) );
	result["writeBlobObjectMs"] = writeTimings.get( "writeBlobObjectMs", bp::object( 0.0 ) );
	result["writeBlobObjectResizeMs"] = writeTimings.get( "writeBlobObjectResizeMs", bp::object( 0.0 ) );
	result["writeBlobObjectCopyMs"] = writeTimings.get( "writeBlobObjectCopyMs", bp::object( 0.0 ) );
	result["writeBlobPlugSetMs"] = writeTimings.get( "writeBlobPlugSetMs", bp::object( 0.0 ) );
	result["writeSetBlobMs"] = writeTimings.get( "writeSetBlobMs", bp::object( 0.0 ) );
	result["syncMs"] = syncMs;
	result["sampleBuildBaseDecodeMs"] = sampleBuildBaseDecodeMs;
	result["sampleBuildJitterMs"] = sampleBuildJitterMs;
	result["sampleBuildConstructMs"] = sampleBuildConstructMs;
	result["sampleDictExtractMs"] = sampleDictExtractMs;
	result["sampleCompileMs"] = sampleCompileMs;
	result["sampleExtractMs"] = sampleExtractMs;
	result["surfaceResolveMs"] = surfaceResolveMs;
	result["surfaceResolveListMs"] = surfaceResolveListMs;
	result["surfaceResolveCountMs"] = surfaceResolveCountMs;
	result["surfaceResolveCandidateMs"] = surfaceResolveCandidateMs;
	result["surfaceResolveHitSearchMs"] = surfaceResolveHitSearchMs;
	result["surfaceResolveHitBuildMs"] = surfaceResolveHitBuildMs;
	result["simdBackend"] = brushResolveBackendName();
	result["fallbackAuthorMs"] = fallbackAuthorMs;
	result["fallbackPythonAuthorMs"] = fallbackPythonAuthorMs;
	result["fallbackRecordConvertMs"] = fallbackRecordConvertMs;
	result["recordBuildMs"] = recordBuildMs;
	result["recordBuildPythonAuthorMs"] = recordBuildPythonAuthorMs;
	result["recordBuildConvertMs"] = recordBuildConvertMs;
	result["resultPackMs"] = resultPackMs;
	result["inputSampleCount"] = inputSampleCount;
	result["resolvedSampleCount"] = resolvedSampleCount;
	result["fallbackSampleCount"] = fallbackSampleCount;
	result["surfacePointCountTotal"] = surfacePointCountTotal;
	result["surfacePointCountMax"] = surfacePointCountMax;
	result["surfaceSamplesZeroHits"] = surfaceSamplesZeroHits;
	result["surfaceSamplesOneHit"] = surfaceSamplesOneHit;
	result["surfaceSamplesManyHits"] = surfaceSamplesManyHits;
	if( !fallbackDiagnostics.empty() )
	{
		result["fallbackDiagnostics"] = fallbackDiagnostics;
	}
	return result;
}

bp::object PaintedPoints::brushErasePoints( const bp::object &samples, float radius, int eraseSpace )
{
	if( eraseSpace != 0 )
	{
		return bp::import( "GafferScatterPaint._core_painted_points_surface_helpers" ).attr( "brushErasePoints" )( bp::ptr( this ), samples, radius, eraseSpace );
	}

	const auto loadSchemaStart = Clock::now();
	std::string loadError;
	bp::dict loadTimings;
	CacheSchema schema = loadCacheSchemaForNode( this, &loadError, &loadTimings );
	if( !loadError.empty() )
	{
		throw std::runtime_error( loadError );
	}
	std::string pendingLoadError;
	const CacheSchema pendingSchema = loadPendingPaintSchemaForNode( this, &pendingLoadError );
	if( !pendingLoadError.empty() )
	{
		throw std::runtime_error( pendingLoadError );
	}
	mergePendingPaintSchemaInto( schema, pendingSchema );
	applyInteractiveOverlayToSchema( this, schema );
	const double loadSchemaMs = elapsedMilliseconds( loadSchemaStart );

	const float clampedRadius = std::max( 0.001f, radius );
	const float radiusSquared = clampedRadius * clampedRadius;
	const SampleGroups sampleGroups = buildSampleGroups( schema, samples, clampedRadius );
	std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> pointIdsByStroke;

	const auto pointScanStart = Clock::now();
	for( const PointRecord &point : schema.points )
	{
		auto groupIt = sampleGroups.find( { point.targetPathId, point.instanceId, point.instanceSourcePathId } );
		if( groupIt == sampleGroups.end() && point.instanceId == 0 && point.instanceSourcePathId == 0 )
		{
			groupIt = sampleGroups.find( { point.targetPathId, 0, 0 } );
		}
		if( groupIt == sampleGroups.end() )
		{
			continue;
		}

		const Imath::V3f pointPosition( point.restWorldP[0], point.restWorldP[1], point.restWorldP[2] );
		if( pointMatchesSampleGroup( groupIt->second, pointPosition, clampedRadius, radiusSquared ) )
		{
			pointIdsByStroke[point.strokeId].push_back( point.pointId );
		}
	}
	const double pointScanMs = elapsedMilliseconds( pointScanStart );

	if( pointIdsByStroke.empty() )
	{
		bp::dict result = brushEraseResult( 0, 0, eraseSpace );
		result["loadSchemaMs"] = loadSchemaMs;
		result["loadResolvePathMs"] = loadTimings.get( "loadResolvePathMs", bp::object( 0.0 ) );
		result["loadReadBytesMs"] = loadTimings.get( "loadReadBytesMs", bp::object( 0.0 ) );
		result["loadBlobExtractMs"] = loadTimings.get( "loadBlobExtractMs", bp::object( 0.0 ) );
		result["loadUnpackMs"] = loadTimings.get( "loadUnpackMs", bp::object( 0.0 ) );
		result["loadPopulateMetadataMs"] = loadTimings.get( "loadPopulateMetadataMs", bp::object( 0.0 ) );
		result["unpackChecksumMs"] = loadTimings.get( "unpackChecksumMs", bp::object( 0.0 ) );
		result["unpackHeaderMs"] = loadTimings.get( "unpackHeaderMs", bp::object( 0.0 ) );
		result["unpackNodeMs"] = loadTimings.get( "unpackNodeMs", bp::object( 0.0 ) );
		result["unpackLockMs"] = loadTimings.get( "unpackLockMs", bp::object( 0.0 ) );
		result["unpackScenePathsMs"] = loadTimings.get( "unpackScenePathsMs", bp::object( 0.0 ) );
		result["unpackLayersMs"] = loadTimings.get( "unpackLayersMs", bp::object( 0.0 ) );
		result["unpackStrokesMs"] = loadTimings.get( "unpackStrokesMs", bp::object( 0.0 ) );
		result["unpackChunksMs"] = loadTimings.get( "unpackChunksMs", bp::object( 0.0 ) );
		result["unpackPointsMs"] = loadTimings.get( "unpackPointsMs", bp::object( 0.0 ) );
		result["unpackSelectionSetsMs"] = loadTimings.get( "unpackSelectionSetsMs", bp::object( 0.0 ) );
		result["unpackDiagnosticsMs"] = loadTimings.get( "unpackDiagnosticsMs", bp::object( 0.0 ) );
		result["unpackUpgradesMs"] = loadTimings.get( "unpackUpgradesMs", bp::object( 0.0 ) );
		result["unpackPointBackupsMs"] = loadTimings.get( "unpackPointBackupsMs", bp::object( 0.0 ) );
		result["pointScanMs"] = pointScanMs;
		result["selectionCleanupMs"] = 0.0;
		result["rebuildMs"] = 0.0;
		result["diagnosticsMs"] = 0.0;
		result["writeMs"] = 0.0;
		result["writePopulateMetadataMs"] = 0.0;
		result["writeLockMs"] = 0.0;
		result["writePackMs"] = 0.0;
		result["packHeaderMs"] = 0.0;
		result["packNodeMs"] = 0.0;
		result["packLockMs"] = 0.0;
		result["packScenePathsMs"] = 0.0;
		result["packLayersMs"] = 0.0;
		result["packStrokesMs"] = 0.0;
		result["packChunksMs"] = 0.0;
		result["packPointsMs"] = 0.0;
		result["packPointsReserveMs"] = 0.0;
		result["packPointsReserveReallocated"] = 0.0;
		result["packPointsCapacityBeforeBytes"] = 0.0;
		result["packPointsCapacityAfterBytes"] = 0.0;
		result["packPointsResizeMs"] = 0.0;
		result["packPointsFillMs"] = 0.0;
		result["packPointsCopyMs"] = 0.0;
		result["packPointsChecksumInlineMs"] = 0.0;
		result["packPointsChecksumMs"] = 0.0;
		result["packSelectionSetsMs"] = 0.0;
		result["packDiagnosticsMs"] = 0.0;
		result["packUpgradesMs"] = 0.0;
		result["packPointBackupsMs"] = 0.0;
		result["packFinalChecksumMs"] = 0.0;
		result["packFinalHeaderWriteMs"] = 0.0;
		result["packFinalBufferMs"] = 0.0;
		result["writeResolvePathMs"] = 0.0;
		result["writeBackupMs"] = 0.0;
		result["writeBytesMs"] = 0.0;
		result["writeClearBlobMs"] = 0.0;
		result["writeReleaseLockMs"] = 0.0;
		result["writeBlobObjectMs"] = 0.0;
		result["writeBlobObjectResizeMs"] = 0.0;
		result["writeBlobObjectCopyMs"] = 0.0;
		result["writeBlobPlugSetMs"] = 0.0;
		result["writeSetBlobMs"] = 0.0;
		result["syncMs"] = 0.0;
		return result;
	}

	std::unordered_set<std::uint64_t> removedPointIds;
	std::set<std::uint64_t> changedStrokeIds;
	for( const auto &entry : pointIdsByStroke )
	{
		changedStrokeIds.insert( entry.first );
		removedPointIds.insert( entry.second.begin(), entry.second.end() );
	}

	schema.points.erase(
		std::remove_if(
			schema.points.begin(), schema.points.end(),
			[&removedPointIds]( const PointRecord &point ) {
				return removedPointIds.count( point.pointId );
			}
		),
		schema.points.end()
	);

	auto filterIds = []( std::vector<std::uint64_t> &ids, const std::set<std::uint64_t> &validIds ) {
		ids.erase(
			std::remove_if(
				ids.begin(), ids.end(),
				[&validIds]( std::uint64_t id ) {
					return !validIds.count( id );
				}
			),
			ids.end()
		);
	};

	std::set<std::uint64_t> validPointIds;
	std::set<std::uint64_t> validStrokeIds;
	for( const PointRecord &point : schema.points )
	{
		validPointIds.insert( point.pointId );
		validStrokeIds.insert( point.strokeId );
	}
	const auto selectionCleanupStart = Clock::now();
	filterIds( schema.currentSelection.pointIds, validPointIds );
	filterIds( schema.currentSelection.strokeIds, validStrokeIds );
	schema.currentSelection.pointIds.clear();
	schema.currentSelection.strokeIds.clear();
	for( SelectionSetRecord &selectionSet : schema.selectionSets )
	{
		filterIds( selectionSet.pointIds, validPointIds );
		filterIds( selectionSet.strokeIds, validStrokeIds );
	}
	const double selectionCleanupMs = elapsedMilliseconds( selectionCleanupStart );

	const auto rebuildStart = Clock::now();
	rebuildChunksAndCounts( schema, changedStrokeIds );
	const double rebuildMs = elapsedMilliseconds( rebuildStart );
	const auto diagnosticsStart = Clock::now();
	applyTrustedDiagnostics( this, schema );
	const double diagnosticsMs = elapsedMilliseconds( diagnosticsStart );
	const auto writeStart = Clock::now();
	bp::dict writeTimings;
	eraseInteractivePoints( this, removedPointIds );
	const double writeMs = elapsedMilliseconds( writeStart );
	const auto syncStart = Clock::now();
	syncStateFromSchemaTrusted( this, schema );
	const double syncMs = elapsedMilliseconds( syncStart );
	bp::dict result = brushEraseResult( removedPointIds.size(), changedStrokeIds.size(), eraseSpace );
	result["loadSchemaMs"] = loadSchemaMs;
	result["loadResolvePathMs"] = loadTimings.get( "loadResolvePathMs", bp::object( 0.0 ) );
	result["loadReadBytesMs"] = loadTimings.get( "loadReadBytesMs", bp::object( 0.0 ) );
	result["loadBlobExtractMs"] = loadTimings.get( "loadBlobExtractMs", bp::object( 0.0 ) );
	result["loadUnpackMs"] = loadTimings.get( "loadUnpackMs", bp::object( 0.0 ) );
	result["loadPopulateMetadataMs"] = loadTimings.get( "loadPopulateMetadataMs", bp::object( 0.0 ) );
	result["unpackChecksumMs"] = loadTimings.get( "unpackChecksumMs", bp::object( 0.0 ) );
	result["unpackHeaderMs"] = loadTimings.get( "unpackHeaderMs", bp::object( 0.0 ) );
	result["unpackNodeMs"] = loadTimings.get( "unpackNodeMs", bp::object( 0.0 ) );
	result["unpackLockMs"] = loadTimings.get( "unpackLockMs", bp::object( 0.0 ) );
	result["unpackScenePathsMs"] = loadTimings.get( "unpackScenePathsMs", bp::object( 0.0 ) );
	result["unpackLayersMs"] = loadTimings.get( "unpackLayersMs", bp::object( 0.0 ) );
	result["unpackStrokesMs"] = loadTimings.get( "unpackStrokesMs", bp::object( 0.0 ) );
	result["unpackChunksMs"] = loadTimings.get( "unpackChunksMs", bp::object( 0.0 ) );
	result["unpackPointsMs"] = loadTimings.get( "unpackPointsMs", bp::object( 0.0 ) );
	result["unpackSelectionSetsMs"] = loadTimings.get( "unpackSelectionSetsMs", bp::object( 0.0 ) );
	result["unpackDiagnosticsMs"] = loadTimings.get( "unpackDiagnosticsMs", bp::object( 0.0 ) );
	result["unpackUpgradesMs"] = loadTimings.get( "unpackUpgradesMs", bp::object( 0.0 ) );
	result["unpackPointBackupsMs"] = loadTimings.get( "unpackPointBackupsMs", bp::object( 0.0 ) );
	result["pointScanMs"] = pointScanMs;
	result["selectionCleanupMs"] = selectionCleanupMs;
	result["rebuildMs"] = rebuildMs;
	result["diagnosticsMs"] = diagnosticsMs;
	result["writeMs"] = writeMs;
	result["writePopulateMetadataMs"] = writeTimings.get( "writePopulateMetadataMs", bp::object( 0.0 ) );
	result["writeLockMs"] = writeTimings.get( "writeLockMs", bp::object( 0.0 ) );
	result["writePackMs"] = writeTimings.get( "writePackMs", bp::object( 0.0 ) );
	result["packHeaderMs"] = writeTimings.get( "packHeaderMs", bp::object( 0.0 ) );
	result["packNodeMs"] = writeTimings.get( "packNodeMs", bp::object( 0.0 ) );
	result["packLockMs"] = writeTimings.get( "packLockMs", bp::object( 0.0 ) );
	result["packScenePathsMs"] = writeTimings.get( "packScenePathsMs", bp::object( 0.0 ) );
	result["packLayersMs"] = writeTimings.get( "packLayersMs", bp::object( 0.0 ) );
	result["packStrokesMs"] = writeTimings.get( "packStrokesMs", bp::object( 0.0 ) );
	result["packChunksMs"] = writeTimings.get( "packChunksMs", bp::object( 0.0 ) );
	result["packPointsMs"] = writeTimings.get( "packPointsMs", bp::object( 0.0 ) );
	result["packPointsReserveMs"] = writeTimings.get( "packPointsReserveMs", bp::object( 0.0 ) );
	result["packPointsReserveReallocated"] = writeTimings.get( "packPointsReserveReallocated", bp::object( 0.0 ) );
	result["packPointsCapacityBeforeBytes"] = writeTimings.get( "packPointsCapacityBeforeBytes", bp::object( 0.0 ) );
	result["packPointsCapacityAfterBytes"] = writeTimings.get( "packPointsCapacityAfterBytes", bp::object( 0.0 ) );
	result["packPointsResizeMs"] = writeTimings.get( "packPointsResizeMs", bp::object( 0.0 ) );
	result["packPointsFillMs"] = writeTimings.get( "packPointsFillMs", bp::object( 0.0 ) );
	result["packPointsCopyMs"] = writeTimings.get( "packPointsCopyMs", bp::object( 0.0 ) );
	result["packPointsChecksumInlineMs"] = writeTimings.get( "packPointsChecksumInlineMs", bp::object( 0.0 ) );
	result["packPointsChecksumMs"] = writeTimings.get( "packPointsChecksumMs", bp::object( 0.0 ) );
	result["packSelectionSetsMs"] = writeTimings.get( "packSelectionSetsMs", bp::object( 0.0 ) );
	result["packDiagnosticsMs"] = writeTimings.get( "packDiagnosticsMs", bp::object( 0.0 ) );
	result["packUpgradesMs"] = writeTimings.get( "packUpgradesMs", bp::object( 0.0 ) );
	result["packPointBackupsMs"] = writeTimings.get( "packPointBackupsMs", bp::object( 0.0 ) );
	result["packFinalChecksumMs"] = writeTimings.get( "packFinalChecksumMs", bp::object( 0.0 ) );
	result["packFinalHeaderWriteMs"] = writeTimings.get( "packFinalHeaderWriteMs", bp::object( 0.0 ) );
	result["packFinalBufferMs"] = writeTimings.get( "packFinalBufferMs", bp::object( 0.0 ) );
	result["writeResolvePathMs"] = writeTimings.get( "writeResolvePathMs", bp::object( 0.0 ) );
	result["writeBackupMs"] = writeTimings.get( "writeBackupMs", bp::object( 0.0 ) );
	result["writeBytesMs"] = writeTimings.get( "writeBytesMs", bp::object( 0.0 ) );
	result["writeClearBlobMs"] = writeTimings.get( "writeClearBlobMs", bp::object( 0.0 ) );
	result["writeReleaseLockMs"] = writeTimings.get( "writeReleaseLockMs", bp::object( 0.0 ) );
	result["writeBlobObjectMs"] = writeTimings.get( "writeBlobObjectMs", bp::object( 0.0 ) );
	result["writeBlobObjectResizeMs"] = writeTimings.get( "writeBlobObjectResizeMs", bp::object( 0.0 ) );
	result["writeBlobObjectCopyMs"] = writeTimings.get( "writeBlobObjectCopyMs", bp::object( 0.0 ) );
	result["writeBlobPlugSetMs"] = writeTimings.get( "writeBlobPlugSetMs", bp::object( 0.0 ) );
	result["writeSetBlobMs"] = writeTimings.get( "writeSetBlobMs", bp::object( 0.0 ) );
	result["syncMs"] = syncMs;
	return result;
}

bp::object PaintedPoints::ensureLayerAndStroke( const std::string &layerName, const std::string &strokeName )
{
	const auto totalStart = Clock::now();
	const std::string resolvedLayerName = trimmedName( layerName ).empty() ? "Layer 1" : trimmedName( layerName );
	const std::string resolvedStrokeName = trimmedName( strokeName ).empty() ? "Stroke 1" : trimmedName( strokeName );

	const auto loadStoreStart = Clock::now();
	std::string loadError;
	bp::dict loadTimings;
	CacheSchema schema = loadCacheSchemaForNode( this, &loadError, &loadTimings );
	const double loadStoreMs = elapsedMilliseconds( loadStoreStart );
	if( !loadError.empty() )
	{
		throw std::runtime_error( loadError );
	}

	const auto layerLookupStart = Clock::now();
	LayerRecord *layer = findLayerByName( schema, resolvedLayerName );
	const double layerLookupMs = elapsedMilliseconds( layerLookupStart );

	bool layerCreated = false;
	bool strokeCreated = false;
	double writeMs = 0.0;

	if( !layer )
	{
		LayerRecord newLayer;
		newLayer.layerId = schema.nextIds.layer++;
		newLayer.name = resolvedLayerName;
		newLayer.order = static_cast<std::int32_t>( schema.layers.size() );
		newLayer.enabled = true;
		newLayer.visible = true;
		newLayer.mute = false;
		newLayer.solo = false;
		newLayer.timeVarying = false;
		newLayer.mode = StrokeFrameMode::Persistent;
		newLayer.frameStart = 0;
		newLayer.frameEnd = 0;
		newLayer.holdOutsideRange = true;
		newLayer.firstStrokeId = 0;
		newLayer.lastStrokeId = 0;
		newLayer.colorEnabled = false;
		newLayer.color = { 1.0f, 1.0f, 1.0f };
		schema.layers.push_back( newLayer );
		reindexLayerOrder( schema );
		layer = findLayerByName( schema, resolvedLayerName );
		layerCreated = true;
	}

	if( !layer )
	{
		throw std::runtime_error( "Unable to resolve layer" );
	}

	const auto strokeLookupStart = Clock::now();
	StrokeRecord *stroke = findStrokeByName( schema, layer->layerId, resolvedStrokeName );
	const double strokeLookupMs = elapsedMilliseconds( strokeLookupStart );

	if( !stroke )
	{
		std::size_t existingStrokeCount = 0;
		for( const StrokeRecord &candidate : schema.strokes )
		{
			if( candidate.layerId == layer->layerId )
			{
				++existingStrokeCount;
			}
		}

		StrokeRecord newStroke;
		newStroke.strokeId = schema.nextIds.stroke++;
		newStroke.layerId = layer->layerId;
		newStroke.name = resolvedStrokeName;
		newStroke.order = static_cast<std::int32_t>( existingStrokeCount );
		newStroke.mode = StrokeFrameMode::Persistent;
		newStroke.frameStart = 0;
		newStroke.frameEnd = 0;
		newStroke.createdTimeUnixMicros = unixTimeMicros();
		newStroke.firstChunkId = 0;
		newStroke.lastChunkId = 0;
		newStroke.pointCount = 0;
		newStroke.targetCount = 0;
		newStroke.selectionMaskId = 0;
		newStroke.colorEnabled = false;
		newStroke.color = { 1.0f, 1.0f, 1.0f };
		schema.strokes.push_back( newStroke );
		reindexStrokeOrder( schema, layer->layerId );
		stroke = findStrokeByName( schema, layer->layerId, resolvedStrokeName );
		strokeCreated = true;
	}

	if( !stroke )
	{
		throw std::runtime_error( "Unable to resolve stroke" );
	}

	if( layerCreated || strokeCreated )
	{
		const auto writeStart = Clock::now();
		rebuildChunksAndCounts( schema, {} );
		applyTrustedDiagnostics( this, schema );
		bp::dict writeTimings;
		writeCacheSchemaDirect( this, schema, &writeTimings );
		syncStateFromSchemaTrusted( this, schema );
		writeMs = elapsedMilliseconds( writeStart );
		loadTimings.update( writeTimings );
		layer = findLayerByName( schema, resolvedLayerName );
		stroke = findStrokeByName( schema, layer ? layer->layerId : 0, resolvedStrokeName );
	}

	return ensureLayerStrokeResult(
		layer ? layer->layerId : 0,
		stroke ? stroke->strokeId : 0,
		layerCreated,
		strokeCreated,
		loadStoreMs,
		layerLookupMs,
		strokeLookupMs,
		writeMs,
		bp::extract<double>( loadTimings.get( "loadResolvePathMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "loadReadBytesMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "loadBlobExtractMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "loadUnpackMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "loadPopulateMetadataMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "unpackChecksumMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "unpackHeaderMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "unpackNodeMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "unpackLockMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "unpackScenePathsMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "unpackLayersMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "unpackStrokesMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "unpackChunksMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "unpackPointsMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "unpackSelectionSetsMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "unpackDiagnosticsMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "unpackUpgradesMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "unpackPointBackupsMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "writePopulateMetadataMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "writeLockMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "writePackMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packHeaderMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packNodeMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packLockMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packScenePathsMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packLayersMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packStrokesMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packChunksMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packPointsMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packPointsReserveMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packPointsReserveReallocated", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packPointsCapacityBeforeBytes", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packPointsCapacityAfterBytes", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packPointsResizeMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packPointsFillMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packPointsCopyMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packPointsChecksumInlineMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packPointsChecksumMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packSelectionSetsMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packDiagnosticsMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packUpgradesMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packPointBackupsMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packFinalChecksumMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packFinalHeaderWriteMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "packFinalBufferMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "writeResolvePathMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "writeBackupMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "writeBytesMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "writeClearBlobMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "writeReleaseLockMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "writeBlobObjectMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "writeBlobObjectResizeMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "writeBlobObjectCopyMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "writeBlobPlugSetMs", bp::object( 0.0 ) ) ),
		bp::extract<double>( loadTimings.get( "writeSetBlobMs", bp::object( 0.0 ) ) ),
		elapsedMilliseconds( totalStart )
	);
}
