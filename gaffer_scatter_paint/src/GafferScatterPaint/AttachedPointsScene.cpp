#include "AttachedPointsPrivate.h"

using namespace Gaffer;
using namespace GafferScatterPaint;
using namespace GafferScatterPaint::AttachedPointsPrivate;
using namespace GafferScene;
using namespace IECore;
using namespace IECoreScene;

void AttachedPoints::hashBound( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
	if( path == ScenePlug::stringToPath( outputLocationPlug()->getValue().empty() ? "/scatter" : outputLocationPlug()->getValue() ) )
	{
		h = outPlug()->objectHash( path );
		h.append( "AttachedPointsBound" );
		return;
	}
	h = inPlug()->boundHash( path );
}

void AttachedPoints::hashTransform( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
	const ScenePath outputPath = ScenePlug::stringToPath( outputLocationPlug()->getValue().empty() ? "/scatter" : outputLocationPlug()->getValue() );
	if( path == outputPath )
	{
		h.append( "AttachedPointsTransform" );
		return;
	}
	h = inPlug()->transformHash( path );
}

void AttachedPoints::hashAttributes( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
	const ScenePath outputPath = ScenePlug::stringToPath( outputLocationPlug()->getValue().empty() ? "/scatter" : outputLocationPlug()->getValue() );
	if( path == outputPath )
	{
		ScenePath parentPath = outputPath;
		if( !parentPath.empty() )
		{
			parentPath.pop_back();
		}
		h = inPlug()->attributesHash( parentPath );
		h.append( "AttachedPointsAttributes" );
		return;
	}
	h = inPlug()->attributesHash( path );
}

void AttachedPoints::hashObject( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
	const ScenePath outputPath = ScenePlug::stringToPath( outputLocationPlug()->getValue().empty() ? "/scatter" : outputLocationPlug()->getValue() );
	if( path == outputPath )
	{
		pointTypePlug()->hash( h );
		outputLocationPlug()->hash( h );
		includeAttributesPlug()->hash( h );
		exportPresetPlug()->hash( h );
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
	h = inPlug()->objectHash( path );
}

void AttachedPoints::hashChildNames( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const
{
	const ScenePath outputPath = ScenePlug::stringToPath( outputLocationPlug()->getValue().empty() ? "/scatter" : outputLocationPlug()->getValue() );
	if( path == outputPath )
	{
		outputLocationPlug()->hash( h );
		h.append( "AttachedPointsLeafChildNames" );
		return;
	}
	ScenePath parentPath = outputPath;
	if( !parentPath.empty() )
	{
		parentPath.pop_back();
	}
	if( path == parentPath )
	{
		h = inPlug()->childNamesHash( path );
		outputLocationPlug()->hash( h );
		return;
	}
	h = inPlug()->childNamesHash( path );
}

void AttachedPoints::hashGlobals( const Context *context, const ScenePlug *parent, MurmurHash &h ) const { inPlug()->globalsPlug()->hash( h ); }
void AttachedPoints::hashSetNames( const Context *context, const ScenePlug *parent, MurmurHash &h ) const { inPlug()->setNamesPlug()->hash( h ); }
void AttachedPoints::hashSet( const InternedString &setName, const Context *context, const ScenePlug *parent, MurmurHash &h ) const { h = inPlug()->setHash( setName ); }

Imath::Box3f AttachedPoints::computeBound( const ScenePath &path, const Context *context, const ScenePlug *parent ) const
{
	const ScenePath outputPath = ScenePlug::stringToPath( outputLocationPlug()->getValue().empty() ? "/scatter" : outputLocationPlug()->getValue() );
	if( path == outputPath )
	{
		ConstObjectPtr object = outPlug()->object( path );
		const PointsPrimitive *points = runTimeCast<const PointsPrimitive>( object.get() );
		return points ? points->bound() : Imath::Box3f();
	}
	return inPlug()->bound( path );
}

Imath::M44f AttachedPoints::computeTransform( const ScenePath &path, const Context *context, const ScenePlug *parent ) const
{
	const ScenePath outputPath = ScenePlug::stringToPath( outputLocationPlug()->getValue().empty() ? "/scatter" : outputLocationPlug()->getValue() );
	if( path == outputPath )
	{
		return Imath::M44f();
	}
	return inPlug()->transform( path );
}

ConstCompoundObjectPtr AttachedPoints::computeAttributes( const ScenePath &path, const Context *context, const ScenePlug *parent ) const
{
	const ScenePath outputPath = ScenePlug::stringToPath( outputLocationPlug()->getValue().empty() ? "/scatter" : outputLocationPlug()->getValue() );
	if( path == outputPath )
	{
		ScenePath parentPath = outputPath;
		if( !parentPath.empty() )
		{
			parentPath.pop_back();
		}
		return inPlug()->attributes( parentPath );
	}
	return inPlug()->attributes( path );
}

ConstObjectPtr AttachedPoints::computeObject( const ScenePath &path, const Context *context, const ScenePlug *parent ) const
{
	return computeOutputObject( this, path, context );
}

ConstInternedStringVectorDataPtr AttachedPoints::computeChildNames( const ScenePath &path, const Context *context, const ScenePlug *parent ) const
{
	const ScenePath outputPath = ScenePlug::stringToPath( outputLocationPlug()->getValue().empty() ? "/scatter" : outputLocationPlug()->getValue() );
	if( path == outputPath )
	{
		return new InternedStringVectorData();
	}
	ScenePath parentPath = outputPath;
	InternedString leafName;
	if( !parentPath.empty() )
	{
		leafName = parentPath.back();
		parentPath.pop_back();
	}
	if( path != parentPath )
	{
		return inPlug()->childNames( path );
	}

	ConstInternedStringVectorDataPtr inputNames = inPlug()->childNames( path );
	InternedStringVectorDataPtr result = new InternedStringVectorData();
	result->writable() = inputNames ? inputNames->readable() : std::vector<InternedString>();
	if( std::find( result->writable().begin(), result->writable().end(), leafName ) == result->writable().end() )
	{
		result->writable().push_back( leafName );
	}
	return result;
}

ConstCompoundObjectPtr AttachedPoints::computeGlobals( const Context *context, const ScenePlug *parent ) const { return inPlug()->globals(); }
ConstInternedStringVectorDataPtr AttachedPoints::computeSetNames( const Context *context, const ScenePlug *parent ) const { return inPlug()->setNames(); }
ConstPathMatcherDataPtr AttachedPoints::computeSet( const InternedString &setName, const Context *context, const ScenePlug *parent ) const { return inPlug()->set( setName ); }
