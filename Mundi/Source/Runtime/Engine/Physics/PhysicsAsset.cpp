#include "pch.h"
#include "PhysicsAsset.h"

#include "BodySetup.h"

UPhysicsAsset::UPhysicsAsset()
{
}

UPhysicsAsset::~UPhysicsAsset()
{
}

void UPhysicsAsset::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    UObject::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        // JSON MeshJson;
        // if (FJsonSerializer::ReadObject(InOutHandle, "Mesh", MeshJson))
        // {
        //     FString SavedMeshFilePath = {};
        //     FJsonSerializer::ReadString(MeshJson, "MeshFilePath", SavedMeshFilePath);
        //     if (SavedMeshFilePath != MeshFilePath)
        //     {
        //         UE_LOG("스켈레탈과 애셋이 일치하지 않습니다.");
        //         return;
        //     }
        // }
        
        BodySetups.Empty();
        ConstraintSetups.Empty();
        CollisionDisableTable.Empty();
        BoneNameToBodyIndex.Empty();

        // Bodies
        JSON BodiesJson;
        if (FJsonSerializer::ReadObject(InOutHandle, "Bodies", BodiesJson))
        {
            for (auto& Pair : BodiesJson.ObjectRange())
            {
                JSON& BodyJson = Pair.second;
                UBodySetup* NewBody = NewObject<UBodySetup>();
                // BoneName
                FString BoneName;
                FJsonSerializer::ReadString(BodyJson, "Bone", BoneName);
                NewBody->BoneName = FName(BoneName);
                // BodyType 및 기본 치수
                int32 BodyTypeValue = 0;
                FJsonSerializer::ReadInt32(BodyJson, "BodyType", BodyTypeValue);
                NewBody->BodyType = static_cast<EBodySetupType>(BodyTypeValue);
                FJsonSerializer::ReadVector(BodyJson, "BoxExtent", NewBody->BoxExtent);
                FJsonSerializer::ReadFloat(BodyJson, "SphereRadius", NewBody->SphereRadius);
                FJsonSerializer::ReadFloat(BodyJson, "CapsuleHalfHeight", NewBody->CapsuleHalfHeight);
                // AggGeom
                SerializeAggGeom(true, BodyJson["AggGeom"], NewBody->AggGeom);

                BodySetups.Add(NewBody);
            }
        }

        // Constraints
        JSON ConstraintJson;
        if (FJsonSerializer::ReadObject(InOutHandle, "Constraints", ConstraintJson))
        {
            for (auto& Pair : ConstraintJson.ObjectRange())
            {
                JSON& ItemJson = Pair.second;
                FConstraintSetup Setup;
                SerializeConstraintSetup(true, ItemJson, Setup);
                ConstraintSetups.Add(Setup);
            }
        }

        // Solver
        SerializeSolverSettings(true, InOutHandle["SolverSettings"], SolverSettings);

        // CollisionDisableTable (저장된 페어를 읽어 재구성)
        JSON DisablePairs;
        if (FJsonSerializer::ReadObject(InOutHandle, "DisablePairs", DisablePairs))
        {
            for (auto& Pair : DisablePairs.ObjectRange())
            {
                JSON& Entry = Pair.second;
                int32 A = -1, B = -1;
                FJsonSerializer::ReadInt32(Entry, "A", A);
                FJsonSerializer::ReadInt32(Entry, "B", B);
                if (A >= 0 && B >= 0)
                {
                    if (A > B) std::swap(A, B);
                    CollisionDisableTable.Add(TPair<int32, int32>(A, B));
                }
            }
        }

        // BoneName → Index 맵 재구성
        UpdateBodySetupIndexMap();

        // Constraint에 의해 자동 비활성 충돌 설정 (Profile.bDisableCollision)
        for (const FConstraintSetup& Setup : ConstraintSetups)
        {
            int32 ChildIdx = FindBodyIndex(Setup.ConstraintBone1);
            int32 ParentIdx = FindBodyIndex(Setup.ConstraintBone2);
            if (Setup.Profile.bDisableCollision && ChildIdx != -1 && ParentIdx != -1 && ChildIdx != ParentIdx)
            {
                int32 A = ChildIdx;
                int32 B = ParentIdx;
                if (A > B) std::swap(A, B);
                CollisionDisableTable.Add(TPair<int32, int32>(A, B));
            }
        }
    }
    else
    {
        if (!MeshFilePath.empty())
        {
            JSON MeshJson;
            MeshJson["MeshFilePath"] = MeshFilePath;
            InOutHandle["Mesh"] = MeshJson;
        }

        // Bodies
        JSON BodiesJson;
        for (int32 Index = 0; Index < BodySetups.Num(); ++Index)
        {
            JSON BodyJson;
            UBodySetup* Body = BodySetups[Index];
            BodyJson["Bone"] = Body->BoneName.ToString();
            BodyJson["BodyType"] = static_cast<int32>(Body->BodyType);
            BodyJson["BoxExtent"] = FJsonSerializer::VectorToJson(Body->BoxExtent);
            BodyJson["SphereRadius"] = Body->SphereRadius;
            BodyJson["CapsuleHalfHeight"] = Body->CapsuleHalfHeight;
            JSON AggJson;
            SerializeAggGeom(false, AggJson, Body->AggGeom);
            BodyJson["AggGeom"] = AggJson;
            BodiesJson[std::to_string(Index)] = BodyJson;
        }
        InOutHandle["Bodies"] = BodiesJson;

        // Constraints
        JSON ConstraintJson;
        for (int32 Index = 0; Index < ConstraintSetups.Num(); ++Index)
        {
            JSON ItemJson;
            SerializeConstraintSetup(false, ItemJson, ConstraintSetups[Index]);
            ConstraintJson[std::to_string(Index)] = ItemJson;
        }
        InOutHandle["Constraints"] = ConstraintJson;

        // Solver
        JSON SolverJson;
        SerializeSolverSettings(false, SolverJson, SolverSettings);
        InOutHandle["SolverSettings"] = SolverJson;

        // CollisionDisableTable
        JSON DisablePairs;
        int32 PairIdx = 0;
        for (const TPair<int32, int32>& Pair : CollisionDisableTable)
        {
            JSON Item;
            Item["A"] = Pair.first;
            Item["B"] = Pair.second;
            DisablePairs[std::to_string(PairIdx++)] = Item;
        }
        InOutHandle["DisablePairs"] = DisablePairs;
    }
}

