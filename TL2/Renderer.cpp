#include "pch.h"
#include "TextRenderComponent.h"
#include "Shader.h"
#include "StaticMesh.h"
#include "Quad.h"
#include "StaticMeshComponent.h"
#include "BillboardComponent.h"
#include "FViewportClient.h"
#include "FViewport.h"
#include "World.h"
#include "RenderManager.h"
#include "WorldPartitionManager.h"
#include "Renderer.h"
#include "CameraActor.h"
#include "CameraComponent.h"
#include "PrimitiveComponent.h"
#include "StaticMeshActor.h"
#include "GizmoActor.h"
#include "GridActor.h"
#include "Octree.h"
#include "BVHierachy.h"
#include "Occlusion.h"
#include "Frustum.h"
#include "AABoundingBoxComponent.h"
#include "ResourceManager.h"
#include "RHIDevice.h"
#include "Material.h"
#include "Texture.h"
#include "RenderSettings.h"
#include "EditorEngine.h"
#include "DecalComponent.h"

#include <Windows.h>

URenderer::URenderer(D3D11RHI* InDevice) : RHIDevice(InDevice), OcclusionCPU(new FOcclusionCullingManagerCPU())
{
	InitializeLineBatch();

	/* // 오클루전 관련 초기화
	CreateDepthOnlyStates();
	CreateUnitCube();
	CreateOcclusionCB();
	*/
	//RHIDevice->OMSetRenderTargets();

}

URenderer::~URenderer()
{
	if (LineBatchData)
	{
		delete LineBatchData;
	}
}

void URenderer::BeginFrame()
{
	// 백버퍼/깊이버퍼를 클리어
	RHIDevice->ClearBackBuffer();  // 배경색
	RHIDevice->ClearDepthBuffer(1.0f, 0);                 // 깊이값 초기화
	//RHIDevice->CreateBlendState();
	RHIDevice->IASetPrimitiveTopology();
	// RS
	RHIDevice->RSSetViewport();

	//OM
	//RHIDevice->OMSetBlendState();
	RHIDevice->OMSetRenderTargets();

	// TODO - 한 종류 메쉬만 스폰했을 때 깨지는 현상 방지 임시이므로 고쳐야합니다
	// ★ 캐시 무효화
	//PreShader = nullptr;
	//PreUMaterial = nullptr; // 이거 주석 처리 시: 피킹하면 그 UStatucjMesh의 텍스쳐가 전부 사라짐
	//PreStaticMesh = nullptr; // 이거 주석 처리 시: 메시가 이상해짐
	//PreViewModeIndex = EViewModeIndex::VMI_Wireframe; // 어차피 SetViewModeType이 다시 셋
}

void URenderer::EndFrame()
{

	RHIDevice->Present();
}

