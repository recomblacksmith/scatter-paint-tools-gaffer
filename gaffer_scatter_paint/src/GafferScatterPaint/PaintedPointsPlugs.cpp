#include "PaintedPointsPrivate.h"

StringPlug *PaintedPoints::targetFilterPlug() { return stringChild( this, g_firstPlugIndex ); }
const StringPlug *PaintedPoints::targetFilterPlug() const { return getChild<StringPlug>( g_firstPlugIndex ); }
StringPlug *PaintedPoints::targetSetFilterPlug() { return stringChild( this, g_firstPlugIndex + 1 ); }
const StringPlug *PaintedPoints::targetSetFilterPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 1 ); }
IntPlug *PaintedPoints::surfaceModePlug() { return intChild( this, g_firstPlugIndex + 2 ); }
const IntPlug *PaintedPoints::surfaceModePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 2 ); }
IntPlug *PaintedPoints::paintThroughModePlug() { return intChild( this, g_firstPlugIndex + 3 ); }
const IntPlug *PaintedPoints::paintThroughModePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 3 ); }
IntPlug *PaintedPoints::relaxObjectivePlug() { return intChild( this, g_firstPlugIndex + 4 ); }
const IntPlug *PaintedPoints::relaxObjectivePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 4 ); }
IntPlug *PaintedPoints::cacheModePlug() { return intChild( this, g_firstPlugIndex + 5 ); }
const IntPlug *PaintedPoints::cacheModePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 5 ); }
StringPlug *PaintedPoints::cachePathPlug() { return stringChild( this, g_firstPlugIndex + 6 ); }
const StringPlug *PaintedPoints::cachePathPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 6 ); }
IntPlug *PaintedPoints::cachePathModePlug() { return intChild( this, g_firstPlugIndex + 7 ); }
const IntPlug *PaintedPoints::cachePathModePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 7 ); }
StringPlug *PaintedPoints::projectRootPlug() { return stringChild( this, g_firstPlugIndex + 8 ); }
const StringPlug *PaintedPoints::projectRootPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 8 ); }
IntPlug *PaintedPoints::lockModePlug() { return intChild( this, g_firstPlugIndex + 9 ); }
const IntPlug *PaintedPoints::lockModePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 9 ); }
BoolPlug *PaintedPoints::backupEnabledPlug() { return boolChild( this, g_firstPlugIndex + 10 ); }
const BoolPlug *PaintedPoints::backupEnabledPlug() const { return getChild<BoolPlug>( g_firstPlugIndex + 10 ); }
IntPlug *PaintedPoints::backupPolicyPlug() { return intChild( this, g_firstPlugIndex + 11 ); }
const IntPlug *PaintedPoints::backupPolicyPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 11 ); }
IntPlug *PaintedPoints::compactionModePlug() { return intChild( this, g_firstPlugIndex + 12 ); }
const IntPlug *PaintedPoints::compactionModePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 12 ); }
IntPlug *PaintedPoints::globalModePrecedencePlug() { return intChild( this, g_firstPlugIndex + 13 ); }
const IntPlug *PaintedPoints::globalModePrecedencePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 13 ); }
Color3fPlug *PaintedPoints::defaultColorPlug() { return getChild<Color3fPlug>( g_firstPlugIndex + 14 ); }
const Color3fPlug *PaintedPoints::defaultColorPlug() const { return getChild<Color3fPlug>( g_firstPlugIndex + 14 ); }
Plug *PaintedPoints::pressureDefaultsPlug() { return plugChild( this, g_firstPlugIndex + 15 ); }
const Plug *PaintedPoints::pressureDefaultsPlug() const { return getChild<Plug>( g_firstPlugIndex + 15 ); }
BoolPlug *PaintedPoints::pressureDefaultsEnabledPlug() { return pressureDefaultsPlug()->getChild<BoolPlug>( 0 ); }
const BoolPlug *PaintedPoints::pressureDefaultsEnabledPlug() const { return pressureDefaultsPlug()->getChild<BoolPlug>( 0 ); }
IntPlug *PaintedPoints::pressureDefaultsMappingModePlug() { return pressureDefaultsPlug()->getChild<IntPlug>( 1 ); }
const IntPlug *PaintedPoints::pressureDefaultsMappingModePlug() const { return pressureDefaultsPlug()->getChild<IntPlug>( 1 ); }
Plug *PaintedPoints::brushDefaultsPlug() { return plugChild( this, g_firstPlugIndex + 16 ); }
const Plug *PaintedPoints::brushDefaultsPlug() const { return getChild<Plug>( g_firstPlugIndex + 16 ); }
FloatPlug *PaintedPoints::brushDefaultsSizePlug() { return brushDefaultsPlug()->getChild<FloatPlug>( 0 ); }
const FloatPlug *PaintedPoints::brushDefaultsSizePlug() const { return brushDefaultsPlug()->getChild<FloatPlug>( 0 ); }
FloatPlug *PaintedPoints::brushDefaultsDensityPlug() { return brushDefaultsPlug()->getChild<FloatPlug>( 1 ); }
const FloatPlug *PaintedPoints::brushDefaultsDensityPlug() const { return brushDefaultsPlug()->getChild<FloatPlug>( 1 ); }
FloatPlug *PaintedPoints::brushDefaultsSoftnessPlug() { return brushDefaultsPlug()->getChild<FloatPlug>( 2 ); }
const FloatPlug *PaintedPoints::brushDefaultsSoftnessPlug() const { return brushDefaultsPlug()->getChild<FloatPlug>( 2 ); }
FloatPlug *PaintedPoints::brushDefaultsSpacingPlug() { return brushDefaultsPlug()->getChild<FloatPlug>( 3 ); }
const FloatPlug *PaintedPoints::brushDefaultsSpacingPlug() const { return brushDefaultsPlug()->getChild<FloatPlug>( 3 ); }
IntPlug *PaintedPoints::brushDefaultsPointsPlug() { return brushDefaultsPlug()->getChild<IntPlug>( 4 ); }
const IntPlug *PaintedPoints::brushDefaultsPointsPlug() const { return brushDefaultsPlug()->getChild<IntPlug>( 4 ); }
IntPlug *PaintedPoints::brushDefaultsRotationModePlug() { return brushDefaultsPlug()->getChild<IntPlug>( 5 ); }
const IntPlug *PaintedPoints::brushDefaultsRotationModePlug() const { return brushDefaultsPlug()->getChild<IntPlug>( 5 ); }
FloatPlug *PaintedPoints::brushDefaultsScaleJitterPlug() { return brushDefaultsPlug()->getChild<FloatPlug>( 6 ); }
const FloatPlug *PaintedPoints::brushDefaultsScaleJitterPlug() const { return brushDefaultsPlug()->getChild<FloatPlug>( 6 ); }
FloatPlug *PaintedPoints::brushDefaultsWidthJitterPlug() { return brushDefaultsPlug()->getChild<FloatPlug>( 7 ); }
const FloatPlug *PaintedPoints::brushDefaultsWidthJitterPlug() const { return brushDefaultsPlug()->getChild<FloatPlug>( 7 ); }
ObjectPlug *PaintedPoints::layersPlug() { return objectChild( this, g_firstPlugIndex + 17 ); }
const ObjectPlug *PaintedPoints::layersPlug() const { return getChild<ObjectPlug>( g_firstPlugIndex + 17 ); }
ObjectPlug *PaintedPoints::selectionSetsPlug() { return objectChild( this, g_firstPlugIndex + 18 ); }
const ObjectPlug *PaintedPoints::selectionSetsPlug() const { return getChild<ObjectPlug>( g_firstPlugIndex + 18 ); }
ObjectPlug *PaintedPoints::cacheBlobPlug() { return objectChild( this, g_firstPlugIndex + 19 ); }
const ObjectPlug *PaintedPoints::cacheBlobPlug() const { return getChild<ObjectPlug>( g_firstPlugIndex + 19 ); }
ObjectPlug *PaintedPoints::pendingPaintBlobPlug() { return objectChild( this, g_firstPlugIndex + 35 ); }
const ObjectPlug *PaintedPoints::pendingPaintBlobPlug() const { return getChild<ObjectPlug>( g_firstPlugIndex + 35 ); }
IntPlug *PaintedPoints::interactiveRevisionPlug() { return intChild( this, g_firstPlugIndex + 36 ); }
const IntPlug *PaintedPoints::interactiveRevisionPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 36 ); }
IntPlug *PaintedPoints::authoredPointCountPlug() { return intChild( this, g_firstPlugIndex + 20 ); }
const IntPlug *PaintedPoints::authoredPointCountPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 20 ); }
IntPlug *PaintedPoints::invalidPointCountPlug() { return intChild( this, g_firstPlugIndex + 21 ); }
const IntPlug *PaintedPoints::invalidPointCountPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 21 ); }
IntPlug *PaintedPoints::invalidStrokeCountPlug() { return intChild( this, g_firstPlugIndex + 22 ); }
const IntPlug *PaintedPoints::invalidStrokeCountPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 22 ); }
IntPlug *PaintedPoints::failingFramePlug() { return intChild( this, g_firstPlugIndex + 23 ); }
const IntPlug *PaintedPoints::failingFramePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 23 ); }
ObjectPlug *PaintedPoints::failingTargetPathsPlug() { return objectChild( this, g_firstPlugIndex + 24 ); }
const ObjectPlug *PaintedPoints::failingTargetPathsPlug() const { return getChild<ObjectPlug>( g_firstPlugIndex + 24 ); }
StringPlug *PaintedPoints::lastErrorMessagePlug() { return stringChild( this, g_firstPlugIndex + 25 ); }
const StringPlug *PaintedPoints::lastErrorMessagePlug() const { return getChild<StringPlug>( g_firstPlugIndex + 25 ); }
IntPlug *PaintedPoints::topologyMismatchCountPlug() { return intChild( this, g_firstPlugIndex + 26 ); }
const IntPlug *PaintedPoints::topologyMismatchCountPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 26 ); }
StringPlug *PaintedPoints::validationSummaryPlug() { return stringChild( this, g_firstPlugIndex + 27 ); }
const StringPlug *PaintedPoints::validationSummaryPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 27 ); }
ObjectPlug *PaintedPoints::validationCategoriesPlug() { return objectChild( this, g_firstPlugIndex + 28 ); }
const ObjectPlug *PaintedPoints::validationCategoriesPlug() const { return getChild<ObjectPlug>( g_firstPlugIndex + 28 ); }
StringPlug *PaintedPoints::cacheResolvedPathPlug() { return stringChild( this, g_firstPlugIndex + 29 ); }
const StringPlug *PaintedPoints::cacheResolvedPathPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 29 ); }
IntPlug *PaintedPoints::cacheVersionPlug() { return intChild( this, g_firstPlugIndex + 30 ); }
const IntPlug *PaintedPoints::cacheVersionPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 30 ); }
StringPlug *PaintedPoints::cacheLockedByPlug() { return stringChild( this, g_firstPlugIndex + 31 ); }
const StringPlug *PaintedPoints::cacheLockedByPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 31 ); }
StringPlug *PaintedPoints::cacheLockedHostPlug() { return stringChild( this, g_firstPlugIndex + 32 ); }
const StringPlug *PaintedPoints::cacheLockedHostPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 32 ); }
StringPlug *PaintedPoints::cacheLockedTimePlug() { return stringChild( this, g_firstPlugIndex + 33 ); }
const StringPlug *PaintedPoints::cacheLockedTimePlug() const { return getChild<StringPlug>( g_firstPlugIndex + 33 ); }
StringPlug *PaintedPoints::cacheLockedScriptPlug() { return stringChild( this, g_firstPlugIndex + 34 ); }
const StringPlug *PaintedPoints::cacheLockedScriptPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 34 ); }