void UPhysicsAsset::UpdateBodySetupIndexMap()
{
    BoneNameToBodyIndex.Empty();
    for (int32 i = 0; i < BodySetups.Num(); i++)
    {
        if (BodySetups[i])
        {
            BoneNameToBodyIndex[BodySetups[i]->BoneName] = i;
        }
    }
}

int32 UPhysicsAsset::FindBodyIndex(const FName& BodyName) const
{    
    if (const int32* IndexPtr = BoneNameToBodyIndex.Find(BodyName))
    {
        return *IndexPtr;
    }
    return -1;
}

bool UPhysicsAsset::IsCollisionEnabled(int32 BodyIndexA, int32 BodyIndexB) const
{
    // 자기 자신과 충돌할 수 없다.
    if (BodyIndexA == BodyIndexB)
    {
        return false;
    }

    // 순서 정규화, 작은 값을 앞으로
    if (BodyIndexA > BodyIndexB)
    {
        std::swap(BodyIndexA, BodyIndexB);
    }
    
    if (CollisionDisableTable.Contains(TPair<int32, int32>(BodyIndexA, BodyIndexB)))
    {
        return false;
    }
    
    return true;
}

void UPhysicsAsset::DisableCollision(int32 BodyIndexA, int32 BodyIndexB)
{
    if ((BodyIndexA != BodyIndexB) && (BodyIndexA != -1) && (BodyIndexB != -1))
    {
        // 순서 정규화, 작은 값을 앞으로
        if (BodyIndexA > BodyIndexB)
        {
            std::swap(BodyIndexA, BodyIndexB);
        }
        CollisionDisableTable.Add(TPair{BodyIndexA, BodyIndexB});
    }
}

void UPhysicsAsset::EnableCollision(int32 BodyIndexA, int32 BodyIndexB)
{
    // 순서 정규화, 작은 값을 앞으로
    if (BodyIndexA > BodyIndexB)
    {
        std::swap(BodyIndexA, BodyIndexB);
    }
    if (CollisionDisableTable.Contains(TPair<int32, int32>(BodyIndexA, BodyIndexB)))
    {
        CollisionDisableTable.Remove(TPair{BodyIndexA, BodyIndexB});
    }
}

void UPhysicsAsset::SetBoneNameIndexTable(const FName& BoneName, int32 Index)
{
    BoneNameToBodyIndex.Add(BoneName, Index);
}

void UPhysicsAsset::AddBodySetup(UBodySetup* NewBody)
{
    if (NewBody)
    {
        BodySetups.Add(NewBody);
        BoneNameToBodyIndex[NewBody->BoneName] = BodySetups.Num() - 1;
    }
}

