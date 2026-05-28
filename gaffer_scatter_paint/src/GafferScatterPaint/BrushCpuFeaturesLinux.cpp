#include "BrushCpuFeatures.h"

namespace GafferScatterPaint
{

BrushCpuFeatures detectBrushCpuFeatures()
{
	BrushCpuFeatures result;
#if defined(__x86_64__) || defined(__i386__)
	__builtin_cpu_init();
	result.sse42 = __builtin_cpu_supports( "sse4.2" );
	result.avx2 = __builtin_cpu_supports( "avx2" );
	result.fma = __builtin_cpu_supports( "fma" );
#endif
	return result;
}

} // namespace GafferScatterPaint
