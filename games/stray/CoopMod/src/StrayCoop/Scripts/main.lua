-- Stray network research prototype. No save or asset modifications.
local Context=dofile("Mods/StrayCoop/Scripts/context.lua")
LoopAsync(1000,function()
    ExecuteInGameThread(function()
        local settings=Context.settings()
        if settings then settings.AutoPauseEnabled=false end
    end)
    return false
end)
local function log(text)
    print("[StrayCoop] " .. tostring(text) .. "\n")
end

local function valid(object)
    return object ~= nil and object:IsValid()
end

local function label(object)
    if valid(object) then return object:GetFullName() end
    return "<none>"
end

local function inspect()
    log("=== Runtime inventory ===")
    for _, class in ipairs({"World", "GameInstance", "GameModeBase", "PlayerController", "Pawn", "GameStateBase", "NetDriver"}) do
        for _, object in ipairs(FindAllOf(class) or {}) do
            log(class .. ": " .. label(object))
            if class == "PlayerController" then
                log("  pawn: " .. label(object.Pawn) .. "; player: " .. label(object.Player))
            elseif class == "Pawn" then
                log("  replicates=" .. tostring(object.bReplicates) .. "; movement=" .. tostring(object.bReplicateMovement))
            elseif class == "GameModeBase" then
                log("  pawn class=" .. label(object.DefaultPawnClass) .. "; controller class=" .. label(object.PlayerControllerClass))
            end
        end
    end
    for _, path in ipairs({"/Script/OnlineSubsystemUtils.IpNetDriver", "/Script/Engine.GameplayStatics.OpenLevel", "/Script/Engine.PlayerController.ClientTravel"}) do
        log(path .. ": " .. label(StaticFindObject(path)))
    end
    log("=== End inventory ===")
end

local function guarded(callback)
    return function()
        ExecuteInGameThread(function()
            local ok, err = pcall(callback)
            if not ok then log("ERROR: " .. tostring(err)) end
        end)
    end
end

RegisterKeyBind(Key.F6, guarded(inspect))
RegisterKeyBind(Key.F7, function() DumpAllObjects() end)
RegisterKeyBind(Key.F10, function() GenerateSDK() end)

-- A local command file lets the development tools request actions without UI typing.
-- Commands are deliberately a small whitelist, never evaluated as Lua or shell code.
local pending = false
LoopAsync(500, function()
    if pending then return false end
    local file = io.open("Mods/StrayCoop/command.txt", "r")
    if not file then return false end
    local command = file:read("*a"):match("^%s*(.-)%s*$")
    file:close()
    if command == "" then return false end
    file = io.open("Mods/StrayCoop/command.txt", "w")
    if file then file:close() end
    pending = true
    ExecuteInGameThread(function()
        local ok, err = pcall(function()
            if command == "inspect" then inspect()
            elseif command == "dump" then DumpAllObjects()
            elseif command == "sdk" then GenerateSDK()
            else
                local runtime = dofile("Mods/StrayCoop/Scripts/runtime.lua")
                runtime.dispatch(command)
            end
        end)
        if not ok then log("ERROR: " .. tostring(err)) end
        pending = false
    end)
    return false
end)

log("Loaded network research prototype. F6: inventory, F7: objects, F10: SDK.")
