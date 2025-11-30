#pragma once
#include "Object.h"
#include "Name.h"
#include "UBodySetupCore.generated.h"

UCLASS(DisplayName = "충돌 기하 데이터", Description = "공유 가능한 충돌 기하 데이터 입니다")
class UBodySetupCore : public UObject
{
public:
    GENERATED_REFLECTION_BODY()
    UBodySetupCore() : BoneName() {}
    // 일단 BoneName만 구현
    FName BoneName;
    
};