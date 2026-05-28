#pragma once

#include "GafferScatterPaint/Export.h"

#include "Imath/ImathVec.h"

#include <cstdint>
#include <string>
#include <vector>

namespace GafferScatterPaint
{

struct GAFFERSCATTERPAINT_API BrushSampleData
{
	Imath::V3f point = Imath::V3f( 0.0f );
	Imath::V3f normal = Imath::V3f( 0.0f, 1.0f, 0.0f );
	Imath::V3f barycentric = Imath::V3f( 1.0f, 0.0f, 0.0f );
	Imath::V2f tangentRotation = Imath::V2f( 0.0f );
	float width = 0.001f;
	float scale = 1.0f;
	float normalSpin = 0.0f;
	float pressureDensity = 0.0f;
	float pressureSoftness = 0.0f;
	std::uint64_t seed = 0;
	std::string sourcePath;
	std::uint32_t instanceId = 0;
	std::string instanceSourcePath;
	std::string sampleOrigin;
	int triangleIndex = 0;
	int sampleSourceIndex = 0;
	int sampleExpansionIndex = 0;
	bool attachmentResolved = false;
	bool sampleExpanded = false;
	bool valid = true;
};

using BrushSampleDataList = std::vector<BrushSampleData>;

} // namespace GafferScatterPaint
