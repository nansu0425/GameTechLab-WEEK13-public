#pragma once

#include "Name.h"

/** 휠 설정에 필요한 최소 정보 */
struct FWheelSetup
{
    FName BoneName;             // 바퀴를 연결할 Skeletal Mesh의 본 이름
    float SuspensionOffsetZ = 0.0f;    // 서스펜션 Z축 오프셋
    float WheelRadius = 0.0f;          // 바퀴의 반지름
    bool  bIsDriveWheel = false;       // 구동륜 여부
};