void URenderer::RenderSceneForView(UWorld* World, ACameraActor* InCamera, FViewport* Viewport)
{
	if (!Viewport) return;
	if (!World) return; // update current world

	FViewportClient* Client = Viewport->GetViewportClient();
	if (!Client) return;

	ACameraActor* Camera = Client->GetCamera();
	if (!Camera) return;

	//기즈모 카메라 설정
	if (World->GetGizmoActor())
		World->GetGizmoActor()->SetCameraActor(Camera);

	World->GetRenderSettings().SetViewModeIndex(Client->GetViewModeIndex());
	{
		if (!World || !Camera || !Viewport) return;

		int objCount = static_cast<int>(World->GetActors().size());
		int visibleCount = 0;
		float zNear = 0.1f, zFar = 100.f;
		// 뷰포트의 실제 크기로 aspect ratio 계산
		float ViewportAspectRatio = static_cast<float>(Viewport->GetSizeX()) / static_cast<float>(Viewport->GetSizeY());
		if (Viewport->GetSizeY() == 0) ViewportAspectRatio = 1.0f; // 0으로 나누기 방지

		// Provide per-viewport size to renderer (used by overlay/gizmo scaling)
		SetCurrentViewportSize(Viewport->GetSizeX(), Viewport->GetSizeY());

		FMatrix ViewMatrix = Camera->GetViewMatrix();
		FMatrix ProjectionMatrix = Camera->GetProjectionMatrix(ViewportAspectRatio, Viewport);

		Frustum ViewFrustum;
		UCameraComponent* CamComp = nullptr;
		if (CamComp = Camera->GetCameraComponent())
		{
			ViewFrustum = CreateFrustumFromCamera(*CamComp, ViewportAspectRatio);
			zNear = CamComp->GetNearClip();
			zFar = CamComp->GetFarClip();
		}
		FVector rgb(1.0f, 1.0f, 1.0f);

		// === Begin Line Batch for all actors ===
		BeginLineBatch();

		// === Draw Actors with Show Flag checks ===
		// Compute effective view mode with ShowFlag override for wireframe
		EViewModeIndex EffectiveViewMode = World->GetRenderSettings().GetViewModeIndex();
		if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Wireframe))
		{
			EffectiveViewMode = EViewModeIndex::VMI_Wireframe;
		}
		SetViewModeType(EffectiveViewMode);

		// ============ Culling Logic Dispatch ========= //
		//for (AActor* Actor : World->GetActors())
		//	Actor->SetCulled(true);
		//if (World->GetPartitionManager())
		//	World->GetPartitionManager()->FrustumQuery(ViewFrustum);

		RHIDevice->UpdateHighLightConstantBuffers(false, rgb, 0, 0, 0, 0);

		// ---------------------- CPU HZB Occlusion ----------------------
		if (bUseCPUOcclusion)
		{
			// 1) 그리드 사이즈 보정(해상도 변화 대응)
			UpdateOcclusionGridSizeForViewport(Viewport);

			// 2) 오클루더/오클루디 수집
			TArray<FCandidateDrawable> Occluders, Occludees;
			BuildCpuOcclusionSets(ViewFrustum, ViewMatrix, ProjectionMatrix, zNear, zFar,
				Occluders, Occludees);

			// 3) 오클루더로 저해상도 깊이 빌드 + HZB
			OcclusionCPU->BuildOccluderDepth(Occluders, Viewport->GetSizeX(), Viewport->GetSizeY());
			OcclusionCPU->BuildHZB();

			// 4) 가시성 판정 → VisibleFlags[UUID] = 0/1
			//     VisibleFlags 크기 보장
			uint32_t maxUUID = 0;
			for (auto& C : Occludees) maxUUID = std::max(maxUUID, C.ActorIndex);
			if (VisibleFlags.size() <= size_t(maxUUID))
				VisibleFlags.assign(size_t(maxUUID + 1), 1); // 기본 보임

			OcclusionCPU->TestOcclusion(Occludees, Viewport->GetSizeX(), Viewport->GetSizeY(), VisibleFlags);
		}
		// ----------------------------------------------------------------

		{	// 일반 액터들 렌더링

			// 렌더 목록 수집
			FVisibleRenderProxySet RenderProxySet;

			if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Primitives))
			{
				for (AActor* Actor : World->GetActors())
				{
					if (!Actor) continue;
					if (Actor->GetActorHiddenInGame()) continue;
					if (Actor->GetCulled()) continue; // 컬링된 액터는 스킵

					// ★★★ CPU 오클루전 컬링: UUID로 보임 여부 확인
					if (bUseCPUOcclusion)
					{
						uint32_t id = Actor->UUID;
						if (id < VisibleFlags.size() && VisibleFlags[id] == 0)
						{
							continue; // 가려짐 → 스킵c
						}
					}

					if (Cast<AStaticMeshActor>(Actor) && !World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_StaticMeshes))
					{
						continue;
					}

					for (USceneComponent* Component : Actor->GetSceneComponents())
					{
						if (!Component)
						{
							continue;
						}
						if (UActorComponent* ActorComp = Cast<UActorComponent>(Component))
						{
							if (!ActorComp->IsActive())
							{
								continue;
							}

							if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
							{
								RenderProxySet.Primitives.Add(Primitive);
							}
							else if (UDecalComponent* Decal = Cast<UDecalComponent>(Component))
							{
								RenderProxySet.Decals.Add(Decal);
							}
						}
					}
				}

				SetViewModeType(EffectiveViewMode);
				RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqual);

				// Primitives 그리기
				for (UPrimitiveComponent* Primitive : RenderProxySet.Primitives)
				{
					Primitive->Render(this, ViewMatrix, ProjectionMatrix);

					visibleCount++;
				}

				// NOTE: 디버깅용 임시 Decals 외곽선 그리기
				for (UDecalComponent* Decal : RenderProxySet.Decals)
				{
					Decal->RenderDebugVolume(this, ViewMatrix, ProjectionMatrix);
				}

				SetViewModeType(EffectiveViewMode);
				RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqualReadOnly);
				RHIDevice->OMSetBlendState(true);

				// Decals 그리기
				for (UDecalComponent* Decal : RenderProxySet.Decals)
				{
					// Todo: RenderProxySet.Primitives 중에 Decals과 충돌한 Primitives 추출
					// [임시] 모든 오브젝트를 데칼과 충돌했다 판정 / 추후 실제로 충돌한 Primitives만 다시 그리도록 변경
					TArray<UPrimitiveComponent*> TargetPrimitives = RenderProxySet.Primitives;

					for (UPrimitiveComponent* Target : TargetPrimitives)
					{
						Decal->RenderAffectedPrimitives(this, Target, ViewMatrix, ProjectionMatrix);
					}

					visibleCount++;
				}
			}
		}

		// 엔진 액터들 (그리드 등)
		for (AActor* EngineActor : World->GetEditorActors())
		{
			if (!EngineActor || EngineActor->GetActorHiddenInGame())
				continue;

			if (Cast<AGridActor>(EngineActor) && !World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Grid))
				continue;

			for (USceneComponent* Component : EngineActor->GetSceneComponents())
			{
				if (!Component || !Component->IsActive())
					continue;
				if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
				{
					SetViewModeType(EffectiveViewMode);
					Primitive->Render(this, ViewMatrix, ProjectionMatrix);
					RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqual);
				}
			}
			RHIDevice->OMSetBlendState(false);
		}

		// Debug draw (exclusive: BVH first, else Octree)
		if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_BVHDebug) &&
			World->GetPartitionManager())
		{
			if (FBVHierachy* BVH = World->GetPartitionManager()->GetBVH())
			{
				BVH->DebugDraw(this);
			}
		}
		else if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_OctreeDebug) &&
			World->GetPartitionManager())
		{
			if (FOctree* Octree = World->GetPartitionManager()->GetSceneOctree())
			{
				Octree->DebugDraw(this);
			}
		}
		EndLineBatch(FMatrix::Identity(), ViewMatrix, ProjectionMatrix);

		RHIDevice->UpdateHighLightConstantBuffers(false, rgb, 0, 0, 0, 0);
		if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Culling))
		{
			UE_LOG("Obj count: %d, Visible count: %d\r\n", objCount, visibleCount);
		}
	}
}

