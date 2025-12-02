#pragma once
#include "vehicle/PxVehicleUpdate.h"
#include "MovementComponent.h"
#include "WheelSetup.h"
#include "USimpleWheeledVehicleMovementComponent.generated.h"

// PhysX vehicle 전방 선언
namespace physx
{
    class PxVehicleWheelsSimData;
    class PxVehicleDrive4W;
    class PxVehicleWheels;
    class PxRigidDynamic;
    class PxVehicleDrivableSurfaceToTireFrictionPairs;
    struct PxWheelQueryResult;
    struct PxVehicleWheelQueryResult;
}

class USkeletalMeshComponent;

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
	void BeginPlay() override;
    void EndPlay() override;

    // =======================================================================
    // 업데이트 API
    // =======================================================================

    void InitializeComponent() override {};

    // UActorComponent 인터페이스 오버라이드
    void TickComponent(float DeltaTime) override;

    /** 서스펜션 레이캐스트 업데이트(시뮬레이션 전 수행) */
    void PerformSuspensionRaycasts();

    /** 사용자 입력(Throttle/Steering)을 PxVehicleDrive4WRawInputData에 매핑 */
    void ApplyInputToPhysX(float DeltaTime);

    /** Vehicle simulate/update 호출 */
    void SimulateVehicle(float DeltaTime);

    /** PhysX 결과로부터 차체 및 휠의 포즈를 업데이트 */
    void UpdateVehiclePoseFromPhysX();

    // =======================================================================
    // 핵심 멤버 변수 (Properties)
    // =======================================================================

	const static int32 NumWheels4W = 4; // 4륜 고정

    /** 바퀴 설정 배열 */
    UPROPERTY(EditAnywhere, Category = "Vehicle")
    TArray<FWheelSetup> WheelSetups;

    /** 차량 전체 질량 (PhysX Body와 연동) */
    UPROPERTY(EditAnywhere, Category = "Vehicle")
    float VehicleMass = 1200.0f;

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

    // --- 입력 처리 함수 (API) ---

    /** 스로틀 입력 설정 (-1.0f ~ 1.0f) */
    void SetThrottleInput(float Throttle);

    /** 스티어링 입력 설정 (-1.0f ~ 1.0f) */
    void SetSteeringInput(float Steering);

    /** 브레이크 입력 설정 (0.0f ~ 1.0f) */
    void SetBrakeInput(float Brake);

    /** 핸드브레이크 입력 설정 (0.0f ~ 1.0f) */
    void SetHandbrakeInput(float Handbrake);

    /** WheelSetup 변경 후 재초기화 */
    void ResetVehicle();

protected:
    // --- PhysX Vehicle 관련 멤버 ---

    /** PhysX PxVehicleDrive4W 인스턴스 */
    physx::PxVehicleDrive4W* PxVehicleDrive4WInstance = nullptr;

    /** PxVehicleWheels 인스턴스 (바퀴 시뮬레이션 데이터 포함) */
    physx::PxVehicleWheels* PxVehicleWheelsInstance = nullptr;

    /**
     * @brief 차량 차체로 사용할 Dynamic Actor
	 * @note 직접 생성하지 않고, 소유한 UpdatedComponent의 PhysX Body를 참조합니다.
     */
    physx::PxRigidDynamic* PxVehicleActor = nullptr;

    /** 노면-타이어 마찰 매핑 테이블 (타이어와 노면 타입은 단일 타입 & 마찰계수 1 고정) */
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


    // =======================================================================
    // 내부 리소스 관리
    // =======================================================================

    /** PxVehicleDrive4W 인스턴스 및 관련 PhysX 구조체 초기화 */
    bool InitVehiclePhysX();

    /** Vehicle 관련 PhysX 리소스 정리 */
    void CleanupVehiclePhysX();

    /**
     * @brief UpdatedComponent가 유효한 차량 메시인지 보장 (필요 시 자동 탐색)
     * @note SkeletalMeshComponent 또는 StaticMeshComponent만을 유효한 컴포넌트로 취급합니다.
     * UpdatedComponent -> 루트 컴포넌트 -> Actor의 SkeletalMeshComponent -> StaticMeshComponent 순으로 탐색합니다.
     */
    void EnsureUpdatedComponentIsValid();

    /** 초기화 진행 여부 */
    bool bVehicleInitialized = false;

    /** 반복 로그 방지를 위한 상태 플래그 */
    bool bWarnedMissingBodyComponent = false;
    bool bWarnedPhysicsUninitialized = false;
    bool bWarnedWheelSetup = false;
    bool bWarnedMissingWheelBone = false;

    /** PhysScene 등록 상태 */
    bool bRegisteredWithPhysScene = false;

    /** UpdatedComponent가 루트가 아닐 때 경고를 주기 위한 카운터 (60프레임마다) */
    int32 NonRootComponentWarningCounter = 0;

    /** 초기 휠 본의 로컬 위치를 캐싱 (서스펜션 승강 계산용) */
    TArray<FVector> InitialWheelLocalPositions;

    /** PhysScene 레지스트리에 등록/해제 */
    void RegisterWithPhysScene();
    void UnregisterFromPhysScene();
};
