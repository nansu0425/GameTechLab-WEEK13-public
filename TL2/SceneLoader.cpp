#include "pch.h"
#include "SceneLoader.h"

#include <algorithm>
#include <iomanip>

static bool ParsePerspectiveCamera(const JSON& Root, FPerspectiveCameraData& OutCam)
{
    if (!Root.hasKey("PerspectiveCamera"))
        return false;

    const JSON& Cam = Root.at("PerspectiveCamera");

    // 배열형 벡터(Location, Rotation) 파싱 (스칼라 실패 시 무시)
    auto readVec3 = [](JSON arr, FVector& outVec)
        {
            try
            {
                outVec = FVector(
                    (float)arr[0].ToFloat(),
                    (float)arr[1].ToFloat(),
                    (float)arr[2].ToFloat());
            }
            catch (...) {} // 실패 시 기본값 유지
        };

    if (Cam.hasKey("Location"))
        readVec3(Cam.at("Location"), OutCam.Location);
    if (Cam.hasKey("Rotation"))
        readVec3(Cam.at("Rotation"), OutCam.Rotation);

    // 스칼라 또는 [스칼라] 모두 허용
    auto readScalarFlexible = [](const JSON& parent, const char* key, float& outVal)
        {
            if (!parent.hasKey(key)) return;
            const JSON& node = parent.at(key);
            try
            {
                // 배열 형태 시도 (예: "FOV": [60.0])
                outVal = (float)node.at(0).ToFloat();
            }
            catch (...)
            {
                // 스칼라 (예: "FOV": 60.0)
                outVal = (float)node.ToFloat();
            }
        };

    readScalarFlexible(Cam, "FOV", OutCam.FOV);
    readScalarFlexible(Cam, "NearClip", OutCam.NearClip);
    readScalarFlexible(Cam, "FarClip", OutCam.FarClip);

    return true;
}

// ─────────────────────────────────────────────
// NextUUID 메타만 읽어오는 간단한 헬퍼
// 저장 포맷상 "NextUUID"는 "마지막으로 사용된 UUID"이므로,
// 호출 측에서 +1 해서 SetNextUUID 해야 함
// ─────────────────────────────────────────────
bool FSceneLoader::TryReadNextUUID(const FString& FilePath, uint32& OutNextUUID)
{
    std::ifstream file(FilePath);
    if (!file.is_open())
    {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    try
    {
        JSON j = JSON::Load(content);
        if (j.hasKey("NextUUID"))
        {
            // 정수 파서가 없으면 ToFloat로 받아서 캐스팅
			OutNextUUID = static_cast<uint32>(j.at("NextUUID").ToInt());
            return true;
        }
    }
    catch (...)
    {
        // 무시하고 false 반환
    }
    return false;
}
