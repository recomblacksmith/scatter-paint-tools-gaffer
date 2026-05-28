#include "BrushResolveKernels.h"

#include <immintrin.h>

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

void closestPointOnTriangleAVX2( const ClosestPointTriangleInput &input, ClosestPointTriangleOutput &output )
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
		output.point = store3( _mm_fmadd_ps( ab, _mm_set1_ps( v ), a ) );
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
		output.point = store3( _mm_fmadd_ps( ac, _mm_set1_ps( w ), a ) );
		output.barycentric = {1.0f - w, 0.0f, w};
		setDistanceSquared( input, output );
		return;
	}

	const float va = ( d3 * d6 ) - ( d5 * d4 );
	if( va <= 0.0f && ( d4 - d3 ) >= 0.0f && ( d5 - d6 ) >= 0.0f )
	{
		const __m128 bc = _mm_sub_ps( c, b );
		const float w = ( d4 - d3 ) / ( ( d4 - d3 ) + ( d5 - d6 ) );
		output.point = store3( _mm_fmadd_ps( bc, _mm_set1_ps( w ), b ) );
		output.barycentric = {0.0f, 1.0f - w, w};
		setDistanceSquared( input, output );
		return;
	}

	const float denominator = 1.0f / ( va + vb + vc );
	const float v = vb * denominator;
	const float w = vc * denominator;
	output.point = store3( _mm_fmadd_ps( ac, _mm_set1_ps( w ), _mm_fmadd_ps( ab, _mm_set1_ps( v ), a ) ) );
	output.barycentric = {1.0f - v - w, v, w};
	setDistanceSquared( input, output );
}

