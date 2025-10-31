function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    Obj:PrintLocation()
    
    co = coroutine.create(function ()
            for i=1,10 do
                print("co", i)
                coroutine.yield()
            end
        end)
    --[[CoroutineTest()]]--
end


function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
    Obj:PrintLocation()
end

function OnOverlap(OtherActor)
    OtherActor:PrintLocation();
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
    --[[Obj:PrintLocation()]]--
end