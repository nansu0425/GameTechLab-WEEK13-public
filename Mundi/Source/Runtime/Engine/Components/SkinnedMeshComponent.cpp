#include "pch.h"
#include "SkinnedMeshComponent.h"
#include "MeshBatchElement.h"
#include "SceneView.h"

USkinnedMeshComponent::USkinnedMeshComponent() : SkeletalMesh(nullptr)
{
   bCanEverTick = true;
}

USkinnedMeshComponent::~USkinnedMeshComponent()
{
   if (VertexBuffer)
   {
      VertexBuffer->Release();
      VertexBuffer = nullptr;
   }

   // GPU 스키닝 버퍼 해제
   if (GPUSkinnedVertexBuffer)
   {
      GPUSkinnedVertexBuffer->Release();
      GPUSkinnedVertexBuffer = nullptr;
   }
   if (BoneMatricesBuffer)
   {
      BoneMatricesBuffer->Release();
      BoneMatricesBuffer = nullptr;
   }
   if (BoneNormalMatricesBuffer)
   {
      BoneNormalMatricesBuffer->Release();
      BoneNormalMatricesBuffer = nullptr;
   }
}

void USkinnedMeshComponent::BeginPlay()
{
   Super::BeginPlay();
}

void USkinnedMeshComponent::TickComponent(float DeltaTime)
{
   UMeshComponent::TickComponent(DeltaTime);
}

void USkinnedMeshComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
   Super::Serialize(bInIsLoading, InOutHandle);

   if (bInIsLoading)
   {
      SetSkeletalMesh(SkeletalMesh->GetPathFileName());
   }
   // @TODO - UStaticMeshComponent처럼 프로퍼티 기반 직렬화 로직 추가
}

void USkinnedMeshComponent::DuplicateSubObjects()
{
   Super::DuplicateSubObjects();

   // GPU/CPU 모드에 따라 적절한 버텍스 버퍼 생성
   if (bUseGPUSkinning)
   {
      SkeletalMesh->CreateGPUSkinnedVertexBuffer(&GPUSkinnedVertexBuffer);
      // PIE 종료 후 본 버퍼를 다시 생성해야 하므로 dirty 플래그 설정
      BoneMatricesBuffer = nullptr;
   }
   else
   {
      SkeletalMesh->CreateVertexBuffer(&VertexBuffer);
   }

   // PIE 종료 후 스키닝 데이터를 다시 계산하도록 플래그 설정
   bSkinningMatricesDirty = true;
}

void USkinnedMeshComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
    if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData()) { return; }

   // 스키닝 데이터가 변경되었으면 업데이트
   if (bSkinningMatricesDirty)
   {
      if (bUseGPUSkinning)
      {
         // GPU 스키닝: 본 행렬 버퍼만 업데이트
         UpdateBoneMatrixBuffer();
      }
      else
      {
         // CPU 스키닝: 버텍스 버퍼 업데이트
         SkeletalMesh->UpdateVertexBuffer(SkinnedVertices, VertexBuffer);
      }
      bSkinningMatricesDirty = false;
   }

    const TArray<FGroupInfo>& MeshGroupInfos = SkeletalMesh->GetMeshGroupInfo();
    auto DetermineMaterialAndShader = [&](uint32 SectionIndex) -> TPair<UMaterialInterface*, UShader*>
    {
       UMaterialInterface* Material = GetMaterial(SectionIndex);
       UShader* Shader = nullptr;

       if (Material && Material->GetShader())
       {
          Shader = Material->GetShader();
       }
       else
       {
          // UE_LOG("USkinnedMeshComponent: 머티리얼이 없거나 셰이더가 없어서 기본 머티리얼 사용 section %u.", SectionIndex);
          Material = UResourceManager::GetInstance().GetDefaultMaterial();
          if (Material)
          {
             Shader = Material->GetShader();
          }
          if (!Material || !Shader)
          {
             UE_LOG("USkinnedMeshComponent: 기본 머티리얼이 없습니다.");
             return { nullptr, nullptr };
          }
       }
       return { Material, Shader };
    };

    const bool bHasSections = !MeshGroupInfos.IsEmpty();
    const uint32 NumSectionsToProcess = bHasSections ? static_cast<uint32>(MeshGroupInfos.size()) : 1;

    for (uint32 SectionIndex = 0; SectionIndex < NumSectionsToProcess; ++SectionIndex)
    {
       uint32 IndexCount = 0;
       uint32 StartIndex = 0;

       if (bHasSections)
       {
          const FGroupInfo& Group = MeshGroupInfos[SectionIndex];
          IndexCount = Group.IndexCount;
          StartIndex = Group.StartIndex;
       }
       else
       {
          IndexCount = SkeletalMesh->GetIndexCount();
          StartIndex = 0;
       }

       if (IndexCount == 0)
       {
          continue;
       }

       auto [MaterialToUse, ShaderToUse] = DetermineMaterialAndShader(SectionIndex);
       if (!MaterialToUse || !ShaderToUse)
       {
          continue;
       }

       FMeshBatchElement BatchElement;
       TArray<FShaderMacro> ShaderMacros = View->ViewShaderMacros;
       if (0 < MaterialToUse->GetShaderMacros().Num())
       {
          ShaderMacros.Append(MaterialToUse->GetShaderMacros());
       }

       // GPU 스키닝 매크로 추가
       if (bUseGPUSkinning)
       {
          FShaderMacro GPUSkinningMacro;
          GPUSkinningMacro.Name = FName("GPU_SKINNING");
          GPUSkinningMacro.Definition = FName("1");
          ShaderMacros.Add(GPUSkinningMacro);
       }

       FShaderVariant* ShaderVariant = ShaderToUse->GetOrCompileShaderVariant(ShaderMacros);

       if (ShaderVariant)
       {
          BatchElement.VertexShader = ShaderVariant->VertexShader;
          BatchElement.PixelShader = ShaderVariant->PixelShader;
          BatchElement.InputLayout = ShaderVariant->InputLayout;
       }
       
       BatchElement.Material = MaterialToUse;

       // GPU/CPU 모드에 따라 적절한 버텍스 버퍼 및 스트라이드 설정
       if (bUseGPUSkinning)
       {
          BatchElement.VertexBuffer = GPUSkinnedVertexBuffer;
          BatchElement.VertexStride = sizeof(FSkinnedVertex);
       }
       else
       {
          BatchElement.VertexBuffer = VertexBuffer;
          BatchElement.VertexStride = SkeletalMesh->GetVertexStride();
       }

       BatchElement.IndexBuffer = SkeletalMesh->GetIndexBuffer();
       
       BatchElement.IndexCount = IndexCount;
       BatchElement.StartIndex = StartIndex;
       BatchElement.BaseVertexIndex = 0;
       BatchElement.WorldMatrix = GetWorldMatrix();
       BatchElement.ObjectID = InternalIndex;
       BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

       // GPU 스키닝 모드일 때 본 버퍼 설정
       if (bUseGPUSkinning && BoneMatricesBuffer)
       {
          // AddRef to keep buffer alive even if component is destroyed
          BoneMatricesBuffer->AddRef();
          BatchElement.BoneMatricesBuffer = BoneMatricesBuffer;
       }
       else
       {
          BatchElement.BoneMatricesBuffer = nullptr;
       }

       OutMeshBatchElements.Add(BatchElement);
    }
}

