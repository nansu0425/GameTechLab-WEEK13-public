#pragma once
#include "PhysicsCore.h"
#include "PhysicsUserData.h"

using namespace physx;

struct FBodyInstance;

enum class EJointMotion : uint8
{
    Locked,
    Limited,
    Free
};

struct FConstraintFrame
{
    FVector Pos = FVector::Zero();
    FVector PriAxis = FVector::Zero();
    FVector SecAxis = FVector::Zero();
    FConstraintFrame() = default;
    FConstraintFrame(const FVector& InPos, const FVector& InPriAxis, const FVector& InSecAxis)
        : Pos(InPos), PriAxis(InPriAxis), SecAxis(InSecAxis) {}
};

struct FConstraintProfileProperties
{
    float Swing1LimitsAngle = 45.0f;
    EJointMotion Swing1Motion = EJointMotion::Limited;
    
    float Swing2LimitsAngle = 45.0f;
    EJointMotion Swing2Motion = EJointMotion::Limited;
    
    float TwistLimit = 45.0f;
    EJointMotion TwistMotion = EJointMotion::Limited;

    float LinearLimit = 0.0f;
    EJointMotion LinearMotionX = EJointMotion::Limited;
    EJointMotion LinearMotionY = EJointMotion::Limited;
    EJointMotion LinearMotionZ = EJointMotion::Limited;

    /* 탄성을 주기위한 Drive 요소 */
    bool bEnableDrive = false;
    // 스프링 강도(복원력)
    float Stiffness = 0.0f;
    // 감쇠
    float Damping = 0.0f;
    float DriveForceLimit = FLT_MAX;

    bool bDisableCollision = true;

    FConstraintProfileProperties() = default;
    FConstraintProfileProperties(const FConstraintProfileProperties& Other)
        : Swing1LimitsAngle(Other.Swing1LimitsAngle),
          Swing1Motion(Other.Swing1Motion),
          Swing2LimitsAngle(Other.Swing2LimitsAngle),
          Swing2Motion(Other.Swing2Motion),
          TwistLimit(Other.TwistLimit),
          TwistMotion(Other.TwistMotion),
          LinearLimit(Other.LinearLimit),
          LinearMotionX(Other.LinearMotionX),
          LinearMotionY(Other.LinearMotionY),
          LinearMotionZ(Other.LinearMotionZ),
          bEnableDrive(Other.bEnableDrive),
          Stiffness(Other.Stiffness),
          Damping(Other.Damping),
          DriveForceLimit(Other.DriveForceLimit),
          bDisableCollision(Other.bDisableCollision)
    {}
};

struct FConstraintInstanceBase
{
    FConstraintInstanceBase();
    virtual ~FConstraintInstanceBase() = default;
    void Reset();
    
    int32 ContraintIndex = 0;
    physx::PxD6Joint* Joint = nullptr;
    FPhysScene* PhysScene = nullptr;    

    FPhysScene* GetPhysicsScene() { return PhysScene; }
    const FPhysScene* GetPhysicsScene() const { return PhysScene; }
};

struct FConstraintInstance : public FConstraintInstanceBase
{
    FName JointName;

    // 언리얼은 FName으로 연결
    // 편의를 위해서 FBodyInstance* 사용
    FBodyInstance* ChildBody;
    FBodyInstance* ParentBody;

    /* Child Body */
    FVector Pos1;
    // 관절 주 축, 주로 X 축
    FVector PriAxis1;
    // 관절 보조 축, 주 축에 수직, 주로 Y
    FVector SecAxis1;

    /* Parent Body */
    FVector Pos2;
    // 관절 주 축, 주로 X 축
    FVector PriAxis2;
    // 관절 보조 축, 주 축에 수직, 주로 Y
    FVector SecAxis2;

    // 각도의 기준을 바꾸는 변수
    // 살짝 굽혀진걸 0도로 바꿀 수 있음
    FQuat AngularRotationOffset;

    // 메시 스케일에 따라서 이동 제한(linear limit)도 스케일
    bool bScaleLinearLimits;

    float AverageMass;

    FConstraintProfileProperties Profile;

    FUserData UserData;

    FConstraintInstance();
    FConstraintInstance(const FConstraintInstance& Other);
    FConstraintInstance(FConstraintInstance&& Other) noexcept;
    ~FConstraintInstance() override;

    FConstraintInstance& operator=(const FConstraintInstance& Other);
    FConstraintInstance& operator=( FConstraintInstance&& Other) noexcept;

    void Initialize(const FConstraintFrame& ChildFrame, const FConstraintFrame& ParentFrame);
    void Initialize();
    void UpdateProfile();
    void UpdateFrames();

    void SetRefFrame1(const FVector& Pos, const FVector& PriAxis, const FVector& SecAxis);
    void SetRefFrame1(const FConstraintFrame& Frame);
    void SetRefFrame2(const FVector& Pos, const FVector& PriAxis, const FVector& SecAxis);
    void SetRefFrame2(const FConstraintFrame& Frame);
    void SetAngularRotationOffset(const FQuat& InAngularRotationOffset);

