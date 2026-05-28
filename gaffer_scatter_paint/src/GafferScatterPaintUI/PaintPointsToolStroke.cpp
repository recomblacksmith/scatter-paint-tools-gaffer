#include "PaintPointsToolPrivate.h"

#include "GafferScatterPaint/PaintedPoints.h"

#include <chrono>

using namespace Gaffer;
using namespace GafferScatterPaint;
using namespace GafferScatterPaintUI;
using namespace GafferScatterPaintUI::PaintPointsToolPrivate;
using namespace GafferSceneUI;
using namespace IECoreScene;
namespace bp = boost::python;

namespace
{

using Clock = std::chrono::steady_clock;

double elapsedMilliseconds( const Clock::time_point &start )
{
	return std::chrono::duration<double, std::milli>( Clock::now() - start ).count();
}

}

size_t PaintPointsTool::commitStrokePoints( const StrokePoints &strokePoints )
{
	const auto totalStart = Clock::now();
	ScriptNode *script = toolScriptNode( this );
	if( !script )
	{
		logWarning( "Stroke commit requested without a ScriptNode." );
		statusPlug()->setValue( "Paint mode stroke commit failed: no ScriptNode available." );
		return 0;
	}

	IECorePython::ScopedGILLock gilLock;
	Node *node = nullptr;
	size_t committed = 0;
	int resolved = 0;
	int fallback = 0;
	double targetResolveMs = 0.0;
	double mirroredSyncMs = 0.0;
	double ensureLayerStrokeMs = 0.0;
		double beforeDiagnosticsMs = 0.0;
		double sampleBuildMs = 0.0;
		double brushPaintMs = 0.0;
		double paintCommitMs = 0.0;
		double afterCommitDiagnosticsMs = 0.0;
	double loadSchemaMs = 0.0;
	double loadResolvePathMs = 0.0;
	double loadReadBytesMs = 0.0;
	double loadBlobExtractMs = 0.0;
	double loadUnpackMs = 0.0;
	double loadPopulateMetadataMs = 0.0;
	double unpackChecksumMs = 0.0;
	double unpackHeaderMs = 0.0;
	double unpackNodeMs = 0.0;
	double unpackLockMs = 0.0;
	double unpackScenePathsMs = 0.0;
	double unpackLayersMs = 0.0;
	double unpackStrokesMs = 0.0;
	double unpackChunksMs = 0.0;
	double unpackPointsMs = 0.0;
	double unpackSelectionSetsMs = 0.0;
	double unpackDiagnosticsMs = 0.0;
	double unpackUpgradesMs = 0.0;
	double unpackPointBackupsMs = 0.0;
	double storeBuildMs = 0.0;
	double schemaDictMs = 0.0;
	double nodeMetadataMs = 0.0;
	double pythonSetupMs = 0.0;
	double surfaceCandidatesMs = 0.0;
	double rebuildMs = 0.0;
	double trustedDiagnosticsMs = 0.0;
	double writeMs = 0.0;
	double writePopulateMetadataMs = 0.0;
	double writeLockMs = 0.0;
	double writePackMs = 0.0;
	double packHeaderMs = 0.0;
	double packNodeMs = 0.0;
	double packLockMs = 0.0;
	double packScenePathsMs = 0.0;
	double packLayersMs = 0.0;
	double packStrokesMs = 0.0;
	double packChunksMs = 0.0;
	double packPointsMs = 0.0;
	double packPointsReserveMs = 0.0;
	double packPointsReserveReallocated = 0.0;
	double packPointsCapacityBeforeBytes = 0.0;
	double packPointsCapacityAfterBytes = 0.0;
	double packPointsResizeMs = 0.0;
	double packPointsFillMs = 0.0;
	double packPointsCopyMs = 0.0;
	double packPointsChecksumInlineMs = 0.0;
	double packPointsChecksumMs = 0.0;
	double packSelectionSetsMs = 0.0;
	double packDiagnosticsMs = 0.0;
	double packUpgradesMs = 0.0;
	double packPointBackupsMs = 0.0;
	double packFinalChecksumMs = 0.0;
	double packFinalHeaderWriteMs = 0.0;
	double packFinalBufferMs = 0.0;
	double writeResolvePathMs = 0.0;
	double writeBackupMs = 0.0;
	double writeBytesMs = 0.0;
	double writeClearBlobMs = 0.0;
	double writeReleaseLockMs = 0.0;
	double writeBlobObjectMs = 0.0;
	double writeBlobObjectResizeMs = 0.0;
	double writeBlobObjectCopyMs = 0.0;
	double writeBlobPlugSetMs = 0.0;
	double writeSetBlobMs = 0.0;
	double syncMs = 0.0;
	double sampleDictExtractMs = 0.0;
	double sampleCompileMs = 0.0;
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
	int inputSampleCount = 0;
	int resolvedSampleCount = 0;
	int fallbackSampleCount = 0;
	int surfacePointCountTotal = 0;
	int surfacePointCountMax = 0;
	int surfaceCandidateCount = 0;
		int surfaceSamplesZeroHits = 0;
		int surfaceSamplesOneHit = 0;
		int surfaceSamplesManyHits = 0;
		bool filteredMode = false;
		std::string simdBackend = "unknown";
	double overlayMs = 0.0;
	{
		UndoScope undoScope( script );
		const auto targetResolveStart = Clock::now();
		node = targetNodeOrCreate( this, script );
		targetResolveMs = elapsedMilliseconds( targetResolveStart );
		if( !node )
		{
			logWarning( "Stroke commit failed because no PaintedPoints target could be found or created." );
			statusPlug()->setValue( "Paint mode stroke commit failed: no PaintedPoints target was available." );
			return 0;
		}
		const auto mirroredSyncStart = Clock::now();
		syncMirroredControlsToNode( node );
		mirroredSyncMs = elapsedMilliseconds( mirroredSyncStart );

		const auto ensureLayerStrokeStart = Clock::now();
		const auto ids = ensureLayerAndStroke( this, node );
		ensureLayerStrokeMs = elapsedMilliseconds( ensureLayerStrokeStart );
		const auto beforeDiagnosticsStart = Clock::now();
		logRefreshDiagnostics( this, script, node, "beforeStrokeCommit" );
		beforeDiagnosticsMs = elapsedMilliseconds( beforeDiagnosticsStart );
		logInfo(
			sequenceTag( "COMMIT" ) + "Committing drag stroke to node=" + node->getName().string() +
			" layerId=" + std::to_string( ids.first ) +
			" strokeId=" + std::to_string( ids.second ) +
			" pointCount=" + std::to_string( strokePoints.size() )
		);
		const auto sampleBuildStart = Clock::now();
		BrushSampleDataList samples = brushSampleDataFromStroke( this, strokePoints, &sampleBuildBaseDecodeMs );
		samples = expandBrushSampleData( this, samples, &sampleBuildJitterMs );
		sampleBuildConstructMs = 0.0;
		sampleBuildMs = elapsedMilliseconds( sampleBuildStart );
		const auto brushPaintStart = Clock::now();
		bp::dict paintResult;
		if( PaintedPoints *paintedPoints = IECore::runTimeCast<PaintedPoints>( node ) )
		{
			paintResult = bp::extract<bp::dict>( paintedPoints->brushPaintCommitTyped( bp::object( ids.second ), samples, true ) );
		}
		else
		{
			bp::object pythonToolNode = pythonNode( node );
			paintResult = bp::extract<bp::dict>(
				pythonToolNode.attr( "brushPaintCommit" )( bp::object( ids.second ), brushSamplesToPythonList( samples ), true )
			);
		}
		brushPaintMs = elapsedMilliseconds( brushPaintStart );
		const double authoredBuildMs = bp::extract<double>( paintResult.get( "authoredBuildMs", bp::object( 0.0 ) ) );
		loadSchemaMs = bp::extract<double>( paintResult.get( "loadSchemaMs", bp::object( 0.0 ) ) );
		loadResolvePathMs = bp::extract<double>( paintResult.get( "loadResolvePathMs", bp::object( 0.0 ) ) );
		loadReadBytesMs = bp::extract<double>( paintResult.get( "loadReadBytesMs", bp::object( 0.0 ) ) );
		loadBlobExtractMs = bp::extract<double>( paintResult.get( "loadBlobExtractMs", bp::object( 0.0 ) ) );
		loadUnpackMs = bp::extract<double>( paintResult.get( "loadUnpackMs", bp::object( 0.0 ) ) );
		loadPopulateMetadataMs = bp::extract<double>( paintResult.get( "loadPopulateMetadataMs", bp::object( 0.0 ) ) );
		unpackChecksumMs = bp::extract<double>( paintResult.get( "unpackChecksumMs", bp::object( 0.0 ) ) );
		unpackHeaderMs = bp::extract<double>( paintResult.get( "unpackHeaderMs", bp::object( 0.0 ) ) );
		unpackNodeMs = bp::extract<double>( paintResult.get( "unpackNodeMs", bp::object( 0.0 ) ) );
		unpackLockMs = bp::extract<double>( paintResult.get( "unpackLockMs", bp::object( 0.0 ) ) );
		unpackScenePathsMs = bp::extract<double>( paintResult.get( "unpackScenePathsMs", bp::object( 0.0 ) ) );
		unpackLayersMs = bp::extract<double>( paintResult.get( "unpackLayersMs", bp::object( 0.0 ) ) );
		unpackStrokesMs = bp::extract<double>( paintResult.get( "unpackStrokesMs", bp::object( 0.0 ) ) );
		unpackChunksMs = bp::extract<double>( paintResult.get( "unpackChunksMs", bp::object( 0.0 ) ) );
		unpackPointsMs = bp::extract<double>( paintResult.get( "unpackPointsMs", bp::object( 0.0 ) ) );
		unpackSelectionSetsMs = bp::extract<double>( paintResult.get( "unpackSelectionSetsMs", bp::object( 0.0 ) ) );
		unpackDiagnosticsMs = bp::extract<double>( paintResult.get( "unpackDiagnosticsMs", bp::object( 0.0 ) ) );
		unpackUpgradesMs = bp::extract<double>( paintResult.get( "unpackUpgradesMs", bp::object( 0.0 ) ) );
		unpackPointBackupsMs = bp::extract<double>( paintResult.get( "unpackPointBackupsMs", bp::object( 0.0 ) ) );
		storeBuildMs = bp::extract<double>( paintResult.get( "storeBuildMs", bp::object( 0.0 ) ) );
		schemaDictMs = bp::extract<double>( paintResult.get( "schemaDictMs", bp::object( 0.0 ) ) );
		nodeMetadataMs = bp::extract<double>( paintResult.get( "nodeMetadataMs", bp::object( 0.0 ) ) );
		pythonSetupMs = bp::extract<double>( paintResult.get( "pythonSetupMs", bp::object( 0.0 ) ) );
		surfaceCandidatesMs = bp::extract<double>( paintResult.get( "surfaceCandidatesMs", bp::object( 0.0 ) ) );
		rebuildMs = bp::extract<double>( paintResult.get( "rebuildMs", bp::object( 0.0 ) ) );
		trustedDiagnosticsMs = bp::extract<double>( paintResult.get( "diagnosticsMs", bp::object( 0.0 ) ) );
		writeMs = bp::extract<double>( paintResult.get( "writeMs", bp::object( 0.0 ) ) );
		writePopulateMetadataMs = bp::extract<double>( paintResult.get( "writePopulateMetadataMs", bp::object( 0.0 ) ) );
		writeLockMs = bp::extract<double>( paintResult.get( "writeLockMs", bp::object( 0.0 ) ) );
		writePackMs = bp::extract<double>( paintResult.get( "writePackMs", bp::object( 0.0 ) ) );
		packHeaderMs = bp::extract<double>( paintResult.get( "packHeaderMs", bp::object( 0.0 ) ) );
		packNodeMs = bp::extract<double>( paintResult.get( "packNodeMs", bp::object( 0.0 ) ) );
		packLockMs = bp::extract<double>( paintResult.get( "packLockMs", bp::object( 0.0 ) ) );
		packScenePathsMs = bp::extract<double>( paintResult.get( "packScenePathsMs", bp::object( 0.0 ) ) );
		packLayersMs = bp::extract<double>( paintResult.get( "packLayersMs", bp::object( 0.0 ) ) );
		packStrokesMs = bp::extract<double>( paintResult.get( "packStrokesMs", bp::object( 0.0 ) ) );
		packChunksMs = bp::extract<double>( paintResult.get( "packChunksMs", bp::object( 0.0 ) ) );
		packPointsMs = bp::extract<double>( paintResult.get( "packPointsMs", bp::object( 0.0 ) ) );
		packPointsReserveMs = bp::extract<double>( paintResult.get( "packPointsReserveMs", bp::object( 0.0 ) ) );
		packPointsReserveReallocated = bp::extract<double>( paintResult.get( "packPointsReserveReallocated", bp::object( 0.0 ) ) );
		packPointsCapacityBeforeBytes = bp::extract<double>( paintResult.get( "packPointsCapacityBeforeBytes", bp::object( 0.0 ) ) );
		packPointsCapacityAfterBytes = bp::extract<double>( paintResult.get( "packPointsCapacityAfterBytes", bp::object( 0.0 ) ) );
		packPointsResizeMs = bp::extract<double>( paintResult.get( "packPointsResizeMs", bp::object( 0.0 ) ) );
		packPointsFillMs = bp::extract<double>( paintResult.get( "packPointsFillMs", bp::object( 0.0 ) ) );
		packPointsCopyMs = bp::extract<double>( paintResult.get( "packPointsCopyMs", bp::object( 0.0 ) ) );
		packPointsChecksumInlineMs = bp::extract<double>( paintResult.get( "packPointsChecksumInlineMs", bp::object( 0.0 ) ) );
		packPointsChecksumMs = bp::extract<double>( paintResult.get( "packPointsChecksumMs", bp::object( 0.0 ) ) );
		packSelectionSetsMs = bp::extract<double>( paintResult.get( "packSelectionSetsMs", bp::object( 0.0 ) ) );
		packDiagnosticsMs = bp::extract<double>( paintResult.get( "packDiagnosticsMs", bp::object( 0.0 ) ) );
		packUpgradesMs = bp::extract<double>( paintResult.get( "packUpgradesMs", bp::object( 0.0 ) ) );
		packPointBackupsMs = bp::extract<double>( paintResult.get( "packPointBackupsMs", bp::object( 0.0 ) ) );
		packFinalChecksumMs = bp::extract<double>( paintResult.get( "packFinalChecksumMs", bp::object( 0.0 ) ) );
		packFinalHeaderWriteMs = bp::extract<double>( paintResult.get( "packFinalHeaderWriteMs", bp::object( 0.0 ) ) );
		packFinalBufferMs = bp::extract<double>( paintResult.get( "packFinalBufferMs", bp::object( 0.0 ) ) );
		writeResolvePathMs = bp::extract<double>( paintResult.get( "writeResolvePathMs", bp::object( 0.0 ) ) );
		writeBackupMs = bp::extract<double>( paintResult.get( "writeBackupMs", bp::object( 0.0 ) ) );
		writeBytesMs = bp::extract<double>( paintResult.get( "writeBytesMs", bp::object( 0.0 ) ) );
		writeClearBlobMs = bp::extract<double>( paintResult.get( "writeClearBlobMs", bp::object( 0.0 ) ) );
		writeReleaseLockMs = bp::extract<double>( paintResult.get( "writeReleaseLockMs", bp::object( 0.0 ) ) );
		writeBlobObjectMs = bp::extract<double>( paintResult.get( "writeBlobObjectMs", bp::object( 0.0 ) ) );
		writeBlobObjectResizeMs = bp::extract<double>( paintResult.get( "writeBlobObjectResizeMs", bp::object( 0.0 ) ) );
		writeBlobObjectCopyMs = bp::extract<double>( paintResult.get( "writeBlobObjectCopyMs", bp::object( 0.0 ) ) );
		writeBlobPlugSetMs = bp::extract<double>( paintResult.get( "writeBlobPlugSetMs", bp::object( 0.0 ) ) );
		writeSetBlobMs = bp::extract<double>( paintResult.get( "writeSetBlobMs", bp::object( 0.0 ) ) );
		syncMs = bp::extract<double>( paintResult.get( "syncMs", bp::object( 0.0 ) ) );
		sampleDictExtractMs = bp::extract<double>( paintResult.get( "sampleDictExtractMs", bp::object( 0.0 ) ) );
		sampleCompileMs = bp::extract<double>( paintResult.get( "sampleCompileMs", bp::object( 0.0 ) ) );
		sampleExtractMs = bp::extract<double>( paintResult.get( "sampleExtractMs", bp::object( 0.0 ) ) );
		surfaceResolveMs = bp::extract<double>( paintResult.get( "surfaceResolveMs", bp::object( 0.0 ) ) );
		surfaceResolveListMs = bp::extract<double>( paintResult.get( "surfaceResolveListMs", bp::object( 0.0 ) ) );
		surfaceResolveCountMs = bp::extract<double>( paintResult.get( "surfaceResolveCountMs", bp::object( 0.0 ) ) );
		surfaceResolveCandidateMs = bp::extract<double>( paintResult.get( "surfaceResolveCandidateMs", bp::object( 0.0 ) ) );
		surfaceResolveHitSearchMs = bp::extract<double>( paintResult.get( "surfaceResolveHitSearchMs", bp::object( 0.0 ) ) );
		surfaceResolveHitBuildMs = bp::extract<double>( paintResult.get( "surfaceResolveHitBuildMs", bp::object( 0.0 ) ) );
		fallbackAuthorMs = bp::extract<double>( paintResult.get( "fallbackAuthorMs", bp::object( 0.0 ) ) );
		fallbackPythonAuthorMs = bp::extract<double>( paintResult.get( "fallbackPythonAuthorMs", bp::object( 0.0 ) ) );
		fallbackRecordConvertMs = bp::extract<double>( paintResult.get( "fallbackRecordConvertMs", bp::object( 0.0 ) ) );
		recordBuildMs = bp::extract<double>( paintResult.get( "recordBuildMs", bp::object( 0.0 ) ) );
		recordBuildPythonAuthorMs = bp::extract<double>( paintResult.get( "recordBuildPythonAuthorMs", bp::object( 0.0 ) ) );
		recordBuildConvertMs = bp::extract<double>( paintResult.get( "recordBuildConvertMs", bp::object( 0.0 ) ) );
		resultPackMs = bp::extract<double>( paintResult.get( "resultPackMs", bp::object( 0.0 ) ) );
		inputSampleCount = bp::extract<int>( paintResult.get( "inputSampleCount", bp::object( 0 ) ) );
		resolvedSampleCount = bp::extract<int>( paintResult.get( "resolvedSampleCount", bp::object( 0 ) ) );
		fallbackSampleCount = bp::extract<int>( paintResult.get( "fallbackSampleCount", bp::object( 0 ) ) );
		surfacePointCountTotal = bp::extract<int>( paintResult.get( "surfacePointCountTotal", bp::object( 0 ) ) );
		surfacePointCountMax = bp::extract<int>( paintResult.get( "surfacePointCountMax", bp::object( 0 ) ) );
		surfaceCandidateCount = bp::extract<int>( paintResult.get( "surfaceCandidateCount", bp::object( 0 ) ) );
		surfaceSamplesZeroHits = bp::extract<int>( paintResult.get( "surfaceSamplesZeroHits", bp::object( 0 ) ) );
		surfaceSamplesOneHit = bp::extract<int>( paintResult.get( "surfaceSamplesOneHit", bp::object( 0 ) ) );
		surfaceSamplesManyHits = bp::extract<int>( paintResult.get( "surfaceSamplesManyHits", bp::object( 0 ) ) );
		filteredMode = bp::extract<bool>( paintResult.get( "filteredMode", bp::object( false ) ) );
		simdBackend = bp::extract<std::string>( paintResult.get( "simdBackend", bp::object( std::string( "unknown" ) ) ) );
		paintCommitMs = std::max( 0.0, brushPaintMs - authoredBuildMs );
		brushPaintMs = authoredBuildMs;
		committed = uint64FromObject( paintResult.get( "committed", bp::object() ) );
		resolved = bp::extract<int>( paintResult.get( "resolved", bp::object( 0 ) ) );
		fallback = bp::extract<int>( paintResult.get( "fallback", bp::object( 0 ) ) );
		const std::string fallbackDiagnostics = bp::extract<std::string>( paintResult.get( "fallbackDiagnostics", bp::object( std::string() ) ) );
		logInfo(
			sequenceTag( "COMMIT" ) + "Committed drag stroke to node=" + node->getName().string() +
			" committed=" + std::to_string( committed ) +
			" resolved=" + std::to_string( resolved ) +
			" fallback=" + std::to_string( fallback )
		);
		if( !fallbackDiagnostics.empty() )
		{
			logInfo( sequenceTag( "COMMIT" ) + fallbackDiagnostics );
		}
		const auto afterCommitDiagnosticsStart = Clock::now();
		logRefreshDiagnostics( this, script, node, "afterStrokeCommit" );
		afterCommitDiagnosticsMs = elapsedMilliseconds( afterCommitDiagnosticsStart );
	}
	// UndoScope (which IS-A DirtyPropagationScope) has now destructed,
	// so all dirty propagation has been flushed before we wait on the gadget.
	const auto refreshStart = Clock::now();
	refreshSceneGadget( "stroke commit" );
	const double refreshMs = elapsedMilliseconds( refreshStart );
	const auto overlayStart = Clock::now();
	std::optional<int> knownPointCount;
	if( node == m_cachedPointCountNode && m_cachedPointCount )
	{
		knownPointCount = *m_cachedPointCount + static_cast<int>( committed );
	}
	refreshPointCountOverlay( node, knownPointCount );
	overlayMs = elapsedMilliseconds( overlayStart );
	logInfo(
		sequenceTag( "TIMING" ) + "stroke commit totalMs=" + std::to_string( elapsedMilliseconds( totalStart ) ) +
		" targetResolveMs=" + std::to_string( targetResolveMs ) +
		" mirroredSyncMs=" + std::to_string( mirroredSyncMs ) +
		" ensureLayerStrokeMs=" + std::to_string( ensureLayerStrokeMs ) +
		" beforeDiagnosticsMs=" + std::to_string( beforeDiagnosticsMs ) +
		" sampleBuildMs=" + std::to_string( sampleBuildMs ) +
		" sampleBuildBaseDecodeMs=" + std::to_string( sampleBuildBaseDecodeMs ) +
		" sampleBuildJitterMs=" + std::to_string( sampleBuildJitterMs ) +
		" sampleBuildConstructMs=" + std::to_string( sampleBuildConstructMs ) +
		" brushPaintMs=" + std::to_string( brushPaintMs ) +
		" paintStrokeCommitMs=" + std::to_string( paintCommitMs ) +
		" loadSchemaMs=" + std::to_string( loadSchemaMs ) +
		" loadResolvePathMs=" + std::to_string( loadResolvePathMs ) +
		" loadReadBytesMs=" + std::to_string( loadReadBytesMs ) +
		" loadBlobExtractMs=" + std::to_string( loadBlobExtractMs ) +
		" loadUnpackMs=" + std::to_string( loadUnpackMs ) +
		" loadPopulateMetadataMs=" + std::to_string( loadPopulateMetadataMs ) +
		" unpackChecksumMs=" + std::to_string( unpackChecksumMs ) +
		" unpackHeaderMs=" + std::to_string( unpackHeaderMs ) +
		" unpackNodeMs=" + std::to_string( unpackNodeMs ) +
		" unpackLockMs=" + std::to_string( unpackLockMs ) +
		" unpackScenePathsMs=" + std::to_string( unpackScenePathsMs ) +
		" unpackLayersMs=" + std::to_string( unpackLayersMs ) +
		" unpackStrokesMs=" + std::to_string( unpackStrokesMs ) +
		" unpackChunksMs=" + std::to_string( unpackChunksMs ) +
		" unpackPointsMs=" + std::to_string( unpackPointsMs ) +
		" unpackSelectionSetsMs=" + std::to_string( unpackSelectionSetsMs ) +
		" unpackDiagnosticsMs=" + std::to_string( unpackDiagnosticsMs ) +
		" unpackUpgradesMs=" + std::to_string( unpackUpgradesMs ) +
		" unpackPointBackupsMs=" + std::to_string( unpackPointBackupsMs ) +
		" storeBuildMs=" + std::to_string( storeBuildMs ) +
		" schemaDictMs=" + std::to_string( schemaDictMs ) +
		" nodeMetadataMs=" + std::to_string( nodeMetadataMs ) +
		" pythonSetupMs=" + std::to_string( pythonSetupMs ) +
		" surfaceCandidatesMs=" + std::to_string( surfaceCandidatesMs ) +
		" rebuildMs=" + std::to_string( rebuildMs ) +
		" trustedDiagnosticsMs=" + std::to_string( trustedDiagnosticsMs ) +
		" writeMs=" + std::to_string( writeMs ) +
		" writePopulateMetadataMs=" + std::to_string( writePopulateMetadataMs ) +
		" writeLockMs=" + std::to_string( writeLockMs ) +
		" writePackMs=" + std::to_string( writePackMs ) +
		" packHeaderMs=" + std::to_string( packHeaderMs ) +
		" packNodeMs=" + std::to_string( packNodeMs ) +
		" packLockMs=" + std::to_string( packLockMs ) +
		" packScenePathsMs=" + std::to_string( packScenePathsMs ) +
		" packLayersMs=" + std::to_string( packLayersMs ) +
		" packStrokesMs=" + std::to_string( packStrokesMs ) +
		" packChunksMs=" + std::to_string( packChunksMs ) +
		" packPointsMs=" + std::to_string( packPointsMs ) +
		" packPointsReserveMs=" + std::to_string( packPointsReserveMs ) +
		" packPointsReserveReallocated=" + std::to_string( packPointsReserveReallocated ) +
		" packPointsCapacityBeforeBytes=" + std::to_string( packPointsCapacityBeforeBytes ) +
		" packPointsCapacityAfterBytes=" + std::to_string( packPointsCapacityAfterBytes ) +
		" packPointsResizeMs=" + std::to_string( packPointsResizeMs ) +
		" packPointsFillMs=" + std::to_string( packPointsFillMs ) +
		" packPointsCopyMs=" + std::to_string( packPointsCopyMs ) +
		" packPointsChecksumInlineMs=" + std::to_string( packPointsChecksumInlineMs ) +
		" packPointsChecksumMs=" + std::to_string( packPointsChecksumMs ) +
		" packSelectionSetsMs=" + std::to_string( packSelectionSetsMs ) +
		" packDiagnosticsMs=" + std::to_string( packDiagnosticsMs ) +
		" packUpgradesMs=" + std::to_string( packUpgradesMs ) +
		" packPointBackupsMs=" + std::to_string( packPointBackupsMs ) +
		" packFinalChecksumMs=" + std::to_string( packFinalChecksumMs ) +
		" packFinalHeaderWriteMs=" + std::to_string( packFinalHeaderWriteMs ) +
		" packFinalBufferMs=" + std::to_string( packFinalBufferMs ) +
		" writeResolvePathMs=" + std::to_string( writeResolvePathMs ) +
		" writeBackupMs=" + std::to_string( writeBackupMs ) +
		" writeBytesMs=" + std::to_string( writeBytesMs ) +
		" writeClearBlobMs=" + std::to_string( writeClearBlobMs ) +
		" writeReleaseLockMs=" + std::to_string( writeReleaseLockMs ) +
		" writeBlobObjectMs=" + std::to_string( writeBlobObjectMs ) +
		" writeBlobObjectResizeMs=" + std::to_string( writeBlobObjectResizeMs ) +
		" writeBlobObjectCopyMs=" + std::to_string( writeBlobObjectCopyMs ) +
		" writeBlobPlugSetMs=" + std::to_string( writeBlobPlugSetMs ) +
		" writeSetBlobMs=" + std::to_string( writeSetBlobMs ) +
		" syncMs=" + std::to_string( syncMs ) +
		" sampleDictExtractMs=" + std::to_string( sampleDictExtractMs ) +
		" sampleCompileMs=" + std::to_string( sampleCompileMs ) +
		" sampleExtractMs=" + std::to_string( sampleExtractMs ) +
		" surfaceResolveMs=" + std::to_string( surfaceResolveMs ) +
		" surfaceResolveListMs=" + std::to_string( surfaceResolveListMs ) +
		" surfaceResolveCountMs=" + std::to_string( surfaceResolveCountMs ) +
		" surfaceResolveCandidateMs=" + std::to_string( surfaceResolveCandidateMs ) +
		" surfaceResolveHitSearchMs=" + std::to_string( surfaceResolveHitSearchMs ) +
		" surfaceResolveHitBuildMs=" + std::to_string( surfaceResolveHitBuildMs ) +
		" fallbackAuthorMs=" + std::to_string( fallbackAuthorMs ) +
		" fallbackPythonAuthorMs=" + std::to_string( fallbackPythonAuthorMs ) +
		" fallbackRecordConvertMs=" + std::to_string( fallbackRecordConvertMs ) +
		" recordBuildMs=" + std::to_string( recordBuildMs ) +
		" recordBuildPythonAuthorMs=" + std::to_string( recordBuildPythonAuthorMs ) +
		" recordBuildConvertMs=" + std::to_string( recordBuildConvertMs ) +
		" resultPackMs=" + std::to_string( resultPackMs ) +
		" inputSampleCount=" + std::to_string( inputSampleCount ) +
		" resolvedSampleCount=" + std::to_string( resolvedSampleCount ) +
		" fallbackSampleCount=" + std::to_string( fallbackSampleCount ) +
		" surfacePointCountTotal=" + std::to_string( surfacePointCountTotal ) +
		" surfacePointCountMax=" + std::to_string( surfacePointCountMax ) +
		" surfaceCandidateCount=" + std::to_string( surfaceCandidateCount ) +
		" surfaceSamplesZeroHits=" + std::to_string( surfaceSamplesZeroHits ) +
		" surfaceSamplesOneHit=" + std::to_string( surfaceSamplesOneHit ) +
		" surfaceSamplesManyHits=" + std::to_string( surfaceSamplesManyHits ) +
		" simdBackend=" + simdBackend +
		" filteredMode=" + std::string( filteredMode ? "true" : "false" ) +
		" afterCommitDiagnosticsMs=" + std::to_string( afterCommitDiagnosticsMs ) +
		" refreshMs=" + std::to_string( refreshMs ) +
		" overlayMs=" + std::to_string( overlayMs ) +
		" committed=" + std::to_string( committed ) +
		" resolved=" + std::to_string( resolved ) +
		" fallback=" + std::to_string( fallback )
	);
	statusPlug()->setValue(
		"Paint mode committed " + std::to_string( committed ) +
		" points (resolved=" + std::to_string( resolved ) +
		", fallback=" + std::to_string( fallback ) + ")."
	);
	return committed;
}