FAABB USkinnedMeshComponent::GetWorldAABB() const
{
   return {};
   // const FTransform WorldTransform = GetWorldTransform();
   // const FMatrix WorldMatrix = GetWorldMatrix();
   //
   // if (!SkeletalMesh)
   // {
   //    const FVector Origin = WorldTransform.TransformPosition(FVector());
   //    return FAABB(Origin, Origin);
   // }
   //
   // const FAABB LocalBound = SkeletalMesh->GetLocalBound(); // <-- 이 함수 구현 필요
   // const FVector LocalMin = LocalBound.Min;
   // const FVector LocalMax = LocalBound.Max;
   //
   // // ... (이하 AABB 계산 로직은 UStaticMeshComponent와 동일) ...
   // const FVector LocalCorners[8] = {
   //    FVector(LocalMin.X, LocalMin.Y, LocalMin.Z),
   //    FVector(LocalMax.X, LocalMin.Y, LocalMin.Z),
   //    // ... (나머지 6개 코너) ...
   //    FVector(LocalMax.X, LocalMax.Y, LocalMax.Z)
   // };
   //
   // FVector4 WorldMin4 = FVector4(LocalCorners[0].X, LocalCorners[0].Y, LocalCorners[0].Z, 1.0f) * WorldMatrix;
   // FVector4 WorldMax4 = WorldMin4;
   //
   // for (int32 CornerIndex = 1; CornerIndex < 8; ++CornerIndex)
   // {
   //    const FVector4 WorldPos = FVector4(LocalCorners[CornerIndex].X
   //       , LocalCorners[CornerIndex].Y
   //       , LocalCorners[CornerIndex].Z
   //       , 1.0f)
   //       * WorldMatrix;
   //    WorldMin4 = WorldMin4.ComponentMin(WorldPos);
   //    WorldMax4 = WorldMax4.ComponentMax(WorldPos);
   // }
   //
   // FVector WorldMin = FVector(WorldMin4.X, WorldMin4.Y, WorldMin4.Z);
   // FVector WorldMax = FVector(WorldMax4.X, WorldMax4.Y, WorldMax4.Z);
   // return FAABB(WorldMin, WorldMax);
}

void USkinnedMeshComponent::OnTransformUpdated()
{
   Super::OnTransformUpdated();
   MarkWorldPartitionDirty();
}

void USkinnedMeshComponent::SetSkeletalMesh(const FString& PathFileName)
{
   ClearDynamicMaterials();

   SkeletalMesh = UResourceManager::GetInstance().Load<USkeletalMesh>(PathFileName);

   if (VertexBuffer)
   {
      VertexBuffer->Release();
      VertexBuffer = nullptr;
   }
   if (GPUSkinnedVertexBuffer)
   {
      GPUSkinnedVertexBuffer->Release();
      GPUSkinnedVertexBuffer = nullptr;
   }

   if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshData())
   {
      // GPU/CPU 모드에 따라 적절한 버텍스 버퍼 생성
      if (bUseGPUSkinning)
      {
         SkeletalMesh->CreateGPUSkinnedVertexBuffer(&GPUSkinnedVertexBuffer);
      }
      else
      {
         SkeletalMesh->CreateVertexBuffer(&VertexBuffer);
      }

      const TArray<FMatrix> IdentityMatrices(SkeletalMesh->GetBoneCount(), FMatrix::Identity());
      UpdateSkinningMatrices(IdentityMatrices, IdentityMatrices);
      PerformSkinning();
      
      const TArray<FGroupInfo>& GroupInfos = SkeletalMesh->GetMeshGroupInfo();
       MaterialSlots.resize(GroupInfos.size());
       for (int i = 0; i < GroupInfos.size(); ++i)
      {
         // FGroupInfo에 InitialMaterialName이 있다고 가정
         SetMaterialByName(i, GroupInfos[i].InitialMaterialName);
      }
      MarkWorldPartitionDirty();
   }
   else
   {
      SkeletalMesh = nullptr;
      UpdateSkinningMatrices(TArray<FMatrix>(), TArray<FMatrix>());
      PerformSkinning();
   }
}