void URenderer::UpdateOcclusionGridSizeForViewport(FViewport* Viewport)
{
	if (!Viewport) return;
	int vw = (1 > Viewport->GetSizeX()) ? 1 : Viewport->GetSizeX();
	int vh = (1 > Viewport->GetSizeY()) ? 1 : Viewport->GetSizeY();
	int gw = std::max(1, vw / std::max(1, OcclGridDiv));
	int gh = std::max(1, vh / std::max(1, OcclGridDiv));
	// 매 프레임 호출해도 싸다. 내부에서 동일크기면 skip
	OcclusionCPU->Initialize(gw, gh);
}

void URenderer::BuildCpuOcclusionSets(
	const Frustum& ViewFrustum,
	const FMatrix& View,
	const FMatrix& Proj,
	float ZNear,
	float ZFar,
	TArray<FCandidateDrawable>& OutOccluders,
	TArray<FCandidateDrawable>& OutOccludees)
{
	OutOccluders.clear();
	OutOccludees.clear();

	size_t estimatedCount = 0;
	for (AActor* Actor : World->GetActors())
	{
		if (Actor && !Actor->GetActorHiddenInGame() && !Actor->GetCulled())
		{
			if (Actor->IsA<AStaticMeshActor>()) estimatedCount++;
		}
	}
	OutOccluders.reserve(estimatedCount);
	OutOccludees.reserve(estimatedCount);

	const FMatrix VP = View * Proj; // 행벡터: p_world * View * Proj

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor) continue;
		if (Actor->GetActorHiddenInGame()) continue;
		if (Actor->GetCulled()) continue;

		AStaticMeshActor* SMA = Cast<AStaticMeshActor>(Actor);
		if (!SMA) continue;

		UAABoundingBoxComponent* Box = Cast<UAABoundingBoxComponent>(SMA->CollisionComponent);
		if (!Box) continue;

		OutOccluders.emplace_back();
		FCandidateDrawable& occluder = OutOccluders.back();
		occluder.ActorIndex = Actor->UUID;
		occluder.Bound = Box->GetWorldBound();
		occluder.WorldViewProj = VP;
		occluder.WorldView = View;
		occluder.ZNear = ZNear;
		occluder.ZFar = ZFar;

		OutOccludees.emplace_back(occluder);
	}
}