size_t PaintPointsTool::eraseStrokePoints( const StrokePoints &strokePoints )
{
	const auto totalStart = Clock::now();
	ScriptNode *script = toolScriptNode( this );
	if( !script )
	{
		statusPlug()->setValue( "Erase mode failed: no ScriptNode available." );
		return 0;
	}

	IECorePython::ScopedGILLock gilLock;
	Node *node = findPaintedPointsNode( this, script );
	if( !node )
	{
		statusPlug()->setValue( "Erase mode failed: no PaintedPoints target was found." );
		return 0;
	}

	const float radius = std::max( 0.001f, brushSizePlug()->getValue() );
	bp::object pythonToolNode = pythonNode( node );
	bp::dict eraseResult;
	size_t removed = 0;
	size_t strokeCount = 0;
	double sampleBuildMs = 0.0;
	double brushEraseMs = 0.0;
	double selectionClearMs = 0.0;
	double loadSchemaMs = 0.0;
	double loadResolvePathMs = 0.0;
	double loadReadBytesMs = 0.0;
	double loadBlobExtractMs = 0.0;
	double loadUnpackMs = 0.0;
	double loadPopulateMetadataMs = 0.0;
	double unpackChecksumMs = 0.0;
	double unpackHeaderMs = 0.0;
	double unpackNodeMs = 0.0;
	double unpackLockMs = 0.0;
	double unpackScenePathsMs = 0.0;
	double unpackLayersMs = 0.0;
	double unpackStrokesMs = 0.0;
	double unpackChunksMs = 0.0;
	double unpackPointsMs = 0.0;
	double unpackSelectionSetsMs = 0.0;
	double unpackDiagnosticsMs = 0.0;
	double unpackUpgradesMs = 0.0;
	double unpackPointBackupsMs = 0.0;
	double pointScanMs = 0.0;
	double selectionCleanupMs = 0.0;
	double rebuildMs = 0.0;
	double trustedDiagnosticsMs = 0.0;
	double writeMs = 0.0;
	double writePopulateMetadataMs = 0.0;
	double writeLockMs = 0.0;
	double writePackMs = 0.0;
	double packHeaderMs = 0.0;
	double packNodeMs = 0.0;
	double packLockMs = 0.0;
	double packScenePathsMs = 0.0;
	double packLayersMs = 0.0;
	double packStrokesMs = 0.0;
	double packChunksMs = 0.0;
	double packPointsMs = 0.0;
	double packPointsReserveMs = 0.0;
	double packPointsReserveReallocated = 0.0;
	double packPointsCapacityBeforeBytes = 0.0;
	double packPointsCapacityAfterBytes = 0.0;
	double packPointsResizeMs = 0.0;
	double packPointsFillMs = 0.0;
	double packPointsCopyMs = 0.0;
	double packPointsChecksumInlineMs = 0.0;
	double packPointsChecksumMs = 0.0;
	double packSelectionSetsMs = 0.0;
	double packDiagnosticsMs = 0.0;
	double packUpgradesMs = 0.0;
	double packPointBackupsMs = 0.0;
	double packFinalChecksumMs = 0.0;
	double packFinalHeaderWriteMs = 0.0;
	double packFinalBufferMs = 0.0;
	double writeResolvePathMs = 0.0;
	double writeBackupMs = 0.0;
	double writeBytesMs = 0.0;
	double writeClearBlobMs = 0.0;
	double writeReleaseLockMs = 0.0;
	double writeBlobObjectMs = 0.0;
	double writeBlobObjectResizeMs = 0.0;
	double writeBlobObjectCopyMs = 0.0;
	double writeBlobPlugSetMs = 0.0;
	double writeSetBlobMs = 0.0;
	double syncMs = 0.0;
	double overlayMs = 0.0;
	{
		UndoScope undoScope( script );
		syncMirroredControlsToNode( node );
		const auto sampleBuildStart = Clock::now();
		const bp::list samples = brushSamplesFromStroke( this, strokePoints );
		sampleBuildMs = elapsedMilliseconds( sampleBuildStart );
		const auto brushEraseStart = Clock::now();
		eraseResult = bp::extract<bp::dict>(
			pythonToolNode.attr( "brushErasePoints" )( samples, radius, eraseSpacePlug()->getValue() )
		);
		brushEraseMs = elapsedMilliseconds( brushEraseStart );
		removed = uint64FromObject( eraseResult.get( "removedCount", bp::object() ) );
		strokeCount = uint64FromObject( eraseResult.get( "strokeCount", bp::object() ) );
		loadSchemaMs = bp::extract<double>( eraseResult.get( "loadSchemaMs", bp::object( 0.0 ) ) );
		loadResolvePathMs = bp::extract<double>( eraseResult.get( "loadResolvePathMs", bp::object( 0.0 ) ) );
		loadReadBytesMs = bp::extract<double>( eraseResult.get( "loadReadBytesMs", bp::object( 0.0 ) ) );
		loadBlobExtractMs = bp::extract<double>( eraseResult.get( "loadBlobExtractMs", bp::object( 0.0 ) ) );
		loadUnpackMs = bp::extract<double>( eraseResult.get( "loadUnpackMs", bp::object( 0.0 ) ) );
		loadPopulateMetadataMs = bp::extract<double>( eraseResult.get( "loadPopulateMetadataMs", bp::object( 0.0 ) ) );
		unpackChecksumMs = bp::extract<double>( eraseResult.get( "unpackChecksumMs", bp::object( 0.0 ) ) );
		unpackHeaderMs = bp::extract<double>( eraseResult.get( "unpackHeaderMs", bp::object( 0.0 ) ) );
		unpackNodeMs = bp::extract<double>( eraseResult.get( "unpackNodeMs", bp::object( 0.0 ) ) );
		unpackLockMs = bp::extract<double>( eraseResult.get( "unpackLockMs", bp::object( 0.0 ) ) );
		unpackScenePathsMs = bp::extract<double>( eraseResult.get( "unpackScenePathsMs", bp::object( 0.0 ) ) );
		unpackLayersMs = bp::extract<double>( eraseResult.get( "unpackLayersMs", bp::object( 0.0 ) ) );
		unpackStrokesMs = bp::extract<double>( eraseResult.get( "unpackStrokesMs", bp::object( 0.0 ) ) );
		unpackChunksMs = bp::extract<double>( eraseResult.get( "unpackChunksMs", bp::object( 0.0 ) ) );
		unpackPointsMs = bp::extract<double>( eraseResult.get( "unpackPointsMs", bp::object( 0.0 ) ) );
		unpackSelectionSetsMs = bp::extract<double>( eraseResult.get( "unpackSelectionSetsMs", bp::object( 0.0 ) ) );
		unpackDiagnosticsMs = bp::extract<double>( eraseResult.get( "unpackDiagnosticsMs", bp::object( 0.0 ) ) );
		unpackUpgradesMs = bp::extract<double>( eraseResult.get( "unpackUpgradesMs", bp::object( 0.0 ) ) );
		unpackPointBackupsMs = bp::extract<double>( eraseResult.get( "unpackPointBackupsMs", bp::object( 0.0 ) ) );
		pointScanMs = bp::extract<double>( eraseResult.get( "pointScanMs", bp::object( 0.0 ) ) );
		selectionCleanupMs = bp::extract<double>( eraseResult.get( "selectionCleanupMs", bp::object( 0.0 ) ) );
		rebuildMs = bp::extract<double>( eraseResult.get( "rebuildMs", bp::object( 0.0 ) ) );
		trustedDiagnosticsMs = bp::extract<double>( eraseResult.get( "diagnosticsMs", bp::object( 0.0 ) ) );
		writeMs = bp::extract<double>( eraseResult.get( "writeMs", bp::object( 0.0 ) ) );
		writePopulateMetadataMs = bp::extract<double>( eraseResult.get( "writePopulateMetadataMs", bp::object( 0.0 ) ) );
		writeLockMs = bp::extract<double>( eraseResult.get( "writeLockMs", bp::object( 0.0 ) ) );
		writePackMs = bp::extract<double>( eraseResult.get( "writePackMs", bp::object( 0.0 ) ) );
		packHeaderMs = bp::extract<double>( eraseResult.get( "packHeaderMs", bp::object( 0.0 ) ) );
		packNodeMs = bp::extract<double>( eraseResult.get( "packNodeMs", bp::object( 0.0 ) ) );
		packLockMs = bp::extract<double>( eraseResult.get( "packLockMs", bp::object( 0.0 ) ) );
		packScenePathsMs = bp::extract<double>( eraseResult.get( "packScenePathsMs", bp::object( 0.0 ) ) );
		packLayersMs = bp::extract<double>( eraseResult.get( "packLayersMs", bp::object( 0.0 ) ) );
		packStrokesMs = bp::extract<double>( eraseResult.get( "packStrokesMs", bp::object( 0.0 ) ) );
		packChunksMs = bp::extract<double>( eraseResult.get( "packChunksMs", bp::object( 0.0 ) ) );
		packPointsMs = bp::extract<double>( eraseResult.get( "packPointsMs", bp::object( 0.0 ) ) );
		packPointsReserveMs = bp::extract<double>( eraseResult.get( "packPointsReserveMs", bp::object( 0.0 ) ) );
		packPointsReserveReallocated = bp::extract<double>( eraseResult.get( "packPointsReserveReallocated", bp::object( 0.0 ) ) );
		packPointsCapacityBeforeBytes = bp::extract<double>( eraseResult.get( "packPointsCapacityBeforeBytes", bp::object( 0.0 ) ) );
		packPointsCapacityAfterBytes = bp::extract<double>( eraseResult.get( "packPointsCapacityAfterBytes", bp::object( 0.0 ) ) );
		packPointsResizeMs = bp::extract<double>( eraseResult.get( "packPointsResizeMs", bp::object( 0.0 ) ) );
		packPointsFillMs = bp::extract<double>( eraseResult.get( "packPointsFillMs", bp::object( 0.0 ) ) );
		packPointsCopyMs = bp::extract<double>( eraseResult.get( "packPointsCopyMs", bp::object( 0.0 ) ) );
		packPointsChecksumInlineMs = bp::extract<double>( eraseResult.get( "packPointsChecksumInlineMs", bp::object( 0.0 ) ) );
		packPointsChecksumMs = bp::extract<double>( eraseResult.get( "packPointsChecksumMs", bp::object( 0.0 ) ) );
		packSelectionSetsMs = bp::extract<double>( eraseResult.get( "packSelectionSetsMs", bp::object( 0.0 ) ) );
		packDiagnosticsMs = bp::extract<double>( eraseResult.get( "packDiagnosticsMs", bp::object( 0.0 ) ) );
		packUpgradesMs = bp::extract<double>( eraseResult.get( "packUpgradesMs", bp::object( 0.0 ) ) );
		packPointBackupsMs = bp::extract<double>( eraseResult.get( "packPointBackupsMs", bp::object( 0.0 ) ) );
		packFinalChecksumMs = bp::extract<double>( eraseResult.get( "packFinalChecksumMs", bp::object( 0.0 ) ) );
		packFinalHeaderWriteMs = bp::extract<double>( eraseResult.get( "packFinalHeaderWriteMs", bp::object( 0.0 ) ) );
		packFinalBufferMs = bp::extract<double>( eraseResult.get( "packFinalBufferMs", bp::object( 0.0 ) ) );
		writeResolvePathMs = bp::extract<double>( eraseResult.get( "writeResolvePathMs", bp::object( 0.0 ) ) );
		writeBackupMs = bp::extract<double>( eraseResult.get( "writeBackupMs", bp::object( 0.0 ) ) );
		writeBytesMs = bp::extract<double>( eraseResult.get( "writeBytesMs", bp::object( 0.0 ) ) );
		writeClearBlobMs = bp::extract<double>( eraseResult.get( "writeClearBlobMs", bp::object( 0.0 ) ) );
		writeReleaseLockMs = bp::extract<double>( eraseResult.get( "writeReleaseLockMs", bp::object( 0.0 ) ) );
		writeBlobObjectMs = bp::extract<double>( eraseResult.get( "writeBlobObjectMs", bp::object( 0.0 ) ) );
		writeBlobObjectResizeMs = bp::extract<double>( eraseResult.get( "writeBlobObjectResizeMs", bp::object( 0.0 ) ) );
		writeBlobObjectCopyMs = bp::extract<double>( eraseResult.get( "writeBlobObjectCopyMs", bp::object( 0.0 ) ) );
		writeBlobPlugSetMs = bp::extract<double>( eraseResult.get( "writeBlobPlugSetMs", bp::object( 0.0 ) ) );
		writeSetBlobMs = bp::extract<double>( eraseResult.get( "writeSetBlobMs", bp::object( 0.0 ) ) );
		syncMs = bp::extract<double>( eraseResult.get( "syncMs", bp::object( 0.0 ) ) );
	}

	if( !removed )
	{
		logInfo(
			sequenceTag( "TIMING" ) + "erase brush commit totalMs=" + std::to_string( elapsedMilliseconds( totalStart ) ) +
			" sampleBuildMs=" + std::to_string( sampleBuildMs ) +
			" brushEraseMs=" + std::to_string( brushEraseMs ) +
			" loadSchemaMs=" + std::to_string( loadSchemaMs ) +
			" loadResolvePathMs=" + std::to_string( loadResolvePathMs ) +
			" loadReadBytesMs=" + std::to_string( loadReadBytesMs ) +
			" loadBlobExtractMs=" + std::to_string( loadBlobExtractMs ) +
			" loadUnpackMs=" + std::to_string( loadUnpackMs ) +
			" loadPopulateMetadataMs=" + std::to_string( loadPopulateMetadataMs ) +
			" unpackChecksumMs=" + std::to_string( unpackChecksumMs ) +
			" unpackHeaderMs=" + std::to_string( unpackHeaderMs ) +
			" unpackNodeMs=" + std::to_string( unpackNodeMs ) +
			" unpackLockMs=" + std::to_string( unpackLockMs ) +
			" unpackScenePathsMs=" + std::to_string( unpackScenePathsMs ) +
			" unpackLayersMs=" + std::to_string( unpackLayersMs ) +
			" unpackStrokesMs=" + std::to_string( unpackStrokesMs ) +
			" unpackChunksMs=" + std::to_string( unpackChunksMs ) +
			" unpackPointsMs=" + std::to_string( unpackPointsMs ) +
			" unpackSelectionSetsMs=" + std::to_string( unpackSelectionSetsMs ) +
			" unpackDiagnosticsMs=" + std::to_string( unpackDiagnosticsMs ) +
			" unpackUpgradesMs=" + std::to_string( unpackUpgradesMs ) +
			" unpackPointBackupsMs=" + std::to_string( unpackPointBackupsMs ) +
			" pointScanMs=" + std::to_string( pointScanMs ) +
			" selectionCleanupMs=" + std::to_string( selectionCleanupMs ) +
			" rebuildMs=" + std::to_string( rebuildMs ) +
			" trustedDiagnosticsMs=" + std::to_string( trustedDiagnosticsMs ) +
			" writeMs=" + std::to_string( writeMs ) +
			" writePopulateMetadataMs=" + std::to_string( writePopulateMetadataMs ) +
			" writeLockMs=" + std::to_string( writeLockMs ) +
			" writePackMs=" + std::to_string( writePackMs ) +
			" packHeaderMs=" + std::to_string( packHeaderMs ) +
			" packNodeMs=" + std::to_string( packNodeMs ) +
			" packLockMs=" + std::to_string( packLockMs ) +
			" packScenePathsMs=" + std::to_string( packScenePathsMs ) +
			" packLayersMs=" + std::to_string( packLayersMs ) +
			" packStrokesMs=" + std::to_string( packStrokesMs ) +
			" packChunksMs=" + std::to_string( packChunksMs ) +
			" packPointsMs=" + std::to_string( packPointsMs ) +
			" packPointsReserveMs=" + std::to_string( packPointsReserveMs ) +
			" packPointsReserveReallocated=" + std::to_string( packPointsReserveReallocated ) +
			" packPointsCapacityBeforeBytes=" + std::to_string( packPointsCapacityBeforeBytes ) +
			" packPointsCapacityAfterBytes=" + std::to_string( packPointsCapacityAfterBytes ) +
			" packPointsResizeMs=" + std::to_string( packPointsResizeMs ) +
			" packPointsFillMs=" + std::to_string( packPointsFillMs ) +
			" packPointsCopyMs=" + std::to_string( packPointsCopyMs ) +
			" packPointsChecksumInlineMs=" + std::to_string( packPointsChecksumInlineMs ) +
			" packPointsChecksumMs=" + std::to_string( packPointsChecksumMs ) +
			" packSelectionSetsMs=" + std::to_string( packSelectionSetsMs ) +
			" packDiagnosticsMs=" + std::to_string( packDiagnosticsMs ) +
			" packUpgradesMs=" + std::to_string( packUpgradesMs ) +
			" packPointBackupsMs=" + std::to_string( packPointBackupsMs ) +
			" packFinalChecksumMs=" + std::to_string( packFinalChecksumMs ) +
			" packFinalHeaderWriteMs=" + std::to_string( packFinalHeaderWriteMs ) +
			" packFinalBufferMs=" + std::to_string( packFinalBufferMs ) +
			" writeResolvePathMs=" + std::to_string( writeResolvePathMs ) +
			" writeBackupMs=" + std::to_string( writeBackupMs ) +
			" writeBytesMs=" + std::to_string( writeBytesMs ) +
			" writeClearBlobMs=" + std::to_string( writeClearBlobMs ) +
			" writeReleaseLockMs=" + std::to_string( writeReleaseLockMs ) +
			" writeBlobObjectMs=" + std::to_string( writeBlobObjectMs ) +
			" writeBlobObjectResizeMs=" + std::to_string( writeBlobObjectResizeMs ) +
			" writeBlobObjectCopyMs=" + std::to_string( writeBlobObjectCopyMs ) +
			" writeBlobPlugSetMs=" + std::to_string( writeBlobPlugSetMs ) +
			" writeSetBlobMs=" + std::to_string( writeSetBlobMs ) +
			" syncMs=" + std::to_string( syncMs ) +
			" selectionClearMs=" + std::to_string( selectionClearMs ) +
			" refreshMs=0.000000 removed=0 strokeCount=0 eraseSpace=" + std::to_string( eraseSpacePlug()->getValue() )
		);
		statusPlug()->setValue(
			"Erase mode (" + eraseSpaceName( eraseSpacePlug()->getValue() ) + ") found no authored points under the brush path."
		);
		return 0;
	}

	double refreshMs = 0.0;
	if( removed )
	{
		const auto refreshStart = Clock::now();
		refreshSceneGadget( "erase brush commit" );
		refreshMs = elapsedMilliseconds( refreshStart );
		const auto overlayStart = Clock::now();
		std::optional<int> knownPointCount;
		if( node == m_cachedPointCountNode && m_cachedPointCount )
		{
			knownPointCount = std::max( 0, *m_cachedPointCount - static_cast<int>( removed ) );
		}
		refreshPointCountOverlay( node, knownPointCount );
		overlayMs = elapsedMilliseconds( overlayStart );
	}
	logInfo(
		sequenceTag( "TIMING" ) + "erase brush commit totalMs=" + std::to_string( elapsedMilliseconds( totalStart ) ) +
		" sampleBuildMs=" + std::to_string( sampleBuildMs ) +
		" brushEraseMs=" + std::to_string( brushEraseMs ) +
		" loadSchemaMs=" + std::to_string( loadSchemaMs ) +
		" loadResolvePathMs=" + std::to_string( loadResolvePathMs ) +
		" loadReadBytesMs=" + std::to_string( loadReadBytesMs ) +
		" loadBlobExtractMs=" + std::to_string( loadBlobExtractMs ) +
		" loadUnpackMs=" + std::to_string( loadUnpackMs ) +
		" loadPopulateMetadataMs=" + std::to_string( loadPopulateMetadataMs ) +
		" unpackChecksumMs=" + std::to_string( unpackChecksumMs ) +
		" unpackHeaderMs=" + std::to_string( unpackHeaderMs ) +
		" unpackNodeMs=" + std::to_string( unpackNodeMs ) +
		" unpackLockMs=" + std::to_string( unpackLockMs ) +
		" unpackScenePathsMs=" + std::to_string( unpackScenePathsMs ) +
		" unpackLayersMs=" + std::to_string( unpackLayersMs ) +
		" unpackStrokesMs=" + std::to_string( unpackStrokesMs ) +
		" unpackChunksMs=" + std::to_string( unpackChunksMs ) +
		" unpackPointsMs=" + std::to_string( unpackPointsMs ) +
		" unpackSelectionSetsMs=" + std::to_string( unpackSelectionSetsMs ) +
		" unpackDiagnosticsMs=" + std::to_string( unpackDiagnosticsMs ) +
		" unpackUpgradesMs=" + std::to_string( unpackUpgradesMs ) +
		" unpackPointBackupsMs=" + std::to_string( unpackPointBackupsMs ) +
		" pointScanMs=" + std::to_string( pointScanMs ) +
		" selectionCleanupMs=" + std::to_string( selectionCleanupMs ) +
		" rebuildMs=" + std::to_string( rebuildMs ) +
		" trustedDiagnosticsMs=" + std::to_string( trustedDiagnosticsMs ) +
		" writeMs=" + std::to_string( writeMs ) +
		" writePopulateMetadataMs=" + std::to_string( writePopulateMetadataMs ) +
		" writeLockMs=" + std::to_string( writeLockMs ) +
		" writePackMs=" + std::to_string( writePackMs ) +
		" packHeaderMs=" + std::to_string( packHeaderMs ) +
		" packNodeMs=" + std::to_string( packNodeMs ) +
		" packLockMs=" + std::to_string( packLockMs ) +
		" packScenePathsMs=" + std::to_string( packScenePathsMs ) +
		" packLayersMs=" + std::to_string( packLayersMs ) +
		" packStrokesMs=" + std::to_string( packStrokesMs ) +
		" packChunksMs=" + std::to_string( packChunksMs ) +
		" packPointsMs=" + std::to_string( packPointsMs ) +
		" packPointsReserveMs=" + std::to_string( packPointsReserveMs ) +
		" packPointsReserveReallocated=" + std::to_string( packPointsReserveReallocated ) +
		" packPointsCapacityBeforeBytes=" + std::to_string( packPointsCapacityBeforeBytes ) +
		" packPointsCapacityAfterBytes=" + std::to_string( packPointsCapacityAfterBytes ) +
		" packPointsResizeMs=" + std::to_string( packPointsResizeMs ) +
		" packPointsFillMs=" + std::to_string( packPointsFillMs ) +
		" packPointsCopyMs=" + std::to_string( packPointsCopyMs ) +
		" packPointsChecksumInlineMs=" + std::to_string( packPointsChecksumInlineMs ) +
		" packPointsChecksumMs=" + std::to_string( packPointsChecksumMs ) +
		" packSelectionSetsMs=" + std::to_string( packSelectionSetsMs ) +
		" packDiagnosticsMs=" + std::to_string( packDiagnosticsMs ) +
		" packUpgradesMs=" + std::to_string( packUpgradesMs ) +
		" packPointBackupsMs=" + std::to_string( packPointBackupsMs ) +
		" packFinalChecksumMs=" + std::to_string( packFinalChecksumMs ) +
		" packFinalHeaderWriteMs=" + std::to_string( packFinalHeaderWriteMs ) +
		" packFinalBufferMs=" + std::to_string( packFinalBufferMs ) +
		" writeResolvePathMs=" + std::to_string( writeResolvePathMs ) +
		" writeBackupMs=" + std::to_string( writeBackupMs ) +
		" writeBytesMs=" + std::to_string( writeBytesMs ) +
		" writeClearBlobMs=" + std::to_string( writeClearBlobMs ) +
		" writeReleaseLockMs=" + std::to_string( writeReleaseLockMs ) +
		" writeBlobObjectMs=" + std::to_string( writeBlobObjectMs ) +
		" writeBlobObjectResizeMs=" + std::to_string( writeBlobObjectResizeMs ) +
		" writeBlobObjectCopyMs=" + std::to_string( writeBlobObjectCopyMs ) +
		" writeBlobPlugSetMs=" + std::to_string( writeBlobPlugSetMs ) +
		" writeSetBlobMs=" + std::to_string( writeSetBlobMs ) +
		" syncMs=" + std::to_string( syncMs ) +
		" selectionClearMs=" + std::to_string( selectionClearMs ) +
		" refreshMs=" + std::to_string( refreshMs ) +
		" overlayMs=" + std::to_string( overlayMs ) +
		" removed=" + std::to_string( removed ) +
		" strokeCount=" + std::to_string( strokeCount ) +
		" eraseSpace=" + std::to_string( eraseSpacePlug()->getValue() )
	);
	statusPlug()->setValue(
		"Erase mode (" + eraseSpaceName( eraseSpacePlug()->getValue() ) + ") removed " + std::to_string( removed ) +
		" points across " + std::to_string( strokeCount ) + " strokes."
	);
	return removed;
}

