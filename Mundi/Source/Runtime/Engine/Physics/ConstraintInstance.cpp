#include "pch.h"
#include "ConstraintInstance.h"

#include "BodyInstance.h"
#include "BodyInstanceImpl.h"
#include "PhysicsTypes.h"

FConstraintInstanceBase::FConstraintInstanceBase()
{
    Reset();
}

void FConstraintInstanceBase::Reset()
{
    if (Joint)
    {
        Joint->release();
        Joint = nullptr;
    }
    ContraintIndex = 0;    
    PhysScene = nullptr;
}

FConstraintInstance::FConstraintInstance()
    : FConstraintInstanceBase(),
      AngularRotationOffset(FQuat::Identity()),
      bScaleLinearLimits(true),
      AverageMass(0.0f),
      ChildBody(nullptr),
      ParentBody(nullptr),
      UserData(this)
{
    Pos1 = FVector(0.0f, 0.0f, 0.0f);
    PriAxis1 = FVector(1.0f, 0.0f, 0.0f);
    SecAxis1 = FVector(0.0f, 1.0f, 0.0f);

    Pos2 = FVector(0.0f, 0.0f, 0.0f);
    PriAxis2 = FVector(1.0f, 0.0f, 0.0f);
    SecAxis2 = FVector(0.0f, 1.0f, 0.0f);
}

FConstraintInstance::FConstraintInstance(const FConstraintInstance& Other)
    : FConstraintInstanceBase() // Base는 초기화 (Joint = nullptr)
    , JointName(Other.JointName)
    , ChildBody(Other.ChildBody)
    , ParentBody(Other.ParentBody)
    , Pos1(Other.Pos1), PriAxis1(Other.PriAxis1), SecAxis1(Other.SecAxis1)
    , Pos2(Other.Pos2), PriAxis2(Other.PriAxis2), SecAxis2(Other.SecAxis2)
    , AngularRotationOffset(Other.AngularRotationOffset)
    , bScaleLinearLimits(Other.bScaleLinearLimits)
    , AverageMass(Other.AverageMass)
    , Profile(Other.Profile)
    , UserData(this)
{
}

FConstraintInstance::FConstraintInstance(FConstraintInstance&& Other) noexcept
    : FConstraintInstanceBase()
    , JointName(std::move(Other.JointName))
    , ChildBody(Other.ChildBody)
    , ParentBody(Other.ParentBody)
    , Pos1(Other.Pos1), PriAxis1(Other.PriAxis1), SecAxis1(Other.SecAxis1)
    , Pos2(Other.Pos2), PriAxis2(Other.PriAxis2), SecAxis2(Other.SecAxis2)
    , AngularRotationOffset(Other.AngularRotationOffset)
    , bScaleLinearLimits(Other.bScaleLinearLimits)
    , AverageMass(Other.AverageMass)
    , Profile(Other.Profile) // Profile 내부에 무거운게 없다면 복사됨
    , UserData(this) // [중요] 이동해왔어도 내 주소는 'this'
{
    // [핵심] 리소스 소유권 이전 (Steal Resource)
    this->Joint = Other.Joint;
    this->PhysScene = Other.PhysScene;
    this->ContraintIndex = Other.ContraintIndex;

    // [핵심] PhysX Back-pointer 갱신
    if (this->Joint)
    {
        this->Joint->userData = &this->UserData;
    }

    // 원본 초기화 (Reset)
    Other.Joint = nullptr;
    Other.PhysScene = nullptr;
    Other.ContraintIndex = 0;
}

FConstraintInstance::~FConstraintInstance()
{
    Reset();
}

