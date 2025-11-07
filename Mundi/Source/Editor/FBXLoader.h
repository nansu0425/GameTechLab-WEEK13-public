#pragma once
#include "Object.h"
#include "fbxsdk.h"

class UFbxLoader : public UObject
{
public:

	DECLARE_CLASS(UFbxLoader, UObject)
	static UFbxLoader& GetInstance();
	UFbxLoader();

	FSkeletalMeshData LoadFbxMesh(const FString& FilePath);

	void LoadMeshFromNode(FbxNode* InNode, FSkeletalMeshData& MeshData);

	void LoadMeshFromAttribute(FbxNodeAttribute* InAttribute, FSkeletalMeshData& MeshData);

	void LoadMesh(FbxMesh* InMesh, FSkeletalMeshData& MeshData);

	FbxString GetAttributeTypeName(FbxNodeAttribute* InAttribute);

protected:
	~UFbxLoader() override;
private:
	
	

	UFbxLoader(const UFbxLoader&) = delete;
	UFbxLoader& operator=(const UFbxLoader&) = delete;

	FbxManager* SdkManager = nullptr;

	
};