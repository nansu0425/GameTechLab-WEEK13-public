#pragma once

#include "MovementComponent.h"
#include "USimpleWheeledVehicleMovementComponent.generated.h"

// PhysX vehicle 전방 선언
namespace physx
{
    class PxVehicleWheelsSimData;
    class PxVehicleDrive4W;
    class PxVehicleWheels;
    class PxRigidDynamic;
}

class USkeletalMeshComponent;

/** 휠 설정에 필요한 최소 정보 */
struct FWheelSetup
{
    FName BoneName;             // 바퀴를 연결할 Skeletal Mesh의 본 이름
    float SuspensionOffsetZ;    // 서스펜션 Z축 오프셋
    float WheelRadius;          // 바퀴의 반지름
    bool  bIsDriveWheel;        // 구동륜 여부
};

/** 언리얼 엔진의 USimpleWheeledVehicleMovementComponent를 모방한 컴포넌트 */
UCLASS(DisplayName = "심플 휠 비히클 무브먼트 컴포넌트", Description = "비히클 움직임을 구현하는 컴포넌트입니다.")
class USimpleWheeledVehicleMovementComponent : public UMovementComponent
{
public:
    GENERATED_REFLECTION_BODY()

public:
    USimpleWheeledVehicleMovementComponent();
protected:
    ~USimpleWheeledVehicleMovementComponent() override;
public:
    // 등록/해제/EndPlay 훅
    void OnRegister(UWorld* InWorld) override;
    void OnUnregister() override;
    void EndPlay() override;

    // UActorComponent 인터페이스 오버라이드
    void TickComponent(float DeltaTime) override;

    // --- 핵심 멤버 변수 (Properties) ---

    /** 차량 전체 질량 (PhysX Body와 연동) */
    UPROPERTY(EditAnywhere, Category = "Vehicle")
    float VehicleMass = 1200.0f;

    /** 바퀴 설정 배열 */
    UPROPERTY(EditAnywhere, Category = "Vehicle")
    TArray<FWheelSetup> WheelSetups;

    /** 최대 구동력 (스로틀 1.0f일 때 적용되는 토크) */
    UPROPERTY(EditAnywhere, Category = "Vehicle")
    float MaxDriveTorque = 500.0f;

    /** 최대 스티어링 각 (Degree) */
    UPROPERTY(EditAnywhere, Category = "Vehicle")
    float MaxSteerAngle = 40.0f;

    /** 기본 브레이크 토크 (풀 브레이크 입력 시) */
    UPROPERTY(EditAnywhere, Category = "Vehicle")
    float MaxBrakeTorque = 1500.0f;

    /** 핸드브레이크 토크 (후륜 고정용) */
    UPROPERTY(EditAnywhere, Category = "Vehicle")
    float MaxHandbrakeTorque = 2000.0f;

    /** 자동 기어 여부 */
    UPROPERTY(EditAnywhere, Category = "Vehicle")
    bool bUseAutoGears = true;

    // --- 입력 처리 함수 (API) ---

    /** 스로틀 입력 설정 (-1.0f ~ 1.0f) */
    void SetThrottleInput(float Throttle);

    /** 스티어링 입력 설정 (-1.0f ~ 1.0f) */
    void SetSteeringInput(float Steering);

    /** 브레이크 입력 설정 (0.0f ~ 1.0f) */
    void SetBrakeInput(float Brake);

    /** 핸드브레이크 입력 설정 (0.0f ~ 1.0f) */
    void SetHandbrakeInput(float Handbrake);

    /** 기어 업/다운 (수동 변속 시 사용) */
    void GearUp();
    void GearDown();

protected:
    // --- PhysX Vehicle 관련 멤버 ---

    /** PhysX PxVehicleDrive4W 인스턴스 (팀원 C의 핵심 작업) */
    physx::PxVehicleDrive4W* PxVehicleDrive4WInstance = nullptr;

    /** PxVehicleWheels 인스턴스 (바퀴 시뮬레이션 데이터 포함) */
    physx::PxVehicleWheels* PxVehicleWheelsInstance = nullptr;

    /** 차량 차체로 사용할 Dynamic Actor */
    physx::PxRigidDynamic* PxVehicleActor = nullptr;

    /** 노면-타이어 마찰 매핑 테이블 (기본 1x1) */
    physx::PxVehicleDrivableSurfaceToTireFrictionPairs* TireFrictionPairs = nullptr;

    /** 휠별 레이캐스트/쿼리 결과 버퍼 */
    TArray<physx::PxWheelQueryResult> WheelQueryResults;

    /** Vehicle API에 전달할 휠 쿼리 결과 래퍼 */
    physx::PxVehicleWheelQueryResult VehicleQueryResults;

    /** 사용자 입력 상태 저장 */
    float ThrottleInput;
    float SteeringInput;
    float BrakeInput;
    float HandbrakeInput;

    /** UpdatedComponent를 차량용 메시(Skeletal/Static)로만 제한 */
    void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;

    // --- 내부 초기화/업데이트 함수 ---

    /** PxVehicleDrive4W 인스턴스 및 관련 PhysX 구조체 초기화 */
    bool InitVehiclePhysX();

    /** 사용자 입력(Throttle/Steering)을 PxVehicleDrive4WRawInputData에 매핑 */
    void ApplyInputToPhysX(float DeltaTime);

    /** 서스펜션 레이캐스트 업데이트 */
    void PerformSuspensionRaycasts();

    /** Vehicle simulate/update 호출 */
    void SimulateVehicle(float DeltaTime);

    /** PhysX 결과로부터 차체 및 휠의 포즈를 업데이트 */
    void UpdateVehiclePoseFromPhysX();

    /** Vehicle 관련 PhysX 리소스 정리 */
    void CleanupVehiclePhysX();

    /** UpdatedComponent가 유효한 차량 메시인지 보장 (필요 시 자동 탐색) */
    void EnsureUpdatedComponentIsValid();

    /** 초기화 진행 여부 */
    bool bVehicleInitialized = false;

    /** 반복 로그 방지를 위한 상태 플래그 */
    bool bWarnedMissingBodyComponent = false;
    bool bWarnedPhysicsUninitialized = false;
    bool bWarnedWheelSetup = false;
    bool bWarnedMissingWheelBone = false;
};