void URenderer::DrawIndexedPrimitiveComponent(UStaticMesh* InMesh, D3D11_PRIMITIVE_TOPOLOGY InTopology, const TArray<FMaterialSlot>& InComponentMaterialSlots)
{
	UINT stride = 0;
	switch (InMesh->GetVertexType())
	{
	case EVertexLayoutType::PositionColor:
		stride = sizeof(FVertexSimple);
		break;
	case EVertexLayoutType::PositionColorTexturNormal:
		stride = sizeof(FVertexDynamic);
		break;
	case EVertexLayoutType::PositionTextBillBoard:
		stride = sizeof(FBillboardVertexInfo_GPU);
		break;
	case EVertexLayoutType::PositionBillBoard:
		stride = sizeof(FBillboardVertex);
		break;
	default:
		// Handle unknown or unsupported vertex types
		assert(false && "Unknown vertex type!");
		return; // or log an error
	}
	UINT offset = 0;

	ID3D11Buffer* VertexBuffer = InMesh->GetVertexBuffer();
	ID3D11Buffer* IndexBuffer = InMesh->GetIndexBuffer();
	uint32 VertexCount = InMesh->GetVertexCount();
	uint32 IndexCount = InMesh->GetIndexCount();

	RHIDevice->GetDeviceContext()->IASetVertexBuffers(
		0, 1, &VertexBuffer, &stride, &offset
	);

	RHIDevice->GetDeviceContext()->IASetIndexBuffer(
		IndexBuffer, DXGI_FORMAT_R32_UINT, 0
	);

	RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(InTopology);
	RHIDevice->PSSetDefaultSampler(0);

	if (InMesh->HasMaterial())
	{
		const TArray<FGroupInfo>& MeshGroupInfos = InMesh->GetMeshGroupInfo();
		const uint32 NumMeshGroupInfos = static_cast<uint32>(MeshGroupInfos.size());
		for (uint32 i = 0; i < NumMeshGroupInfos; ++i)
		{
			UMaterial* const Material = UResourceManager::GetInstance().Get<UMaterial>(InComponentMaterialSlots[i].MaterialName.ToString());
			const FObjMaterialInfo& MaterialInfo = Material->GetMaterialInfo();
			ID3D11ShaderResourceView* srv = nullptr;
			bool bHasTexture = false;
			if (!MaterialInfo.DiffuseTextureFileName.empty())
			{
				// UTF-8 -> UTF-16 변환 (Windows)
				int needW = ::MultiByteToWideChar(CP_UTF8, 0, MaterialInfo.DiffuseTextureFileName.c_str(), -1, nullptr, 0);
				std::wstring WTextureFileName;
				if (needW > 0)
				{
					WTextureFileName.resize(needW - 1);
					::MultiByteToWideChar(CP_UTF8, 0, MaterialInfo.DiffuseTextureFileName.c_str(), -1, WTextureFileName.data(), needW);
				}
				// 반환 여기서 로드 
				if (FTextureData* TextureData = UResourceManager::GetInstance().CreateOrGetTextureData(WTextureFileName))
				{
					if (TextureData->TextureSRV)
					{
						srv = TextureData->TextureSRV;
						bHasTexture = true;
					}
				}
			}
			RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &srv);
			RHIDevice->UpdatePixelConstantBuffers(MaterialInfo, true, bHasTexture); // 성공 여부 기반
			RHIDevice->GetDeviceContext()->DrawIndexed(MeshGroupInfos[i].IndexCount, MeshGroupInfos[i].StartIndex, 0);
		}
	}
	else
	{
		FObjMaterialInfo ObjMaterialInfo;
		RHIDevice->UpdatePixelConstantBuffers(ObjMaterialInfo, false, false); // PSSet도 해줌
		RHIDevice->GetDeviceContext()->DrawIndexed(IndexCount, 0, 0);
	}
}