FConstraintInstance& FConstraintInstance::operator=(const FConstraintInstance& Other)
{
    if (this != &Other)
    {
        // 1. 기존 리소스 정리
        // (이미 연결된 조인트가 있다면 끊어야 함)
        if (Joint)
        {
            Joint->release();
            Joint = nullptr;
        }
        
        // 2. 데이터 복사
        JointName = Other.JointName;
        ChildBody = Other.ChildBody;
        ParentBody = Other.ParentBody;
        Pos1 = Other.Pos1; PriAxis1 = Other.PriAxis1; SecAxis1 = Other.SecAxis1;
        Pos2 = Other.Pos2; PriAxis2 = Other.PriAxis2; SecAxis2 = Other.SecAxis2;
        AngularRotationOffset = Other.AngularRotationOffset;
        bScaleLinearLimits = Other.bScaleLinearLimits;
        AverageMass = Other.AverageMass;
        Profile = Other.Profile;

        // 3. UserData는 건드리지 않음 (내 주소 유지)
        
        // 4. Base 클래스 데이터 (PhysScene 등)는 상황에 따라 복사하거나 초기화
        PhysScene = nullptr; // 보통 복사 시에는 씬 등록 전이라 가정
    }
    return *this;
}

FConstraintInstance& FConstraintInstance::operator=(FConstraintInstance&& Other) noexcept
{
    if (this != &Other)
    {
        // 1. 기존 리소스 정리
        if (Joint)
        {
            Joint->release();
            Joint = nullptr;
        }

        // 2. 데이터 이동/복사
        JointName = std::move(Other.JointName);
        ChildBody = Other.ChildBody;
        ParentBody = Other.ParentBody;
        Pos1 = Other.Pos1; PriAxis1 = Other.PriAxis1; SecAxis1 = Other.SecAxis1;
        Pos2 = Other.Pos2; PriAxis2 = Other.PriAxis2; SecAxis2 = Other.SecAxis2;
        AngularRotationOffset = Other.AngularRotationOffset;
        bScaleLinearLimits = Other.bScaleLinearLimits;
        AverageMass = Other.AverageMass;
        Profile = Other.Profile;

        // 3. 리소스 소유권 이전
        this->Joint = Other.Joint;
        this->PhysScene = Other.PhysScene;
        this->ContraintIndex = Other.ContraintIndex;

        // 4. [핵심] PhysX Back-pointer 갱신
        if (this->Joint)
        {
            this->Joint->userData = &this->UserData;
        }

        // 5. 원본 초기화
        Other.Joint = nullptr;
        Other.PhysScene = nullptr;
        Other.ContraintIndex = 0;
    }
    return *this;
}

void FConstraintInstance::Initialize(const FConstraintFrame& ChildFrame, const FConstraintFrame& ParentFrame)
{
    // SetRef~ 함수로 설정하면 UpdateFrames가 호출된다.
    // Initialize() 함수 내부의 PxD6JointCreate 함수가 setLocalPose 함수 역할도 함
    // PxD6JointCreate는 생성자 역할, setLocalPose는 setter 역할
    Pos1 = ChildFrame.Pos;
    PriAxis1 = ChildFrame.PriAxis;
    SecAxis1 = ChildFrame.SecAxis;

    Pos2 = ParentFrame.Pos;
    PriAxis2 = ParentFrame.PriAxis;
    SecAxis2 = ParentFrame.SecAxis;
    
    Initialize();
}

void FConstraintInstance::Initialize()
{
    // parent가 없으면 월드 고정
    if (!ChildBody)
    {
        return;
    }

    FPhysicsCore& Physics = FPhysicsCore::Get();
    physx::PxRigidActor* Actor0 = ChildBody->GetImpl()->GetPxRigidActor();
    if (!Actor0)
    {
        return;
    }
    
    physx::PxRigidActor* Actor1 = ParentBody ? ParentBody->GetImpl()->GetPxRigidActor() : nullptr;

    physx::PxTransform LocalPose0 = CalcPxTransform(Pos1, PriAxis1, SecAxis1);
    physx::PxTransform LocalPose1 = CalcPxTransform(Pos2, PriAxis2, SecAxis2);

    Joint = physx::PxD6JointCreate(*Physics.GetPhysics(), Actor0, LocalPose0, Actor1, LocalPose1);
    if (Joint)
    {
        UpdateProfile();
        SetDisableCollision(Profile.bDisableCollision);
        Joint->userData = this;
    }
}

