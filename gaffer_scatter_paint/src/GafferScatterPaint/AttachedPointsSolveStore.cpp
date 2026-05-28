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

const std::vector<std::string> g_validationCategoryNames = {
	"cache",
	"lock",
	"topology",
	"attachment",
	"exportReadiness",
	"upgradeState",
	"diagnostics",
};

bool isNone( const bp::object &value )
{
	return value.ptr() == Py_None;
}

std::string pyString( const bp::object &value )
{
	return bp::extract<std::string>( bp::str( value ) );
}

bp::object dictGet( const bp::dict &dictionary, const char *key )
{
	return dictionary.has_key( key ) ? dictionary[key] : bp::object();
}

std::vector<std::string> stringVector( const bp::object &value )
{
	std::vector<std::string> result;
	if( isNone( value ) )
	{
		return result;
	}

	for( bp::stl_input_iterator<bp::object> it( value ), end; it != end; ++it )
	{
		result.push_back( pyString( *it ) );
	}
	return result;
}

std::vector<float> floatVector( const bp::object &value, const std::vector<float> &defaultValue )
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
	return result.empty() ? defaultValue : result;
}

Imath::V3f vectorFromValues( const bp::object &value, const Imath::V3f &defaultValue )
{
	const std::vector<float> values = floatVector( value, { defaultValue.x, defaultValue.y, defaultValue.z } );
	if( values.size() != 3 )
	{
		return defaultValue;
	}
	return Imath::V3f( values[0], values[1], values[2] );
}

Imath::V2f vector2FromValues( const bp::object &value, const Imath::V2f &defaultValue )
{
	const std::vector<float> values = floatVector( value, { defaultValue.x, defaultValue.y } );
	if( values.size() != 2 )
	{
		return defaultValue;
	}
	return Imath::V2f( values[0], values[1] );
}

Imath::Quatf quatFromValues( const bp::object &value, const Imath::Quatf &defaultValue )
{
	if( isNone( value ) )
	{
		return defaultValue;
	}

	std::vector<float> values;
	for( bp::stl_input_iterator<bp::object> it( value ), end; it != end; ++it )
	{
		values.push_back( extractOr<float>( *it, 0.0f ) );
	}
	if( values.size() != 4 )
	{
		return defaultValue;
	}
	return Imath::Quatf( values[0], Imath::V3f( values[1], values[2], values[3] ) );
}

