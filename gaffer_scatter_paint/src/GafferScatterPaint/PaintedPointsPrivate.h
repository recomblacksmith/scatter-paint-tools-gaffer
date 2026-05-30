#pragma once

#include "GafferScatterPaint/PaintedPoints.h"
#include "GafferScatterPaint/CacheFormat.h"
#include "CacheFormatDict.h"
#include "CacheValidation.h"
#include "PaintedPointsInteractiveStore.h"

#include "GafferScatterPaint/AttachedPoints.h"
#include "GafferScatterPaint/StaticPoints.h"

#include "Gaffer/Context.h"
#include "Gaffer/Action.h"
#include "Gaffer/NumericPlug.h"
#include "Gaffer/Plug.h"
#include "Gaffer/ScriptNode.h"
#include "Gaffer/StringPlug.h"
#include "Gaffer/TypedObjectPlug.h"
#include "Gaffer/UndoScope.h"

#include "GafferScene/SceneWriter.h"
#include "GafferScene/ObjectToScene.h"
#include "GafferScene/Group.h"
#include "GafferScene/SetAlgo.h"

#include "IECore/CompoundObject.h"
#include "IECore/PathMatcher.h"
#include "IECore/RunTimeTyped.h"
#include "IECore/SimpleTypedData.h"
#include "IECore/VectorTypedData.h"
#include "IECore/Writer.h"

#include "IECoreScene/MeshPrimitive.h"
#include "IECoreScene/PointsPrimitive.h"
#include "IECoreScene/PrimitiveVariable.h"

#include "boost/python.hpp"
#include "boost/python/stl_iterator.hpp"

namespace bp = boost::python;

#include "PaintedPointsPythonUtils.h"
#include "PaintedPointsDiagnostics.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#if defined(_WIN32)
#include <process.h>
using ssize_t = std::intptr_t;
#else
#include <unistd.h>
#endif
#include <vector>

using namespace Gaffer;
using namespace GafferScatterPaint;
using namespace GafferScene;
using namespace IECore;

namespace
{

constexpr std::uint32_t g_chunkPointLimit = 8192;

constexpr std::uint32_t g_schemaVersion = 2;
constexpr std::uint32_t g_cacheModeExternal = 1;
constexpr std::uint32_t g_cachePathModeRelativeToScript = 1;
constexpr std::uint32_t g_cachePathModeRelativeToProject = 2;
constexpr std::uint32_t g_lockModeSessionAware = 0;
constexpr std::uint32_t g_backupPolicyOn = 1;
constexpr std::uint32_t g_lockStaleSeconds = 6 * 60 * 60;
constexpr std::uint32_t g_relaxObjectivePreserveSilhouette = 0;
constexpr std::uint32_t g_relaxObjectiveEvenRedistribution = 1;
constexpr char g_backupSuffix[] = ".bak";
constexpr char g_schemaDescription[] = "Gaffer Scatter Paint cache schema v2";

AttachedPoints *findAttachedPointsForPaintedNode( const PaintedPoints *node );

std::string pathFromId( const bp::dict &store, const char *key, std::uint32_t pathId );

#include "PaintedPointsNodeUtils.h"
#include "PaintedPointsGeometryUtils.h"
#include "PaintedPointsStoreUtils.h"
#include "PaintedPointsSchemaUtils.h"

std::uint64_t nextId( bp::dict store, const char *key )
{
	bp::dict nextIds = bp::extract<bp::dict>( store["nextIds"] );
	const std::uint64_t result = dictValue<std::uint64_t>( nextIds, key, 1 );
	nextIds[key] = result + 1;
	return result;
}

#include "PaintedPointsModelUtils.h"
#include "PaintedPointsRuntimeUtils.h"

} // namespace