// TEXT BillBoard 용 
void URenderer::DrawIndexedPrimitiveComponent(UTextRenderComponent* Comp, D3D11_PRIMITIVE_TOPOLOGY InTopology)
{
	UINT Stride = sizeof(FBillboardVertexInfo_GPU);
	ID3D11Buffer* VertexBuff = Comp->GetStaticMesh()->GetVertexBuffer();
	ID3D11Buffer* IndexBuff = Comp->GetStaticMesh()->GetIndexBuffer();

	RHIDevice->GetDeviceContext()->IASetInputLayout(Comp->GetMaterial()->GetShader()->GetInputLayout());

	UINT offset = 0;
	RHIDevice->GetDeviceContext()->IASetVertexBuffers(
		0, 1, &VertexBuff, &Stride, &offset
	);
	RHIDevice->GetDeviceContext()->IASetIndexBuffer(
		IndexBuff, DXGI_FORMAT_R32_UINT, 0
	);
	ID3D11ShaderResourceView* TextureSRV = Comp->GetMaterial()->GetTexture()->GetShaderResourceView();
	RHIDevice->PSSetDefaultSampler(0);
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &TextureSRV);
	RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(InTopology);
	const uint32 indexCnt = Comp->GetStaticMesh()->GetIndexCount();
	RHIDevice->GetDeviceContext()->DrawIndexed(indexCnt, 0, 0);
}