void closestPointOnTriangleBlockAVX2( const ClosestPointTriangleBlockInput &input, ClosestPointTriangleBlockOutput &output )
{
	const __m256 zero = _mm256_set1_ps( 0.0f );
	const __m256 one = _mm256_set1_ps( 1.0f );
	const __m256 px = _mm256_set1_ps( input.point[0] );
	const __m256 py = _mm256_set1_ps( input.point[1] );
	const __m256 pz = _mm256_set1_ps( input.point[2] );

	const __m256 ax = _mm256_loadu_ps( input.ax );
	const __m256 ay = _mm256_loadu_ps( input.ay );
	const __m256 az = _mm256_loadu_ps( input.az );
	const __m256 bx = _mm256_loadu_ps( input.bx );
	const __m256 by = _mm256_loadu_ps( input.by );
	const __m256 bz = _mm256_loadu_ps( input.bz );
	const __m256 cx = _mm256_loadu_ps( input.cx );
	const __m256 cy = _mm256_loadu_ps( input.cy );
	const __m256 cz = _mm256_loadu_ps( input.cz );

	const __m256 abx = _mm256_sub_ps( bx, ax );
	const __m256 aby = _mm256_sub_ps( by, ay );
	const __m256 abz = _mm256_sub_ps( bz, az );
	const __m256 acx = _mm256_sub_ps( cx, ax );
	const __m256 acy = _mm256_sub_ps( cy, ay );
	const __m256 acz = _mm256_sub_ps( cz, az );
	const __m256 apx = _mm256_sub_ps( px, ax );
	const __m256 apy = _mm256_sub_ps( py, ay );
	const __m256 apz = _mm256_sub_ps( pz, az );

	auto dot3 = []( __m256 x0, __m256 y0, __m256 z0, __m256 x1, __m256 y1, __m256 z1 ) {
		return _mm256_add_ps( _mm256_add_ps( _mm256_mul_ps( x0, x1 ), _mm256_mul_ps( y0, y1 ) ), _mm256_mul_ps( z0, z1 ) );
	};
	auto blend = []( __m256 oldValue, __m256 newValue, __m256 mask ) {
		return _mm256_blendv_ps( oldValue, newValue, mask );
	};
	auto andnot = []( __m256 a, __m256 b ) {
		return _mm256_andnot_ps( a, b );
	};

	const __m256 d1 = dot3( abx, aby, abz, apx, apy, apz );
	const __m256 d2 = dot3( acx, acy, acz, apx, apy, apz );

	__m256 outPx = zero;
	__m256 outPy = zero;
	__m256 outPz = zero;
	__m256 outBx = one;
	__m256 outBy = zero;
	__m256 outBz = zero;
	__m256 assigned = zero;

	__m256 mask = _mm256_and_ps(
		_mm256_cmp_ps( d1, zero, _CMP_LE_OQ ),
		_mm256_cmp_ps( d2, zero, _CMP_LE_OQ )
	);
	outPx = blend( outPx, ax, mask );
	outPy = blend( outPy, ay, mask );
	outPz = blend( outPz, az, mask );
	assigned = _mm256_or_ps( assigned, mask );

	const __m256 bpx = _mm256_sub_ps( px, bx );
	const __m256 bpy = _mm256_sub_ps( py, by );
	const __m256 bpz = _mm256_sub_ps( pz, bz );
	const __m256 d3 = dot3( abx, aby, abz, bpx, bpy, bpz );
	const __m256 d4 = dot3( acx, acy, acz, bpx, bpy, bpz );
	mask = _mm256_and_ps(
		andnot( assigned, _mm256_cmp_ps( d3, zero, _CMP_GE_OQ ) ),
		_mm256_cmp_ps( d4, d3, _CMP_LE_OQ )
	);
	outPx = blend( outPx, bx, mask );
	outPy = blend( outPy, by, mask );
	outPz = blend( outPz, bz, mask );
	outBx = blend( outBx, zero, mask );
	outBy = blend( outBy, one, mask );
	outBz = blend( outBz, zero, mask );
	assigned = _mm256_or_ps( assigned, mask );

	const __m256 vc = _mm256_sub_ps( _mm256_mul_ps( d1, d4 ), _mm256_mul_ps( d3, d2 ) );
	const __m256 edgeVMask = _mm256_and_ps(
		andnot( assigned, _mm256_cmp_ps( vc, zero, _CMP_LE_OQ ) ),
		_mm256_and_ps( _mm256_cmp_ps( d1, zero, _CMP_GE_OQ ), _mm256_cmp_ps( d3, zero, _CMP_LE_OQ ) )
	);
	const __m256 vEdge = _mm256_div_ps( d1, _mm256_sub_ps( d1, d3 ) );
	const __m256 edgeVx = _mm256_fmadd_ps( abx, vEdge, ax );
	const __m256 edgeVy = _mm256_fmadd_ps( aby, vEdge, ay );
	const __m256 edgeVz = _mm256_fmadd_ps( abz, vEdge, az );
	outPx = blend( outPx, edgeVx, edgeVMask );
	outPy = blend( outPy, edgeVy, edgeVMask );
	outPz = blend( outPz, edgeVz, edgeVMask );
	outBx = blend( outBx, _mm256_sub_ps( one, vEdge ), edgeVMask );
	outBy = blend( outBy, vEdge, edgeVMask );
	outBz = blend( outBz, zero, edgeVMask );
	assigned = _mm256_or_ps( assigned, edgeVMask );

	const __m256 cpx = _mm256_sub_ps( px, cx );
	const __m256 cpy = _mm256_sub_ps( py, cy );
	const __m256 cpz = _mm256_sub_ps( pz, cz );
	const __m256 d5 = dot3( abx, aby, abz, cpx, cpy, cpz );
	const __m256 d6 = dot3( acx, acy, acz, cpx, cpy, cpz );
	mask = _mm256_and_ps(
		andnot( assigned, _mm256_cmp_ps( d6, zero, _CMP_GE_OQ ) ),
		_mm256_cmp_ps( d5, d6, _CMP_LE_OQ )
	);
	outPx = blend( outPx, cx, mask );
	outPy = blend( outPy, cy, mask );
	outPz = blend( outPz, cz, mask );
	outBx = blend( outBx, zero, mask );
	outBy = blend( outBy, zero, mask );
	outBz = blend( outBz, one, mask );
	assigned = _mm256_or_ps( assigned, mask );

	const __m256 vb = _mm256_sub_ps( _mm256_mul_ps( d5, d2 ), _mm256_mul_ps( d1, d6 ) );
	const __m256 edgeWMask = _mm256_and_ps(
		andnot( assigned, _mm256_cmp_ps( vb, zero, _CMP_LE_OQ ) ),
		_mm256_and_ps( _mm256_cmp_ps( d2, zero, _CMP_GE_OQ ), _mm256_cmp_ps( d6, zero, _CMP_LE_OQ ) )
	);
	const __m256 wEdge = _mm256_div_ps( d2, _mm256_sub_ps( d2, d6 ) );
	const __m256 edgeWx = _mm256_fmadd_ps( acx, wEdge, ax );
	const __m256 edgeWy = _mm256_fmadd_ps( acy, wEdge, ay );
	const __m256 edgeWz = _mm256_fmadd_ps( acz, wEdge, az );
	outPx = blend( outPx, edgeWx, edgeWMask );
	outPy = blend( outPy, edgeWy, edgeWMask );
	outPz = blend( outPz, edgeWz, edgeWMask );
	outBx = blend( outBx, _mm256_sub_ps( one, wEdge ), edgeWMask );
	outBy = blend( outBy, zero, edgeWMask );
	outBz = blend( outBz, wEdge, edgeWMask );
	assigned = _mm256_or_ps( assigned, edgeWMask );

	const __m256 va = _mm256_sub_ps( _mm256_mul_ps( d3, d6 ), _mm256_mul_ps( d5, d4 ) );
	const __m256 d43 = _mm256_sub_ps( d4, d3 );
	const __m256 d56 = _mm256_sub_ps( d5, d6 );
	const __m256 edgeBCMask = _mm256_and_ps(
		andnot( assigned, _mm256_cmp_ps( va, zero, _CMP_LE_OQ ) ),
		_mm256_and_ps( _mm256_cmp_ps( d43, zero, _CMP_GE_OQ ), _mm256_cmp_ps( d56, zero, _CMP_GE_OQ ) )
	);
	const __m256 bcx = _mm256_sub_ps( cx, bx );
	const __m256 bcy = _mm256_sub_ps( cy, by );
	const __m256 bcz = _mm256_sub_ps( cz, bz );
	const __m256 bcW = _mm256_div_ps( d43, _mm256_add_ps( d43, d56 ) );
	const __m256 edgeBCx = _mm256_fmadd_ps( bcx, bcW, bx );
	const __m256 edgeBCy = _mm256_fmadd_ps( bcy, bcW, by );
	const __m256 edgeBCz = _mm256_fmadd_ps( bcz, bcW, bz );
	outPx = blend( outPx, edgeBCx, edgeBCMask );
	outPy = blend( outPy, edgeBCy, edgeBCMask );
	outPz = blend( outPz, edgeBCz, edgeBCMask );
	outBx = blend( outBx, zero, edgeBCMask );
	outBy = blend( outBy, _mm256_sub_ps( one, bcW ), edgeBCMask );
	outBz = blend( outBz, bcW, edgeBCMask );
	assigned = _mm256_or_ps( assigned, edgeBCMask );

	const __m256 remainingMask = _mm256_cmp_ps( assigned, zero, _CMP_EQ_OQ );
	const __m256 denominator = _mm256_div_ps( one, _mm256_add_ps( va, _mm256_add_ps( vb, vc ) ) );
	const __m256 vFace = _mm256_mul_ps( vb, denominator );
	const __m256 wFace = _mm256_mul_ps( vc, denominator );
	const __m256 faceX = _mm256_fmadd_ps( acx, wFace, _mm256_fmadd_ps( abx, vFace, ax ) );
	const __m256 faceY = _mm256_fmadd_ps( acy, wFace, _mm256_fmadd_ps( aby, vFace, ay ) );
	const __m256 faceZ = _mm256_fmadd_ps( acz, wFace, _mm256_fmadd_ps( abz, vFace, az ) );
	outPx = blend( outPx, faceX, remainingMask );
	outPy = blend( outPy, faceY, remainingMask );
	outPz = blend( outPz, faceZ, remainingMask );
	outBx = blend( outBx, _mm256_sub_ps( one, _mm256_add_ps( vFace, wFace ) ), remainingMask );
	outBy = blend( outBy, vFace, remainingMask );
	outBz = blend( outBz, wFace, remainingMask );

	const __m256 dx = _mm256_sub_ps( outPx, px );
	const __m256 dy = _mm256_sub_ps( outPy, py );
	const __m256 dz = _mm256_sub_ps( outPz, pz );
	const __m256 dist2 = dot3( dx, dy, dz, dx, dy, dz );

	_mm256_storeu_ps( output.pointX.data(), outPx );
	_mm256_storeu_ps( output.pointY.data(), outPy );
	_mm256_storeu_ps( output.pointZ.data(), outPz );
	_mm256_storeu_ps( output.baryX.data(), outBx );
	_mm256_storeu_ps( output.baryY.data(), outBy );
	_mm256_storeu_ps( output.baryZ.data(), outBz );
	_mm256_storeu_ps( output.distanceSquared.data(), dist2 );
}

const BrushResolveKernelSet g_kernels = {
	"avx2",
	1,
	&closestPointOnTriangleAVX2,
	nullptr,
};

} // namespace

const BrushResolveKernelSet &brushResolveAVX2Kernels()
{
	return g_kernels;
}

} // namespace GafferScatterPaint