    void SetDisableCollision(bool bDisable);

    FBodyInstance* GetChildBody() const { return ChildBody; }
    FBodyInstance* GetParentBody() const { return ParentBody; }

    float GetLinearLimit() const { return Profile.Swing1LimitsAngle; }
    void SetLinearLimit(float Limit, EJointMotion XMotion, EJointMotion YMotion, EJointMotion ZMotion)
    {
        Profile.LinearMotionX = XMotion;
        Profile.LinearMotionY = YMotion;
        Profile.LinearMotionZ = ZMotion;
        Profile.LinearLimit = Limit;
        UpdateProfile();
    }

    float GetAngularSwing1Limit() const { return Profile.Swing1LimitsAngle; }
    void SetAngularSwing1Limit(float Limit, EJointMotion SwingMotion)
    {
        Profile.Swing1Motion = SwingMotion;
        Profile.Swing1LimitsAngle = Limit;
        UpdateProfile();
    }
    
    float GetAngularSwing2Limit() const { return Profile.Swing2LimitsAngle; }
    void SetAngularSwing2Limit(float Limit, EJointMotion SwingMotion)
    {
        Profile.Swing2Motion = SwingMotion;
        Profile.Swing2LimitsAngle = Limit;
        UpdateProfile();
    }

    float GetAngularTwistLimit() const { return Profile.TwistLimit; }
    void SetAngularTwistLimit(float Limit, EJointMotion TwistMotion)
    {
        Profile.TwistMotion = TwistMotion;
        Profile.TwistLimit = Limit;
        UpdateProfile();
    }

    bool IsValid() const { return Joint != nullptr; }

private:
    physx::PxTransform CalcPxTransform(const FVector& Pos, const FVector& PriAxis, const FVector& SecAxis);
    static physx::PxD6Motion::Enum ConvertMotion(EJointMotion Motion);
};

struct FConstraintSetup
{
    // ═══════════════════════════════════════════════════════════════════════
    // 1. 식별 및 위상 (Topology)
    // ═══════════════════════════════════════════════════════════════════════
    
    /** 이 관절의 고유 이름 (예: "Leg_L_Joint") */
    FName JointName;

    /** 연결될 Child 뼈 이름 (예: "Calf_L") */
    FName ConstraintBone1; 

    /** 연결될 Parent 뼈 이름 (예: "Thigh_L") */
    FName ConstraintBone2;

    // ═══════════════════════════════════════════════════════════════════════
    // 2. 기하학적 배치 (Geometry)
    // ═══════════════════════════════════════════════════════════════════════
    
    /** Child Body 기준 관절 위치 및 축 */
    FConstraintFrame Frame1;

    /** Parent Body 기준 관절 위치 및 축 */
    FConstraintFrame Frame2;

    /** 초기 회전 오프셋 (0도 기준 보정) */
    FQuat AngularRotationOffset = FQuat::Identity();

    /** 스케일에 따른 선형 제한 크기 조절 여부 */
    bool bScaleLinearLimits = true;


    // ═══════════════════════════════════════════════════════════════════════
    // 3. 물리적 물성 (Physics Properties)
    // ═══════════════════════════════════════════════════════════════════════

    /** 제한(Limit), 모터(Drive), 충돌 등 물리 설정값 */
    FConstraintProfileProperties Profile;


    // ═══════════════════════════════════════════════════════════════════════
    // 생성자
    // ═══════════════════════════════════════════════════════════════════════
    FConstraintSetup() = default;

    /**
     * @brief 편의 생성자
     */
    FConstraintSetup(FName InName, FName Bone1, FName Bone2)
        : JointName(InName), ConstraintBone1(Bone1), ConstraintBone2(Bone2)
    {
    }

    /**
     * @brief 이 Setup 데이터를 런타임 Instance에 복사하는 헬퍼 함수
     * @param OutInstance 데이터를 받을 런타임 객체
     */
    void CopyToInstance(FConstraintInstance& OutInstance) const
    {
        OutInstance.JointName = JointName;
        
        // Frame 데이터를 풀어서 넣음
        OutInstance.Pos1 = Frame1.Pos;
        OutInstance.PriAxis1 = Frame1.PriAxis;
        OutInstance.SecAxis1 = Frame1.SecAxis;

        OutInstance.Pos2 = Frame2.Pos;
        OutInstance.PriAxis2 = Frame2.PriAxis;
        OutInstance.SecAxis2 = Frame2.SecAxis;

        OutInstance.AngularRotationOffset = AngularRotationOffset;
        OutInstance.bScaleLinearLimits = bScaleLinearLimits;
        
        // 프로필(Limit, Drive) 복사
        OutInstance.Profile = Profile;
    }
};