// 단일 Quad 용 -> Billboard에서 호출 
void URenderer::DrawIndexedPrimitiveComponent(UBillboardComponent* Comp, D3D11_PRIMITIVE_TOPOLOGY InTopology)
{
	UINT Stride = sizeof(FBillboardVertex);
	ID3D11Buffer* VertexBuff = Comp->GetStaticMesh()->GetVertexBuffer();
	ID3D11Buffer* IndexBuff = Comp->GetStaticMesh()->GetIndexBuffer();

	// Input layout comes from the shader bound to the material
	RHIDevice->GetDeviceContext()->IASetInputLayout(Comp->GetMaterial()->GetShader()->GetInputLayout());

	UINT offset = 0;
	RHIDevice->GetDeviceContext()->IASetVertexBuffers(0, 1, &VertexBuff, &Stride, &offset);
	RHIDevice->GetDeviceContext()->IASetIndexBuffer(IndexBuff, DXGI_FORMAT_R32_UINT, 0);

    // Bind texture via ResourceManager to support DDS/PNG
    ID3D11ShaderResourceView* srv = nullptr;
    if (Comp->GetMaterial())
    {
        const FString& TextName = Comp->GetTextureName();
        if (!TextName.empty())
        {
            int needW = ::MultiByteToWideChar(CP_UTF8, 0, TextName.c_str(), -1, nullptr, 0);
            std::wstring WTextureFileName;
            if (needW > 0)
            {
                WTextureFileName.resize(needW - 1);
                ::MultiByteToWideChar(CP_UTF8, 0, TextName.c_str(), -1, WTextureFileName.data(), needW);
            }
            if (FTextureData* TextureData = UResourceManager::GetInstance().CreateOrGetTextureData(WTextureFileName))
            {
                if (TextureData->TextureSRV)
                {
                    srv = TextureData->TextureSRV;
                }
            }
        }
    }
    RHIDevice->PSSetDefaultSampler(0);
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &srv);

    // Ensure correct alpha blending just for this draw
	RHIDevice->OMSetBlendState(true);

	RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(InTopology);
	const uint32 indexCnt = Comp->GetStaticMesh()->GetIndexCount();
	RHIDevice->GetDeviceContext()->DrawIndexed(indexCnt, 0, 0);

    // Restore blend state so others aren't affected
	RHIDevice->OMSetBlendState(false);
}

void URenderer::SetViewModeType(EViewModeIndex ViewModeIndex)
{
	if (PreViewModeIndex != ViewModeIndex)
	{
		//UE_LOG("Change ViewMode");
		RHIDevice->RSSetState(ViewModeIndex);
		if (ViewModeIndex == EViewModeIndex::VMI_Wireframe)
			RHIDevice->UpdateColorConstantBuffers(FVector4{ 1.f, 0.f, 0.f, 1.f });
		else
			RHIDevice->UpdateColorConstantBuffers(FVector4{ 1.f, 1.f, 1.f, 0.f });
		PreViewModeIndex = ViewModeIndex;
	}
}

void URenderer::InitializeLineBatch()
{
	// Create UDynamicMesh for efficient line batching
	DynamicLineMesh = UResourceManager::GetInstance().Load<ULineDynamicMesh>("Line");

	// Initialize with maximum capacity (MAX_LINES * 2 vertices, MAX_LINES * 2 indices)
	uint32 maxVertices = MAX_LINES * 2;
	uint32 maxIndices = MAX_LINES * 2;
	DynamicLineMesh->Load(maxVertices, maxIndices, RHIDevice->GetDevice());

	// Create FMeshData for accumulating line data
	LineBatchData = new FMeshData();

	// Load line shader
	LineShader = UResourceManager::GetInstance().Load<UShader>("ShaderLine.hlsl", EVertexLayoutType::PositionColor);
	//LineShader = UResourceManager::GetInstance().Load<UShader>("StaticMeshShader.hlsl", EVertexLayoutType::PositionColorTexturNormal);

}

void URenderer::BeginLineBatch()
{
	if (!LineBatchData) return;

	bLineBatchActive = true;

	// Clear previous batch data
	LineBatchData->Vertices.clear();
	LineBatchData->Color.clear();
	LineBatchData->Indices.clear();
}

void URenderer::AddLine(const FVector& Start, const FVector& End, const FVector4& Color)
{
	if (!bLineBatchActive || !LineBatchData) return;

	uint32 startIndex = static_cast<uint32>(LineBatchData->Vertices.size());

	// Add vertices
	LineBatchData->Vertices.push_back(Start);
	LineBatchData->Vertices.push_back(End);

	// Add colors
	LineBatchData->Color.push_back(Color);
	LineBatchData->Color.push_back(Color);

	// Add indices for line (2 vertices per line)
	LineBatchData->Indices.push_back(startIndex);
	LineBatchData->Indices.push_back(startIndex + 1);
}