void PaintedPoints::affects( const Plug *input, AffectedPlugsContainer &outputs ) const
{
	SceneProcessor::affects( input, outputs );

	const bool authoredStoreChanged =
		input == cacheBlobPlug() ||
		input == pendingPaintBlobPlug() ||
		input == interactiveRevisionPlug() ||
		input == layersPlug() ||
		input == selectionSetsPlug();

	if( authoredStoreChanged )
	{
		outputs.push_back( authoredPointCountPlug() );
		outputs.push_back( invalidPointCountPlug() );
		outputs.push_back( invalidStrokeCountPlug() );
		outputs.push_back( failingFramePlug() );
		outputs.push_back( failingTargetPathsPlug() );
		outputs.push_back( lastErrorMessagePlug() );
		outputs.push_back( topologyMismatchCountPlug() );
		outputs.push_back( validationSummaryPlug() );
		outputs.push_back( validationCategoriesPlug() );
		outputs.push_back( cacheVersionPlug() );
		outputs.push_back( outPlug()->boundPlug() );
		outputs.push_back( outPlug()->objectPlug() );
		outputs.push_back( outPlug()->childNamesPlug() );
		outputs.push_back( outPlug()->globalsPlug() );
	}

	if(
		input == cachePathPlug() ||
		input == cachePathModePlug() ||
		input == projectRootPlug() ||
		input == defaultColorPlug() ||
		input == cacheBlobPlug()
	)
	{
		outputs.push_back( cacheResolvedPathPlug() );
		outputs.push_back( cacheLockedByPlug() );
		outputs.push_back( cacheLockedHostPlug() );
		outputs.push_back( cacheLockedTimePlug() );
		outputs.push_back( cacheLockedScriptPlug() );
	}
	
	if( input == inPlug() )
	{
		outputs.push_back( outPlug()->boundPlug() );
		outputs.push_back( outPlug()->transformPlug() );
		outputs.push_back( outPlug()->attributesPlug() );
		outputs.push_back( outPlug()->objectPlug() );
		outputs.push_back( outPlug()->childNamesPlug() );
		outputs.push_back( outPlug()->globalsPlug() );
		outputs.push_back( outPlug()->setNamesPlug() );
	}

}

