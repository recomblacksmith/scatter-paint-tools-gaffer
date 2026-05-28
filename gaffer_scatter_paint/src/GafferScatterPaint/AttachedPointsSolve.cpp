#include "AttachedPointsPrivate.h"

#include <chrono>
#include <cstdlib>

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

namespace
{

using Clock = std::chrono::steady_clock;

double elapsedMilliseconds( const Clock::time_point &start )
{
	return std::chrono::duration<double, std::milli>( Clock::now() - start ).count();
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

void logInfoIfEnabled( const std::string &context, const std::string &message )
{
	if( !diagnosticsEnabled() )
	{
		return;
	}

	IECore::msg( IECore::Msg::Level::Info, context, message );
}

std::string formatTimingMessage(
	const std::string &phase,
	double totalMs,
	double storeLoadMs,
	double activeStrokeMs,
	double solveLoopMs,
	double fallbackMs,
	int authoredPointCount,
	int resolvedPointCount,
	int unresolvedPointCount,
	bool strictUnresolved,
	bool keepLastValidOutput,
	bool lastValidUsed,
	bool threw = false
)
{
	std::ostringstream message;
	message
		<< phase
		<< " totalMs=" << totalMs
		<< " storeLoadMs=" << storeLoadMs
		<< " activeStrokeMs=" << activeStrokeMs
		<< " solveLoopMs=" << solveLoopMs
		<< " fallbackMs=" << fallbackMs
		<< " authoredPointCount=" << authoredPointCount
		<< " resolvedPointCount=" << resolvedPointCount
		<< " unresolvedPointCount=" << unresolvedPointCount
		<< " strictUnresolved=" << ( strictUnresolved ? "true" : "false" )
		<< " keepLastValidOutput=" << ( keepLastValidOutput ? "true" : "false" )
		<< " lastValidUsed=" << ( lastValidUsed ? "true" : "false" );
	if( threw )
	{
		message << " threw=true";
	}
	return message.str();
}

} // namespace

int currentFrame( const Context *context )
{
	return static_cast<int>( std::lround( context->getFrame() ) );
}

struct SolveResult
{
	std::vector<OutputPointRecord> pointRecords;
	int cacheVersion = g_schemaVersion;
	int resolvedPointCount = 0;
	int unresolvedPointCount = 0;
	int invalidPointCount = 0;
	int invalidStrokeCount = 0;
	int failingFrame = 0;
	int topologyMismatchCount = 0;
	int lastValidFrame = 0;
	std::vector<std::string> failingTargetPaths;
	std::vector<std::string> attachmentFailureReasons;
	std::vector<std::string> validationCategories;
	std::string solveStatus = "No authored scatter data connected.";
	bool hasAuthoredStore = false;
	bool lastValidUsed = false;
};

struct LastValidState
{
	std::vector<OutputPointRecord> pointRecords;
	int frame = 0;
};

std::mutex g_lastValidStateMutex;
std::unordered_map<const AttachedPoints *, LastValidState> g_lastValidStates;

void updateLastValidState( const AttachedPoints *node, const std::vector<OutputPointRecord> &pointRecords, int frame )
{
	std::lock_guard<std::mutex> lock( g_lastValidStateMutex );
	g_lastValidStates[node] = LastValidState{ pointRecords, frame };
}

bool readLastValidState( const AttachedPoints *node, LastValidState &state )
{
	std::lock_guard<std::mutex> lock( g_lastValidStateMutex );
	const auto it = g_lastValidStates.find( node );
	if( it == g_lastValidStates.end() )
	{
		return false;
	}
	state = it->second;
	return true;
}

void clearLastValidState( const AttachedPoints *node )
{
	std::lock_guard<std::mutex> lock( g_lastValidStateMutex );
	g_lastValidStates.erase( node );
}

std::string pathFromId( const std::vector<std::string> &paths, int pathId )
{
	if( pathId <= 0 )
	{
		return std::string();
	}
	if( static_cast<size_t>( pathId ) > paths.size() )
	{
		return std::string();
	}
	return paths[pathId - 1];
}

std::string sourcePathFromPoint( const StoreSnapshot &store, const AuthoredPointRecord &point )
{
	return pathFromId( store.scenePaths, point.targetPathId );
}

std::string instanceSourcePathFromPoint( const StoreSnapshot &store, const AuthoredPointRecord &point )
{
	return pathFromId( store.instanceSourcePaths, point.instanceSourcePathId );
}

std::string resolvedSourcePathFromPoint( const StoreSnapshot &store, const AuthoredPointRecord &point )
{
	const std::string instanceSourcePath = instanceSourcePathFromPoint( store, point );
	if( !instanceSourcePath.empty() )
	{
		return instanceSourcePath + "/" + std::to_string( point.instanceId );
	}
	return sourcePathFromPoint( store, point );
}

int normalizedAnchorMode( int mode )
{
	return mode >= 0 && mode <= 3 ? mode : 0;
}

int normalizedSurfaceSolveMode( int mode )
{
	return mode == 0 ? 0 : 0;
}

bool hybridSurfaceSolveEnabled( const AttachedPoints *node )
{
	return normalizedSurfaceSolveMode( node->surfaceSolveModePlug()->getValue() ) == 0;
}

bool usesStoredFallbackAnchor( int anchorMode )
{
	const int normalizedMode = normalizedAnchorMode( anchorMode );
	return normalizedMode == 1 || normalizedMode == 2;
}

bool topologyGenerationStale( const AuthoredPointRecord &rawPoint, int frame )
{
	return rawPoint.topologyGeneration > 0 && rawPoint.topologyGeneration != frame;
}

bool canUseDeformingStoredFallback( const AuthoredPointRecord &rawPoint, int frame )
{
	return topologyGenerationStale( rawPoint, frame ) && rawPoint.anchorModeUsed == 0;
}

std::string topologyFailureReason( const AuthoredPointRecord &rawPoint, int frame, const char *defaultReason )
{
	return topologyGenerationStale( rawPoint, frame ) ? "topologyGenerationMismatch" : defaultReason;
}

Imath::Color3f resolvedAuthoredColor(
	const StoreSnapshot &store,
	const AuthoredLayerRecord *layer,
	const AuthoredStrokeRecord *stroke,
	const AuthoredPointRecord &point
)
{
	if( point.colorEnabled )
	{
		return point.color;
	}
	if( stroke && stroke->colorEnabled )
	{
		return stroke->color;
	}
	if( layer && layer->colorEnabled )
	{
		return layer->color;
	}
	return store.defaultColor;
}

void applyStoredFallbackPoint(
	const AuthoredPointRecord &rawPoint,
	const std::string &sourcePath,
	const Imath::M44f *fullTransform,
	OutputPointRecord &point
)
{
	point.sourcePath = sourcePath;
	if( fullTransform )
	{
		point.position = rawPoint.restObjectP * ( *fullTransform );
		Imath::V3f transformedNormal;
		Imath::V3f transformedUp;
		fullTransform->multDirMatrix( rawPoint.restNormal, transformedNormal );
		fullTransform->multDirMatrix( rawPoint.restUp, transformedUp );
		point.normal = normalized( transformedNormal, Imath::V3f( 0, 1, 0 ) );
		point.up = orthogonalized( transformedUp, point.normal, Imath::V3f( 0, 0, 1 ) );
	}
	else
	{
		point.position = rawPoint.restWorldP;
		point.normal = normalized( rawPoint.restNormal, Imath::V3f( 0, 1, 0 ) );
		point.up = orthogonalized( rawPoint.restUp, point.normal, Imath::V3f( 0, 0, 1 ) );
	}
	point.orient = composedOrient( point.normal, point.up, point.normalSpin, point.tangentRotation );
	point.attachmentResolved = true;
}

ScenePlug::ScenePath &pathSolveScenePath( PathSolveCache &pathCache )
{
	if( !pathCache.scenePathParsed )
	{
		pathCache.scenePath = ScenePlug::stringToPath( pathCache.sourcePath );
		pathCache.scenePathParsed = true;
	}
	return pathCache.scenePath;
}

bool ensurePathExists( const AttachedPoints *node, PathSolveCache &pathCache )
{
	if( !pathCache.existsChecked )
	{
		pathCache.exists = node->inPlug()->exists( pathSolveScenePath( pathCache ) );
		pathCache.existsChecked = true;
	}
	return pathCache.exists;
}

const Imath::M44f *pathTransform( const AttachedPoints *node, PathSolveCache &pathCache )
{
	if( !ensurePathExists( node, pathCache ) )
	{
		return nullptr;
	}

	if( !pathCache.transformChecked )
	{
		try
		{
			pathCache.fullTransform = node->inPlug()->fullTransform( pathSolveScenePath( pathCache ) );
			pathCache.hasTransform = true;
		}
		catch( ... )
		{
			pathCache.hasTransform = false;
		}
		pathCache.transformChecked = true;
	}

	return pathCache.hasTransform ? &pathCache.fullTransform : nullptr;
}

bool ensureGeometryCache( const AttachedPoints *node, PathSolveCache &pathCache )
{
	if( pathCache.geometryChecked )
	{
		return pathCache.geometrySupported;
	}

	pathCache.geometryChecked = true;
	pathCache.geometryLoadFailed = false;
	pathCache.geometrySupported = false;
	if( !ensurePathExists( node, pathCache ) )
	{
		return false;
	}

	try
	{
		pathCache.object = node->inPlug()->object( pathSolveScenePath( pathCache ) );
		pathCache.fullTransform = node->inPlug()->fullTransform( pathSolveScenePath( pathCache ) );
		pathCache.hasTransform = true;
		pathCache.transformChecked = true;
	}
	catch( ... )
	{
		pathCache.object = nullptr;
		pathCache.hasTransform = false;
		pathCache.transformChecked = true;
		pathCache.geometryLoadFailed = true;
		return false;
	}

	pathCache.geometrySupported = meshTriangleData( pathCache.object, pathCache.triangleData ) && supportsCurrentSolveMesh( pathCache.triangleData );
	return pathCache.geometrySupported;
}

int normalizedFrameMode( int mode )
{
	return mode >= 0 && mode <= 2 ? mode : 0;
}

bool strokeFrameActive( const AuthoredStrokeRecord &stroke, int currentFrame )
{
	int frameStart = stroke.frameStart;
	int frameEnd = stroke.frameEnd;
	if( frameStart == 0 && frameEnd == 0 )
	{
		return true;
	}
	if( frameStart > frameEnd )
	{
		std::swap( frameStart, frameEnd );
	}
	return frameStart <= currentFrame && currentFrame <= frameEnd;
}

bool layerFrameActive( const AuthoredLayerRecord &layer, int currentFrame )
{
	int frameStart = layer.frameStart;
	int frameEnd = layer.frameEnd;
	if( frameStart == 0 && frameEnd == 0 )
	{
		return true;
	}
	if( frameStart > frameEnd )
	{
		std::swap( frameStart, frameEnd );
	}
	if( frameStart <= currentFrame && currentFrame <= frameEnd )
	{
		return true;
	}
	return layer.holdOutsideRange;
}

struct ActiveStrokeRecord
{
	const AuthoredLayerRecord *layer = nullptr;
	const AuthoredStrokeRecord *stroke = nullptr;
	int effectiveMode = 0;
	bool inheritedLayerMode = false;
};

enum class PointSolveMode
{
	StoredFallback,
	GeometryResolve,
};

struct PointSolveWork
{
	const AuthoredLayerRecord *layer = nullptr;
	const AuthoredStrokeRecord *stroke = nullptr;
	const AuthoredPointRecord *rawPoint = nullptr;
	std::string sourcePath;
	size_t outputIndex = 0;
	PointSolveMode mode = PointSolveMode::GeometryResolve;
};

std::vector<ActiveStrokeRecord> activeStrokesForFrame( const StoreSnapshot &store, int currentFrame, int &activeLayerCount )
{
	std::vector<const AuthoredLayerRecord *> visibleLayers;
	visibleLayers.reserve( store.layers.size() );
	for( const AuthoredLayerRecord &layer : store.layers )
	{
		if( layer.enabled && layer.visible && !layer.mute && layerFrameActive( layer, currentFrame ) )
		{
			visibleLayers.push_back( &layer );
		}
	}

	std::stable_sort(
		visibleLayers.begin(),
		visibleLayers.end(),
		[]( const AuthoredLayerRecord *a, const AuthoredLayerRecord *b ) {
			return a->order < b->order;
		}
	);

	if( std::any_of( visibleLayers.begin(), visibleLayers.end(), []( const AuthoredLayerRecord *layer ) { return layer->solo; } ) )
	{
		std::vector<const AuthoredLayerRecord *> soloLayers;
		soloLayers.reserve( visibleLayers.size() );
		for( const AuthoredLayerRecord *layer : visibleLayers )
		{
			if( layer->solo )
			{
				soloLayers.push_back( layer );
			}
		}
		visibleLayers.swap( soloLayers );
	}

	activeLayerCount = static_cast<int>( visibleLayers.size() );

	std::vector<ActiveStrokeRecord> activeStrokes;
	for( const AuthoredLayerRecord *layer : visibleLayers )
	{
		std::vector<const AuthoredStrokeRecord *> layerStrokes;
		for( const AuthoredStrokeRecord &stroke : store.strokes )
		{
			if( stroke.layerId == layer->layerId && strokeFrameActive( stroke, currentFrame ) )
			{
				layerStrokes.push_back( &stroke );
			}
		}

		std::stable_sort(
			layerStrokes.begin(),
			layerStrokes.end(),
			[]( const AuthoredStrokeRecord *a, const AuthoredStrokeRecord *b ) {
				return a->order < b->order;
			}
		);

		const int layerMode = normalizedFrameMode( layer->mode );
		for( const AuthoredStrokeRecord *stroke : layerStrokes )
		{
			const int strokeMode = normalizedFrameMode( stroke->mode );
			const bool inheritedLayerMode = strokeMode == 0 && layerMode != 0;
			activeStrokes.push_back( ActiveStrokeRecord{ layer, stroke, inheritedLayerMode ? layerMode : strokeMode, inheritedLayerMode } );
		}
	}

	std::vector<ActiveStrokeRecord> explicitOverrideStrokes;
	for( const ActiveStrokeRecord &activeStroke : activeStrokes )
	{
		if( activeStroke.effectiveMode == 2 && !activeStroke.inheritedLayerMode )
		{
			explicitOverrideStrokes.push_back( activeStroke );
		}
	}

	if( !explicitOverrideStrokes.empty() )
	{
		const ActiveStrokeRecord best = *std::max_element(
			explicitOverrideStrokes.begin(),
			explicitOverrideStrokes.end(),
			[]( const ActiveStrokeRecord &a, const ActiveStrokeRecord &b ) {
				if( a.layer->order != b.layer->order )
				{
					return a.layer->order < b.layer->order;
				}
				return a.stroke->order < b.stroke->order;
			}
		);
		return { best };
	}

	std::vector<const AuthoredLayerRecord *> overrideLayers;
	for( const AuthoredLayerRecord *layer : visibleLayers )
	{
		if( normalizedFrameMode( layer->mode ) == 2 )
		{
			overrideLayers.push_back( layer );
		}
	}

	if( !overrideLayers.empty() )
	{
		const AuthoredLayerRecord *bestLayer = *std::max_element(
			overrideLayers.begin(),
			overrideLayers.end(),
			[]( const AuthoredLayerRecord *a, const AuthoredLayerRecord *b ) {
				return a->order < b->order;
			}
		);

		std::vector<ActiveStrokeRecord> filtered;
		for( const ActiveStrokeRecord &activeStroke : activeStrokes )
		{
			if( activeStroke.layer == bestLayer )
			{
				filtered.push_back( activeStroke );
			}
		}
		return filtered;
	}

	return activeStrokes;
}

SolveResult solvePoints( const AttachedPoints *node, const Context *context )
{
	const auto totalStart = Clock::now();
	double storeLoadMs = 0.0;
	double activeStrokeMs = 0.0;
	double solveLoopMs = 0.0;
	double fallbackMs = 0.0;
	SolveResult result;
	std::string loadError;
	const int frame = currentFrame( context );
	const auto storeLoadStart = Clock::now();
	const StoreSnapshot store = loadStoreSnapshot( node, loadError );
	storeLoadMs = elapsedMilliseconds( storeLoadStart );
	if( !store.available )
	{
		result.solveStatus = loadError.empty() ? "No authored scatter data connected." : loadError;
		if( node->strictUnresolvedPlug()->getValue() && node->pointsPlug()->getInput() )
		{
			result.unresolvedPointCount = 1;
		}
		logInfoIfEnabled(
			"AttachedPoints.solvePoints",
			formatTimingMessage(
				"solvePoints",
				elapsedMilliseconds( totalStart ),
				storeLoadMs,
				activeStrokeMs,
				solveLoopMs,
				fallbackMs,
				0,
				result.resolvedPointCount,
				result.unresolvedPointCount,
				node->strictUnresolvedPlug()->getValue(),
				node->keepLastValidOutputPlug()->getValue(),
				result.lastValidUsed
			)
		);
		return result;
	}

	result.hasAuthoredStore = true;
	result.cacheVersion = store.schemaVersion;
	result.validationCategories = store.validationCategories;
	result.invalidPointCount = 0;
	result.invalidStrokeCount = store.invalidStrokeCount;
	result.failingFrame = store.failingFrame;
	result.failingTargetPaths = store.failingTargetPaths;

	std::map<int, std::vector<const AuthoredPointRecord *>> pointsByStroke;
	for( const AuthoredPointRecord &point : store.points )
	{
		pointsByStroke[point.strokeId].push_back( &point );
	}

	int layerCount = 0;
	const auto activeStrokeStart = Clock::now();
	const std::vector<ActiveStrokeRecord> activeStrokes = activeStrokesForFrame( store, frame, layerCount );
	activeStrokeMs = elapsedMilliseconds( activeStrokeStart );
	const int strokeCount = static_cast<int>( activeStrokes.size() );
	const bool hybridSurfaceSolve = hybridSurfaceSolveEnabled( node );
	const bool allowCrossMeshReproject = hybridSurfaceSolve && node->allowCrossMeshReprojectPlug()->getValue();
	int totalPointCount = 0;
	std::set<std::string> failingPathsSet( result.failingTargetPaths.begin(), result.failingTargetPaths.end() );
	std::map<std::string, int> failureCounts;
	std::vector<PointSolveWork> pointSolveWork;
	std::vector<std::string> groupedSourcePaths;
	std::unordered_map<std::string, std::vector<size_t>> pointWorkBySourcePath;
	std::unordered_map<std::string, PathSolveCache> pathCaches;
	result.pointRecords.reserve( store.points.size() );

	const auto solveLoopStart = Clock::now();
	for( const ActiveStrokeRecord &activeStroke : activeStrokes )
	{
		const AuthoredStrokeRecord &stroke = *activeStroke.stroke;
		const auto pointIt = pointsByStroke.find( stroke.strokeId );
		if( pointIt == pointsByStroke.end() )
		{
			continue;
		}

		const std::vector<const AuthoredPointRecord *> &strokePoints = pointIt->second;
		totalPointCount += static_cast<int>( strokePoints.size() );

		for( const AuthoredPointRecord *rawPointPtr : strokePoints )
		{
			const AuthoredPointRecord &rawPoint = *rawPointPtr;
			OutputPointRecord point;
			point.pointId = rawPoint.pointId;
			point.strokeId = rawPoint.strokeId;
			point.width = rawPoint.width;
			point.scale = rawPoint.uniformScale;
			point.normalSpin = rawPoint.normalSpin;
			point.tangentRotation = rawPoint.tangentRotation;
			point.seed = rawPoint.seed;
			point.triangleIndex = rawPoint.triangleIndex;
			point.barycentric = rawPoint.barycentric;
			point.color = resolvedAuthoredColor( store, activeStroke.layer, activeStroke.stroke, rawPoint );
			point.sourcePath = resolvedSourcePathFromPoint( store, rawPoint );
			point.position = rawPoint.restWorldP;
			point.normal = rawPoint.restNormal;
			point.up = rawPoint.restUp;
			point.orient = composedOrient( point.normal, point.up, point.normalSpin, point.tangentRotation );
			point.attachmentResolved = rawPoint.anchorModeUsed != 3;

			if( !rawPoint.valid )
			{
				++result.invalidPointCount;
				result.pointRecords.push_back( point );
				continue;
			}

			if( !point.attachmentResolved )
			{
					result.pointRecords.push_back( point );
					++result.unresolvedPointCount;
					continue;
			}

			if( point.sourcePath.empty() && !( hybridSurfaceSolve && usesStoredFallbackAnchor( rawPoint.anchorModeUsed ) ) )
			{
				++failureCounts["missingSourcePath"];
				point.attachmentResolved = false;
				result.pointRecords.push_back( point );
				++result.unresolvedPointCount;
				continue;
			}

			const size_t outputIndex = result.pointRecords.size();
			result.pointRecords.push_back( point );
			PointSolveWork work;
			work.layer = activeStroke.layer;
			work.stroke = activeStroke.stroke;
			work.rawPoint = &rawPoint;
			work.sourcePath = point.sourcePath;
			work.outputIndex = outputIndex;
			work.mode = hybridSurfaceSolve && usesStoredFallbackAnchor( rawPoint.anchorModeUsed ) ? PointSolveMode::StoredFallback : PointSolveMode::GeometryResolve;
			if( pointWorkBySourcePath.find( work.sourcePath ) == pointWorkBySourcePath.end() )
				{
					groupedSourcePaths.push_back( work.sourcePath );
				}
			pointSolveWork.push_back( work );
			pointWorkBySourcePath[work.sourcePath].push_back( pointSolveWork.size() - 1 );
		}
	}

	for( const std::string &sourcePath : groupedSourcePaths )
	{
		PathSolveCache &pathCache = pathCaches[sourcePath];
		pathCache.sourcePath = sourcePath;
		const std::vector<size_t> &workItems = pointWorkBySourcePath[sourcePath];
		for( size_t workIndex : workItems )
		{
			const PointSolveWork &work = pointSolveWork[workIndex];
			const AuthoredPointRecord &rawPoint = *work.rawPoint;
			OutputPointRecord &point = result.pointRecords[work.outputIndex];

			if( work.mode == PointSolveMode::StoredFallback )
			{
				applyStoredFallbackPoint( rawPoint, point.sourcePath, pathTransform( node, pathCache ), point );
				++result.resolvedPointCount;
				continue;
			}

			if( !ensurePathExists( node, pathCache ) )
			{
				++failureCounts["missingTargetPath"];
				failingPathsSet.insert( point.sourcePath );
				point.attachmentResolved = false;
				++result.unresolvedPointCount;
				continue;
			}

			if( !ensureGeometryCache( node, pathCache ) )
			{
				if( pathCache.geometryLoadFailed )
				{
					++failureCounts["missingTargetPath"];
				}
				else
				{
					++failureCounts["unsupportedTargetObject"];
				}
				failingPathsSet.insert( point.sourcePath );
				point.attachmentResolved = false;
				++result.unresolvedPointCount;
				continue;
			}

			const std::array<int, 4> *triangle = triangleForIndex( pathCache.triangleData, rawPoint.triangleIndex );
			if( !triangle )
			{
				if( allowCrossMeshReproject )
				{
					applyStoredFallbackPoint( rawPoint, point.sourcePath, &pathCache.fullTransform, point );
					point.attachmentResolved = false;
					++failureCounts[topologyFailureReason( rawPoint, frame, "missingTriangle" )];
					++result.topologyMismatchCount;
					failingPathsSet.insert( point.sourcePath );
					++result.unresolvedPointCount;
					continue;
				}
				if( hybridSurfaceSolve && canUseDeformingStoredFallback( rawPoint, frame ) )
				{
					applyStoredFallbackPoint( rawPoint, point.sourcePath, &pathCache.fullTransform, point );
					++result.resolvedPointCount;
					++failureCounts["topologyGenerationMismatch"];
					++result.topologyMismatchCount;
					failingPathsSet.insert( point.sourcePath );
					continue;
				}
				++failureCounts[topologyFailureReason( rawPoint, frame, "missingTriangle" )];
				++result.topologyMismatchCount;
				failingPathsSet.insert( point.sourcePath );
				point.attachmentResolved = false;
				++result.unresolvedPointCount;
				continue;
			}

			if(
				( *triangle )[1] < 0 || ( *triangle )[2] < 0 || ( *triangle )[3] < 0 ||
				static_cast<size_t>( ( *triangle )[1] ) >= pathCache.triangleData.positions.size() ||
				static_cast<size_t>( ( *triangle )[2] ) >= pathCache.triangleData.positions.size() ||
				static_cast<size_t>( ( *triangle )[3] ) >= pathCache.triangleData.positions.size()
			)
			{
				if( allowCrossMeshReproject )
				{
					applyStoredFallbackPoint( rawPoint, point.sourcePath, &pathCache.fullTransform, point );
					point.attachmentResolved = false;
					++failureCounts[topologyFailureReason( rawPoint, frame, "invalidTriangleVertices" )];
					++result.topologyMismatchCount;
					failingPathsSet.insert( point.sourcePath );
					++result.unresolvedPointCount;
					continue;
				}
				if( hybridSurfaceSolve && canUseDeformingStoredFallback( rawPoint, frame ) )
				{
					applyStoredFallbackPoint( rawPoint, point.sourcePath, &pathCache.fullTransform, point );
					++result.resolvedPointCount;
					++failureCounts["topologyGenerationMismatch"];
					++result.topologyMismatchCount;
					failingPathsSet.insert( point.sourcePath );
					continue;
				}
				++failureCounts[topologyFailureReason( rawPoint, frame, "invalidTriangleVertices" )];
				++result.topologyMismatchCount;
				failingPathsSet.insert( point.sourcePath );
				point.attachmentResolved = false;
				++result.unresolvedPointCount;
				continue;
			}

			const Imath::V3f barycentric = rawPoint.barycentric;
			const Imath::V3f a = pathCache.triangleData.positions[( *triangle )[1]];
			const Imath::V3f b = pathCache.triangleData.positions[( *triangle )[2]];
			const Imath::V3f c = pathCache.triangleData.positions[( *triangle )[3]];
			const Imath::V3f objectPoint = trianglePoint( a, b, c, barycentric );
			const Imath::V3f faceNormal = normalized( cross( b - a, c - a ), Imath::V3f( 0, 1, 0 ) );
			const Imath::V3f objectNormal = triangleNormal( pathCache.triangleData.mesh.get(), *triangle, barycentric, faceNormal );
			const Imath::V3f objectUp = triangleUp( a, b, c, objectNormal );
			const Imath::V3f worldPoint = objectPoint * pathCache.fullTransform;
			Imath::V3f transformedNormal;
			Imath::V3f transformedUp;
			pathCache.fullTransform.multDirMatrix( objectNormal, transformedNormal );
			pathCache.fullTransform.multDirMatrix( objectUp, transformedUp );
			const Imath::V3f worldNormal = normalized( transformedNormal, Imath::V3f( 0, 1, 0 ) );
			const Imath::V3f worldUp = orthogonalized( transformedUp, worldNormal, Imath::V3f( 0, 0, 1 ) );

			point.position = worldPoint;
			point.normal = worldNormal;
			point.up = worldUp;
			point.orient = composedOrient( worldNormal, worldUp, point.normalSpin, point.tangentRotation );
			point.attachmentResolved = true;
			++result.resolvedPointCount;
		}
	}
	solveLoopMs = elapsedMilliseconds( solveLoopStart );

	for( const auto &failure : failureCounts )
	{
		result.attachmentFailureReasons.push_back( failure.first + ":" + std::to_string( failure.second ) );
	}

	if( !failureCounts.empty() )
	{
		result.validationCategories.push_back( "attachment" );
	}

	result.failingTargetPaths.assign( failingPathsSet.begin(), failingPathsSet.end() );

	std::ostringstream status;
	status
		<< layerCount << " active layers, "
		<< strokeCount << " active strokes, "
		<< result.resolvedPointCount << " resolved points, "
		<< result.unresolvedPointCount << " fallback points, "
		<< totalPointCount << " authored points ready for attachment solve";
	if( !result.attachmentFailureReasons.empty() )
	{
		status << ". Failures: ";
		for( size_t i = 0; i < result.attachmentFailureReasons.size(); ++i )
		{
			if( i )
			{
				status << ", ";
			}
			status << result.attachmentFailureReasons[i];
		}
	}
	result.solveStatus = loadError.empty() ? status.str() : "Invalid authored data. " + status.str();

	const auto fallbackStart = Clock::now();
	if( node->keepLastValidOutputPlug()->getValue() )
	{
		LastValidState lastValidState;
		const bool hasLastValidState = readLastValidState( node, lastValidState );
		const bool hasAuthoredPoints = totalPointCount > 0;
		if( hasLastValidState )
		{
			result.lastValidFrame = lastValidState.frame;
		}

		const bool solveHealthy = loadError.empty() && failureCounts.empty() && result.unresolvedPointCount == 0;
		std::ostringstream fallbackDecision;
		fallbackDecision
			<< "fallbackDecision"
			<< " solveHealthy=" << ( solveHealthy ? "true" : "false" )
			<< " pointRecordCount=" << result.pointRecords.size()
			<< " unresolvedPointCount=" << result.unresolvedPointCount
			<< " strictUnresolved=" << ( node->strictUnresolvedPlug()->getValue() ? "true" : "false" )
			<< " keepLastValidOutput=true"
			<< " hasLastValidState=" << ( hasLastValidState ? "true" : "false" )
			<< " loadErrorEmpty=" << ( loadError.empty() ? "true" : "false" )
			<< " failureReasonCount=" << result.attachmentFailureReasons.size();
		if( !result.attachmentFailureReasons.empty() )
		{
			fallbackDecision << " failureReasons=";
			for( size_t i = 0; i < result.attachmentFailureReasons.size(); ++i )
			{
				if( i )
				{
					fallbackDecision << ",";
				}
				fallbackDecision << result.attachmentFailureReasons[i];
			}
		}
		if( solveHealthy && !result.pointRecords.empty() )
		{
			updateLastValidState( node, result.pointRecords, frame );
			result.lastValidFrame = frame;
			fallbackDecision << " action=updateLastValidState frame=" << frame;
		}
		else if( !hasAuthoredPoints )
		{
			clearLastValidState( node );
			result.lastValidFrame = 0;
			fallbackDecision << " action=clearLastValidState noAuthoredPoints=true";
		}
		else if( result.pointRecords.empty() || node->strictUnresolvedPlug()->getValue() )
		{
			if( hasLastValidState )
			{
				result.pointRecords = lastValidState.pointRecords;
				result.lastValidFrame = lastValidState.frame;
				result.lastValidUsed = true;
				result.solveStatus += ". Using last valid output from frame " + std::to_string( lastValidState.frame ) + ".";
				fallbackDecision << " action=useLastValidState frame=" << lastValidState.frame;
			}
			else
			{
				fallbackDecision << " action=noLastValidStateAvailable";
			}
		}
		else
		{
			fallbackDecision << " action=keepCurrentSolveResult";
		}

		logInfoIfEnabled( "AttachedPoints.solvePoints", fallbackDecision.str() );
	}
	fallbackMs = elapsedMilliseconds( fallbackStart );

	logInfoIfEnabled(
		"AttachedPoints.solvePoints",
		formatTimingMessage(
			"solvePoints",
			elapsedMilliseconds( totalStart ),
			storeLoadMs,
			activeStrokeMs,
			solveLoopMs,
			fallbackMs,
			totalPointCount,
			result.resolvedPointCount,
			result.unresolvedPointCount,
			node->strictUnresolvedPlug()->getValue(),
			node->keepLastValidOutputPlug()->getValue(),
			result.lastValidUsed
		)
	);

	return result;
}

void setStringVectorPlug( ObjectPlug *plug, const std::vector<std::string> &values )
{
	plug->setValue( stringVectorData( values ) );
}

ConstObjectPtr computeOutputObject( const AttachedPoints *node, const ScenePlug::ScenePath &path, const Context *context )
{
	const auto totalStart = Clock::now();
	const std::string outputLocation = node->outputLocationPlug()->getValue().empty() ? "/scatter" : node->outputLocationPlug()->getValue();
	const ScenePlug::ScenePath outputPath = ScenePlug::stringToPath( outputLocation );
	if( path != outputPath )
	{
		return node->inPlug()->object( path );
	}

	SolveResult result = solvePoints( node, context );
	if( node->strictUnresolvedPlug()->getValue() && result.unresolvedPointCount > 0 )
	{
		logInfoIfEnabled(
			"AttachedPoints.computeOutputObject",
			formatTimingMessage(
				"computeOutputObject",
				elapsedMilliseconds( totalStart ),
				0.0,
				0.0,
				0.0,
				0.0,
				static_cast<int>( result.pointRecords.size() ),
				result.resolvedPointCount,
				result.unresolvedPointCount,
				node->strictUnresolvedPlug()->getValue(),
				node->keepLastValidOutputPlug()->getValue(),
				result.lastValidUsed,
				true
			)
		);
		throw IECore::Exception( result.solveStatus );
	}
	logInfoIfEnabled(
		"AttachedPoints.computeOutputObject",
		formatTimingMessage(
			"computeOutputObject",
			elapsedMilliseconds( totalStart ),
			0.0,
			0.0,
			0.0,
			0.0,
			static_cast<int>( result.pointRecords.size() ),
			result.resolvedPointCount,
			result.unresolvedPointCount,
			node->strictUnresolvedPlug()->getValue(),
			node->keepLastValidOutputPlug()->getValue(),
			result.lastValidUsed
		)
	);
	return pointsPrimitiveFromRecords(
		result.pointRecords,
		node->pointTypePlug()->getValue(),
		exportPresetValue( node->exportPresetPlug()->getValue() ),
		node->includeAttributesPlug()->getValue(),
		node->debugColorPlug()->getValue()
	);
}

} // namespace AttachedPointsPrivate

} // namespace GafferScatterPaint

