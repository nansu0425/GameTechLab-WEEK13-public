#pragma once

#include "Name.h"

/** 휠 설정에 필요한 최소 정보 */
struct FWheelSetup
{
    FName BoneName;                            // 바퀴를 연결할 Skeletal Mesh의 본 이름
	FVector DefaultPosition = FVector::Zero(); // 바퀴의 기본 위치 (로컬 오프셋)
    float WheelRadius = 0.0f;                  // 바퀴의 반지름
    bool  bIsDriveWheel = false;               // 구동륜 여부
	bool  bIsSteerableWheel = false;           // 조향륜 여부

	int32 BoneIndex = -1;                      // 본 인덱스 캐시 (자동 할당. UI에 노출 X)
};