void PaintedPoints::hashBound( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const { inPlug()->boundPlug()->hash( h ); }
void PaintedPoints::hashTransform( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const { inPlug()->transformPlug()->hash( h ); }
void PaintedPoints::hashAttributes( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const { inPlug()->attributesPlug()->hash( h ); }
void PaintedPoints::hashObject( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const { inPlug()->objectPlug()->hash( h ); }
void PaintedPoints::hashChildNames( const ScenePath &path, const Context *context, const ScenePlug *parent, MurmurHash &h ) const { inPlug()->childNamesPlug()->hash( h ); }
void PaintedPoints::hashGlobals( const Context *context, const ScenePlug *parent, MurmurHash &h ) const { inPlug()->globalsPlug()->hash( h ); }
void PaintedPoints::hashSetNames( const Context *context, const ScenePlug *parent, MurmurHash &h ) const { inPlug()->setNamesPlug()->hash( h ); }
void PaintedPoints::hashSet( const InternedString &setName, const Context *context, const ScenePlug *parent, MurmurHash &h ) const { h = inPlug()->setHash( setName ); }

Imath::Box3f PaintedPoints::computeBound( const ScenePath &path, const Context *context, const ScenePlug *parent ) const { return inPlug()->bound( path ); }
Imath::M44f PaintedPoints::computeTransform( const ScenePath &path, const Context *context, const ScenePlug *parent ) const { return inPlug()->transform( path ); }
ConstCompoundObjectPtr PaintedPoints::computeAttributes( const ScenePath &path, const Context *context, const ScenePlug *parent ) const { return inPlug()->attributes( path ); }
ConstObjectPtr PaintedPoints::computeObject( const ScenePath &path, const Context *context, const ScenePlug *parent ) const { return inPlug()->object( path ); }
ConstInternedStringVectorDataPtr PaintedPoints::computeChildNames( const ScenePath &path, const Context *context, const ScenePlug *parent ) const { return inPlug()->childNames( path ); }
ConstCompoundObjectPtr PaintedPoints::computeGlobals( const Context *context, const ScenePlug *parent ) const { return inPlug()->globals(); }
ConstInternedStringVectorDataPtr PaintedPoints::computeSetNames( const Context *context, const ScenePlug *parent ) const { return inPlug()->setNames(); }
ConstPathMatcherDataPtr PaintedPoints::computeSet( const InternedString &setName, const Context *context, const ScenePlug *parent ) const { return inPlug()->set( setName ); }
