#pragma once

namespace GafferScatterPaint
{

struct BrushCpuFeatures
{
	bool sse42 = false;
	bool avx2 = false;
	bool fma = false;
};

BrushCpuFeatures detectBrushCpuFeatures();

} // namespace GafferScatterPaint
