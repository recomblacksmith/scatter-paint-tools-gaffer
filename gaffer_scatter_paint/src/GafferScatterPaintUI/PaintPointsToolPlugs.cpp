#include "PaintPointsToolPrivate.h"

#include <cstdlib>

using namespace Gaffer;
using namespace GafferScatterPaintUI;
using namespace GafferScatterPaintUI::PaintPointsToolPrivate;
using namespace GafferSceneUI;
using namespace IECoreScene;
namespace bp = boost::python;

namespace GafferScatterPaintUI
{

namespace PaintPointsToolPrivate
{

#include "PaintPointsToolLoggingUtils.h"
#include "PaintPointsToolNodeUtils.h"
#include "PaintPointsToolSelectionUtils.h"
#include "PaintPointsToolEditUtils.h"
#include "PaintPointsToolSampleUtils.h"
#include "PaintPointsToolGeometryUtils.h"

} // namespace PaintPointsToolPrivate

} // namespace GafferScatterPaintUI

IntPlug *PaintPointsTool::modePlug() { return getChild<IntPlug>( g_firstPlugIndex ); }
const IntPlug *PaintPointsTool::modePlug() const { return getChild<IntPlug>( g_firstPlugIndex ); }
FloatPlug *PaintPointsTool::brushSizePlug() { return getChild<FloatPlug>( g_firstPlugIndex + 1 ); }
const FloatPlug *PaintPointsTool::brushSizePlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 1 ); }
IntPlug *PaintPointsTool::pointsPlug() { return getChild<IntPlug>( g_firstPlugIndex + 2 ); }
const IntPlug *PaintPointsTool::pointsPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 2 ); }
FloatPlug *PaintPointsTool::densityPlug() { return getChild<FloatPlug>( g_firstPlugIndex + 3 ); }
const FloatPlug *PaintPointsTool::densityPlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 3 ); }
FloatPlug *PaintPointsTool::softnessPlug() { return getChild<FloatPlug>( g_firstPlugIndex + 4 ); }
const FloatPlug *PaintPointsTool::softnessPlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 4 ); }
FloatPlug *PaintPointsTool::spacingPlug() { return getChild<FloatPlug>( g_firstPlugIndex + 5 ); }
const FloatPlug *PaintPointsTool::spacingPlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 5 ); }
IntPlug *PaintPointsTool::rotationModePlug() { return getChild<IntPlug>( g_firstPlugIndex + 6 ); }
const IntPlug *PaintPointsTool::rotationModePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 6 ); }
FloatPlug *PaintPointsTool::scaleJitterPlug() { return getChild<FloatPlug>( g_firstPlugIndex + 7 ); }
const FloatPlug *PaintPointsTool::scaleJitterPlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 7 ); }
FloatPlug *PaintPointsTool::widthJitterPlug() { return getChild<FloatPlug>( g_firstPlugIndex + 8 ); }
const FloatPlug *PaintPointsTool::widthJitterPlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 8 ); }
IntPlug *PaintPointsTool::frameModePlug() { return getChild<IntPlug>( g_firstPlugIndex + 9 ); }
const IntPlug *PaintPointsTool::frameModePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 9 ); }
IntPlug *PaintPointsTool::frameStartPlug() { return getChild<IntPlug>( g_firstPlugIndex + 10 ); }
const IntPlug *PaintPointsTool::frameStartPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 10 ); }
IntPlug *PaintPointsTool::frameEndPlug() { return getChild<IntPlug>( g_firstPlugIndex + 11 ); }
const IntPlug *PaintPointsTool::frameEndPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 11 ); }
BoolPlug *PaintPointsTool::muteBehaviorPlug() { return getChild<BoolPlug>( g_firstPlugIndex + 12 ); }
const BoolPlug *PaintPointsTool::muteBehaviorPlug() const { return getChild<BoolPlug>( g_firstPlugIndex + 12 ); }
BoolPlug *PaintPointsTool::soloBehaviorPlug() { return getChild<BoolPlug>( g_firstPlugIndex + 13 ); }
const BoolPlug *PaintPointsTool::soloBehaviorPlug() const { return getChild<BoolPlug>( g_firstPlugIndex + 13 ); }
IntPlug *PaintPointsTool::relaxObjectivePlug() { return getChild<IntPlug>( g_firstPlugIndex + 14 ); }
const IntPlug *PaintPointsTool::relaxObjectivePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 14 ); }
StringPlug *PaintPointsTool::targetFilterPlug() { return getChild<StringPlug>( g_firstPlugIndex + 15 ); }
const StringPlug *PaintPointsTool::targetFilterPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 15 ); }
StringPlug *PaintPointsTool::targetSetFilterPlug() { return getChild<StringPlug>( g_firstPlugIndex + 16 ); }
const StringPlug *PaintPointsTool::targetSetFilterPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 16 ); }
IntPlug *PaintPointsTool::surfaceModePlug() { return getChild<IntPlug>( g_firstPlugIndex + 17 ); }
const IntPlug *PaintPointsTool::surfaceModePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 17 ); }
IntPlug *PaintPointsTool::paintThroughModePlug() { return getChild<IntPlug>( g_firstPlugIndex + 18 ); }
const IntPlug *PaintPointsTool::paintThroughModePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 18 ); }
IntPlug *PaintPointsTool::globalModePrecedencePlug() { return getChild<IntPlug>( g_firstPlugIndex + 19 ); }
const IntPlug *PaintPointsTool::globalModePrecedencePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 19 ); }
BoolPlug *PaintPointsTool::pressureDefaultsEnabledPlug() { return getChild<BoolPlug>( g_firstPlugIndex + 20 ); }
const BoolPlug *PaintPointsTool::pressureDefaultsEnabledPlug() const { return getChild<BoolPlug>( g_firstPlugIndex + 20 ); }
IntPlug *PaintPointsTool::pressureDefaultsMappingModePlug() { return getChild<IntPlug>( g_firstPlugIndex + 21 ); }
const IntPlug *PaintPointsTool::pressureDefaultsMappingModePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 21 ); }
ObjectPlug *PaintPointsTool::pressureDefaultsDensityCurvePlug() { return getChild<ObjectPlug>( g_firstPlugIndex + 22 ); }
const ObjectPlug *PaintPointsTool::pressureDefaultsDensityCurvePlug() const { return getChild<ObjectPlug>( g_firstPlugIndex + 22 ); }
ObjectPlug *PaintPointsTool::pressureDefaultsSoftnessCurvePlug() { return getChild<ObjectPlug>( g_firstPlugIndex + 23 ); }
const ObjectPlug *PaintPointsTool::pressureDefaultsSoftnessCurvePlug() const { return getChild<ObjectPlug>( g_firstPlugIndex + 23 ); }
IntPlug *PaintPointsTool::eraseSpacePlug() { return getChild<IntPlug>( g_firstPlugIndex + 24 ); }
const IntPlug *PaintPointsTool::eraseSpacePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 24 ); }
FloatPlug *PaintPointsTool::pressureValuePlug() { return getChild<FloatPlug>( g_firstPlugIndex + 25 ); }
const FloatPlug *PaintPointsTool::pressureValuePlug() const { return getChild<FloatPlug>( g_firstPlugIndex + 25 ); }
StringPlug *PaintPointsTool::targetNodePlug() { return getChild<StringPlug>( g_firstPlugIndex + 26 ); }
const StringPlug *PaintPointsTool::targetNodePlug() const { return getChild<StringPlug>( g_firstPlugIndex + 26 ); }
StringPlug *PaintPointsTool::layerNamePlug() { return getChild<StringPlug>( g_firstPlugIndex + 27 ); }
const StringPlug *PaintPointsTool::layerNamePlug() const { return getChild<StringPlug>( g_firstPlugIndex + 27 ); }
StringPlug *PaintPointsTool::strokeNamePlug() { return getChild<StringPlug>( g_firstPlugIndex + 28 ); }
const StringPlug *PaintPointsTool::strokeNamePlug() const { return getChild<StringPlug>( g_firstPlugIndex + 28 ); }
IntPlug *PaintPointsTool::layerEditActionPlug() { return getChild<IntPlug>( g_firstPlugIndex + 29 ); }
const IntPlug *PaintPointsTool::layerEditActionPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 29 ); }
BoolPlug *PaintPointsTool::layerVisiblePlug() { return getChild<BoolPlug>( g_firstPlugIndex + 30 ); }
const BoolPlug *PaintPointsTool::layerVisiblePlug() const { return getChild<BoolPlug>( g_firstPlugIndex + 30 ); }
BoolPlug *PaintPointsTool::layerMutePlug() { return getChild<BoolPlug>( g_firstPlugIndex + 31 ); }
const BoolPlug *PaintPointsTool::layerMutePlug() const { return getChild<BoolPlug>( g_firstPlugIndex + 31 ); }
BoolPlug *PaintPointsTool::layerSoloPlug() { return getChild<BoolPlug>( g_firstPlugIndex + 32 ); }
const BoolPlug *PaintPointsTool::layerSoloPlug() const { return getChild<BoolPlug>( g_firstPlugIndex + 32 ); }
IntPlug *PaintPointsTool::layerFrameStartPlug() { return getChild<IntPlug>( g_firstPlugIndex + 33 ); }
const IntPlug *PaintPointsTool::layerFrameStartPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 33 ); }
IntPlug *PaintPointsTool::layerFrameEndPlug() { return getChild<IntPlug>( g_firstPlugIndex + 34 ); }
const IntPlug *PaintPointsTool::layerFrameEndPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 34 ); }
IntPlug *PaintPointsTool::layerMoveToIndexPlug() { return getChild<IntPlug>( g_firstPlugIndex + 35 ); }
const IntPlug *PaintPointsTool::layerMoveToIndexPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 35 ); }
IntPlug *PaintPointsTool::layerModePlug() { return getChild<IntPlug>( g_firstPlugIndex + 36 ); }
const IntPlug *PaintPointsTool::layerModePlug() const { return getChild<IntPlug>( g_firstPlugIndex + 36 ); }
IntPlug *PaintPointsTool::strokeEditActionPlug() { return getChild<IntPlug>( g_firstPlugIndex + 37 ); }
const IntPlug *PaintPointsTool::strokeEditActionPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 37 ); }
IntPlug *PaintPointsTool::strokeMoveToIndexPlug() { return getChild<IntPlug>( g_firstPlugIndex + 38 ); }
const IntPlug *PaintPointsTool::strokeMoveToIndexPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 38 ); }
StringPlug *PaintPointsTool::strokeMergeTargetPlug() { return getChild<StringPlug>( g_firstPlugIndex + 39 ); }
const StringPlug *PaintPointsTool::strokeMergeTargetPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 39 ); }
IntPlug *PaintPointsTool::previewCountPlug() { return getChild<IntPlug>( g_firstPlugIndex + 40 ); }
const IntPlug *PaintPointsTool::previewCountPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 40 ); }
IntPlug *PaintPointsTool::primedPointCountPlug() { return getChild<IntPlug>( g_firstPlugIndex + 41 ); }
const IntPlug *PaintPointsTool::primedPointCountPlug() const { return getChild<IntPlug>( g_firstPlugIndex + 41 ); }
StringPlug *PaintPointsTool::statusPlug() { return getChild<StringPlug>( g_firstPlugIndex + 42 ); }
const StringPlug *PaintPointsTool::statusPlug() const { return getChild<StringPlug>( g_firstPlugIndex + 42 ); }
