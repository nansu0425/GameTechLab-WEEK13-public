function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    co = coroutine.create(function ()
           for i=1,10 do
             print("co" .. i)
             print(i)
             coroutine.yield()
           end
         end)
    coroutine.resume(co)
    coroutine.resume(co)
    coroutine.resume(co)
    coroutine.resume(co)
    coroutine.resume(co)
    --[[Obj:PrintLocation()]]--
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
    --[[Obj:PrintLocation()]]--
end

function OnOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end