#include "BrushSimdDispatch.h"

#include "BrushCpuFeatures.h"

#include <cstdlib>
#include <string>

namespace GafferScatterPaint
{

namespace
{

const BrushResolveKernelSet &selectKernels()
{
	const char *envValue = std::getenv( "GAFFER_SCATTER_PAINT_SIMD" );
	const std::string mode = envValue && envValue[0] ? envValue : "auto";
	if( mode == "scalar" )
	{
		return brushResolveScalarKernels();
	}

	const BrushCpuFeatures features = detectBrushCpuFeatures();
	if( mode == "avx2" )
	{
		return ( features.avx2 && features.fma ) ? brushResolveAVX2Kernels() : brushResolveScalarKernels();
	}
	if( mode == "sse42" )
	{
		return features.sse42 ? brushResolveSSE42Kernels() : brushResolveScalarKernels();
	}
	if( features.avx2 && features.fma )
	{
		return brushResolveAVX2Kernels();
	}
	if( features.sse42 )
	{
		return brushResolveSSE42Kernels();
	}
	return brushResolveScalarKernels();
}

} // namespace

const BrushResolveKernelSet &brushResolveKernels()
{
	static const BrushResolveKernelSet &g_kernels = selectKernels();
	return g_kernels;
}

const char *brushResolveBackendName()
{
	return brushResolveKernels().backendName;
}

} // namespace GafferScatterPaint
