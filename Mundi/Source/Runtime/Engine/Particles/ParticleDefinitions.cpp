#include "pch.h"
#include "ParticleDefinitions.h"
#include <cstring>
#include <malloc.h>  // _aligned_malloc, _aligned_free

// 언리얼 엔진 호환: 16바이트 정렬 메모리 할당
// FMemory::Malloc(Size, 16) 방식과 동일하게 캐시 라인 최적화
// 반환값: 할당 성공 시 true, 실패 시 false
bool FParticleDataContainer::Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts)
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
		// 16바이트 정렬 메모리 할당 (캐시 라인 최적화)
		// 언리얼 엔진: FMemory::Malloc(MemBlockSize, 16)과 동일
		ParticleData = static_cast<uint8*>(_aligned_malloc(MemBlockSize, 16));

		if (ParticleData)
		{
			memset(ParticleData, 0, MemBlockSize);

			// 인덱스 포인터 설정 (파티클 데이터 뒤에 위치)
			ParticleIndices = (uint16*)(ParticleData + ParticleDataNumBytes);
			return true;  // 할당 성공
		}
		else
		{
			// 할당 실패 시 초기화
			ParticleIndices = nullptr;
			MemBlockSize = 0;
			ParticleDataNumBytes = 0;
			ParticleIndicesNumShorts = 0;
			return false;  // 할당 실패
		}
	}

	// MemBlockSize가 0이면 할당할 필요 없음 (성공으로 간주)
	return true;
}

// 언리얼 엔진 호환: 정렬된 메모리 해제
void FParticleDataContainer::Free()
{
	if (ParticleData)
	{
		// 정렬된 메모리는 _aligned_free로 해제
		_aligned_free(ParticleData);
		ParticleData = nullptr;
		ParticleIndices = nullptr;
	}

	MemBlockSize = 0;
	ParticleDataNumBytes = 0;
	ParticleIndicesNumShorts = 0;
}
