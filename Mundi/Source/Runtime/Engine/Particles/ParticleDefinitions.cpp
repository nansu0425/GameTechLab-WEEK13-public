#include "pch.h"
#include "ParticleDefinitions.h"
#include <cstring>

// 메모리 할당 (언리얼 엔진 호환)
void FParticleDataContainer::Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts)
{
	// 기존 메모리 해제
	Free();

	// 새로운 크기 저장
	ParticleDataNumBytes = InParticleDataNumBytes;
	ParticleIndicesNumShorts = InParticleIndicesNumShorts;

	// 전체 메모리 블록 크기 계산 (파티클 데이터 + 인덱스)
	MemBlockSize = ParticleDataNumBytes + (ParticleIndicesNumShorts * sizeof(uint16));

	if (MemBlockSize > 0)
	{
		// 메모리 할당
		ParticleData = new uint8[MemBlockSize];
		memset(ParticleData, 0, MemBlockSize);

		// 인덱스 포인터 설정 (파티클 데이터 뒤에 위치)
		ParticleIndices = (uint16*)(ParticleData + ParticleDataNumBytes);
	}
}

// 메모리 해제 (언리얼 엔진 호환)
void FParticleDataContainer::Free()
{
	if (ParticleData)
	{
		delete[] ParticleData;
		ParticleData = nullptr;
		ParticleIndices = nullptr;
	}

	MemBlockSize = 0;
	ParticleDataNumBytes = 0;
	ParticleIndicesNumShorts = 0;
}