void URenderer::AddLines(const TArray<FVector>& StartPoints, const TArray<FVector>& EndPoints, const TArray<FVector4>& Colors)
{
	if (!bLineBatchActive || !LineBatchData) return;

	// Validate input arrays have same size
	if (StartPoints.size() != EndPoints.size() || StartPoints.size() != Colors.size())
		return;

	uint32 startIndex = static_cast<uint32>(LineBatchData->Vertices.size());

	// Reserve space for efficiency
	size_t lineCount = StartPoints.size();
	LineBatchData->Vertices.reserve(LineBatchData->Vertices.size() + lineCount * 2);
	LineBatchData->Color.reserve(LineBatchData->Color.size() + lineCount * 2);
	LineBatchData->Indices.reserve(LineBatchData->Indices.size() + lineCount * 2);

	// Add all lines at once
	for (size_t i = 0; i < lineCount; ++i)
	{
		uint32 currentIndex = startIndex + static_cast<uint32>(i * 2);

		// Add vertices
		LineBatchData->Vertices.push_back(StartPoints[i]);
		LineBatchData->Vertices.push_back(EndPoints[i]);

		// Add colors
		LineBatchData->Color.push_back(Colors[i]);
		LineBatchData->Color.push_back(Colors[i]);

		// Add indices for line (2 vertices per line)
		LineBatchData->Indices.push_back(currentIndex);
		LineBatchData->Indices.push_back(currentIndex + 1);
	}
}

void URenderer::EndLineBatch(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix)
{
	if (!bLineBatchActive || !LineBatchData || !DynamicLineMesh || LineBatchData->Vertices.empty())
	{
		bLineBatchActive = false;
		return;
	}

	// Clamp to GPU buffer capacity to avoid full drop when overflowing
	const uint32 totalLines = static_cast<uint32>(LineBatchData->Indices.size() / 2);
	if (totalLines > MAX_LINES)
	{
		const uint32 clampedLines = MAX_LINES;
		const uint32 clampedVerts = clampedLines * 2;
		const uint32 clampedIndices = clampedLines * 2;
		LineBatchData->Vertices.resize(clampedVerts);
		LineBatchData->Color.resize(clampedVerts);
		LineBatchData->Indices.resize(clampedIndices);
	}

    // Efficiently update dynamic mesh data (no buffer recreation!)
    if (!DynamicLineMesh->UpdateData(LineBatchData, RHIDevice->GetDeviceContext()))
    {
        bLineBatchActive = false;
        return;
    }
    
    // Set up rendering state
	RHIDevice->UpdateConstantBuffers(ModelMatrix, ViewMatrix, ProjectionMatrix);
	RHIDevice->PrepareShader(LineShader);
    
    // Render using dynamic mesh
    if (DynamicLineMesh->GetCurrentVertexCount() > 0 && DynamicLineMesh->GetCurrentIndexCount() > 0)
    {
        UINT stride = sizeof(FVertexSimple);
        UINT offset = 0;
        
        ID3D11Buffer* vertexBuffer = DynamicLineMesh->GetVertexBuffer();
        ID3D11Buffer* indexBuffer = DynamicLineMesh->GetIndexBuffer();
        
        RHIDevice->GetDeviceContext()->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        RHIDevice->GetDeviceContext()->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        // Overlay 스텐실(=1) 영역은 그리지 않도록 스텐실 테스트 설정
		RHIDevice->OMSetDepthStencilState_StencilRejectOverlay();
        RHIDevice->GetDeviceContext()->DrawIndexed(DynamicLineMesh->GetCurrentIndexCount(), 0, 0);
        // 상태 복구
		RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqual);
    }
    
    bLineBatchActive = false;
}

void URenderer::ClearLineBatch()
{
	if (!LineBatchData) return;

	LineBatchData->Vertices.clear();
	LineBatchData->Color.clear();
	LineBatchData->Indices.clear();

	bLineBatchActive = false;
}
