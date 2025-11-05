#include "pch.h"
#include "Vector.h"

void FVector::Log()
{
	char buf[160];
	sprintf_s(buf, "Vector(%f, %f, %f)", X, Y, Z);
	UE_LOG(buf);
}

const FMatrix FMatrix::ZUpToYUp = FMatrix
(
	0, 0, 1, 0,
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
);

const FMatrix FMatrix::YUpToZUp = FMatrix
(
	0, 1, 0, 0,
	0, 0, 1, 0,
	1, 0, 0, 0,
	0, 0, 0, 1
);


FMatrix FMatrix::OrthoMatrix(const FAABB & AABB)
{
	return OrthoMatrix(AABB.Max.X, AABB.Min.X, AABB.Max.Y, AABB.Min.Y, AABB.Max.Z, AABB.Min.Z);
}

FMatrix FMatrix::OrthoMatrix(float R, float L, float T, float B, float F, float N)
{
	// A, 0, 0, 0
	// 0, B, 0, 0,
	// 0, 0, C, 0,
	// D, E, F, 1
	const float M_A = 2 / (R - L);
	const float M_B = 2 / (T - B);
	const float M_C = 1 / (F - N);
	const float M_D = -(R + L) / (R - L);
	const float M_E = -(T + B) / (T - B);
	const float M_F = -N / (F - N);
	return FMatrix(
		M_A, 0, 0, 0,
		0, M_B, 0, 0,
		0, 0, M_C, 0,
		M_D, M_E, M_F, 1);
}

FMatrix FMatrix::CreateProjectionMatrix(float FieldOfView, float AspectRatio, float ViewWidth, float ViewHeight, float NearClip, float FarClip, float ZoomFactor, ECameraProjectionMode ProjectionMode)
{
	if (ProjectionMode == ECameraProjectionMode::Perspective)
	{
		return FMatrix::PerspectiveFovLH(
			DegreesToRadians(FieldOfView),
			AspectRatio,
			NearClip, FarClip);
	}
	else // Orthographic
	{
		// world unit = 100 pixels (예시)
		const float PixelsPerWorldUnit = 10.0f;

		// 뷰포트 크기를 월드 단위로 변환
		float OrthoWidth = (ViewWidth / PixelsPerWorldUnit) * ZoomFactor;
		float OrthoHeight = (ViewHeight / PixelsPerWorldUnit) * ZoomFactor;

		return FMatrix::OrthoLH(
			OrthoWidth,
			OrthoHeight,
			NearClip, FarClip);
	}
}