StoreSnapshot snapshotFromSchema( const CacheSchema &schema )
{
	StoreSnapshot result;
	result.available = true;
	result.schemaVersion = static_cast<int>( CacheHeader::schemaVersion );
	result.invalidPointCount = static_cast<int>( schema.diagnostics.invalidPointCount );
	result.invalidStrokeCount = static_cast<int>( schema.diagnostics.invalidStrokeCount );
	result.failingFrame = schema.diagnostics.failingFrame;
	result.defaultColor = Imath::Color3f( schema.node.defaultColor[0], schema.node.defaultColor[1], schema.node.defaultColor[2] );
	result.failingTargetPaths = schema.diagnostics.failingTargetPaths;
	result.scenePaths = schema.scenePaths;
	result.instanceSourcePaths = schema.instanceSourcePaths;

	for( const ValidationCategory category : schema.diagnostics.categories )
	{
		const int categoryId = static_cast<int>( category );
		if( categoryId >= 0 && static_cast<size_t>( categoryId ) < g_validationCategoryNames.size() )
		{
			result.validationCategories.push_back( g_validationCategoryNames[categoryId] );
		}
		else
		{
			result.validationCategories.push_back( "unknown:" + std::to_string( categoryId ) );
		}
	}

	for( const LayerRecord &layer : schema.layers )
	{
		AuthoredLayerRecord record;
		record.layerId = static_cast<int>( layer.layerId );
		record.order = layer.order;
		record.enabled = layer.enabled;
		record.visible = layer.visible;
		record.mute = layer.mute;
		record.solo = layer.solo;
		record.mode = static_cast<int>( layer.mode );
		record.frameStart = layer.frameStart;
		record.frameEnd = layer.frameEnd;
		record.holdOutsideRange = layer.holdOutsideRange;
		record.colorEnabled = layer.colorEnabled;
		record.color = Imath::Color3f( layer.color[0], layer.color[1], layer.color[2] );
		result.layers.push_back( record );
	}

	for( const StrokeRecord &stroke : schema.strokes )
	{
		AuthoredStrokeRecord record;
		record.strokeId = static_cast<int>( stroke.strokeId );
		record.layerId = static_cast<int>( stroke.layerId );
		record.order = stroke.order;
		record.mode = static_cast<int>( stroke.mode );
		record.frameStart = stroke.frameStart;
		record.frameEnd = stroke.frameEnd;
		record.colorEnabled = stroke.colorEnabled;
		record.color = Imath::Color3f( stroke.color[0], stroke.color[1], stroke.color[2] );
		result.strokes.push_back( record );
	}

	for( const PointRecord &point : schema.points )
	{
		AuthoredPointRecord record;
		record.pointId = static_cast<int>( point.pointId );
		record.strokeId = static_cast<int>( point.strokeId );
		record.layerId = static_cast<int>( point.layerId );
		record.valid = point.valid;
		record.targetPathId = static_cast<int>( point.targetPathId );
		record.instanceId = static_cast<int>( point.instanceId );
		record.instanceSourcePathId = static_cast<int>( point.instanceSourcePathId );
		record.triangleIndex = static_cast<int>( point.triangleIndex );
		record.barycentric = Imath::V3f( point.barycentric[0], point.barycentric[1], point.barycentric[2] );
		record.restObjectP = Imath::V3f( point.restObjectP[0], point.restObjectP[1], point.restObjectP[2] );
		record.restWorldP = Imath::V3f( point.restWorldP[0], point.restWorldP[1], point.restWorldP[2] );
		record.restUV = Imath::V2f( point.restUV[0], point.restUV[1] );
		record.restNormal = Imath::V3f( point.restNormal[0], point.restNormal[1], point.restNormal[2] );
		record.restUp = Imath::V3f( point.restUp[0], point.restUp[1], point.restUp[2] );
		record.uniformScale = point.uniformScale;
		record.width = point.width;
		record.normalSpin = point.normalSpin;
		record.tangentRotation = Imath::V2f( point.tangentRotation[0], point.tangentRotation[1] );
		record.seed = static_cast<int>( point.seed );
		record.anchorModeUsed = static_cast<int>( point.anchorModeUsed );
		record.topologyGeneration = static_cast<int>( point.topologyGeneration );
		record.colorEnabled = point.colorEnabled;
		record.color = Imath::Color3f( point.color[0], point.color[1], point.color[2] );
		result.points.push_back( record );
	}

	return result;
}

