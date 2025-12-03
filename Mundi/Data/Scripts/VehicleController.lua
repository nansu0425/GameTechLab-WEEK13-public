-- VehicleController.lua
-- ULuaScriptComponent를 통해 USimpleWheeledVehicleMovementComponent에 입력을 전달

local VehicleMovement = nil

local function GetVehicleComponent()
    if VehicleMovement then
        return VehicleMovement
    end

    local comp = GetComponent(Obj, "USimpleWheeledVehicleMovementComponent")
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
    local w = InputManager:IsKeyDown("W")
    local s = InputManager:IsKeyDown("S")
    local a = InputManager:IsKeyDown("A")
    local d = InputManager:IsKeyDown("D")
    local shift = InputManager:IsKeyDown(0x10)  -- VK_SHIFT
    local space = InputManager:IsKeyDown(0x20)  -- VK_SPACE

    -- 스로틀/스티어 값 계산 (-1.0 ~ 1.0)
    local throttle = 0.0
    if w then throttle = throttle + 1.0 end
    if s then throttle = throttle - 1.0 end

    local steering = 0.0
    if d then steering = steering + 1.0 end
    if a then steering = steering - 1.0 end

    -- 브레이크 입력 (0.0 ~ 1.0)
    local brake = space and 1.0 or 0.0
    local handbrake = shift and 1.0 or 0.0

    -- 컴포넌트에 전달
    Vehicle:SetThrottleInput(throttle)
    Vehicle:SetSteeringInput(steering)
    Vehicle:SetBrakeInput(brake)
    Vehicle:SetHandbrakeInput(handbrake)
end
