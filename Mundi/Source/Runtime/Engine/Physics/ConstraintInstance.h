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
    
    float TwistLimits = 45.0f;
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
    ~FConstraintInstance() override;

    void Initialize(const FConstraintFrame& ChildFrame, const FConstraintFrame& ParentFrame);
    void Initialize();
    void UpdateProfile();
    void UpdateFrames();

    void SetRefFrame1(const FVector& Pos, const FVector& PriAxis, const FVector& SecAxis);
    void SetRefFrame2(const FVector& Pos, const FVector& PriAxis, const FVector& SecAxis);
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

    float GetAngularTwistLimit() const { return Profile.TwistLimits; }
    void SetAngularTwistLimit(float Limit, EJointMotion TwistMotion)
    {
        Profile.TwistMotion = TwistMotion;
        Profile.TwistLimits = Limit;
        UpdateProfile();
    }

    bool IsValid() const { return Joint != nullptr; }

private:
    physx::PxTransform CalcPxTransform(const FVector& Pos, const FVector& PriAxis, const FVector& SecAxis);
    static physx::PxD6Motion::Enum ConvertMotion(EJointMotion Motion);
};