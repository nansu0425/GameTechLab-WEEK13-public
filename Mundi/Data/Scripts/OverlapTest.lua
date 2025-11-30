-- OverlapTest.lua
-- Trigger Overlap event test script
-- Usage: Set BoxComponent Actor as trigger (bIsTrigger = true)
--        Add LuaScriptComponent and set ScriptFilePath to "Data/Scripts/OverlapTest.lua"

local overlapCount = 0
local activeOverlaps = {}  -- Currently overlapping actors

function BeginPlay()
    Obj.Tag = "TriggerZone"

    print("========================================")
    print("[OverlapTest] BeginPlay - Waiting for trigger...")
    print("[OverlapTest] Events will be called when")
    print("[OverlapTest] other objects enter/exit this trigger.")
    print("========================================")
end

function EndPlay()
    print("[OverlapTest] EndPlay - Total overlap count: " .. overlapCount)
end

function OnBeginOverlap(OtherActor)
    overlapCount = overlapCount + 1

    print("========================================")
    print("[OverlapTest] OnBeginOverlap called! (#" .. overlapCount .. ")")

    if OtherActor then
        local tag = tostring(OtherActor.Tag)
        print("[OverlapTest]   Entered Actor Tag: " .. tag)

        -- Add overlapping actor
        activeOverlaps[tag] = true

        -- Print entered actor location
        local otherLoc = OtherActor.Location
        if otherLoc then
            print(string.format("[OverlapTest]   Enter Location: (%.2f, %.2f, %.2f)",
                otherLoc.X, otherLoc.Y, otherLoc.Z))
        end
    else
        print("[OverlapTest]   Entered Actor: nil")
    end

    -- Print current overlapping actor count
    local count = 0
    for _ in pairs(activeOverlaps) do count = count + 1 end
    print("[OverlapTest]   Currently overlapping: " .. count)

    print("========================================")
end

function OnEndOverlap(OtherActor)
    print("========================================")
    print("[OverlapTest] OnEndOverlap called!")

    if OtherActor then
        local tag = tostring(OtherActor.Tag)
        print("[OverlapTest]   Exited Actor Tag: " .. tag)

        -- Remove overlapping actor
        activeOverlaps[tag] = nil

        -- Print exited actor location
        local otherLoc = OtherActor.Location
        if otherLoc then
            print(string.format("[OverlapTest]   Exit Location: (%.2f, %.2f, %.2f)",
                otherLoc.X, otherLoc.Y, otherLoc.Z))
        end
    else
        print("[OverlapTest]   Exited Actor: nil")
    end

    -- Print remaining overlapping actor count
    local count = 0
    for _ in pairs(activeOverlaps) do count = count + 1 end
    print("[OverlapTest]   Remaining overlaps: " .. count)

    print("========================================")
end

function Tick(dt)
    -- Add tick logic if needed
end
