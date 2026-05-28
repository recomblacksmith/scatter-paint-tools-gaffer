#include "GafferScatterPaint/CacheFormat.h"

#include <chrono>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <iostream>

namespace GafferScatterPaint
{

std::string cacheDescription()
{
	return "Gaffer Scatter Paint cache schema v2";
}

std::string cacheSchemaSummary()
{
	return
		"Storage: embedded/external, "
		"records: layers/strokes/chunks/points/selections, "
		"locking: session-aware, "
		"upgrade: explicit and versioned";
}

CacheSchema defaultCacheSchema()
{
	CacheSchema schema;
	schema.diagnostics.validationSummary = "Empty scatter paint cache";
	schema.diagnostics.categories = {
		ValidationCategory::Cache,
		ValidationCategory::Lock,
		ValidationCategory::Topology,
		ValidationCategory::Attachment,
		ValidationCategory::ExportReadiness,
		ValidationCategory::UpgradeState,
		ValidationCategory::Diagnostics,
	};
	return schema;
}

namespace
{

using Clock = std::chrono::steady_clock;

double elapsedMilliseconds( const Clock::time_point &start )
{
	return std::chrono::duration<double, std::milli>( Clock::now() - start ).count();
}

constexpr std::uint32_t CONTENT_FLAG_DIAGNOSTICS = 1 << 0;
constexpr std::uint32_t CONTENT_FLAG_UPGRADES = 1 << 1;
constexpr std::uint32_t CONTENT_FLAG_EXTERNAL = 1 << 2;
constexpr std::array<char, 8> LOCK_MAGIC = {'G', 'S', 'P', 'L', 'O', 'C', 'K', '\0'};
constexpr std::uint64_t CHECKSUM_OFFSET_BASIS = 1469598103934665603ULL;
constexpr std::uint64_t CHECKSUM_PRIME = 1099511628211ULL;
constexpr size_t PACKED_POINT_RECORD_SIZE = 166;

#pragma pack(push, 1)
struct PackedPointRecord
{
	std::uint64_t pointId;
	std::uint64_t strokeId;
	std::uint64_t layerId;
	std::uint32_t targetPathId;
	std::uint32_t instanceId;
	std::uint32_t instanceSourcePathId;
	std::uint32_t triangleIndex;
	std::array<float, 3> barycentric;
	std::array<float, 3> restObjectP;
	std::array<float, 3> restWorldP;
	std::array<float, 2> restUV;
	std::array<float, 3> restNormal;
	std::array<float, 3> restUp;
	float width;
	float uniformScale;
	std::uint32_t seed;
	float normalSpin;
	std::array<float, 2> tangentRotation;
	float pressureDensity;
	float pressureSoftness;
	std::uint8_t valid;
	std::int32_t lastValidFrame;
	std::uint32_t anchorModeUsed;
	std::uint32_t topologyGeneration;
	std::uint8_t colorEnabled;
	std::array<float, 3> color;
};
#pragma pack(pop)

constexpr size_t PACKED_POINT_PREFIX_SIZE = offsetof( PackedPointRecord, valid );
static_assert( sizeof( PackedPointRecord ) == PACKED_POINT_RECORD_SIZE, "Packed point record size mismatch" );
static_assert( offsetof( PointRecord, valid ) == PACKED_POINT_PREFIX_SIZE, "PointRecord prefix layout mismatch" );

std::uint32_t contentFlags( const CacheSchema &schema )
{
	std::uint32_t flags = 0;
	if( schema.diagnostics.invalidPointCount > 0 || schema.diagnostics.invalidStrokeCount > 0 || schema.diagnostics.topologyMismatchCount > 0 )
	{
		flags |= CONTENT_FLAG_DIAGNOSTICS;
	}
	if( !schema.upgrades.empty() )
	{
		flags |= CONTENT_FLAG_UPGRADES;
	}
	if( schema.node.storageMode == CacheStorageMode::External )
	{
		flags |= CONTENT_FLAG_EXTERNAL;
	}
	return flags;
}

size_t packedStringSize( const std::string &value )
{
	return sizeof( std::uint32_t ) + value.size();
}

size_t packedStringListSize( const std::vector<std::string> &values )
{
	size_t size = sizeof( std::uint32_t );
	for( const auto &value : values )
	{
		size += packedStringSize( value );
	}
	return size;
}

size_t packedU64ListSize( const std::vector<std::uint64_t> &values )
{
	return sizeof( std::uint32_t ) + values.size() * sizeof( std::uint64_t );
}

size_t estimatedPackCapacity( const CacheSchema &schema )
{
	size_t size = 0;
	size += CacheHeader::magic.size() + sizeof( std::uint32_t ) * 5 + sizeof( std::uint64_t );

	// Header / nextIds payload.
	size += sizeof( std::uint32_t ) + sizeof( std::uint64_t ) * 5;

	// Node metadata.
	size += sizeof( std::uint32_t ) * 2;
	size += packedStringSize( schema.node.projectRoot );
	size += packedStringSize( schema.node.cachePath );
	size += packedStringSize( schema.node.exportPreset );
	size += sizeof( bool ) * 2;
	size += sizeof( float ) * 3;

	// Lock metadata.
	size += sizeof( std::uint32_t );
	size += packedStringSize( schema.lock.user );
	size += packedStringSize( schema.lock.host );
	size += packedStringSize( schema.lock.timestampUtc );
	size += packedStringSize( schema.lock.scriptPath );
	size += packedStringSize( schema.lock.projectPath );
	size += packedStringSize( schema.lock.sessionId );

	// Scene path lists.
	size += packedStringListSize( schema.scenePaths );
	size += packedStringListSize( schema.instanceSourcePaths );

	// Layers.
	size += sizeof( std::uint32_t );
	for( const auto &layer : schema.layers )
	{
		size += sizeof( std::uint64_t );
		size += packedStringSize( layer.name );
		size += sizeof( std::int32_t );
		size += sizeof( bool ) * 6;
		size += sizeof( std::uint32_t );
		size += sizeof( std::int32_t ) * 2;
		size += sizeof( bool );
		size += sizeof( std::uint64_t ) * 2;
		size += sizeof( bool );
		size += sizeof( float ) * 3;
	}

	// Strokes.
	size += sizeof( std::uint32_t );
	for( const auto &stroke : schema.strokes )
	{
		size += sizeof( std::uint64_t ) * 2;
		size += packedStringSize( stroke.name );
		size += sizeof( std::int32_t ) * 3;
		size += sizeof( std::uint32_t ) * 3;
		size += sizeof( std::uint64_t ) * 4;
		size += sizeof( bool );
		size += sizeof( float ) * 3;
	}

	// Chunks.
	size += sizeof( std::uint32_t );
	size += schema.chunks.size() * ( sizeof( std::uint64_t ) * 2 + sizeof( std::uint32_t ) * 4 + sizeof( bool ) );

	// Points.
	size += sizeof( std::uint32_t ) + schema.points.size() * PACKED_POINT_RECORD_SIZE;

	// Selection sets and current selection.
	size += sizeof( std::uint32_t );
	for( const auto &selectionSet : schema.selectionSets )
	{
		size += sizeof( std::uint64_t );
		size += packedStringSize( selectionSet.name );
		size += packedU64ListSize( selectionSet.pointIds );
		size += packedU64ListSize( selectionSet.strokeIds );
	}
	size += packedU64ListSize( schema.currentSelection.pointIds );
	size += packedU64ListSize( schema.currentSelection.strokeIds );

	// Diagnostics.
	size += sizeof( std::uint64_t ) * 3;
	size += sizeof( std::int32_t );
	size += packedStringListSize( schema.diagnostics.failingTargetPaths );
	size += packedStringSize( schema.diagnostics.lastErrorMessage );
	size += packedStringSize( schema.diagnostics.validationSummary );
	size += sizeof( std::uint32_t ) + schema.diagnostics.categories.size() * sizeof( std::uint32_t );

	// Upgrades.
	size += sizeof( std::uint32_t );
	for( const auto &upgrade : schema.upgrades )
	{
		size += sizeof( std::uint32_t ) * 2;
		size += packedStringSize( upgrade.timestampUtc );
		size += packedStringSize( upgrade.report );
	}

	// Point backups.
	size += sizeof( std::uint32_t );
	size += schema.pointBackups.size() * ( sizeof( std::uint64_t ) + sizeof( std::uint32_t ) * 2 + sizeof( float ) * 3 + sizeof( bool ) );

	return size;
}

void checksum64Update( std::uint64_t &checksum, const char *data, size_t size )
{
	const unsigned char *bytes = reinterpret_cast<const unsigned char *>( data );
	const unsigned char *end = bytes + size;
	while( end - bytes >= 64 )
	{
		checksum ^= static_cast<std::uint64_t>( bytes[0] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[1] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[2] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[3] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[4] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[5] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[6] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[7] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[8] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[9] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[10] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[11] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[12] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[13] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[14] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[15] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[16] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[17] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[18] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[19] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[20] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[21] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[22] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[23] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[24] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[25] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[26] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[27] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[28] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[29] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[30] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[31] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[32] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[33] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[34] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[35] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[36] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[37] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[38] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[39] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[40] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[41] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[42] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[43] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[44] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[45] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[46] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[47] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[48] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[49] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[50] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[51] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[52] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[53] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[54] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[55] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[56] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[57] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[58] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[59] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[60] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[61] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[62] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[63] );
		checksum *= CHECKSUM_PRIME;
		bytes += 64;
	}
	while( end - bytes >= 8 )
	{
		checksum ^= static_cast<std::uint64_t>( bytes[0] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[1] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[2] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[3] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[4] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[5] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[6] );
		checksum *= CHECKSUM_PRIME;
		checksum ^= static_cast<std::uint64_t>( bytes[7] );
		checksum *= CHECKSUM_PRIME;
		bytes += 8;
	}
	while( bytes < end )
	{
		checksum ^= static_cast<std::uint64_t>( *bytes++ );
		checksum *= CHECKSUM_PRIME;
	}
}

std::uint64_t checksum64( const char *data, size_t size )
{
	std::uint64_t checksum = CHECKSUM_OFFSET_BASIS;
	checksum64Update( checksum, data, size );
	return checksum;
}

std::uint64_t checksum64( const std::string &blobBytes )
{
	return checksum64( blobBytes.data(), blobBytes.size() );
}

template<typename Buffer>
char *bufferData( Buffer &buffer )
{
	return reinterpret_cast<char *>( buffer.data() );
}

template<typename Buffer>
const char *bufferData( const Buffer &buffer )
{
	return reinterpret_cast<const char *>( buffer.data() );
}

template<typename Buffer>
void appendBytes( Buffer &buffer, const void *data, size_t size, std::uint64_t *checksum = nullptr )
{
	if( size == 0 )
	{
		return;
	}
	using ValueType = typename Buffer::value_type;
	const ValueType *typedData = reinterpret_cast<const ValueType *>( data );
	buffer.insert( buffer.end(), typedData, typedData + size );
	if( checksum )
	{
		checksum64Update( *checksum, reinterpret_cast<const char *>( data ), size );
	}
}

template<typename Buffer, typename T>
void appendPod( Buffer &buffer, const T &value, std::uint64_t *checksum = nullptr )
{
	appendBytes( buffer, &value, sizeof( T ), checksum );
}

template<typename Buffer>
void writeU32( Buffer &buffer, std::uint32_t value, std::uint64_t *checksum = nullptr )
{
	appendPod( buffer, value, checksum );
}

template<typename Buffer>
void writeU64( Buffer &buffer, std::uint64_t value, std::uint64_t *checksum = nullptr )
{
	appendPod( buffer, value, checksum );
}

template<typename Buffer>
void writeI32( Buffer &buffer, std::int32_t value, std::uint64_t *checksum = nullptr )
{
	appendPod( buffer, value, checksum );
}

template<typename Buffer>
void writeF32( Buffer &buffer, float value, std::uint64_t *checksum = nullptr )
{
	appendPod( buffer, value, checksum );
}

template<typename Buffer>
void writeBool( Buffer &buffer, bool value, std::uint64_t *checksum = nullptr )
{
	char c = value ? 1 : 0;
	buffer.push_back( static_cast<typename Buffer::value_type>( c ) );
	if( checksum )
	{
		checksum64Update( *checksum, &c, 1 );
	}
}

template<typename Buffer>
void writeString( Buffer &buffer, const std::string &value, std::uint64_t *checksum = nullptr )
{
	writeU32( buffer, static_cast<std::uint32_t>( value.size() ), checksum );
	if( !value.empty() )
	{
		appendBytes( buffer, value.data(), value.size(), checksum );
	}
}

template<typename Buffer>
void writeU64List( Buffer &buffer, const std::vector<std::uint64_t> &values, std::uint64_t *checksum = nullptr )
{
	writeU32( buffer, static_cast<std::uint32_t>( values.size() ), checksum );
	appendBytes( buffer, values.data(), values.size() * sizeof( std::uint64_t ), checksum );
}

template<typename Buffer>
void writeStringList( Buffer &buffer, const std::vector<std::string> &values, std::uint64_t *checksum = nullptr )
{
	writeU32( buffer, static_cast<std::uint32_t>( values.size() ), checksum );
	for( const std::string &value : values )
	{
		writeString( buffer, value, checksum );
	}
}

template<typename Buffer>
void writeF32List( Buffer &buffer, const float *values, size_t count, std::uint64_t *checksum = nullptr )
{
	appendBytes( buffer, values, count * sizeof( float ), checksum );
}

template<typename Buffer>
void packCacheSchemaImpl( const CacheSchema &schema, Buffer &buffer, CachePackTimings *timings )
{
	buffer.clear();
	buffer.reserve( estimatedPackCapacity( schema ) );
	CachePackTimings localTimings;
	CachePackTimings &packTimings = timings ? *timings : localTimings;
	appendBytes( buffer, CacheHeader::magic.data(), CacheHeader::magic.size() );
	writeU32( buffer, schema.header.endianMarker );
	writeU32( buffer, schema.header.pluginVersionMajor );
	writeU32( buffer, schema.header.pluginVersionMinor );
	writeU32( buffer, schema.header.pluginVersionPatch );
	writeU32( buffer, contentFlags( schema ) );
	const size_t checksumOffset = buffer.size();
	writeU64( buffer, 0 );
	const size_t payloadOffset = buffer.size();
	std::uint64_t payloadChecksum = CHECKSUM_OFFSET_BASIS;
	const auto headerStart = Clock::now();
	writeU32( buffer, CacheHeader::schemaVersion, &payloadChecksum );
	writeU64( buffer, schema.nextIds.layer, &payloadChecksum );
	writeU64( buffer, schema.nextIds.stroke, &payloadChecksum );
	writeU64( buffer, schema.nextIds.point, &payloadChecksum );
	writeU64( buffer, schema.nextIds.selectionSet, &payloadChecksum );
	writeU64( buffer, schema.nextIds.chunk, &payloadChecksum );
	packTimings.packHeaderMs = elapsedMilliseconds( headerStart );

	const auto nodeStart = Clock::now();
	writeU32( buffer, static_cast<std::uint32_t>( schema.node.storageMode ), &payloadChecksum );
	writeU32( buffer, static_cast<std::uint32_t>( schema.node.pathMode ), &payloadChecksum );
	writeString( buffer, schema.node.projectRoot, &payloadChecksum );
	writeString( buffer, schema.node.cachePath, &payloadChecksum );
	writeString( buffer, schema.node.exportPreset, &payloadChecksum );
	writeBool( buffer, schema.node.backupEnabled, &payloadChecksum );
	writeBool( buffer, schema.node.diagnosticsSnapshotEnabled, &payloadChecksum );
	writeF32List( buffer, schema.node.defaultColor.data(), 3, &payloadChecksum );
	packTimings.packNodeMs = elapsedMilliseconds( nodeStart );

	const auto lockStart = Clock::now();
	writeU32( buffer, static_cast<std::uint32_t>( schema.lock.mode ), &payloadChecksum );
	writeString( buffer, schema.lock.user, &payloadChecksum );
	writeString( buffer, schema.lock.host, &payloadChecksum );
	writeString( buffer, schema.lock.timestampUtc, &payloadChecksum );
	writeString( buffer, schema.lock.scriptPath, &payloadChecksum );
	writeString( buffer, schema.lock.projectPath, &payloadChecksum );
	writeString( buffer, schema.lock.sessionId, &payloadChecksum );
	packTimings.packLockMs = elapsedMilliseconds( lockStart );

	const auto scenePathsStart = Clock::now();
	writeStringList( buffer, schema.scenePaths, &payloadChecksum );
	writeStringList( buffer, schema.instanceSourcePaths, &payloadChecksum );
	packTimings.packScenePathsMs = elapsedMilliseconds( scenePathsStart );

	const auto layersStart = Clock::now();
	writeU32( buffer, static_cast<std::uint32_t>( schema.layers.size() ), &payloadChecksum );
	for( const auto &layer : schema.layers )
	{
		writeU64( buffer, layer.layerId, &payloadChecksum );
		writeString( buffer, layer.name, &payloadChecksum );
		writeI32( buffer, layer.order, &payloadChecksum );
		writeBool( buffer, layer.enabled, &payloadChecksum );
		writeBool( buffer, layer.visible, &payloadChecksum );
		writeBool( buffer, layer.mute, &payloadChecksum );
		writeBool( buffer, layer.solo, &payloadChecksum );
		writeBool( buffer, layer.timeVarying, &payloadChecksum );
		writeU32( buffer, static_cast<std::uint32_t>( layer.mode ), &payloadChecksum );
		writeI32( buffer, layer.frameStart, &payloadChecksum );
		writeI32( buffer, layer.frameEnd, &payloadChecksum );
		writeBool( buffer, layer.holdOutsideRange, &payloadChecksum );
		writeU64( buffer, layer.firstStrokeId, &payloadChecksum );
		writeU64( buffer, layer.lastStrokeId, &payloadChecksum );
		writeBool( buffer, layer.colorEnabled, &payloadChecksum );
		writeF32List( buffer, layer.color.data(), 3, &payloadChecksum );
	}
	packTimings.packLayersMs = elapsedMilliseconds( layersStart );

	const auto strokesStart = Clock::now();
	writeU32( buffer, static_cast<std::uint32_t>( schema.strokes.size() ), &payloadChecksum );
	for( const auto &stroke : schema.strokes )
	{
		writeU64( buffer, stroke.strokeId, &payloadChecksum );
		writeU64( buffer, stroke.layerId, &payloadChecksum );
		writeString( buffer, stroke.name, &payloadChecksum );
		writeI32( buffer, stroke.order, &payloadChecksum );
		writeU32( buffer, static_cast<std::uint32_t>( stroke.mode ), &payloadChecksum );
		writeI32( buffer, stroke.frameStart, &payloadChecksum );
		writeI32( buffer, stroke.frameEnd, &payloadChecksum );
		writeU64( buffer, stroke.createdTimeUnixMicros, &payloadChecksum );
		writeU64( buffer, stroke.firstChunkId, &payloadChecksum );
		writeU64( buffer, stroke.lastChunkId, &payloadChecksum );
		writeU32( buffer, stroke.pointCount, &payloadChecksum );
		writeU32( buffer, stroke.targetCount, &payloadChecksum );
		writeU64( buffer, stroke.selectionMaskId, &payloadChecksum );
		writeBool( buffer, stroke.colorEnabled, &payloadChecksum );
		writeF32List( buffer, stroke.color.data(), 3, &payloadChecksum );
	}
	packTimings.packStrokesMs = elapsedMilliseconds( strokesStart );

	const auto chunksStart = Clock::now();
	writeU32( buffer, static_cast<std::uint32_t>( schema.chunks.size() ), &payloadChecksum );
	for( const auto &chunk : schema.chunks )
	{
		writeU64( buffer, chunk.chunkId, &payloadChecksum );
		writeU64( buffer, chunk.strokeId, &payloadChecksum );
		writeU32( buffer, chunk.chunkIndex, &payloadChecksum );
		writeU32( buffer, chunk.pointStart, &payloadChecksum );
		writeU32( buffer, chunk.pointCount, &payloadChecksum );
		writeU32( buffer, chunk.generation, &payloadChecksum );
		writeBool( buffer, chunk.deleted, &payloadChecksum );
	}
	packTimings.packChunksMs = elapsedMilliseconds( chunksStart );

	const auto pointsStart = Clock::now();
	const std::uint32_t pointCount = static_cast<std::uint32_t>( schema.points.size() );
	const size_t pointsSectionStart = buffer.size();
	const size_t pointsSectionSize = sizeof( pointCount ) + static_cast<size_t>( pointCount ) * PACKED_POINT_RECORD_SIZE;
	const size_t pointsCapacityBefore = buffer.capacity();
	const auto pointsReserveStart = Clock::now();
	buffer.reserve( pointsSectionStart + pointsSectionSize );
	packTimings.packPointsReserveMs = elapsedMilliseconds( pointsReserveStart );
	const size_t pointsCapacityAfterReserve = buffer.capacity();
	packTimings.packPointsReserveReallocated = pointsCapacityAfterReserve != pointsCapacityBefore ? 1.0 : 0.0;
	packTimings.packPointsCapacityBeforeBytes = static_cast<double>( pointsCapacityBefore );
	packTimings.packPointsCapacityAfterBytes = static_cast<double>( pointsCapacityAfterReserve );
	const auto pointsResizeStart = Clock::now();
	buffer.resize( pointsSectionStart + pointsSectionSize );
	packTimings.packPointsResizeMs = elapsedMilliseconds( pointsResizeStart );
	char *pointsSection = bufferData( buffer ) + pointsSectionStart;
	std::memcpy( pointsSection, &pointCount, sizeof( pointCount ) );
	PackedPointRecord *packedPoints = reinterpret_cast<PackedPointRecord *>( pointsSection + sizeof( pointCount ) );
	const auto pointsFillStart = Clock::now();
	const PointRecord *point = schema.points.data();
	const PointRecord *pointEnd = point + schema.points.size();
	PackedPointRecord *packedPoint = packedPoints;
	for( ; point != pointEnd; ++point, ++packedPoint )
	{
		std::memcpy( packedPoint, point, PACKED_POINT_PREFIX_SIZE );
		packedPoint->valid = point->valid ? 1 : 0;
		packedPoint->lastValidFrame = point->lastValidFrame;
		packedPoint->anchorModeUsed = static_cast<std::uint32_t>( point->anchorModeUsed );
		packedPoint->topologyGeneration = point->topologyGeneration;
		packedPoint->colorEnabled = point->colorEnabled ? 1 : 0;
		std::memcpy( packedPoint->color.data(), point->color.data(), sizeof( packedPoint->color ) );
	}
	packTimings.packPointsFillMs = elapsedMilliseconds( pointsFillStart );
	packTimings.packPointsCopyMs = 0.0;
	packTimings.packPointsChecksumInlineMs = 0.0;
	const auto pointsChecksumStart = Clock::now();
	checksum64Update( payloadChecksum, pointsSection, pointsSectionSize );
	packTimings.packPointsChecksumMs = elapsedMilliseconds( pointsChecksumStart );
	packTimings.packPointsMs = elapsedMilliseconds( pointsStart );

	const auto selectionSetsStart = Clock::now();
	writeU32( buffer, static_cast<std::uint32_t>( schema.selectionSets.size() ), &payloadChecksum );
	for( const auto &selectionSet : schema.selectionSets )
	{
		writeU64( buffer, selectionSet.selectionSetId, &payloadChecksum );
		writeString( buffer, selectionSet.name, &payloadChecksum );
		writeU64List( buffer, selectionSet.pointIds, &payloadChecksum );
		writeU64List( buffer, selectionSet.strokeIds, &payloadChecksum );
	}
	writeU64List( buffer, schema.currentSelection.pointIds, &payloadChecksum );
	writeU64List( buffer, schema.currentSelection.strokeIds, &payloadChecksum );
	packTimings.packSelectionSetsMs = elapsedMilliseconds( selectionSetsStart );

	const auto diagnosticsStart = Clock::now();
	writeU64( buffer, schema.diagnostics.invalidPointCount, &payloadChecksum );
	writeU64( buffer, schema.diagnostics.invalidStrokeCount, &payloadChecksum );
	writeU64( buffer, schema.diagnostics.topologyMismatchCount, &payloadChecksum );
	writeI32( buffer, schema.diagnostics.failingFrame, &payloadChecksum );
	writeStringList( buffer, schema.diagnostics.failingTargetPaths, &payloadChecksum );
	writeString( buffer, schema.diagnostics.lastErrorMessage, &payloadChecksum );
	writeString( buffer, schema.diagnostics.validationSummary, &payloadChecksum );
	writeU32( buffer, static_cast<std::uint32_t>( schema.diagnostics.categories.size() ), &payloadChecksum );
	for( ValidationCategory category : schema.diagnostics.categories )
	{
		writeU32( buffer, static_cast<std::uint32_t>( category ), &payloadChecksum );
	}
	packTimings.packDiagnosticsMs = elapsedMilliseconds( diagnosticsStart );

	const auto upgradesStart = Clock::now();
	writeU32( buffer, static_cast<std::uint32_t>( schema.upgrades.size() ), &payloadChecksum );
	for( const auto &upgrade : schema.upgrades )
	{
		writeU32( buffer, upgrade.sourceSchemaVersion, &payloadChecksum );
		writeU32( buffer, upgrade.upgradedSchemaVersion, &payloadChecksum );
		writeString( buffer, upgrade.timestampUtc, &payloadChecksum );
		writeString( buffer, upgrade.report, &payloadChecksum );
	}
	packTimings.packUpgradesMs = elapsedMilliseconds( upgradesStart );

	const auto pointBackupsStart = Clock::now();
	writeU32( buffer, static_cast<std::uint32_t>( schema.pointBackups.size() ), &payloadChecksum );
	for( const auto &backupPair : schema.pointBackups )
	{
		writeU64( buffer, backupPair.first, &payloadChecksum );
		writeU32( buffer, backupPair.second.targetPathId, &payloadChecksum );
		writeU32( buffer, backupPair.second.triangleIndex, &payloadChecksum );
		writeF32List( buffer, backupPair.second.barycentric.data(), 3, &payloadChecksum );
		writeBool( buffer, backupPair.second.attachmentResolved, &payloadChecksum );
	}
	packTimings.packPointBackupsMs = elapsedMilliseconds( pointBackupsStart );

	const auto finalChecksumStart = Clock::now();
	const std::uint64_t checksum = payloadChecksum;
	packTimings.packFinalChecksumMs = elapsedMilliseconds( finalChecksumStart );
	const auto finalHeaderWriteStart = Clock::now();
	std::memcpy( bufferData( buffer ) + checksumOffset, &checksum, sizeof( checksum ) );
	packTimings.packFinalHeaderWriteMs = elapsedMilliseconds( finalHeaderWriteStart );
	packTimings.packFinalBufferMs = packTimings.packFinalChecksumMs + packTimings.packFinalHeaderWriteMs;
}

void requireAvailable( const std::string &blob, size_t offset, size_t size )
{
	if( offset + size > blob.size() )
	{
		throw std::runtime_error( "Unexpected end of scatter paint blob" );
	}
}

std::uint32_t readU32( const std::string &blob, size_t &offset )
{
	std::uint32_t value = 0;
	requireAvailable( blob, offset, sizeof( value ) );
	std::memcpy( &value, blob.data() + offset, sizeof( value ) );
	offset += sizeof( value );
	return value;
}

std::uint64_t readU64( const std::string &blob, size_t &offset )
{
	std::uint64_t value = 0;
	requireAvailable( blob, offset, sizeof( value ) );
	std::memcpy( &value, blob.data() + offset, sizeof( value ) );
	offset += sizeof( value );
	return value;
}

std::int32_t readI32( const std::string &blob, size_t &offset )
{
	std::int32_t value = 0;
	requireAvailable( blob, offset, sizeof( value ) );
	std::memcpy( &value, blob.data() + offset, sizeof( value ) );
	offset += sizeof( value );
	return value;
}

float readF32( const std::string &blob, size_t &offset )
{
	float value = 0;
	requireAvailable( blob, offset, sizeof( value ) );
	std::memcpy( &value, blob.data() + offset, sizeof( value ) );
	offset += sizeof( value );
	return value;
}

bool readBool( const std::string &blob, size_t &offset )
{
	requireAvailable( blob, offset, 1 );
	return blob[offset++] != 0;
}

std::string readString( const std::string &blob, size_t &offset )
{
	std::uint32_t size = readU32( blob, offset );
	requireAvailable( blob, offset, size );
	std::string value( blob.data() + offset, size );
	offset += size;
	return value;
}

std::vector<std::uint64_t> readU64List( const std::string &blob, size_t &offset )
{
	std::uint32_t count = readU32( blob, offset );
	std::vector<std::uint64_t> values;
	values.reserve( count );
	for( std::uint32_t i = 0; i < count; ++i )
	{
		values.push_back( readU64( blob, offset ) );
	}
	return values;
}

std::vector<std::string> readStringList( const std::string &blob, size_t &offset )
{
	std::uint32_t count = readU32( blob, offset );
	std::vector<std::string> values;
	values.reserve( count );
	for( std::uint32_t i = 0; i < count; ++i )
	{
		values.push_back( readString( blob, offset ) );
	}
	return values;
}

void readF32List( const std::string &blob, size_t &offset, float *values, size_t count )
{
	for( size_t i = 0; i < count; ++i )
	{
		values[i] = readF32( blob, offset );
	}
}

} // namespace

std::string utcTimestampNow()
{
	using namespace std::chrono;
	const auto now = system_clock::now();
	const std::time_t nowTime = system_clock::to_time_t( now );
	std::tm utcTm;
	gmtime_r( &nowTime, &utcTm );
	std::ostringstream stream;
	stream << std::put_time( &utcTm, "%Y-%m-%dT%H:%M:%SZ" );
	return stream.str();
}

std::uint64_t unixTimeMicros()
{
	using namespace std::chrono;
	return static_cast<std::uint64_t>( duration_cast<microseconds>( system_clock::now().time_since_epoch() ).count() );
}

std::string packLockMetadata( const LockMetadata &metadata )
{
	std::string payload;
	writeU32( payload, static_cast<std::uint32_t>( metadata.mode ) );
	writeString( payload, metadata.user );
	writeString( payload, metadata.host );
	writeString( payload, metadata.timestampUtc );
	writeString( payload, metadata.scriptPath );
	writeString( payload, metadata.projectPath );
	writeString( payload, metadata.sessionId );

	std::string buffer;
	buffer.append( LOCK_MAGIC.data(), LOCK_MAGIC.size() );
	writeU64( buffer, checksum64( payload ) );
	buffer.append( payload );
	return buffer;
}

LockMetadata unpackLockMetadata( const std::string &blob, std::string &error )
{
	LockMetadata metadata;
	if( blob.empty() )
	{
		return metadata;
	}

	if( blob.size() < LOCK_MAGIC.size() || std::memcmp( blob.data(), LOCK_MAGIC.data(), LOCK_MAGIC.size() ) != 0 )
	{
		error = "Invalid scatter paint lock: bad magic";
		return LockMetadata();
	}

	size_t offset = LOCK_MAGIC.size();
	try
	{
		const std::uint64_t checksum = readU64( blob, offset );
		if( checksum != checksum64( blob.data() + offset, blob.size() - offset ) )
		{
			error = "Invalid scatter paint lock: checksum mismatch";
			return LockMetadata();
		}

		metadata.mode = static_cast<LockMode>( readU32( blob, offset ) );
		metadata.user = readString( blob, offset );
		metadata.host = readString( blob, offset );
		metadata.timestampUtc = readString( blob, offset );
		metadata.scriptPath = readString( blob, offset );
		metadata.projectPath = readString( blob, offset );
		metadata.sessionId = readString( blob, offset );
	}
	catch( const std::exception &exc )
	{
		error = std::string( "Failed to decode scatter paint lock: " ) + exc.what();
		return LockMetadata();
	}

	return metadata;
}

bool isStaleLock( const LockMetadata &metadata, std::uint64_t staleSeconds )
{
	if( metadata.timestampUtc.empty() )
	{
		return true;
	}

	std::tm utcTm = {};
	std::istringstream stream( metadata.timestampUtc );
	stream >> std::get_time( &utcTm, "%Y-%m-%dT%H:%M:%SZ" );
	if( stream.fail() )
	{
		return true;
	}

#if defined(_GNU_SOURCE) || defined(__GLIBC__) || defined(__linux__)
	const std::time_t lockTime = timegm( &utcTm );
#else
	const std::time_t lockTime = std::mktime( &utcTm );
#endif
	if( lockTime == static_cast<std::time_t>( -1 ) )
	{
		return true;
	}

	const std::time_t now = std::time( nullptr );
	const std::time_t delta = now >= lockTime ? now - lockTime : lockTime - now;
	return static_cast<std::uint64_t>( delta ) > staleSeconds;
}

std::string packCacheSchema( const CacheSchema &schema, CachePackTimings *timings )
{
	std::string buffer;
	packCacheSchemaImpl( schema, buffer, timings );
	return buffer;
}

void packCacheSchema( const CacheSchema &schema, std::vector<unsigned char> &buffer, CachePackTimings *timings )
{
	packCacheSchemaImpl( schema, buffer, timings );
}

CacheSchema unpackCacheSchema( const std::string &blob, std::string &error, CacheUnpackTimings *timings, bool verifyChecksum )
{
	CacheSchema schema = defaultCacheSchema();
	CacheUnpackTimings localTimings;
	CacheUnpackTimings &unpackTimings = timings ? *timings : localTimings;
	if( blob.empty() )
	{
		return schema;
	}

	if( blob.size() < CacheHeader::magic.size() || std::memcmp( blob.data(), CacheHeader::magic.data(), CacheHeader::magic.size() ) != 0 )
	{
		error = "Invalid scatter paint blob: bad magic";
		return schema;
	}

	size_t offset = CacheHeader::magic.size();
	try
	{
		const auto headerStart = Clock::now();
		schema.header.endianMarker = readU32( blob, offset );
		schema.header.pluginVersionMajor = readU32( blob, offset );
		schema.header.pluginVersionMinor = readU32( blob, offset );
		schema.header.pluginVersionPatch = readU32( blob, offset );
		schema.header.contentFlags = readU32( blob, offset );
		schema.header.checksum = readU64( blob, offset );
		unpackTimings.unpackHeaderMs = elapsedMilliseconds( headerStart );

		if( schema.header.endianMarker != 0x01020304 )
		{
			error = "Invalid scatter paint blob: unsupported endian marker";
			return defaultCacheSchema();
		}

		if( verifyChecksum )
		{
			const auto checksumStart = Clock::now();
			const std::uint64_t payloadChecksum = checksum64( blob.data() + offset, blob.size() - offset );
			unpackTimings.unpackChecksumMs = elapsedMilliseconds( checksumStart );
			if( schema.header.checksum != payloadChecksum )
			{
				error = "Invalid scatter paint blob: checksum mismatch";
				return defaultCacheSchema();
			}
		}
		else
		{
			unpackTimings.unpackChecksumMs = 0.0;
		}

		std::uint32_t schemaVersion = readU32( blob, offset );
		if( schemaVersion != CacheHeader::schemaVersion )
		{
			error = "Invalid scatter paint blob: unsupported schema version " + std::to_string( schemaVersion );
			return defaultCacheSchema();
		}

		schema.nextIds.layer = readU64( blob, offset );
		schema.nextIds.stroke = readU64( blob, offset );
		schema.nextIds.point = readU64( blob, offset );
		schema.nextIds.selectionSet = readU64( blob, offset );
		schema.nextIds.chunk = readU64( blob, offset );

		const auto nodeStart = Clock::now();
		schema.node.storageMode = static_cast<CacheStorageMode>( readU32( blob, offset ) );
		schema.node.pathMode = static_cast<CachePathMode>( readU32( blob, offset ) );
		schema.node.projectRoot = readString( blob, offset );
		schema.node.cachePath = readString( blob, offset );
		schema.node.exportPreset = readString( blob, offset );
		schema.node.backupEnabled = readBool( blob, offset );
		schema.node.diagnosticsSnapshotEnabled = readBool( blob, offset );
		readF32List( blob, offset, schema.node.defaultColor.data(), 3 );
		unpackTimings.unpackNodeMs = elapsedMilliseconds( nodeStart );

		const auto lockStart = Clock::now();
		schema.lock.mode = static_cast<LockMode>( readU32( blob, offset ) );
		schema.lock.user = readString( blob, offset );
		schema.lock.host = readString( blob, offset );
		schema.lock.timestampUtc = readString( blob, offset );
		schema.lock.scriptPath = readString( blob, offset );
		schema.lock.projectPath = readString( blob, offset );
		schema.lock.sessionId = readString( blob, offset );
		unpackTimings.unpackLockMs = elapsedMilliseconds( lockStart );

		const auto scenePathsStart = Clock::now();
		schema.scenePaths = readStringList( blob, offset );
		schema.instanceSourcePaths = readStringList( blob, offset );
		unpackTimings.unpackScenePathsMs = elapsedMilliseconds( scenePathsStart );

		const auto layersStart = Clock::now();
		std::uint32_t layerCount = readU32( blob, offset );
		schema.layers.reserve( layerCount );
		for( std::uint32_t i = 0; i < layerCount; ++i )
		{
			LayerRecord layer;
			layer.layerId = readU64( blob, offset );
			layer.name = readString( blob, offset );
			layer.order = readI32( blob, offset );
			layer.enabled = readBool( blob, offset );
			layer.visible = readBool( blob, offset );
			layer.mute = readBool( blob, offset );
			layer.solo = readBool( blob, offset );
			layer.timeVarying = readBool( blob, offset );
			layer.mode = static_cast<StrokeFrameMode>( readU32( blob, offset ) );
			layer.frameStart = readI32( blob, offset );
			layer.frameEnd = readI32( blob, offset );
			layer.holdOutsideRange = readBool( blob, offset );
			layer.firstStrokeId = readU64( blob, offset );
			layer.lastStrokeId = readU64( blob, offset );
			layer.colorEnabled = readBool( blob, offset );
			readF32List( blob, offset, layer.color.data(), 3 );
			schema.layers.push_back( layer );
		}
		unpackTimings.unpackLayersMs = elapsedMilliseconds( layersStart );

		const auto strokesStart = Clock::now();
		std::uint32_t strokeCount = readU32( blob, offset );
		schema.strokes.reserve( strokeCount );
		for( std::uint32_t i = 0; i < strokeCount; ++i )
		{
			StrokeRecord stroke;
			stroke.strokeId = readU64( blob, offset );
			stroke.layerId = readU64( blob, offset );
			stroke.name = readString( blob, offset );
			stroke.order = readI32( blob, offset );
			stroke.mode = static_cast<StrokeFrameMode>( readU32( blob, offset ) );
			stroke.frameStart = readI32( blob, offset );
			stroke.frameEnd = readI32( blob, offset );
			stroke.createdTimeUnixMicros = readU64( blob, offset );
			stroke.firstChunkId = readU64( blob, offset );
			stroke.lastChunkId = readU64( blob, offset );
			stroke.pointCount = readU32( blob, offset );
			stroke.targetCount = readU32( blob, offset );
			stroke.selectionMaskId = readU64( blob, offset );
			stroke.colorEnabled = readBool( blob, offset );
			readF32List( blob, offset, stroke.color.data(), 3 );
			schema.strokes.push_back( stroke );
		}
		unpackTimings.unpackStrokesMs = elapsedMilliseconds( strokesStart );

		const auto chunksStart = Clock::now();
		std::uint32_t chunkCount = readU32( blob, offset );
		schema.chunks.reserve( chunkCount );
		for( std::uint32_t i = 0; i < chunkCount; ++i )
		{
			PointChunkRecord chunk;
			chunk.chunkId = readU64( blob, offset );
			chunk.strokeId = readU64( blob, offset );
			chunk.chunkIndex = readU32( blob, offset );
			chunk.pointStart = readU32( blob, offset );
			chunk.pointCount = readU32( blob, offset );
			chunk.generation = readU32( blob, offset );
			chunk.deleted = readBool( blob, offset );
			schema.chunks.push_back( chunk );
		}
		unpackTimings.unpackChunksMs = elapsedMilliseconds( chunksStart );

		const auto pointsStart = Clock::now();
		std::uint32_t pointCount = readU32( blob, offset );
		schema.points.reserve( pointCount );
		for( std::uint32_t i = 0; i < pointCount; ++i )
		{
		PointRecord point;
		requireAvailable( blob, offset, PACKED_POINT_RECORD_SIZE );
		const PackedPointRecord *packedPoint = reinterpret_cast<const PackedPointRecord *>( blob.data() + offset );
		point.pointId = packedPoint->pointId;
		point.strokeId = packedPoint->strokeId;
		point.layerId = packedPoint->layerId;
		point.targetPathId = packedPoint->targetPathId;
		point.instanceId = packedPoint->instanceId;
		point.instanceSourcePathId = packedPoint->instanceSourcePathId;
		point.triangleIndex = packedPoint->triangleIndex;
		point.barycentric = packedPoint->barycentric;
		point.restObjectP = packedPoint->restObjectP;
		point.restWorldP = packedPoint->restWorldP;
		point.restUV = packedPoint->restUV;
		point.restNormal = packedPoint->restNormal;
		point.restUp = packedPoint->restUp;
		point.width = packedPoint->width;
		point.uniformScale = packedPoint->uniformScale;
		point.seed = packedPoint->seed;
		point.normalSpin = packedPoint->normalSpin;
		point.tangentRotation = packedPoint->tangentRotation;
		point.pressureDensity = packedPoint->pressureDensity;
		point.pressureSoftness = packedPoint->pressureSoftness;
		point.valid = packedPoint->valid != 0;
		point.lastValidFrame = packedPoint->lastValidFrame;
		point.anchorModeUsed = static_cast<AnchorMode>( packedPoint->anchorModeUsed );
		point.topologyGeneration = packedPoint->topologyGeneration;
		point.colorEnabled = packedPoint->colorEnabled != 0;
		point.color = packedPoint->color;
			offset += PACKED_POINT_RECORD_SIZE;
			schema.points.push_back( point );
		}
		unpackTimings.unpackPointsMs = elapsedMilliseconds( pointsStart );

		const auto selectionSetsStart = Clock::now();
		std::uint32_t selectionSetCount = readU32( blob, offset );
		schema.selectionSets.reserve( selectionSetCount );
		for( std::uint32_t i = 0; i < selectionSetCount; ++i )
		{
			SelectionSetRecord selectionSet;
			selectionSet.selectionSetId = readU64( blob, offset );
			selectionSet.name = readString( blob, offset );
			selectionSet.pointIds = readU64List( blob, offset );
			selectionSet.strokeIds = readU64List( blob, offset );
			schema.selectionSets.push_back( selectionSet );
		}

		schema.currentSelection.pointIds = readU64List( blob, offset );
		schema.currentSelection.strokeIds = readU64List( blob, offset );
		unpackTimings.unpackSelectionSetsMs = elapsedMilliseconds( selectionSetsStart );

		const auto diagnosticsStart = Clock::now();
		schema.diagnostics.invalidPointCount = readU64( blob, offset );
		schema.diagnostics.invalidStrokeCount = readU64( blob, offset );
		schema.diagnostics.topologyMismatchCount = readU64( blob, offset );
		schema.diagnostics.failingFrame = readI32( blob, offset );
		schema.diagnostics.failingTargetPaths = readStringList( blob, offset );
		schema.diagnostics.lastErrorMessage = readString( blob, offset );
		schema.diagnostics.validationSummary = readString( blob, offset );

		std::uint32_t categoryCount = readU32( blob, offset );
		schema.diagnostics.categories.reserve( categoryCount );
		for( std::uint32_t i = 0; i < categoryCount; ++i )
		{
			schema.diagnostics.categories.push_back( static_cast<ValidationCategory>( readU32( blob, offset ) ) );
		}
		unpackTimings.unpackDiagnosticsMs = elapsedMilliseconds( diagnosticsStart );

		const auto upgradesStart = Clock::now();
		std::uint32_t upgradeCount = readU32( blob, offset );
		schema.upgrades.reserve( upgradeCount );
		for( std::uint32_t i = 0; i < upgradeCount; ++i )
		{
			UpgradeRecord upgrade;
			upgrade.sourceSchemaVersion = readU32( blob, offset );
			upgrade.upgradedSchemaVersion = readU32( blob, offset );
			upgrade.timestampUtc = readString( blob, offset );
			upgrade.report = readString( blob, offset );
			schema.upgrades.push_back( upgrade );
		}
		unpackTimings.unpackUpgradesMs = elapsedMilliseconds( upgradesStart );

		if( offset < blob.size() )
		{
			const auto pointBackupsStart = Clock::now();
			std::uint32_t backupCount = readU32( blob, offset );
			schema.pointBackups.reserve( backupCount );
			for( std::uint32_t i = 0; i < backupCount; ++i )
			{
				std::uint64_t pointId = readU64( blob, offset );
				PointBackupRecord backup;
				backup.targetPathId = readU32( blob, offset );
				backup.triangleIndex = readU32( blob, offset );
				readF32List( blob, offset, backup.barycentric.data(), 3 );
				backup.attachmentResolved = readBool( blob, offset );
				schema.pointBackups.emplace_back( pointId, backup );
			}
			unpackTimings.unpackPointBackupsMs = elapsedMilliseconds( pointBackupsStart );
		}

	}
	catch( const std::exception &exc )
	{
		error = std::string( "Failed to decode scatter paint blob: " ) + exc.what();
		return defaultCacheSchema();
	}

	return schema;
}

} // namespace GafferScatterPaint
