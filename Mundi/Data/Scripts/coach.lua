local maxTime = 28.0      -- 전체 색상 변화 시간 (초)
local whiteHoldTime = 8.0 -- 하얀색 유지 시간
local elapsedTime = 0.0
local meshComp = nil
local currentStage = 1 -- (1: White, 2: Yellow, 3: Orange, 4: Red)

-- 시간 비율에 따라 색상 보간 (하양 → 노랑 → 주황 → 빨강)
local function LerpColorByTime(ratio)
    local color = Color(1.0, 1.0, 1.0) -- 기본: 하얀색

    if ratio < 0.33 then
        -- 0~0.33: White → Yellow
        local t = ratio / 0.33
        color = Color(1.0, 1.0, 1.0 - t)
    elseif ratio < 0.66 then
        -- 0.33~0.66: Yellow → Orange
        local t = (ratio - 0.33) / 0.33
        color = Color(1.0, 1.0 - 0.5 * t, 0.0)
    else
        -- 0.66~1.0: Orange → Red
        local t = (ratio - 0.66) / 0.34
        color = Color(1.0, 0.5 - 0.5 * t, 0.0)
    end

    return color
end

-- 색상 단계별로 데칼 소환
local function SpawnStageDecal(stage)
    local prefabPath = ""

    if stage == 2 then
        prefabPath = "Data/Prefabs/CrackDecal1.prefab"
    elseif stage == 3 then
        prefabPath = "Data/Prefabs/CrackDecal2.prefab"
    elseif stage == 4 then
        prefabPath = "Data/Prefabs/CrackDecal3.prefab"
    else
        return
    end

    local decal = SpawnPrefab(prefabPath)
    if decal then
        local decalComp = GetComponent(decal, "UDecalComponent")
        if decalComp then
            decalComp.FadeSpeed = 0
        end

        print(string.format("[Stage %d] Spawned %s at (%.1f, %.1f, %.1f)",
            stage, prefabPath, decal.Location.X, decal.Location.Y, decal.Location.Z))
    end
end

function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    Obj.Tag = "Damageable"

    meshComp = GetComponent(Obj, "UStaticMeshComponent")
    if meshComp then
        meshComp:SetColor(0, "DiffuseColor", Color(1.0, 1.0, 1.0)) -- 초기 하얀색
    end

    currentStage = 1
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function Tick(dt)
    if not meshComp then
        return
    end

    elapsedTime = math.min(elapsedTime + dt, maxTime)

    if elapsedTime <= whiteHoldTime then
        meshComp:SetColor(0, "DiffuseColor", Color(1.0, 1.0, 1.0))
        return
    end

    local activeTime = elapsedTime - whiteHoldTime
    local colorChangeDuration = maxTime - whiteHoldTime
    local ratio = math.min(activeTime / colorChangeDuration, 1.0)
    local newColor = LerpColorByTime(ratio)
    meshComp:SetColor(0, "DiffuseColor", newColor)

    -- 색상 구간 진입 시 데칼 스폰
    if ratio >= 0.0 and ratio < 0.33 and currentStage < 2 then
        print("[Stage 2] Spawn CrackDecal1")
        SpawnStageDecal(2)
        currentStage = 2
    elseif ratio >= 0.33 and ratio < 0.66 and currentStage < 3 then
        print("[Stage 3] Spawn CrackDecal2")
        SpawnStageDecal(3)
        currentStage = 3
    elseif ratio >= 0.66 and currentStage < 4 then
        print("[Stage 4] Spawn CrackDecal3")
        SpawnStageDecal(4)
        currentStage = 4
    end
end