void UPhysicsAsset::ClearAllBodies()
{
    BodySetups.Empty();
    BoneNameToBodyIndex.Empty();
    CollisionDisableTable.Empty();
    ConstraintSetups.Empty();
    CollisionDisableTable.Empty();
}

void UPhysicsAsset::SerializeBoxElem(bool bIsLoading, JSON& InOut, FBoxElem& Elem)
{
    if (bIsLoading)
    {
        FJsonSerializer::ReadVector(InOut, "Center", Elem.Center);
        FJsonSerializer::ReadQuat(InOut, "Rotation", Elem.Rotation);
        FJsonSerializer::ReadFloat(InOut, "X", Elem.X);
        FJsonSerializer::ReadFloat(InOut, "Y", Elem.Y);
        FJsonSerializer::ReadFloat(InOut, "Z", Elem.Z);
    }
    else
    {
        InOut["Center"] = FJsonSerializer::VectorToJson(Elem.Center);
        InOut["Rotation"] = FJsonSerializer::QuatToJson(Elem.Rotation);
        InOut["X"] = Elem.X;
        InOut["Y"] = Elem.Y;
        InOut["Z"] = Elem.Z;
    }
}

void UPhysicsAsset::SerializeSphereElem(bool bIsLoading, JSON& InOut, FSphereElem& Elem)
{
    if (bIsLoading)
    {
        FJsonSerializer::ReadVector(InOut, "Center", Elem.Center);
        FJsonSerializer::ReadFloat(InOut, "Radius", Elem.Radius);
    }
    else
    {
        InOut["Center"] = FJsonSerializer::VectorToJson(Elem.Center);
        InOut["Radius"] = Elem.Radius;
    }
}

void UPhysicsAsset::SerializeSphylElem(bool bIsLoading, JSON& InOut, FSphylElem& Elem)
{
    if (bIsLoading)
    {
        FJsonSerializer::ReadVector(InOut, "Center", Elem.Center);
        FJsonSerializer::ReadQuat(InOut, "Rotation", Elem.Rotation);
        FJsonSerializer::ReadFloat(InOut, "Radius", Elem.Radius);
        FJsonSerializer::ReadFloat(InOut, "Length", Elem.Length);
    }
    else
    {
        InOut["Center"] = FJsonSerializer::VectorToJson(Elem.Center);
        InOut["Rotation"] = FJsonSerializer::QuatToJson(Elem.Rotation);
        InOut["Radius"] = Elem.Radius;
        InOut["Length"] = Elem.Length;
    }
}

void UPhysicsAsset::SerializeConvexElem(bool bIsLoading, JSON& InOut, struct FConvexElem& Elem)
{
    // 정점이랑 쿠킹한거 처리 어케할지 고민좀
    // if (bIsLoading)
    // {
    //     TArray<FVector> Vertices;
    //     FJsonSerializer::ReadVectorArray(InOut, "Vertices", Vertices);
    //     Elem.VertexData = Vertices;
    //     // ConvexMesh 캐시는 런타임에 다시 쿠킹되므로 Json에는 저장하지 않음
    //     Elem.ConvexMesh = nullptr;
    // }
    // else
    // {
    //     InOut["Vertices"] = Elem.VertexData;
    // }
}

