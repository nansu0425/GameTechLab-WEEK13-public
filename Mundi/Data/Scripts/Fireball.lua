function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)

    Obj.Velocity = Vector(0,0, -10) 
    Obj.bIsActive = true
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
 
    print(Obj.Location)
    print(Obj.Velocity)

    if not Obj.bIsActive then
        return 
    end

    Obj.Location = Obj.Location + Obj.Velocity * dt
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end