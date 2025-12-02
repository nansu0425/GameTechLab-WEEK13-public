#pragma once
#include "SViewerWindow.h"

class SPhysicsAssetEditorWindow : public SViewerWindow
{
public:
    SPhysicsAssetEditorWindow();
    virtual ~SPhysicsAssetEditorWindow();

    virtual void OnRender() override;
    virtual void PreRenderViewportUpdate() override;

protected:
    virtual ViewerState* CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context) override;
    virtual void DestroyViewerState(ViewerState*& State) override;
    virtual FString GetWindowTitle() const override { return "Physics Asset Editor"; }

    virtual void RenderHierarchySection() override;
    void RenderPhysicsBodyHierarchy();
    void RenderToolsPanel();

private:
    // Load a skeletal mesh into the active tab
    void LoadSkeletalMesh(ViewerState* State, const FString& Path);

    // ImGui draw callback for viewport rendering
    static void ViewportRenderCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd);

    // Physics body visualization
    EPrimitiveType SelectedPrimitiveType = EPrimitiveType::Sphere;
    bool bShowCollision = true;
    bool bCollisionShapesDirty = false;

    // Wireframe helper functions
    void DrawWireframeBox(ULineComponent* LineComp, const FVector& Center, const FVector& HalfExtents, const FQuat& Rotation, const FVector4& Color);
    void DrawWireframeSphere(ULineComponent* LineComp, const FVector& Center, float Radius, const FQuat& Rotation, const FVector4& Color, int32 Segments = 16);
    void DrawWireframeCapsule(ULineComponent* LineComp, const FVector& Center, float Radius, float HalfHeight, const FQuat& Rotation, const FVector4& Color, int32 Segments = 16);

    // Collision shape management
    void RebuildCollisionShapes();
    void ClearCollisionShapes();

    // Helper function for physics body hierarchy filtering
    bool HasBodyInSubtree(int32 BoneIndex, const TArray<struct FBone>& Bones, const TArray<TArray<int32>>& Children) const;

    // Physics body generation
    bool bUseBoneLengthGeneration = false;
    void CreateBodyForBone(int32 BoneIndex, EPrimitiveType PrimitiveType);
    void GenerateAllBodies(EPrimitiveType PrimitiveType);
    void GenerateBodiesByBoneStructure(EPrimitiveType PrimitiveType);
    void GenerateBodiesByVertexFitting(EPrimitiveType PrimitiveType);

    // Constraints generation
    void CreateConstraintForBone(int32 BoneIndex);
    void GenerateAllConstraints();

    // Helper functions for above
    int32 FindFirstChildBone(int32 BoneIndex, const FSkeleton* Skeleton) const;
    void CalculateBoneLocalShapeTransform(int32 BoneIndex, const FSkeleton* Skeleton, class USkeletalMeshComponent* MeshComp, FVector& OutLocalCenter, FQuat& OutLocalRotation);
    void CalculateBodyDimensions(int32 BoneIndex, const struct FSkeleton* Skeleton, class USkeletalMeshComponent* MeshComp, 
                                 EPrimitiveType PrimitiveType, float& OutRadius, float& OutHalfHeight, FVector& OutExtent) const;
    bool ShouldCreateBodyForBone(int32 BoneIndex, const struct FSkeleton* Skeleton) const;

    // Vertex-driven body generation
    struct FBoneVertexInfluence
    {
        TArray<FVector> Vertices;  // World-space vertex positions influenced by this bone
        float TotalWeight;         // Sum of all weights for this bone
    };

    void BuildBoneVertexInfluenceMap(const struct FSkeletalMeshData* MeshData, TArray<FBoneVertexInfluence>& OutInfluenceMap, float MinWeightThreshold = 0.3f) const;
    FVector CalculatePrincipalAxis(const TArray<FVector>& Vertices) const;
    void FitMinimalSphere(const TArray<FVector>& Vertices, FVector& OutCenter, float& OutRadius) const;
    void FitMinimalCapsule(const TArray<FVector>& Vertices, const FVector& PrincipalAxis, FVector& OutCenter, FQuat& OutRotation, float& OutRadius, float& OutHalfHeight) const;
    void FitMinimalBox(const TArray<FVector>& Vertices, const FVector& PrincipalAxis, FVector& OutCenter, FQuat& OutRotation, FVector& OutExtent) const;

    // Physics Body Icon
    class UTexture* IconSingleBody = nullptr;
    class UTexture* IconMultipleBody = nullptr;
    bool bIconsLoaded = false;
};