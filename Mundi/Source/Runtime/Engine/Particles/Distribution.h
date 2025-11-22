#pragma once

#include "Vector.h"
#include "Color.h"
#include <cstdlib>
#include <ctime>

// 언리얼 엔진 호환: Distribution 시스템
// 파티클 모듈의 속성값을 유연하게 제어 (상수, 균등 분포, 커브 등)

// Distribution 타입
enum class EDistributionType : uint8
{
	Constant,           // 고정값
	Uniform,            // 균등 분포 (Min ~ Max)
	// 나중에 추가 가능: ConstantCurve, Parameter 등
};

// Float Distribution (수명, 속도 크기 등)
struct FDistributionFloat
{
	EDistributionType Type = EDistributionType::Constant;

	// Constant 모드
	float ConstantValue = 0.0f;

	// Uniform 모드 (Min ~ Max 랜덤)
	float MinValue = 0.0f;
	float MaxValue = 1.0f;

	// 생성자
	FDistributionFloat()
		: Type(EDistributionType::Constant)
		, ConstantValue(0.0f)
		, MinValue(0.0f)
		, MaxValue(1.0f)
	{
	}

	explicit FDistributionFloat(float InConstant)
		: Type(EDistributionType::Constant)
		, ConstantValue(InConstant)
		, MinValue(InConstant)
		, MaxValue(InConstant)
	{
	}

	FDistributionFloat(float InMin, float InMax)
		: Type(EDistributionType::Uniform)
		, ConstantValue(InMin)
		, MinValue(InMin)
		, MaxValue(InMax)
	{
	}

	// 값 가져오기
	float GetValue(float RandomStream = 0.0f) const
	{
		if (Type == EDistributionType::Constant)
		{
			return ConstantValue;
		}
		else if (Type == EDistributionType::Uniform)
		{
			// 균등 분포 랜덤 (Min ~ Max)
			float Random = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
			return MinValue + (MaxValue - MinValue) * Random;
		}

		return 0.0f;
	}

	// 정적 생성 헬퍼
	static FDistributionFloat MakeConstant(float Value)
	{
		FDistributionFloat Dist;
		Dist.Type = EDistributionType::Constant;
		Dist.ConstantValue = Value;
		return Dist;
	}

	static FDistributionFloat MakeUniform(float Min, float Max)
	{
		FDistributionFloat Dist;
		Dist.Type = EDistributionType::Uniform;
		Dist.MinValue = Min;
		Dist.MaxValue = Max;
		return Dist;
	}
};

// Vector Distribution (위치, 속도, 크기 등)
struct FDistributionVector
{
	EDistributionType Type = EDistributionType::Constant;

	// Constant 모드
	FVector ConstantValue = FVector(0.0f, 0.0f, 0.0f);

	// Uniform 모드 (각 축별로 Min ~ Max 랜덤)
	FVector MinValue = FVector(0.0f, 0.0f, 0.0f);
	FVector MaxValue = FVector(1.0f, 1.0f, 1.0f);

	// 생성자
	FDistributionVector()
		: Type(EDistributionType::Constant)
		, ConstantValue(FVector(0.0f, 0.0f, 0.0f))
		, MinValue(FVector(0.0f, 0.0f, 0.0f))
		, MaxValue(FVector(1.0f, 1.0f, 1.0f))
	{
	}

	explicit FDistributionVector(const FVector& InConstant)
		: Type(EDistributionType::Constant)
		, ConstantValue(InConstant)
		, MinValue(InConstant)
		, MaxValue(InConstant)
	{
	}

	FDistributionVector(const FVector& InMin, const FVector& InMax)
		: Type(EDistributionType::Uniform)
		, ConstantValue(InMin)
		, MinValue(InMin)
		, MaxValue(InMax)
	{
	}

	// 값 가져오기
	FVector GetValue(float RandomStream = 0.0f) const
	{
		if (Type == EDistributionType::Constant)
		{
			return ConstantValue;
		}
		else if (Type == EDistributionType::Uniform)
		{
			// 각 축별로 균등 분포 랜덤
			float RandomX = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
			float RandomY = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
			float RandomZ = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

			return FVector(
				MinValue.X + (MaxValue.X - MinValue.X) * RandomX,
				MinValue.Y + (MaxValue.Y - MinValue.Y) * RandomY,
				MinValue.Z + (MaxValue.Z - MinValue.Z) * RandomZ
			);
		}

		return FVector(0.0f, 0.0f, 0.0f);
	}

	// 정적 생성 헬퍼
	static FDistributionVector MakeConstant(const FVector& Value)
	{
		FDistributionVector Dist;
		Dist.Type = EDistributionType::Constant;
		Dist.ConstantValue = Value;
		return Dist;
	}

	static FDistributionVector MakeUniform(const FVector& Min, const FVector& Max)
	{
		FDistributionVector Dist;
		Dist.Type = EDistributionType::Uniform;
		Dist.MinValue = Min;
		Dist.MaxValue = Max;
		return Dist;
	}
};

// Color Distribution (색상)
struct FDistributionColor
{
	EDistributionType Type = EDistributionType::Constant;

	// Constant 모드
	FLinearColor ConstantValue = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	// Uniform 모드 (각 채널별로 Min ~ Max 랜덤)
	FLinearColor MinValue = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
	FLinearColor MaxValue = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	// 생성자
	FDistributionColor()
		: Type(EDistributionType::Constant)
		, ConstantValue(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
		, MinValue(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f))
		, MaxValue(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
	{
	}

	explicit FDistributionColor(const FLinearColor& InConstant)
		: Type(EDistributionType::Constant)
		, ConstantValue(InConstant)
		, MinValue(InConstant)
		, MaxValue(InConstant)
	{
	}

	FDistributionColor(const FLinearColor& InMin, const FLinearColor& InMax)
		: Type(EDistributionType::Uniform)
		, ConstantValue(InMin)
		, MinValue(InMin)
		, MaxValue(InMax)
	{
	}

	// 값 가져오기
	FLinearColor GetValue(float RandomStream = 0.0f) const
	{
		if (Type == EDistributionType::Constant)
		{
			return ConstantValue;
		}
		else if (Type == EDistributionType::Uniform)
		{
			// 각 채널별로 균등 분포 랜덤
			float RandomR = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
			float RandomG = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
			float RandomB = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
			float RandomA = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

			return FLinearColor(
				MinValue.R + (MaxValue.R - MinValue.R) * RandomR,
				MinValue.G + (MaxValue.G - MinValue.G) * RandomG,
				MinValue.B + (MaxValue.B - MinValue.B) * RandomB,
				MinValue.A + (MaxValue.A - MinValue.A) * RandomA
			);
		}

		return FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	}

	// 정적 생성 헬퍼
	static FDistributionColor MakeConstant(const FLinearColor& Value)
	{
		FDistributionColor Dist;
		Dist.Type = EDistributionType::Constant;
		Dist.ConstantValue = Value;
		return Dist;
	}

	static FDistributionColor MakeUniform(const FLinearColor& Min, const FLinearColor& Max)
	{
		FDistributionColor Dist;
		Dist.Type = EDistributionType::Uniform;
		Dist.MinValue = Min;
		Dist.MaxValue = Max;
		return Dist;
	}
};
