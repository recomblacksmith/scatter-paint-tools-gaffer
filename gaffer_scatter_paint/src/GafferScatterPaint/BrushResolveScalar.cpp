#include "BrushResolveKernels.h"

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

ClosestPointTriangleOutput closestPointOnTriangleScalarResult( const ClosestPointTriangleInput &input )
{
	ClosestPointTriangleOutput output;
	const float ax = input.a[0];
	const float ay = input.a[1];
	const float az = input.a[2];
	const float bx = input.b[0];
	const float by = input.b[1];
	const float bz = input.b[2];
	const float cx = input.c[0];
	const float cy = input.c[1];
	const float cz = input.c[2];
	const float px = input.point[0];
	const float py = input.point[1];
	const float pz = input.point[2];

	const float abx = bx - ax;
	const float aby = by - ay;
	const float abz = bz - az;
	const float acx = cx - ax;
	const float acy = cy - ay;
	const float acz = cz - az;
	const float apx = px - ax;
	const float apy = py - ay;
	const float apz = pz - az;
	const float d1 = ( abx * apx ) + ( aby * apy ) + ( abz * apz );
	const float d2 = ( acx * apx ) + ( acy * apy ) + ( acz * apz );
	if( d1 <= 0.0f && d2 <= 0.0f )
	{
		output.point = input.a;
		output.barycentric = {1.0f, 0.0f, 0.0f};
		setDistanceSquared( input, output );
		return output;
	}

	const float bpx = px - bx;
	const float bpy = py - by;
	const float bpz = pz - bz;
	const float d3 = ( abx * bpx ) + ( aby * bpy ) + ( abz * bpz );
	const float d4 = ( acx * bpx ) + ( acy * bpy ) + ( acz * bpz );
	if( d3 >= 0.0f && d4 <= d3 )
	{
		output.point = input.b;
		output.barycentric = {0.0f, 1.0f, 0.0f};
		setDistanceSquared( input, output );
		return output;
	}

	const float vc = ( d1 * d4 ) - ( d3 * d2 );
	if( vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f )
	{
		const float v = d1 / ( d1 - d3 );
		output.point = {ax + ( abx * v ), ay + ( aby * v ), az + ( abz * v )};
		output.barycentric = {1.0f - v, v, 0.0f};
		setDistanceSquared( input, output );
		return output;
	}

	const float cpx = px - cx;
	const float cpy = py - cy;
	const float cpz = pz - cz;
	const float d5 = ( abx * cpx ) + ( aby * cpy ) + ( abz * cpz );
	const float d6 = ( acx * cpx ) + ( acy * cpy ) + ( acz * cpz );
	if( d6 >= 0.0f && d5 <= d6 )
	{
		output.point = input.c;
		output.barycentric = {0.0f, 0.0f, 1.0f};
		setDistanceSquared( input, output );
		return output;
	}

	const float vb = ( d5 * d2 ) - ( d1 * d6 );
	if( vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f )
	{
		const float w = d2 / ( d2 - d6 );
		output.point = {ax + ( acx * w ), ay + ( acy * w ), az + ( acz * w )};
		output.barycentric = {1.0f - w, 0.0f, w};
		setDistanceSquared( input, output );
		return output;
	}

	const float va = ( d3 * d6 ) - ( d5 * d4 );
	if( va <= 0.0f && ( d4 - d3 ) >= 0.0f && ( d5 - d6 ) >= 0.0f )
	{
		const float bcx = cx - bx;
		const float bcy = cy - by;
		const float bcz = cz - bz;
		const float w = ( d4 - d3 ) / ( ( d4 - d3 ) + ( d5 - d6 ) );
		output.point = {bx + ( bcx * w ), by + ( bcy * w ), bz + ( bcz * w )};
		output.barycentric = {0.0f, 1.0f - w, w};
		setDistanceSquared( input, output );
		return output;
	}

	const float denominator = 1.0f / ( va + vb + vc );
	const float v = vb * denominator;
	const float w = vc * denominator;
	output.point = {
		ax + ( abx * v ) + ( acx * w ),
		ay + ( aby * v ) + ( acy * w ),
		az + ( abz * v ) + ( acz * w )
	};
	output.barycentric = {1.0f - v - w, v, w};
	setDistanceSquared( input, output );
	return output;
}

void closestPointOnTriangleScalar( const ClosestPointTriangleInput &input, ClosestPointTriangleOutput &output )
{
	output = closestPointOnTriangleScalarResult( input );
}

void closestPointOnTriangleBlockScalar( const ClosestPointTriangleBlockInput &input, ClosestPointTriangleBlockOutput &output )
{
	for( int i = 0; i < 8; ++i )
	{
		ClosestPointTriangleInput laneInput;
		laneInput.a = { input.ax[i], input.ay[i], input.az[i] };
		laneInput.b = { input.bx[i], input.by[i], input.bz[i] };
		laneInput.c = { input.cx[i], input.cy[i], input.cz[i] };
		laneInput.point = input.point;
		const ClosestPointTriangleOutput laneOutput = closestPointOnTriangleScalarResult( laneInput );
		output.pointX[i] = laneOutput.point[0];
		output.pointY[i] = laneOutput.point[1];
		output.pointZ[i] = laneOutput.point[2];
		output.baryX[i] = laneOutput.barycentric[0];
		output.baryY[i] = laneOutput.barycentric[1];
		output.baryZ[i] = laneOutput.barycentric[2];
		const float dx = laneOutput.point[0] - input.point[0];
		const float dy = laneOutput.point[1] - input.point[1];
		const float dz = laneOutput.point[2] - input.point[2];
		output.distanceSquared[i] = ( dx * dx ) + ( dy * dy ) + ( dz * dz );
	}
}

const BrushResolveKernelSet g_kernels = {
	"scalar",
	1,
	&closestPointOnTriangleScalar,
	nullptr,
};

} // namespace

const BrushResolveKernelSet &brushResolveScalarKernels()
{
	return g_kernels;
}

} // namespace GafferScatterPaint
