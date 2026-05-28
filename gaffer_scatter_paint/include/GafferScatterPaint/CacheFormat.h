#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace GafferScatterPaint
{

enum class CacheStorageMode : std::uint32_t
{
	Embedded = 0,
	External = 1,
};

enum class CachePathMode : std::uint32_t
{
	Absolute = 0,
	RelativeToScript = 1,
	RelativeToProject = 2,
};

enum class LockMode : std::uint32_t
{
	SessionAware = 0,
};

enum class StrokeFrameMode : std::uint32_t
{
	Persistent = 0,
	Additive = 1,
	Override = 2,
};

enum class AnchorMode : std::uint32_t
{
	Barycentric = 0,
	HybridFallback = 1,
	UVFallback = 2,
	Reprojected = 3,
};

enum class ValidationCategory : std::uint32_t
{
	Cache = 0,
	Lock = 1,
	Topology = 2,
	Attachment = 3,
	ExportReadiness = 4,
	UpgradeState = 5,
	Diagnostics = 6,
};

struct CacheHeader
{
	static constexpr std::array<char, 8> magic = {'G', 'S', 'P', 'A', 'I', 'N', 'T', '\0'};
	static constexpr std::uint32_t schemaVersion = 2;

	std::uint32_t endianMarker = 0x01020304;
	std::uint32_t pluginVersionMajor = 0;
	std::uint32_t pluginVersionMinor = 5;
	std::uint32_t pluginVersionPatch = 8;
	std::uint32_t contentFlags = 0;
	std::uint64_t checksum = 0;
};

struct LockMetadata
{
	LockMode mode = LockMode::SessionAware;
	std::string user;
	std::string host;
	std::string timestampUtc;
	std::string scriptPath;
	std::string projectPath;
	std::string sessionId;
};

struct NodeMetadata
{
	CacheStorageMode storageMode = CacheStorageMode::Embedded;
	CachePathMode pathMode = CachePathMode::RelativeToScript;
	std::string projectRoot;
	std::string cachePath;
	std::string exportPreset;
	bool backupEnabled = false;
	bool diagnosticsSnapshotEnabled = true;
	std::array<float, 3> defaultColor = {1.0f, 1.0f, 1.0f};
};

struct NextIdsRecord
{
	std::uint64_t layer = 1;
	std::uint64_t stroke = 1;
	std::uint64_t point = 1;
	std::uint64_t selectionSet = 1;
	std::uint64_t chunk = 1;
};

struct LayerRecord
{
	std::uint64_t layerId = 0;
	std::string name;
	std::int32_t order = 0;
	bool enabled = true;
	bool visible = true;
	bool mute = false;
	bool solo = false;
	bool timeVarying = false;
	StrokeFrameMode mode = StrokeFrameMode::Persistent;
	std::int32_t frameStart = 0;
	std::int32_t frameEnd = 0;
	bool holdOutsideRange = true;
	std::uint64_t firstStrokeId = 0;
	std::uint64_t lastStrokeId = 0;
	bool colorEnabled = false;
	std::array<float, 3> color = {1.0f, 1.0f, 1.0f};
};

struct StrokeRecord
{
	std::uint64_t strokeId = 0;
	std::uint64_t layerId = 0;
	std::string name;
	std::int32_t order = 0;
	StrokeFrameMode mode = StrokeFrameMode::Persistent;
	std::int32_t frameStart = 0;
	std::int32_t frameEnd = 0;
	std::uint64_t createdTimeUnixMicros = 0;
	std::uint64_t firstChunkId = 0;
	std::uint64_t lastChunkId = 0;
	std::uint32_t pointCount = 0;
	std::uint32_t targetCount = 0;
	std::uint64_t selectionMaskId = 0;
	bool colorEnabled = false;
	std::array<float, 3> color = {1.0f, 1.0f, 1.0f};
};

struct PointRecord
{
	std::uint64_t pointId = 0;
	std::uint64_t strokeId = 0;
	std::uint64_t layerId = 0;
	std::uint32_t targetPathId = 0;
	std::uint32_t instanceId = 0;
	std::uint32_t instanceSourcePathId = 0;
	std::uint32_t triangleIndex = 0;
	std::array<float, 3> barycentric = {0.0f, 0.0f, 0.0f};
	std::array<float, 3> restObjectP = {0.0f, 0.0f, 0.0f};
	std::array<float, 3> restWorldP = {0.0f, 0.0f, 0.0f};
	std::array<float, 2> restUV = {0.0f, 0.0f};
	std::array<float, 3> restNormal = {0.0f, 1.0f, 0.0f};
	std::array<float, 3> restUp = {0.0f, 0.0f, 1.0f};
	float width = 1.0f;
	float uniformScale = 1.0f;
	std::uint32_t seed = 0;
	float normalSpin = 0.0f;
	std::array<float, 2> tangentRotation = {0.0f, 0.0f};
	float pressureDensity = 1.0f;
	float pressureSoftness = 1.0f;
	bool valid = true;
	std::int32_t lastValidFrame = 0;
	AnchorMode anchorModeUsed = AnchorMode::Barycentric;
	std::uint32_t topologyGeneration = 0;
	bool colorEnabled = false;
	std::array<float, 3> color = {1.0f, 1.0f, 1.0f};
};

struct PointChunkRecord
{
	std::uint64_t chunkId = 0;
	std::uint64_t strokeId = 0;
	std::uint32_t chunkIndex = 0;
	std::uint32_t pointStart = 0;
	std::uint32_t pointCount = 0;
	std::uint32_t generation = 0;
	bool deleted = false;
};

struct SelectionSetRecord
{
	std::uint64_t selectionSetId = 0;
	std::string name;
	std::vector<std::uint64_t> pointIds;
	std::vector<std::uint64_t> strokeIds;
};

struct CurrentSelectionRecord
{
	std::vector<std::uint64_t> pointIds;
	std::vector<std::uint64_t> strokeIds;
};

struct PointBackupRecord
{
	std::uint32_t targetPathId = 0;
	std::uint32_t triangleIndex = 0;
	std::array<float, 3> barycentric = {0.0f, 0.0f, 0.0f};
	bool attachmentResolved = false;
};

struct DiagnosticsSnapshot
{
	std::uint64_t invalidPointCount = 0;
	std::uint64_t invalidStrokeCount = 0;
	std::uint64_t topologyMismatchCount = 0;
	std::int32_t failingFrame = 0;
	std::vector<std::string> failingTargetPaths;
	std::string lastErrorMessage;
	std::string validationSummary;
	std::vector<ValidationCategory> categories;
};

struct UpgradeRecord
{
	std::uint32_t sourceSchemaVersion = 0;
	std::uint32_t upgradedSchemaVersion = CacheHeader::schemaVersion;
	std::string timestampUtc;
	std::string report;
};

struct CacheSchema
{
	CacheHeader header;
	LockMetadata lock;
	NodeMetadata node;
	NextIdsRecord nextIds;
	std::vector<std::string> scenePaths;
	std::vector<std::string> instanceSourcePaths;
	std::vector<LayerRecord> layers;
	std::vector<StrokeRecord> strokes;
	std::vector<PointChunkRecord> chunks;
	std::vector<PointRecord> points;
	std::vector<SelectionSetRecord> selectionSets;
	CurrentSelectionRecord currentSelection;
	DiagnosticsSnapshot diagnostics;
	std::vector<UpgradeRecord> upgrades;
	std::vector<std::pair<std::uint64_t, PointBackupRecord>> pointBackups;
};

struct CachePackTimings
{
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
};

struct CacheUnpackTimings
{
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
};

std::string cacheDescription();
std::string cacheSchemaSummary();
CacheSchema defaultCacheSchema();

std::string utcTimestampNow();
std::uint64_t unixTimeMicros();
std::string packLockMetadata( const LockMetadata &metadata );
LockMetadata unpackLockMetadata( const std::string &blob, std::string &error );
bool isStaleLock( const LockMetadata &metadata, std::uint64_t staleSeconds );

std::string packCacheSchema( const CacheSchema &schema, CachePackTimings *timings = nullptr );
void packCacheSchema( const CacheSchema &schema, std::vector<unsigned char> &buffer, CachePackTimings *timings = nullptr );
CacheSchema unpackCacheSchema( const std::string &blob, std::string &error, CacheUnpackTimings *timings = nullptr, bool verifyChecksum = true );

} // namespace GafferScatterPaint