void AttachedPoints::hash( const ValuePlug *output, const Context *context, MurmurHash &h ) const
{
	if(
		output == cacheVersionPlug() ||
		output == resolvedPointCountPlug() ||
		output == unresolvedPointCountPlug() ||
		output == lastValidFramePlug() ||
		output == invalidPointCountPlug() ||
		output == invalidStrokeCountPlug() ||
		output == failingFramePlug() ||
		output == failingTargetPathsPlug() ||
		output == attachmentFailureReasonsPlug() ||
		output == topologyMismatchCountPlug() ||
		output == solveStatusPlug() ||
		output == validationCategoriesPlug()
	)
	{
		h.append( output->getName().string() );
		outputLocationPlug()->hash( h );
		pointTypePlug()->hash( h );
		surfaceSolveModePlug()->hash( h );
		allowCrossMeshReprojectPlug()->hash( h );
		keepLastValidOutputPlug()->hash( h );
		strictUnresolvedPlug()->hash( h );
		debugColorPlug()->hash( h );
		h.append( currentFrame( context ) );
		hashAuthoredStoreSource( this, h );
		h.append( inPlug()->globalsHash() );
		h.append( inPlug()->setNamesHash() );
		h.append( inPlug()->childNamesHash( ScenePlug::ScenePath() ) );
		return;
	}

	SceneProcessor::hash( output, context, h );
}

