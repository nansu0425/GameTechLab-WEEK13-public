function BeginPlay()
    print("[BeginPlay] ?? " .. Obj.UUID)
    -- AddComponent(Obj, "USpotLightComponent")
    
    -- A = GetComponent(Obj, "USpotLightComponent")
    -- A.Intensity = 2
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
    -- if (OtherActor == fireball)
    --     delete (Obj)
    print("onoverlap")
    Obj.Location.Z = Obj.Location.Z-5
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
    print("[Tick] ??")
end