StoreSnapshot loadStoreSnapshot( const AttachedPoints *node, std::string &loadError )
{
	StoreSnapshot result;

	loadError.clear();
	const Plug *inputPlug = node->pointsPlug()->getInput();
	if( !inputPlug )
	{
		return result;
	}

	const DependencyNode *dependencyNode = runTimeCast<const DependencyNode>( inputPlug->node() );
	if( !dependencyNode )
	{
		return result;
	}

	bp::object storeObject;

	if( const PaintedPoints *paintedPoints = runTimeCast<const PaintedPoints>( dependencyNode ) )
	{
		try
		{
			return snapshotFromSchema( paintedPoints->visibleSchema() );
		}
		catch( const std::exception &e )
		{
			loadError = e.what();
			return result;
		}
	}
	else
	{
		IECorePython::ScopedGILLock gilLock;
		bp::object nodeObject( bp::ptr( const_cast<DependencyNode *>( dependencyNode ) ) );
		if( !PyObject_HasAttrString( nodeObject.ptr(), "cacheSnapshot" ) )
		{
			return result;
		}

		try
		{
			storeObject = nodeObject.attr( "cacheSnapshot" )();
		}
		catch( const bp::error_already_set & )
		{
			PyObject *type = nullptr;
			PyObject *value = nullptr;
			PyObject *traceback = nullptr;
			PyErr_Fetch( &type, &value, &traceback );
			PyErr_NormalizeException( &type, &value, &traceback );
			bp::handle<> valueHandle( bp::allow_null( value ) );
			loadError = value ? pyString( bp::object( valueHandle ) ) : "Unknown authored store load error";
			bp::handle<> typeHandle( bp::allow_null( type ) );
			bp::handle<> tracebackHandle( bp::allow_null( traceback ) );
			PyErr_Clear();
			return result;
		}
	}

	if( isNone( storeObject ) )
	{
		return result;
	}

	bp::extract<bp::dict> storeExtractor( storeObject );
	if( !storeExtractor.check() )
	{
		loadError = "Authored store is not a dictionary.";
		return result;
	}

	const bp::dict store = storeExtractor();
	result.available = true;
	result.schemaVersion = dictValue<int>( store, "schemaVersion", g_schemaVersion );
	result.scenePaths = stringVector( dictGet( store, "scenePaths" ) );
	result.instanceSourcePaths = stringVector( dictGet( store, "instanceSourcePaths" ) );
	bp::extract<bp::dict> nodeExtractor( dictGet( store, "node" ) );
	if( nodeExtractor.check() )
	{
		const bp::dict nodeData = nodeExtractor();
		const std::vector<float> defaultColor = floatVector( dictGet( nodeData, "defaultColor" ), { 1.0f, 1.0f, 1.0f } );
		if( defaultColor.size() == 3 )
		{
			result.defaultColor = Imath::Color3f( defaultColor[0], defaultColor[1], defaultColor[2] );
		}
	}

	bp::extract<bp::dict> diagnosticsExtractor( dictGet( store, "diagnostics" ) );
	if( diagnosticsExtractor.check() )
	{
		const bp::dict diagnostics = diagnosticsExtractor();
		result.invalidPointCount = dictValue<int>( diagnostics, "invalidPointCount", 0 );
		result.invalidStrokeCount = dictValue<int>( diagnostics, "invalidStrokeCount", 0 );
		result.failingFrame = dictValue<int>( diagnostics, "failingFrame", 0 );
		result.failingTargetPaths = stringVector( dictGet( diagnostics, "failingTargetPaths" ) );

		const bp::object categoriesObject = dictGet( diagnostics, "categories" );
		if( !isNone( categoriesObject ) )
		{
			for( bp::stl_input_iterator<bp::object> it( categoriesObject ), end; it != end; ++it )
			{
				const int categoryId = extractOr<int>( *it, 6 );
				if( categoryId >= 0 && static_cast<size_t>( categoryId ) < g_validationCategoryNames.size() )
				{
					result.validationCategories.push_back( g_validationCategoryNames[categoryId] );
				}
				else
				{
					result.validationCategories.push_back( "unknown:" + std::to_string( categoryId ) );
				}
			}
		}
	}

	bp::extract<bp::list> layersExtractor( dictGet( store, "layers" ) );
	if( layersExtractor.check() )
	{
		const bp::list layers = layersExtractor();
		for( bp::stl_input_iterator<bp::object> it( layers ), end; it != end; ++it )
		{
			bp::extract<bp::dict> layerExtractor( *it );
			if( !layerExtractor.check() )
			{
				continue;
			}
			const bp::dict layer = layerExtractor();
			AuthoredLayerRecord record;
			record.layerId = dictValue<int>( layer, "layerId", 0 );
			record.order = dictValue<int>( layer, "order", 0 );
			record.enabled = dictValue<bool>( layer, "enabled", true );
			record.visible = dictValue<bool>( layer, "visible", true );
			record.mute = dictValue<bool>( layer, "mute", false );
			record.solo = dictValue<bool>( layer, "solo", false );
			record.mode = dictValue<int>( layer, "mode", 0 );
			record.frameStart = dictValue<int>( layer, "frameStart", 0 );
			record.frameEnd = dictValue<int>( layer, "frameEnd", 0 );
			record.holdOutsideRange = dictValue<bool>( layer, "holdOutsideRange", true );
			record.colorEnabled = dictValue<bool>( layer, "colorEnabled", false );
			const std::vector<float> color = floatVector( dictGet( layer, "color" ), { 1.0f, 1.0f, 1.0f } );
			if( color.size() == 3 )
			{
				record.color = Imath::Color3f( color[0], color[1], color[2] );
			}
			result.layers.push_back( record );
		}
	}

	bp::extract<bp::list> strokesExtractor( dictGet( store, "strokes" ) );
	if( strokesExtractor.check() )
	{
		const bp::list strokes = strokesExtractor();
		for( bp::stl_input_iterator<bp::object> it( strokes ), end; it != end; ++it )
		{
			bp::extract<bp::dict> strokeExtractor( *it );
			if( !strokeExtractor.check() )
			{
				continue;
			}
			const bp::dict stroke = strokeExtractor();
			AuthoredStrokeRecord record;
			record.strokeId = dictValue<int>( stroke, "strokeId", 0 );
			record.layerId = dictValue<int>( stroke, "layerId", 0 );
			record.order = dictValue<int>( stroke, "order", 0 );
			record.mode = dictValue<int>( stroke, "mode", 0 );
			record.frameStart = dictValue<int>( stroke, "frameStart", 0 );
			record.frameEnd = dictValue<int>( stroke, "frameEnd", 0 );
			record.colorEnabled = dictValue<bool>( stroke, "colorEnabled", false );
			const std::vector<float> color = floatVector( dictGet( stroke, "color" ), { 1.0f, 1.0f, 1.0f } );
			if( color.size() == 3 )
			{
				record.color = Imath::Color3f( color[0], color[1], color[2] );
			}
			result.strokes.push_back( record );
		}
	}

	bp::extract<bp::list> pointsExtractor( dictGet( store, "points" ) );
	if( pointsExtractor.check() )
	{
		const bp::list points = pointsExtractor();
		for( bp::stl_input_iterator<bp::object> it( points ), end; it != end; ++it )
		{
			bp::extract<bp::dict> pointExtractor( *it );
			if( !pointExtractor.check() )
			{
				continue;
			}
			const bp::dict point = pointExtractor();
			AuthoredPointRecord record;
			record.pointId = dictValue<int>( point, "pointId", 0 );
			record.strokeId = dictValue<int>( point, "strokeId", 0 );
			record.layerId = dictValue<int>( point, "layerId", 0 );
			record.valid = dictValue<bool>( point, "valid", true );
			record.targetPathId = dictValue<int>( point, "targetPathId", 0 );
			record.instanceId = dictValue<int>( point, "instanceId", 0 );
			record.instanceSourcePathId = dictValue<int>( point, "instanceSourcePathId", 0 );
			record.triangleIndex = dictValue<int>( point, "triangleIndex", -1 );
			record.barycentric = vectorFromValues( dictGet( point, "barycentric" ), Imath::V3f( 1.0f, 0.0f, 0.0f ) );
			record.restObjectP = vectorFromValues( dictGet( point, "restObjectP" ), Imath::V3f( 0.0f ) );
			record.restWorldP = vectorFromValues( dictGet( point, "restWorldP" ), Imath::V3f( 0.0f ) );
			record.restUV = vector2FromValues( dictGet( point, "restUV" ), Imath::V2f( 0.0f ) );
			record.restNormal = vectorFromValues( dictGet( point, "restNormal" ), Imath::V3f( 0.0f, 1.0f, 0.0f ) );
			record.restUp = vectorFromValues( dictGet( point, "restUp" ), Imath::V3f( 0.0f, 0.0f, 1.0f ) );
			record.uniformScale = dictValue<float>( point, "uniformScale", 1.0f );
			record.width = dictValue<float>( point, "width", 1.0f );
			record.seed = dictValue<int>( point, "seed", 0 );
			record.anchorModeUsed = dictValue<int>( point, "anchorModeUsed", 3 );
			record.topologyGeneration = dictValue<int>( point, "topologyGeneration", 0 );
			record.colorEnabled = dictValue<bool>( point, "colorEnabled", false );
			const std::vector<float> color = floatVector( dictGet( point, "color" ), { 1.0f, 1.0f, 1.0f } );
			if( color.size() == 3 )
			{
				record.color = Imath::Color3f( color[0], color[1], color[2] );
			}
			result.points.push_back( record );
		}
	}

	return result;
}

void hashAuthoredStoreSource( const AttachedPoints *node, MurmurHash &h )
{
	const Plug *inputPlug = node->pointsPlug()->getInput();
	if( !inputPlug )
	{
		return;
	}

	if( const PaintedPoints *paintedPoints = runTimeCast<const PaintedPoints>( inputPlug->node() ) )
	{
		h.append( paintedPoints->cacheBlobPlug()->hash() );
		h.append( paintedPoints->interactiveRevisionPlug()->hash() );
		h.append( paintedPoints->cacheVersionPlug()->hash() );
		h.append( paintedPoints->invalidPointCountPlug()->hash() );
		h.append( paintedPoints->topologyMismatchCountPlug()->hash() );
	}
}

} // namespace AttachedPointsPrivate

} // namespace GafferScatterPaint
