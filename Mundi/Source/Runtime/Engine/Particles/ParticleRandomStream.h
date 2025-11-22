#pragma once

#include "Vector.h"
#include <random>

// 언리얼 엔진 호환: 결정론적 랜덤 시스템
// 파티클 시스템에서 재현 가능한 랜덤 결과를 제공
class FParticleRandomStream
{
public:
	FParticleRandomStream()
		: Generator(0)
		, Distribution(0.0f, 1.0f)
	{
	}

	explicit FParticleRandomStream(int32 Seed)
		: Generator(Seed)
		, Distribution(0.0f, 1.0f)
	{
	}

	// 시드 설정 (결정론적 시뮬레이션)
	void Initialize(int32 Seed)
	{
		Generator.seed(Seed);
	}

	// 0.0 ~ 1.0 범위 float 생성
	float GetFraction()
	{
		return Distribution(Generator);
	}

	// Min ~ Max 범위 float 생성
	float GetRangeFloat(float Min, float Max)
	{
		return Min + (Max - Min) * GetFraction();
	}

	// FVector 랜덤 생성 (각 축별로 Min ~ Max 범위)
	FVector GetRangeVector(const FVector& Min, const FVector& Max)
	{
		return FVector(
			GetRangeFloat(Min.X, Max.X),
			GetRangeFloat(Min.Y, Max.Y),
			GetRangeFloat(Min.Z, Max.Z)
		);
	}

	// 균등 분포 단위 벡터 생성 (구 표면)
	FVector GetUnitVector()
	{
		// 언리얼 엔진 호환: 균등 분포 구 표면 샘플링
		float Theta = GetRangeFloat(0.0f, 2.0f * 3.14159265f);  // 0 ~ 2π
		float Phi = acosf(2.0f * GetFraction() - 1.0f);          // 0 ~ π (균등 분포)

		float SinPhi = sinf(Phi);
		return FVector(
			SinPhi * cosf(Theta),
			SinPhi * sinf(Theta),
			cosf(Phi)
		);
	}

	// -1.0 ~ 1.0 범위 float 생성
	float GetSignedFraction()
	{
		return 2.0f * GetFraction() - 1.0f;
	}

private:
	std::mt19937 Generator;
	std::uniform_real_distribution<float> Distribution;
};