void UPhysicsAsset::SerializeAggGeom(bool bIsLoading, JSON& InOut, FAggregateGeom& Geom)
{
    if (bIsLoading)
      {
          Geom.BoxElems.Empty();
          Geom.SphereElems.Empty();
          Geom.SphylElems.Empty();
          Geom.ConvexElems.Empty();

          // Box
          JSON BoxArray;
          if (FJsonSerializer::ReadObject(InOut, "Boxes", BoxArray))
          {
              for (auto& Pair : BoxArray.ObjectRange())
              {
                  FBoxElem Elem;
                  SerializeBoxElem(true, Pair.second, Elem);
                  Geom.BoxElems.Add(Elem);
              }
          }
          // Sphere
          JSON SphereArray;
          if (FJsonSerializer::ReadObject(InOut, "Spheres", SphereArray))
          {
              for (auto& Pair : SphereArray.ObjectRange())
              {
                  FSphereElem Elem;
                  SerializeSphereElem(true, Pair.second, Elem);
                  Geom.SphereElems.Add(Elem);
              }
          }
          // Sphyl
          JSON SphylArray;
          if (FJsonSerializer::ReadObject(InOut, "Sphyls", SphylArray))
          {
              for (auto& Pair : SphylArray.ObjectRange())
              {
                  FSphylElem Elem;
                  SerializeSphylElem(true, Pair.second, Elem);
                  Geom.SphylElems.Add(Elem);
              }
          }
          // Convex
          JSON ConvexArray;
          if (FJsonSerializer::ReadObject(InOut, "Convexes", ConvexArray))
          {
              for (auto& Pair : ConvexArray.ObjectRange())
              {
                  FConvexElem Elem;
                  SerializeConvexElem(true, Pair.second, Elem);
                  Geom.ConvexElems.Add(Elem);
              }
          }
      }
      else
      {
          JSON BoxArray, SphereArray, SphylArray, ConvexArray;
          for (int32 Idx = 0; Idx < Geom.BoxElems.Num(); ++Idx)
          {
              JSON ElemJson;
              SerializeBoxElem(false, ElemJson, Geom.BoxElems[Idx]);
              BoxArray[std::to_string(Idx)] = ElemJson;
          }
          for (int32 Idx = 0; Idx < Geom.SphereElems.Num(); ++Idx)
          {
              JSON ElemJson;
              SerializeSphereElem(false, ElemJson, Geom.SphereElems[Idx]);
              SphereArray[std::to_string(Idx)] = ElemJson;
          }
          for (int32 Idx = 0; Idx < Geom.SphylElems.Num(); ++Idx)
          {
              JSON ElemJson;
              SerializeSphylElem(false, ElemJson, Geom.SphylElems[Idx]);
              SphylArray[std::to_string(Idx)] = ElemJson;
          }
          for (int32 Idx = 0; Idx < Geom.ConvexElems.Num(); ++Idx)
          {
              JSON ElemJson;
              SerializeConvexElem(false, ElemJson, Geom.ConvexElems[Idx]);
              ConvexArray[std::to_string(Idx)] = ElemJson;
          }
          InOut["Boxes"] = BoxArray;
          InOut["Spheres"] = SphereArray;
          InOut["Sphyls"] = SphylArray;
          InOut["Convexes"] = ConvexArray;
      }
}

void UPhysicsAsset::SerializeConstraintFrame(bool bIsLoading, JSON& InOut, FConstraintFrame& Frame)
{
    if (bIsLoading)
    {
        FJsonSerializer::ReadVector(InOut, "Pos", Frame.Pos);
        FJsonSerializer::ReadVector(InOut, "PriAxis", Frame.PriAxis);
        FJsonSerializer::ReadVector(InOut, "SecAxis", Frame.SecAxis);
    }
    else
    {
        InOut["Pos"] = FJsonSerializer::VectorToJson(Frame.Pos);
        InOut["PriAxis"] = FJsonSerializer::VectorToJson(Frame.PriAxis);
        InOut["SecAxis"] = FJsonSerializer::VectorToJson(Frame.SecAxis);
    }
}

void UPhysicsAsset::SerializeConstraintProfile(bool bIsLoading, JSON& InOut, FConstraintProfileProperties& Profile)
{
    if (bIsLoading)
      {
          FJsonSerializer::ReadFloat(InOut, "Swing1Limit", Profile.Swing1LimitsAngle);
          FJsonSerializer::ReadInt32(InOut, "Swing1Motion", reinterpret_cast<int32&>(Profile.Swing1Motion));
          FJsonSerializer::ReadFloat(InOut, "Swing2Limit", Profile.Swing2LimitsAngle);
          FJsonSerializer::ReadInt32(InOut, "Swing2Motion", reinterpret_cast<int32&>(Profile.Swing2Motion));
          FJsonSerializer::ReadFloat(InOut, "TwistLimit", Profile.TwistLimit);
          FJsonSerializer::ReadInt32(InOut, "TwistMotion", reinterpret_cast<int32&>(Profile.TwistMotion));
          FJsonSerializer::ReadFloat(InOut, "LinearLimit", Profile.LinearLimit);
          FJsonSerializer::ReadInt32(InOut, "LinearX", reinterpret_cast<int32&>(Profile.LinearMotionX));
          FJsonSerializer::ReadInt32(InOut, "LinearY", reinterpret_cast<int32&>(Profile.LinearMotionY));
          FJsonSerializer::ReadInt32(InOut, "LinearZ", reinterpret_cast<int32&>(Profile.LinearMotionZ));
          FJsonSerializer::ReadBool(InOut, "EnableDrive", Profile.bEnableDrive);
          FJsonSerializer::ReadFloat(InOut, "Stiffness", Profile.Stiffness);
          FJsonSerializer::ReadFloat(InOut, "Damping", Profile.Damping);
          FJsonSerializer::ReadFloat(InOut, "DriveForceLimit", Profile.DriveForceLimit);
          FJsonSerializer::ReadBool(InOut, "DisableCollision", Profile.bDisableCollision);
      }
      else
      {
          InOut["Swing1Limit"] = Profile.Swing1LimitsAngle;
          InOut["Swing1Motion"] = static_cast<int32>(Profile.Swing1Motion);
          InOut["Swing2Limit"] = Profile.Swing2LimitsAngle;
          InOut["Swing2Motion"] = static_cast<int32>(Profile.Swing2Motion);
          InOut["TwistLimit"] = Profile.TwistLimit;
          InOut["TwistMotion"] = static_cast<int32>(Profile.TwistMotion);
          InOut["LinearLimit"] = Profile.LinearLimit;
          InOut["LinearX"] = static_cast<int32>(Profile.LinearMotionX);
          InOut["LinearY"] = static_cast<int32>(Profile.LinearMotionY);
          InOut["LinearZ"] = static_cast<int32>(Profile.LinearMotionZ);
          InOut["EnableDrive"] = Profile.bEnableDrive;
          InOut["Stiffness"] = Profile.Stiffness;
          InOut["Damping"] = Profile.Damping;
          InOut["DriveForceLimit"] = Profile.DriveForceLimit;
          InOut["DisableCollision"] = Profile.bDisableCollision;
      }
}

