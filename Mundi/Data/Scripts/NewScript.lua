function BeginPlay()
    print("[BeginPlay]")
    StartCoroutine(EditAfterOneSec)
end

function EditAfterOneSec()
    coroutine.yield("wait_time", 2.0)
    GlobalConfig.Gravity2 = 5
end

function EndPlay()
    print("[EndPlay]")
end

function OnOverlap(OtherActor)
    OtherActor:PrintLocation();
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
end