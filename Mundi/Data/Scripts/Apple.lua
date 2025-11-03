local function NormalizeCopy(V)
    local Out = Vector(V.X, V.Y, V.Z)
    Out:Normalize()
    return Out
end

function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    Obj.Velocity = Vector(1, 0, 0)
    Obj.bIsActive = true

    
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnBeginOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function OnEndOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function Tick(dt)

    local rc = GetComponent(Obj, "URotatingMovementComponent")
    if rc and Obj.Velocity then
        -- 사과는 로컬 y축 기준으로 회전,
        -- 플레이어가 바라보는 방향(즉, 투사체 진행방향)을 기준으로 회전축을 설정해줌
        local dir = NormalizeCopy(Obj.Velocity)
        local UpVector = Vector(0, 0, 1)
        local Axis = FVector.Cross(UpVector, dir)
        Axis:Normalize()

        rc.RotationRate = Axis * 500.0
    end

    Obj.Location = Obj.Location + Obj.Velocity * dt
    if not Obj.bIsActive then
        return 
    end
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end