size_t PaintPointsTool::commitDemoStroke()
{
	logInfo( sequenceTag( "DEMO" ) + "commitDemoStroke requested." );
	if( isEraseMode( this ) )
	{
		return eraseLastStroke();
	}
	if( isSelectLassoMode( this ) )
	{
		statusPlug()->setValue( "SelectLasso uses viewport drag lasso selection." );
		return 0;
	}
	if( isRelaxMode( this ) || isReprojectMode( this ) )
	{
		ScriptNode *script = toolScriptNode( this );
		if( !script )
		{
			statusPlug()->setValue( toolModeName( this ) + " failed: no ScriptNode available." );
			return 0;
		}

		IECorePython::ScopedGILLock gilLock;
		Node *node = findPaintedPointsNode( this, script );
		if( !node )
		{
			statusPlug()->setValue( toolModeName( this ) + " failed: no PaintedPoints target was found." );
			return 0;
		}

		size_t changed = 0;
		{
			UndoScope undoScope( script );
			syncMirroredControlsToNode( node );
			changed = applySelectionAction( this, node, currentSelectionState( node ) );
		}
		if( changed )
		{
			refreshSceneGadget( toolModeName( this ) + " commitDemoStroke" );
		}
		return changed;
	}
	if( isLayerEditMode( this ) )
	{
		ScriptNode *script = toolScriptNode( this );
		if( !script )
		{
			statusPlug()->setValue( "LayerEdit failed: no ScriptNode available." );
			return 0;
		}

		IECorePython::ScopedGILLock gilLock;
		Node *node = findPaintedPointsNode( this, script );
		if( !node )
		{
			statusPlug()->setValue( "LayerEdit failed: no PaintedPoints target was found." );
			return 0;
		}

		size_t changed = 0;
		{
			UndoScope undoScope( script );
			syncMirroredControlsToNode( node );
			changed = applyLayerEditSelection( this, node, currentSelectionState( node ) );
		}
		if( changed )
		{
			refreshSceneGadget( "layer edit" );
		}
		return changed;
	}
	if( isStrokeEditMode( this ) )
	{
		ScriptNode *script = toolScriptNode( this );
		if( !script )
		{
			statusPlug()->setValue( "StrokeEdit failed: no ScriptNode available." );
			return 0;
		}

		IECorePython::ScopedGILLock gilLock;
		Node *node = findPaintedPointsNode( this, script );
		if( !node )
		{
			statusPlug()->setValue( "StrokeEdit failed: no PaintedPoints target was found." );
			return 0;
		}

		size_t changed = 0;
		{
			UndoScope undoScope( script );
			syncMirroredControlsToNode( node );
			changed = applyStrokeEditSelection( this, node, currentSelectionState( node ) );
		}
		if( changed )
		{
			refreshSceneGadget( "stroke edit" );
		}
		return changed;
	}

	ScriptNode *script = toolScriptNode( this );
	if( !script )
	{
		logWarning( sequenceTag( "DEMO" ) + "commitDemoStroke failed because no ScriptNode was available." );
		statusPlug()->setValue( "Paint mode demo commit failed: no ScriptNode available." );
		return 0;
	}

	IECorePython::ScopedGILLock gilLock;
	Node *node = nullptr;
	size_t committed = 0;
	{
		UndoScope undoScope( script );
		node = targetNodeOrCreate( this, script );
		if( !node )
		{
			logWarning( sequenceTag( "DEMO" ) + "commitDemoStroke failed because no PaintedPoints target could be found or created." );
			statusPlug()->setValue( "Paint mode demo commit failed: no PaintedPoints target was available." );
			return 0;
		}
		syncMirroredControlsToNode( node );

		const auto ids = ensureLayerAndStroke( this, node );
		logInfo(
			sequenceTag( "DEMO" ) + "commitDemoStroke target=" + node->getName().string() +
			" layerId=" + std::to_string( ids.first ) +
			" strokeId=" + std::to_string( ids.second ) +
			" previewCount=" + std::to_string( previewCountPlug()->getValue() )
		);
		bp::object pythonToolNode = pythonNode( node );
		committed = bp::extract<size_t>( pythonToolNode.attr( "paintStrokeCommit" )( bp::object( ids.second ), demoPoints( this ), true ) );
		logInfo( sequenceTag( "DEMO" ) + "commitDemoStroke committed=" + std::to_string( committed ) + " target=" + node->getName().string() );
		logRefreshDiagnostics( this, script, node, "afterDemoCommitBeforeRender" );
	}
	// UndoScope destructed -> dirty propagation flushed before refresh.
	refreshSceneGadget( "demo stroke commit" );
	logRefreshDiagnostics( this, script, node, "afterDemoCommitAfterRender" );
	statusPlug()->setValue( "Paint mode committed demo stroke with " + std::to_string( committed ) + " points." );
	return committed;
}

