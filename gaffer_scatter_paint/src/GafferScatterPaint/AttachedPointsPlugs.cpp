#include "AttachedPointsPrivate.h"

using namespace Gaffer;
using namespace GafferScatterPaint;
using namespace GafferScatterPaint::AttachedPointsPrivate;
using namespace GafferScene;
using namespace IECore;
using namespace IECoreScene;

ScenePlug *AttachedPoints::pointsPlug() { return getChild<ScenePlug>( g_firstPlugIndex ); }
const ScenePlug *AttachedPoints::pointsPlug() const { return getChild<ScenePlug>( g_firstPlugIndex ); }
StringPlug *AttachedPoints::outputLocationPlug() { return getChild<StringPlug>( g_firstPlugIndex + 1 ); }
const StringPlug *AttachedPoints::outputLocationPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 1 ); }
StringPlug *AttachedPoints::pointTypePlug() { return getChild<StringPlug>( g_firstPlugIndex + 2 ); }
const StringPlug *AttachedPoints::pointTypePlug() const { return getChild<StringPlug>( g_firstPlugIndex + 2 ); }
StringPlug *AttachedPoints::includeAttributesPlug() { return getChild<StringPlug>( g_firstPlugIndex + 3 ); }
const StringPlug *AttachedPoints::includeAttributesPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 3 ); }
IntPlug *AttachedPoints::exportPresetPlug() { return getChild<IntPlug>( g_firstPlugIndex + 4 ); }
const IntPlug *AttachedPoints::exportPresetPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 4 ); }
IntPlug *AttachedPoints::surfaceSolveModePlug() { return getChild<IntPlug>( g_firstPlugIndex + 5 ); }
const IntPlug *AttachedPoints::surfaceSolveModePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 5 ); }
BoolPlug *AttachedPoints::allowCrossMeshReprojectPlug() { return getChild<BoolPlug>( g_firstPlugIndex + 6 ); }
const BoolPlug *AttachedPoints::allowCrossMeshReprojectPlug() const { return getChild<BoolPlug>( g_firstPlugIndex + 6 ); }
BoolPlug *AttachedPoints::keepLastValidOutputPlug() { return getChild<BoolPlug>( g_firstPlugIndex + 7 ); }
const BoolPlug *AttachedPoints::keepLastValidOutputPlug() const { return getChild<BoolPlug>( g_firstPlugIndex + 7 ); }
BoolPlug *AttachedPoints::strictUnresolvedPlug() { return getChild<BoolPlug>( g_firstPlugIndex + 8 ); }
const BoolPlug *AttachedPoints::strictUnresolvedPlug() const { return getChild<BoolPlug>( g_firstPlugIndex + 8 ); }
BoolPlug *AttachedPoints::debugColorPlug() { return getChild<BoolPlug>( g_firstPlugIndex + 9 ); }
const BoolPlug *AttachedPoints::debugColorPlug() const { return getChild<BoolPlug>( g_firstPlugIndex + 9 ); }
IntPlug *AttachedPoints::cacheVersionPlug() { return getChild<IntPlug>( g_firstPlugIndex + 10 ); }
const IntPlug *AttachedPoints::cacheVersionPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 10 ); }
IntPlug *AttachedPoints::resolvedPointCountPlug() { return getChild<IntPlug>( g_firstPlugIndex + 11 ); }
const IntPlug *AttachedPoints::resolvedPointCountPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 11 ); }
IntPlug *AttachedPoints::unresolvedPointCountPlug() { return getChild<IntPlug>( g_firstPlugIndex + 12 ); }
const IntPlug *AttachedPoints::unresolvedPointCountPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 12 ); }
IntPlug *AttachedPoints::lastValidFramePlug() { return getChild<IntPlug>( g_firstPlugIndex + 13 ); }
const IntPlug *AttachedPoints::lastValidFramePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 13 ); }
IntPlug *AttachedPoints::invalidPointCountPlug() { return getChild<IntPlug>( g_firstPlugIndex + 14 ); }
const IntPlug *AttachedPoints::invalidPointCountPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 14 ); }
IntPlug *AttachedPoints::invalidStrokeCountPlug() { return getChild<IntPlug>( g_firstPlugIndex + 15 ); }
const IntPlug *AttachedPoints::invalidStrokeCountPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 15 ); }
IntPlug *AttachedPoints::failingFramePlug() { return getChild<IntPlug>( g_firstPlugIndex + 16 ); }
const IntPlug *AttachedPoints::failingFramePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 16 ); }
ObjectPlug *AttachedPoints::failingTargetPathsPlug() { return getChild<ObjectPlug>( g_firstPlugIndex + 17 ); }
const ObjectPlug *AttachedPoints::failingTargetPathsPlug() const { return getChild<ObjectPlug>( g_firstPlugIndex + 17 ); }
ObjectPlug *AttachedPoints::attachmentFailureReasonsPlug() { return getChild<ObjectPlug>( g_firstPlugIndex + 18 ); }
const ObjectPlug *AttachedPoints::attachmentFailureReasonsPlug() const { return getChild<ObjectPlug>( g_firstPlugIndex + 18 ); }
IntPlug *AttachedPoints::topologyMismatchCountPlug() { return getChild<IntPlug>( g_firstPlugIndex + 19 ); }
const IntPlug *AttachedPoints::topologyMismatchCountPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 19 ); }
StringPlug *AttachedPoints::solveStatusPlug() { return getChild<StringPlug>( g_firstPlugIndex + 20 ); }
const StringPlug *AttachedPoints::solveStatusPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 20 ); }
ObjectPlug *AttachedPoints::validationCategoriesPlug() { return getChild<ObjectPlug>( g_firstPlugIndex + 21 ); }
const ObjectPlug *AttachedPoints::validationCategoriesPlug() const { return getChild<ObjectPlug>( g_firstPlugIndex + 21 ); }

void AttachedPoints::affects( const Plug *input, AffectedPlugsContainer &outputs ) const
{
	SceneProcessor::affects( input, outputs );

	const bool pointsInputChanged = input == pointsPlug() || pointsPlug()->isAncestorOf( input );
	const bool sceneInputChanged = input == inPlug() || inPlug()->isAncestorOf( input );

	if(
		pointsInputChanged ||
		sceneInputChanged ||
		input == outputLocationPlug() ||
		input == pointTypePlug() ||
		input == includeAttributesPlug() ||
		input == exportPresetPlug() ||
		input == surfaceSolveModePlug() ||
		input == allowCrossMeshReprojectPlug() ||
		input == keepLastValidOutputPlug() ||
		input == strictUnresolvedPlug() ||
		input == debugColorPlug()
	)
	{
		outputs.push_back( cacheVersionPlug() );
		outputs.push_back( resolvedPointCountPlug() );
		outputs.push_back( unresolvedPointCountPlug() );
		outputs.push_back( lastValidFramePlug() );
		outputs.push_back( invalidPointCountPlug() );
		outputs.push_back( invalidStrokeCountPlug() );
		outputs.push_back( failingFramePlug() );
		outputs.push_back( failingTargetPathsPlug() );
		outputs.push_back( attachmentFailureReasonsPlug() );
		outputs.push_back( topologyMismatchCountPlug() );
		outputs.push_back( solveStatusPlug() );
		outputs.push_back( validationCategoriesPlug() );
		outputs.push_back( outPlug()->objectPlug() );
		outputs.push_back( outPlug()->childNamesPlug() );
		outputs.push_back( outPlug()->boundPlug() );
	}

	if( sceneInputChanged || input == outputLocationPlug() )
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