void UPhysicsAsset::SerializeConstraintSetup(bool bIsLoading, JSON& InOut, FConstraintSetup& Setup)
{
    if (bIsLoading)
    {
        FString JointName;
        FJsonSerializer::ReadString(InOut, "JointName", JointName);
        Setup.JointName = FName(JointName);
        
        FString ConstraintBone1;
        FJsonSerializer::ReadString(InOut, "Child", ConstraintBone1);
        Setup.ConstraintBone1 = FName(ConstraintBone1);
        
        FString ConstraintBone2;
        FJsonSerializer::ReadString(InOut, "Parent", ConstraintBone2);
        Setup.ConstraintBone2 = FName(ConstraintBone2);
        
        SerializeConstraintFrame(true, InOut["Frame1"], Setup.Frame1);
        SerializeConstraintFrame(true, InOut["Frame2"], Setup.Frame2);
        FJsonSerializer::ReadQuat(InOut, "AngularOffset", Setup.AngularRotationOffset);
        FJsonSerializer::ReadBool(InOut, "ScaleLinearLimits", Setup.bScaleLinearLimits);
        SerializeConstraintProfile(true, InOut["Profile"], Setup.Profile);
    }
    else
    {
        InOut["JointName"] = Setup.JointName.ToString();
        InOut["Child"] = Setup.ConstraintBone1.ToString();
        InOut["Parent"] = Setup.ConstraintBone2.ToString();
        JSON Frame1Json, Frame2Json, ProfileJson;
        SerializeConstraintFrame(false, Frame1Json, Setup.Frame1);
        SerializeConstraintFrame(false, Frame2Json, Setup.Frame2);
        SerializeConstraintProfile(false, ProfileJson, Setup.Profile);
        InOut["Frame1"] = Frame1Json;
        InOut["Frame2"] = Frame2Json;
        InOut["AngularOffset"] = FJsonSerializer::QuatToJson(Setup.AngularRotationOffset);
        InOut["ScaleLinearLimits"] = Setup.bScaleLinearLimits;
        InOut["Profile"] = ProfileJson;
    }
}

void UPhysicsAsset::SerializeSolverSettings(bool bIsLoading, JSON& InOut, FPhysicsAssetSolverSettings& Settings)
{
    if (bIsLoading)
    {
        FJsonSerializer::ReadInt32(InOut, "PosIter", Settings.PositionIterations);
        FJsonSerializer::ReadInt32(InOut, "VelIter", Settings.VelocityIterations);
        FJsonSerializer::ReadFloat(InOut, "MaxAngular", Settings.MaxAngularVelocity);
        FJsonSerializer::ReadFloat(InOut, "MaxDepen", Settings.MaxDepenetrationVelocity);
        FJsonSerializer::ReadFloat(InOut, "SleepThreshold", Settings.SleepThreshold);
        FJsonSerializer::ReadBool(InOut, "EnableStabilization", Settings.bEnableStabilization);
    }
    else
    {
        InOut["PosIter"] = Settings.PositionIterations;
        InOut["VelIter"] = Settings.VelocityIterations;
        InOut["MaxAngular"] = Settings.MaxAngularVelocity;
        InOut["MaxDepen"] = Settings.MaxDepenetrationVelocity;
        InOut["SleepThreshold"] = Settings.SleepThreshold;
        InOut["EnableStabilization"] = Settings.bEnableStabilization;
    }
}