void USkinnedMeshComponent::SetUseGPUSkinning(bool bInUseGPU)
{
   if (bUseGPUSkinning == bInUseGPU) { return; } // 이미 같은 모드면 아무것도 안 함

   bUseGPUSkinning = bInUseGPU;

   // 버퍼 재생성
   if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData()) { return; }

   if (bUseGPUSkinning)
   {
      // GPU 모드로 전환: GPU 버퍼 생성, CPU 버퍼 해제
      if (VertexBuffer)
      {
         VertexBuffer->Release();
         VertexBuffer = nullptr;
      }
      if (!GPUSkinnedVertexBuffer)
      {
         SkeletalMesh->CreateGPUSkinnedVertexBuffer(&GPUSkinnedVertexBuffer);
      }
      // 본 행렬 버퍼도 업데이트
      bSkinningMatricesDirty = true;
   }
   else
   {
      // CPU 모드로 전환: CPU 버퍼 생성, GPU 버퍼 해제
      if (GPUSkinnedVertexBuffer)
      {
         GPUSkinnedVertexBuffer->Release();
         GPUSkinnedVertexBuffer = nullptr;
      }
      if (BoneMatricesBuffer)
      {
         BoneMatricesBuffer->Release();
         BoneMatricesBuffer = nullptr;
      }
      if (!VertexBuffer)
      {
         SkeletalMesh->CreateVertexBuffer(&VertexBuffer);
      }
      // CPU 스키닝 다시 수행
      bSkinningMatricesDirty = true;
   }
}

void USkinnedMeshComponent::PerformSkinning()
{
   if (!SkeletalMesh || FinalSkinningMatrices.IsEmpty()) { return; }
   if (!bSkinningMatricesDirty) { return; }

   if (bUseGPUSkinning)
   {
      // GPU 스키닝: 본 행렬만 GPU로 업로드
      UpdateBoneMatrixBuffer();
      bSkinningMatricesDirty = false;
   }
   else
   {
      // CPU 스키닝: 기존 방식
      const TArray<FSkinnedVertex>& SrcVertices = SkeletalMesh->GetSkeletalMeshData()->Vertices;
      const int32 NumVertices = SrcVertices.Num();
      SkinnedVertices.SetNum(NumVertices);

      for (int32 Idx = 0; Idx < NumVertices; ++Idx)
      {
         const FSkinnedVertex& SrcVert = SrcVertices[Idx];
         FNormalVertex& DstVert = SkinnedVertices[Idx];

         DstVert.pos = SkinVertexPosition(SrcVert);
         DstVert.normal = SkinVertexNormal(SrcVert);
         DstVert.Tangent = SkinVertexTangent(SrcVert);
         DstVert.tex = SrcVert.UV;
      }
   }
}

void USkinnedMeshComponent::UpdateSkinningMatrices(const TArray<FMatrix>& InSkinningMatrices, const TArray<FMatrix>& InSkinningNormalMatrices)
{
   FinalSkinningMatrices = InSkinningMatrices;
   FinalSkinningNormalMatrices = InSkinningNormalMatrices;
   bSkinningMatricesDirty = true;
}

FVector USkinnedMeshComponent::SkinVertexPosition(const FSkinnedVertex& InVertex) const
{
   FVector BlendedPosition(0.f, 0.f, 0.f);

   for (int32 Idx = 0; Idx < 4; ++Idx)
   {
      const uint32 BoneIndex = InVertex.BoneIndices[Idx];
      const float Weight = InVertex.BoneWeights[Idx];

      if (Weight > 0.f)
      {
         const FMatrix& SkinMatrix = FinalSkinningMatrices[BoneIndex];
         FVector TransformedPosition = SkinMatrix.TransformPosition(InVertex.Position);
         BlendedPosition += TransformedPosition * Weight;
      }
   }

   return BlendedPosition;
}

FVector USkinnedMeshComponent::SkinVertexNormal(const FSkinnedVertex& InVertex) const
{
   FVector BlendedNormal(0.f, 0.f, 0.f);

   for (int32 Idx = 0; Idx < 4; ++Idx)
   {
      const uint32 BoneIndex = InVertex.BoneIndices[Idx];
      const float Weight = InVertex.BoneWeights[Idx];

      if (Weight > 0.f)
      {
         const FMatrix& SkinMatrix = FinalSkinningNormalMatrices[BoneIndex];
         FVector TransformedNormal = SkinMatrix.TransformVector(InVertex.Normal);
         BlendedNormal += TransformedNormal * Weight;
      }
   }

   return BlendedNormal.GetSafeNormal();
}