void AttachedPoints::compute( ValuePlug *output, const Context *context ) const
{
	if(
		output == cacheVersionPlug() ||
		output == resolvedPointCountPlug() ||
		output == unresolvedPointCountPlug() ||
		output == lastValidFramePlug() ||
		output == invalidPointCountPlug() ||
		output == invalidStrokeCountPlug() ||
		output == failingFramePlug() ||
		output == failingTargetPathsPlug() ||
		output == attachmentFailureReasonsPlug() ||
		output == topologyMismatchCountPlug() ||
		output == solveStatusPlug() ||
		output == validationCategoriesPlug()
	)
	{
		const SolveResult result = solvePoints( this, context );
		if( output == cacheVersionPlug() )
		{
			static_cast<IntPlug *>( output )->setValue( result.cacheVersion );
		}
		else if( output == resolvedPointCountPlug() )
		{
			static_cast<IntPlug *>( output )->setValue( result.resolvedPointCount );
		}
		else if( output == unresolvedPointCountPlug() )
		{
			static_cast<IntPlug *>( output )->setValue( result.unresolvedPointCount );
		}
		else if( output == lastValidFramePlug() )
		{
			static_cast<IntPlug *>( output )->setValue( result.lastValidFrame );
		}
		else if( output == invalidPointCountPlug() )
		{
			static_cast<IntPlug *>( output )->setValue( result.invalidPointCount );
		}
		else if( output == invalidStrokeCountPlug() )
		{
			static_cast<IntPlug *>( output )->setValue( result.invalidStrokeCount );
		}
		else if( output == failingFramePlug() )
		{
			static_cast<IntPlug *>( output )->setValue( result.failingFrame );
		}
		else if( output == failingTargetPathsPlug() )
		{
			static_cast<ObjectPlug *>( output )->setValue( stringVectorData( result.failingTargetPaths ) );
		}
		else if( output == attachmentFailureReasonsPlug() )
		{
			static_cast<ObjectPlug *>( output )->setValue( stringVectorData( result.attachmentFailureReasons ) );
		}
		else if( output == topologyMismatchCountPlug() )
		{
			static_cast<IntPlug *>( output )->setValue( result.topologyMismatchCount );
		}
		else if( output == solveStatusPlug() )
		{
			static_cast<StringPlug *>( output )->setValue( result.solveStatus );
		}
		else if( output == validationCategoriesPlug() )
		{
			static_cast<ObjectPlug *>( output )->setValue( stringVectorData( result.validationCategories ) );
		}
		return;
	}

	SceneProcessor::compute( output, context );
}
