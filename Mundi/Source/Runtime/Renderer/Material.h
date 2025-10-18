#pragma once
#include "ResourceBase.h"
#include "Enums.h"

class UShader;
class UTexture;
class UMaterial : public UResourceBase
{
	DECLARE_CLASS(UMaterial, UResourceBase)
public:
    UMaterial();
    void Load(const FString& InFilePath, ID3D11Device* InDevice);

protected:
    ~UMaterial() override = default;

public:
    void SetShader(UShader* ShaderResource);
    UShader* GetShader();

    void SetTexture(UTexture* TextureResource);
    void SetTexture(const FString& TexturePath);
    UTexture* GetTexture();

    void SetMaterialInfo(const FMaterialParameters& InMaterialInfo) { MaterialInfo = InMaterialInfo; }
    const FMaterialParameters& GetMaterialInfo() const { return MaterialInfo; }

    // TEST
    FString& GetTextName() { return TextureName; }
    void SetTextName(FString& InName) { TextureName = InName; };

private:
    // 이 머티리얼이 사용할 셰이더 프로그램 (예: UberLit.hlsl)
	UShader* Shader = nullptr;
    FMaterialParameters MaterialInfo;

    // NOTE: 삭제 필요, FMaterialParameters 만을 사용하도록 변경
	UTexture* Texture= nullptr;
    FString TextureName;
};