FVector4 USkinnedMeshComponent::SkinVertexTangent(const FSkinnedVertex& InVertex) const
{
   const FVector OriginalTangentDir(InVertex.Tangent.X, InVertex.Tangent.Y, InVertex.Tangent.Z);
   const float OriginalSignW = InVertex.Tangent.W;

   FVector BlendedTangentDir(0.f, 0.f, 0.f);

   for (int32 Idx = 0; Idx < 4; ++Idx)
   {
      const uint32 BoneIndex = InVertex.BoneIndices[Idx];
      const float Weight = InVertex.BoneWeights[Idx];

      if (Weight > 0.f)
      {
         const FMatrix& SkinMatrix = FinalSkinningMatrices[BoneIndex];
         FVector TransformedTangentDir = SkinMatrix.TransformVector(OriginalTangentDir);
         BlendedTangentDir += TransformedTangentDir * Weight;
      }
   }

   const FVector FinalTangentDir = BlendedTangentDir.GetSafeNormal();
   return { FinalTangentDir.X, FinalTangentDir.Y, FinalTangentDir.Z, OriginalSignW };
}

void USkinnedMeshComponent::UpdateBoneMatrixBuffer()
{
   constexpr int32 MAX_BONES = 128;

   // 본 행렬과 노멀 행렬을 합친 버퍼 구조체
   struct FBoneMatricesBufferData
   {
      FMatrix BoneMatrices[MAX_BONES];
      FMatrix BoneNormalMatrices[MAX_BONES];
   };

   FBoneMatricesBufferData BufferData;
   ZeroMemory(&BufferData, sizeof(BufferData));

   // 본 행렬 복사 (최대 MAX_BONES개까지)
   const int32 NumBones = FMath::Min(FinalSkinningMatrices.Num(), MAX_BONES);
   for (int32 i = 0; i < NumBones; ++i)
   {
      BufferData.BoneMatrices[i] = FinalSkinningMatrices[i];
      BufferData.BoneNormalMatrices[i] = FinalSkinningNormalMatrices[i];
   }

   // 버퍼가 없으면 생성
   if (!BoneMatricesBuffer)
   {
      D3D11_BUFFER_DESC BufferDesc;
      ZeroMemory(&BufferDesc, sizeof(BufferDesc));
      BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
      BufferDesc.ByteWidth = sizeof(FBoneMatricesBufferData);
      BufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
      BufferDesc.MiscFlags = 0;
      BufferDesc.StructureByteStride = 0;

      D3D11_SUBRESOURCE_DATA InitData;
      ZeroMemory(&InitData, sizeof(InitData));
      InitData.pSysMem = &BufferData;

      ID3D11Device* Device = GEngine.GetRHIDevice()->GetDevice();
      HRESULT hr = Device->CreateBuffer(&BufferDesc, &InitData, &BoneMatricesBuffer);
      if (FAILED(hr))
      {
         UE_LOG("Failed to create bone matrices buffer");
         return;
      }
   }
   else
   {
      // 버퍼 업데이트
      ID3D11DeviceContext* Context = GEngine.GetRHIDevice()->GetDeviceContext();

      D3D11_MAPPED_SUBRESOURCE MappedResource;
      HRESULT hr = Context->Map(BoneMatricesBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
      if (SUCCEEDED(hr))
      {
         memcpy(MappedResource.pData, &BufferData, sizeof(FBoneMatricesBufferData));
         Context->Unmap(BoneMatricesBuffer, 0);
      }
      else
      {
         UE_LOG("Failed to map bone matrices buffer");
      }
   }
}