void FConstraintInstance::UpdateProfile()
{
    if (!Joint)
    {
        return;
    }
    
    Joint->setMotion(PxD6Axis::eTWIST, ConvertMotion(Profile.TwistMotion));
    Joint->setMotion(PxD6Axis::eSWING1, ConvertMotion(Profile.Swing1Motion));
    Joint->setMotion(PxD6Axis::eSWING2, ConvertMotion(Profile.Swing2Motion));

    Joint->setMotion(PxD6Axis::eX, ConvertMotion(Profile.LinearMotionX));
    Joint->setMotion(PxD6Axis::eY, ConvertMotion(Profile.LinearMotionY));
    Joint->setMotion(PxD6Axis::eZ, ConvertMotion(Profile.LinearMotionZ));

    if (Profile.Swing1Motion == EJointMotion::Limited || Profile.Swing2Motion == EJointMotion::Limited)
    {
        float YLimitRad = DegreesToRadians(Profile.Swing1LimitsAngle);
        float ZLimitRad = DegreesToRadians(Profile.Swing2LimitsAngle);

        PxJointLimitCone ConeLimit(YLimitRad, ZLimitRad, 0.01f);
        ConeLimit.stiffness = 0.0f;
        Joint->setSwingLimit(ConeLimit);
    }

    if (Profile.LinearMotionX == EJointMotion::Limited ||
        Profile.LinearMotionY == EJointMotion::Limited ||
        Profile.LinearMotionZ == EJointMotion::Limited)
    {
        PxJointLinearLimit LinearLimit(Profile.LinearLimit, PxSpring(0.0f, 0.0f));
        Joint->setLinearLimit(LinearLimit);
    }

    if (Profile.TwistMotion == EJointMotion::Limited)
    {
        float TwistRad = DegreesToRadians(Profile.TwistLimit);

        PxJointAngularLimitPair TwistLimit(-TwistRad, TwistRad);
        Joint->setTwistLimit(TwistLimit);
    }

    if (Profile.bEnableDrive)
    {
        PxD6JointDrive Drive;
        Drive.stiffness = Profile.Stiffness;
        Drive.damping = Profile.Damping;
        Drive.forceLimit = Profile.DriveForceLimit;

        Joint->setDrive(PxD6Drive::eTWIST, Drive);
        Joint->setDrive(PxD6Drive::eSWING, Drive);
    }
    else
    {
        PxD6JointDrive DisabledDrive(0.0f, 0.0f, FLT_MAX, false);
        Joint->setDrive(PxD6Drive::eTWIST, DisabledDrive);
        Joint->setDrive(PxD6Drive::eSWING, DisabledDrive);
    }
}

void FConstraintInstance::UpdateFrames()
{
    if (!Joint)
    {
        return;
    }

    PxTransform LocalPose0 = CalcPxTransform(Pos1, PriAxis1, SecAxis1);
    PxTransform LocalPose1 = CalcPxTransform(Pos2, PriAxis2, SecAxis2);

    Joint->setLocalPose(PxJointActorIndex::eACTOR0, LocalPose0);
    Joint->setLocalPose(PxJointActorIndex::eACTOR1, LocalPose1);
}

void FConstraintInstance::SetRefFrame1(const FVector& Pos, const FVector& PriAxis, const FVector& SecAxis)
{
    Pos1 = Pos;
    PriAxis1 = PriAxis;
    SecAxis1 = SecAxis;
    if (IsValid())
    {
        UpdateFrames();
    }
}

