-- HitTest.lua
-- 물리 충돌(Hit) 이벤트 테스트용 스크립트
-- 사용법: PhysBoxActor 또는 BoxComponent가 있는 Actor에 LuaScriptComponent를 추가하고
--         ScriptFilePath를 "Data/Scripts/HitTest.lua"로 설정

local hitCount = 0

function BeginPlay()
    Obj.Tag = "HitTestObject"

    print("========================================")
    print("[HitTest] BeginPlay - 충돌 대기 중...")
    print("[HitTest] 이 오브젝트가 다른 물리 오브젝트와")
    print("[HitTest] 충돌하면 OnHit이 호출됩니다.")
    print("========================================")
end

function EndPlay()
    print("[HitTest] EndPlay - 총 충돌 횟수: " .. hitCount)
end

function OnHit(OtherActor)
    hitCount = hitCount + 1

    print("========================================")
    print("[HitTest] OnHit 호출됨! (#" .. hitCount .. ")")

    if OtherActor then
        print("[HitTest]   충돌 상대 Tag: " .. tostring(OtherActor.Tag))

        -- 충돌 상대 위치 출력
        local otherLoc = OtherActor.Location
        if otherLoc then
            print(string.format("[HitTest]   상대 위치: (%.2f, %.2f, %.2f)",
                otherLoc.X, otherLoc.Y, otherLoc.Z))
        end
    else
        print("[HitTest]   충돌 상대: nil")
    end

    -- 자신의 위치 출력
    local myLoc = Obj.Location
    if myLoc then
        print(string.format("[HitTest]   내 위치: (%.2f, %.2f, %.2f)",
            myLoc.X, myLoc.Y, myLoc.Z))
    end

    print("========================================")
end

function Tick(dt)
    -- 필요시 틱 로직 추가
end
