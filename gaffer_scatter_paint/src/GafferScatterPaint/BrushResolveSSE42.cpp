#include "BrushResolveKernels.h"

#include <smmintrin.h>

namespace GafferScatterPaint
{

namespace
{

inline void setDistanceSquared( const ClosestPointTriangleInput &input, ClosestPointTriangleOutput &output )
{
	const float dx = output.point[0] - input.point[0];
	const float dy = output.point[1] - input.point[1];
	const float dz = output.point[2] - input.point[2];
	output.distanceSquared = ( dx * dx ) + ( dy * dy ) + ( dz * dz );
}

inline __m128 load3( const std::array<float, 3> &value )
{
	return _mm_set_ps( 0.0f, value[2], value[1], value[0] );
}

inline float dot3( __m128 a, __m128 b )
{
	return _mm_cvtss_f32( _mm_dp_ps( a, b, 0x71 ) );
}

inline std::array<float, 3> store3( __m128 value )
{
	alignas( 16 ) float tmp[4];
	_mm_store_ps( tmp, value );
	return {tmp[0], tmp[1], tmp[2]};
}

void closestPointOnTriangleSSE42( const ClosestPointTriangleInput &input, ClosestPointTriangleOutput &output )
{
	const __m128 a = load3( input.a );
	const __m128 b = load3( input.b );
	const __m128 c = load3( input.c );
	const __m128 point = load3( input.point );
	const __m128 ab = _mm_sub_ps( b, a );
	const __m128 ac = _mm_sub_ps( c, a );
	const __m128 ap = _mm_sub_ps( point, a );
	const float d1 = dot3( ab, ap );
	const float d2 = dot3( ac, ap );
	if( d1 <= 0.0f && d2 <= 0.0f )
	{
		output.point = input.a;
		output.barycentric = {1.0f, 0.0f, 0.0f};
		setDistanceSquared( input, output );
		return;
	}

	const __m128 bp = _mm_sub_ps( point, b );
	const float d3 = dot3( ab, bp );
	const float d4 = dot3( ac, bp );
	if( d3 >= 0.0f && d4 <= d3 )
	{
		output.point = input.b;
		output.barycentric = {0.0f, 1.0f, 0.0f};
		setDistanceSquared( input, output );
		return;
	}

	const float vc = ( d1 * d4 ) - ( d3 * d2 );
	if( vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f )
	{
		const float v = d1 / ( d1 - d3 );
		output.point = store3( _mm_add_ps( a, _mm_mul_ps( ab, _mm_set1_ps( v ) ) ) );
		output.barycentric = {1.0f - v, v, 0.0f};
		setDistanceSquared( input, output );
		return;
	}

	const __m128 cp = _mm_sub_ps( point, c );
	const float d5 = dot3( ab, cp );
	const float d6 = dot3( ac, cp );
	if( d6 >= 0.0f && d5 <= d6 )
	{
		output.point = input.c;
		output.barycentric = {0.0f, 0.0f, 1.0f};
		setDistanceSquared( input, output );
		return;
	}

	const float vb = ( d5 * d2 ) - ( d1 * d6 );
	if( vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f )
	{
		const float w = d2 / ( d2 - d6 );
		output.point = store3( _mm_add_ps( a, _mm_mul_ps( ac, _mm_set1_ps( w ) ) ) );
		output.barycentric = {1.0f - w, 0.0f, w};
		setDistanceSquared( input, output );
		return;
	}

	const float va = ( d3 * d6 ) - ( d5 * d4 );
	if( va <= 0.0f && ( d4 - d3 ) >= 0.0f && ( d5 - d6 ) >= 0.0f )
	{
		const __m128 bc = _mm_sub_ps( c, b );
		const float w = ( d4 - d3 ) / ( ( d4 - d3 ) + ( d5 - d6 ) );
		output.point = store3( _mm_add_ps( b, _mm_mul_ps( bc, _mm_set1_ps( w ) ) ) );
		output.barycentric = {0.0f, 1.0f - w, w};
		setDistanceSquared( input, output );
		return;
	}

	const float denominator = 1.0f / ( va + vb + vc );
	const float v = vb * denominator;
	const float w = vc * denominator;
	output.point = store3(
		_mm_add_ps(
			a,
			_mm_add_ps(
				_mm_mul_ps( ab, _mm_set1_ps( v ) ),
				_mm_mul_ps( ac, _mm_set1_ps( w ) )
			)
		)
	);
	output.barycentric = {1.0f - v - w, v, w};
	setDistanceSquared( input, output );
}

void closestPointOnTriangleBlockSSE42( const ClosestPointTriangleBlockInput &input, ClosestPointTriangleBlockOutput &output )
{
	for( int lane = 0; lane < 8; lane += 4 )
	{
		const __m128 ax = _mm_loadu_ps( input.ax + lane );
		const __m128 ay = _mm_loadu_ps( input.ay + lane );
		const __m128 az = _mm_loadu_ps( input.az + lane );
		const __m128 bx = _mm_loadu_ps( input.bx + lane );
		const __m128 by = _mm_loadu_ps( input.by + lane );
		const __m128 bz = _mm_loadu_ps( input.bz + lane );
		const __m128 cx = _mm_loadu_ps( input.cx + lane );
		const __m128 cy = _mm_loadu_ps( input.cy + lane );
		const __m128 cz = _mm_loadu_ps( input.cz + lane );
		alignas( 16 ) float axv[4], ayv[4], azv[4], bxv[4], byv[4], bzv[4], cxv[4], cyv[4], czv[4];
		_mm_store_ps( axv, ax ); _mm_store_ps( ayv, ay ); _mm_store_ps( azv, az );
		_mm_store_ps( bxv, bx ); _mm_store_ps( byv, by ); _mm_store_ps( bzv, bz );
		_mm_store_ps( cxv, cx ); _mm_store_ps( cyv, cy ); _mm_store_ps( czv, cz );
		for( int i = 0; i < 4; ++i )
		{
			ClosestPointTriangleInput laneInput;
			laneInput.a = { axv[i], ayv[i], azv[i] };
			laneInput.b = { bxv[i], byv[i], bzv[i] };
			laneInput.c = { cxv[i], cyv[i], czv[i] };
			laneInput.point = input.point;
			ClosestPointTriangleOutput laneOutput;
			closestPointOnTriangleSSE42( laneInput, laneOutput );
			const int dst = lane + i;
			output.pointX[dst] = laneOutput.point[0];
			output.pointY[dst] = laneOutput.point[1];
			output.pointZ[dst] = laneOutput.point[2];
			output.baryX[dst] = laneOutput.barycentric[0];
			output.baryY[dst] = laneOutput.barycentric[1];
			output.baryZ[dst] = laneOutput.barycentric[2];
			const float dx = laneOutput.point[0] - input.point[0];
			const float dy = laneOutput.point[1] - input.point[1];
			const float dz = laneOutput.point[2] - input.point[2];
			output.distanceSquared[dst] = ( dx * dx ) + ( dy * dy ) + ( dz * dz );
		}
	}
}

const BrushResolveKernelSet g_kernels = {
	"sse42",
	1,
	&closestPointOnTriangleSSE42,
	nullptr,
};

} // namespace

const BrushResolveKernelSet &brushResolveSSE42Kernels()
{
	return g_kernels;
}

} // namespace GafferScatterPaint
