-- VehicleController.lua
-- ULuaScriptComponent를 통해 USimpleWheeledVehicleMovementComponent에 입력을 전달

local VehicleMovement = nil

local function GetVehicleComponent()
    if VehicleMovement then
        return VehicleMovement
    end

    local comp = GetComponent(Obj, "UVehicleMovementComponent")
    if not comp then
        print("[VehicleController] Vehicle movement component not found")
        return nil
    end

    VehicleMovement = comp
    return VehicleMovement
end

function BeginPlay()
    GetVehicleComponent()
end

function Tick(dt)
    local Vehicle = GetVehicleComponent()
    if not Vehicle then
        return
    end

    -- 입력 읽기
    -- 테스트용: 풀스로틀, 브레이크/핸드브레이크 해제 고정
    local throttle = 1.0
    local steering = 0.0
    local brake = 0.0
    local handbrake = 0.0

    Vehicle:SetThrottleInput(throttle)
    Vehicle:SetSteeringInput(steering)
    Vehicle:SetBrakeInput(brake)
    Vehicle:SetHandbrakeInput(handbrake)
end