void FConstraintInstance::SetRefFrame1(const FConstraintFrame& Frame)
{
    Pos1 = Frame.Pos;
    PriAxis1 = Frame.PriAxis;
    SecAxis1 = Frame.SecAxis;
    if (IsValid())
    {
        UpdateFrames();
    }
}

void FConstraintInstance::SetRefFrame2(const FVector& Pos, const FVector& PriAxis, const FVector& SecAxis)
{
    Pos2 = Pos;
    PriAxis2 = PriAxis;
    SecAxis2 = SecAxis;
    if (IsValid())
    {
        UpdateFrames();
    }
}

void FConstraintInstance::SetRefFrame2(const FConstraintFrame& Frame)
{
    Pos2 = Frame.Pos;
    PriAxis2 = Frame.PriAxis;
    SecAxis2 = Frame.SecAxis;
    if (IsValid())
    {
        UpdateFrames();
    }
}

void FConstraintInstance::SetAngularRotationOffset(const FQuat& InAngularRotationOffset)
{
    AngularRotationOffset = InAngularRotationOffset;
    if (IsValid())
    {
        UpdateFrames();
    }
}

void FConstraintInstance::SetDisableCollision(bool bDisable)
{
    if (Joint)
    {
        Joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, !bDisable);
    }
}

physx::PxTransform FConstraintInstance::CalcPxTransform(const FVector& Pos, const FVector& PriAxis, const FVector& SecAxis)
{
    using namespace PhysicsConversion;

    // 1. 위치 변환: Mundi → PhysX 좌표계
    physx::PxVec3 PxPos = ToPxVec3(Pos);

    // 2. 축 계산 (Mundi 좌표계에서)
    // PriAxis = Twist 축 (X축)
    // SecAxis = Swing1 축에 사용될 보조 축
    FVector XAxis = PriAxis.GetSafeNormal();
    FVector ZAxis = FVector::Cross(XAxis, SecAxis.GetSafeNormal()).GetSafeNormal();
    if (ZAxis.IsZero())
    {
        // PriAxis와 SecAxis가 평행한 경우 대체 축 사용
        FVector Right = FMath::Abs(XAxis.Y) < 0.99f ? FVector(0.0f, 1.0f, 0.0f) : FVector(-1.0f, 0.0f, 0.0f);
        ZAxis = FVector::Cross(XAxis, Right).GetSafeNormal();
    }
    FVector YAxis = FVector::Cross(ZAxis, XAxis).GetSafeNormal();

    // 3. Mundi 회전 행렬 → FQuat
    // 행렬의 각 열이 새로운 축 방향
    FMatrix RotMatrix = FMatrix::Identity();
    RotMatrix.M[0][0] = XAxis.X; RotMatrix.M[0][1] = XAxis.Y; RotMatrix.M[0][2] = XAxis.Z;
    RotMatrix.M[1][0] = YAxis.X; RotMatrix.M[1][1] = YAxis.Y; RotMatrix.M[1][2] = YAxis.Z;
    RotMatrix.M[2][0] = ZAxis.X; RotMatrix.M[2][1] = ZAxis.Y; RotMatrix.M[2][2] = ZAxis.Z;
    FQuat MundiRot(RotMatrix);

    // 4. AngularRotationOffset 적용 (Mundi 좌표계에서)
    MundiRot = MundiRot * AngularRotationOffset;

    // 5. PhysX 좌표계로 변환
    physx::PxQuat PxRot = ToPxQuat(MundiRot);

    return physx::PxTransform(PxPos, PxRot);
}

physx::PxD6Motion::Enum FConstraintInstance::ConvertMotion(EJointMotion Motion)
{
    switch (Motion)
    {
    case EJointMotion::Locked:
        return PxD6Motion::eLOCKED;
    case EJointMotion::Limited:
        return PxD6Motion::eLIMITED;
    case EJointMotion::Free:
        return PxD6Motion::eFREE;
    default:
        return PxD6Motion::eLOCKED;
    }
}