size_t PaintPointsTool::eraseLastStroke()
{
	logInfo( sequenceTag( "ERASE" ) + "eraseLastStroke requested." );
	ScriptNode *script = toolScriptNode( this );
	if( !script )
	{
		logWarning( sequenceTag( "ERASE" ) + "eraseLastStroke failed because no ScriptNode was available." );
		statusPlug()->setValue( "Erase mode failed: no ScriptNode available." );
		return 0;
	}

	Node *node = findPaintedPointsNode( this, script );
	if( !node )
	{
		logWarning( sequenceTag( "ERASE" ) + "eraseLastStroke failed because no PaintedPoints node was found." );
		statusPlug()->setValue( "Erase mode failed: no PaintedPoints target was found." );
		return 0;
	}

	IECorePython::ScopedGILLock gilLock;
	bp::object pythonToolNode = pythonNode( node );
	bp::object lastStrokeId = pythonToolNode.attr( "lastStrokeId" )();
	if( lastStrokeId.is_none() )
	{
		logInfo( sequenceTag( "ERASE" ) + "eraseLastStroke found no last stroke on target=" + node->getName().string() );
		statusPlug()->setValue( "Erase mode found no stroke to remove." );
		return 0;
	}

	size_t removed = 0;
	{
		UndoScope undoScope( script );
		logInfo( sequenceTag( "ERASE" ) + "eraseLastStroke target=" + node->getName().string() );
		removed = bp::extract<size_t>( pythonToolNode.attr( "eraseCommit" )( lastStrokeId, bp::object(), bp::object( 1.0f ) ) );
		logInfo( sequenceTag( "ERASE" ) + "eraseLastStroke removed=" + std::to_string( removed ) + " target=" + node->getName().string() );
		logRefreshDiagnostics( this, script, node, "afterEraseBeforeRender" );
	}
	// UndoScope destructed -> dirty propagation flushed before refresh.
	refreshSceneGadget( "erase commit" );
	invalidatePointCountOverlayCache();
	refreshPointCountOverlay( node );
	logRefreshDiagnostics( this, script, node, "afterEraseAfterRender" );
	statusPlug()->setValue( "Erase mode removed " + std::to_string( removed ) + " points from the last stroke." );
	return removed;
}
