#pragma once

#include <array>

namespace GafferScatterPaint
{

struct ClosestPointTriangleInput
{
	std::array<float, 3> a = {0.0f, 0.0f, 0.0f};
	std::array<float, 3> b = {0.0f, 0.0f, 0.0f};
	std::array<float, 3> c = {0.0f, 0.0f, 0.0f};
	std::array<float, 3> point = {0.0f, 0.0f, 0.0f};
};

struct ClosestPointTriangleOutput
{
	std::array<float, 3> point = {0.0f, 0.0f, 0.0f};
	std::array<float, 3> barycentric = {1.0f, 0.0f, 0.0f};
	float distanceSquared = 0.0f;
};

struct ClosestPointTriangleBlockInput
{
	const float *ax = nullptr;
	const float *ay = nullptr;
	const float *az = nullptr;
	const float *bx = nullptr;
	const float *by = nullptr;
	const float *bz = nullptr;
	const float *cx = nullptr;
	const float *cy = nullptr;
	const float *cz = nullptr;
	std::array<float, 3> point = {0.0f, 0.0f, 0.0f};
};

struct ClosestPointTriangleBlockOutput
{
	std::array<float, 8> pointX = {0.0f};
	std::array<float, 8> pointY = {0.0f};
	std::array<float, 8> pointZ = {0.0f};
	std::array<float, 8> baryX = {1.0f};
	std::array<float, 8> baryY = {0.0f};
	std::array<float, 8> baryZ = {0.0f};
	std::array<float, 8> distanceSquared = {0.0f};
};

using ClosestPointTriangleFn = void (*)( const ClosestPointTriangleInput &input, ClosestPointTriangleOutput &output );
using ClosestPointTriangleBlockFn = void (*)( const ClosestPointTriangleBlockInput &input, ClosestPointTriangleBlockOutput &output );

struct BrushResolveKernelSet
{
	const char *backendName = "scalar";
	int blockWidth = 1;
	ClosestPointTriangleFn closestPointOnTriangle = nullptr;
	ClosestPointTriangleBlockFn closestPointOnTriangleBlock = nullptr;
};

const BrushResolveKernelSet &brushResolveScalarKernels();
const BrushResolveKernelSet &brushResolveSSE42Kernels();
const BrushResolveKernelSet &brushResolveAVX2Kernels();

} // namespace GafferScatterPaint
