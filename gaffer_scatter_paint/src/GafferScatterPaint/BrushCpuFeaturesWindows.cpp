#include "BrushCpuFeatures.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace GafferScatterPaint
{

namespace
{

#if defined(_MSC_VER) && ( defined(_M_X64) || defined(_M_IX86) )
void cpuid( int output[4], int functionId, int subFunctionId = 0 )
{
	__cpuidex( output, functionId, subFunctionId );
}
#endif

} // namespace

BrushCpuFeatures detectBrushCpuFeatures()
{
	BrushCpuFeatures result;
#if defined(_MSC_VER) && ( defined(_M_X64) || defined(_M_IX86) )
	int registers[4] = {0, 0, 0, 0};
	cpuid( registers, 0 );
	const int maxFunction = registers[0];

	if( maxFunction >= 1 )
	{
		cpuid( registers, 1 );
		result.sse42 = ( registers[2] & ( 1 << 20 ) ) != 0;
		result.fma = ( registers[2] & ( 1 << 12 ) ) != 0;
	}

	if( maxFunction >= 7 )
	{
		cpuid( registers, 7 );
		result.avx2 = ( registers[1] & ( 1 << 5 ) ) != 0;
	}
#endif
	return result;
}

} // namespace GafferScatterPaint
