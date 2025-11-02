function BeginPlay()
    print("[BeginPlay] ?? " .. Obj.UUID)
    -- AddComponent(Obj, "USpotLightComponent")
    
    -- A = GetComponent(Obj, "USpotLightComponent")
    -- A.Intensity = 2
    print(Obj.Tag)
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
    if not OtherActor then
        print("no other actor")
        return
    end
    
    if not OtherActor.Tag then
        print("no tag actor")
        return
    end

    if OtherActor.Tag == "fireball" then
        print("onoverlap")
    end
    -- Obj.Location.Z = Obj.Location.Z-5
    -- if OtherActor.Name == "FireBallActor" then
    --     local hitPos = OtherActor.Location
    --     local tileSize = Obj.TileSize
    --     local x = math.floor(hitPos.X / tileSize.X)
    --     local y = math.floor(hitPos.Y / tileSize.Y)

    --     local index = y * 10 + x + 1  -- 1-based Lua Index
    --     local tile = Obj.Tiles[index]

    --     if tile ~= nil then
    --         Destroy(tile)
    --         Obj.Tiles[index] = nil
    --         print("Tile destroyed at (" .. x .. "," .. y .. ")")
    --     end
    -- end
end

function Tick(dt)
    -- Obj.Location = Obj.Location + Obj.Velocity * dt
end