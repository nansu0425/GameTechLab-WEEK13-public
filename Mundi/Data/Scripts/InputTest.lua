function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    Obj.Velocity = Vector(0, 0, 0)
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
    if InputManager:IsKeyDown(65) then
        print("a")
    elseif InputManager:IsKeyDown(66) then
        print("b")
    end
end