#pragma once
#include "MeshComponent.h"
#include "SkeletalMesh.h"
#include "USkinnedMeshComponent.generated.h"

UCLASS(DisplayName="스킨드 메시 컴포넌트", Description="스켈레탈 메시를 렌더링하는 컴포넌트입니다")
class USkinnedMeshComponent : public UMeshComponent
{
public:
    GENERATED_REFLECTION_BODY()

    USkinnedMeshComponent();
    ~USkinnedMeshComponent() override;

    void BeginPlay() override;
    void TickComponent(float DeltaTime) override;

    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
    void DuplicateSubObjects() override;
    
// Mesh Component Section
public:
    void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;
    
    FAABB GetWorldAABB() const override;
    void OnTransformUpdated() override;

// Skeletal Section
public:
    /**
     * @brief 렌더링할 스켈레탈 메시 에셋 설정 (UStaticMeshComponent::SetStaticMesh와 동일한 역할)
     * @param PathFileName 새 스켈레탈 메시 에셋 경로
     */
    virtual void SetSkeletalMesh(const FString& PathFileName);
    /**
     * @brief 이 컴포넌트의 USkeletalMesh 에셋을 반환
     */
    USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh; }

    /**
     * @brief GPU 스키닝 사용 여부 설정
     */
    void SetUseGPUSkinning(bool bInUseGPU);
    /**
     * @brief GPU 스키닝 사용 여부 반환
     */
    bool IsUsingGPUSkinning() const { return bUseGPUSkinning; }

protected:
    void PerformSkinning();
    /**
     * @brief 자식에게서 원본 메시를 받아 CPU 스키닝을 수행
     * @param InSkinningMatrices 스키닝 매트릭스
     */
    void UpdateSkinningMatrices(const TArray<FMatrix>& InSkinningMatrices, const TArray<FMatrix>& InSkinningNormalMatrices);

    /**
     * @brief GPU 스키닝을 위해 본 행렬을 GPU 버퍼로 업로드
     */
    void UpdateBoneMatrixBuffer();
    
    UPROPERTY(EditAnywhere, Category = "Skeletal Mesh", Tooltip = "Skeletal mesh asset to render")
    USkeletalMesh* SkeletalMesh;

    /**
     * @brief CPU 스키닝 최종 결과물. 렌더러가 이 데이터를 사용합니다.
     */
    TArray<FNormalVertex> SkinnedVertices;
    /**
     * @brief CPU 스키닝 최종 결과물. 렌더러가 이 데이터를 사용합니다.
     */
    TArray<FNormalVertex> NormalSkinnedVertices;

private:
    FVector SkinVertexPosition(const FSkinnedVertex& InVertex) const;
    FVector SkinVertexNormal(const FSkinnedVertex& InVertex) const;
    FVector4 SkinVertexTangent(const FSkinnedVertex& InVertex) const;

    /**
     * @brief 자식이 계산해 준, 현재 프레임의 최종 스키닝 행렬
    */
    TArray<FMatrix> FinalSkinningMatrices;
    TArray<FMatrix> FinalSkinningNormalMatrices;
    bool bSkinningMatricesDirty = true;
    
    /**
     * @brief CPU 스키닝에서 진행하기 때문에, Component별로 VertexBuffer를 가지고 스키닝 업데이트를 진행해야함
    */
    ID3D11Buffer* VertexBuffer = nullptr;

    // GPU Skinning
    /**
     * @brief GPU 스키닝 사용 여부 플래그
     */
    bool bUseGPUSkinning = false;
    /**
     * @brief GPU 스키닝용 버텍스 버퍼 (FSkinnedVertex, 변경되지 않음)
     */
    ID3D11Buffer* GPUSkinnedVertexBuffer = nullptr;
    /**
     * @brief GPU 스키닝용 본 행렬 상수 버퍼
     */
    ID3D11Buffer* BoneMatricesBuffer = nullptr;
    /**
     * @brief GPU 스키닝용 본 노멀 행렬 상수 버퍼 (현재는 하나의 버퍼에 통합됨)
     */
    ID3D11Buffer* BoneNormalMatricesBuffer = nullptr;
};
