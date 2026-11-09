-- Player pause intent is independent of the engine pause we apply for a peer.
local M={}
local root="Mods/StrayCoop/"
local Context=dofile(root.."Scripts/context.lua")
local function valid(o) return o~=nil and o:IsValid() end
local function log(s) print("[StrayCoop/Pause] "..s.."\n") end
local function write_request(state)
    local f=io.open(root.."pause-local.tsv","w")
    if f then f:write("PAUSE\t"..(state.requested and "1" or "0").."\n");f:close() end
end
local function tick(state)
    local _,pc=Context.player()
    if not pc then write_request(state);return end
    local world=pc:GetWorld():GetFullName()
    if state.world and state.world~=world then
        state.requested=false;state.native_requested=false;state.owned=false;state.menu_until=nil
    end
    state.world=world
    -- Native C++ callers can bypass a reflected-function hook. Read the
    -- local HUD stack as well, without searching global level objects.
    local menu=false
    local hud=pc:GetHUD()
    if valid(hud) and hud:IsA("/Script/Hk_project.HKHUD") then
        -- Use the reflected array: this UE4SS build can return stale UObject
        -- wrappers from a temporary TArray returned by GetWidgets().
        hud.m_widgets:ForEach(function(_,entry)
            local widget=entry:get()
            if valid(widget) and widget:IsA("/Script/Hk_project.PauseMenuWidget")
                and widget:IsInViewport() then menu=true end
        end)
    end
    local now=Context.gameplay():GetRealTimeSeconds(pc)
    if menu then state.menu_until=now+0.25 end
    local menu_active=menu or now<(state.menu_until or 0)
    -- Once the menu exists, its lifecycle owns local intent. The native
    -- suspension flag can change independently while the pause UI is open.
    if menu then state.native_requested=false end
    if menu_active then state.owned=true end
    state.requested=menu_active or state.native_requested or false
    write_request(state)
    local f=io.open(root.."pause-remote.tsv","r")
    local data=f and f:read(129) or ""
    if f then f:close() end
    local generation,connected,host,client=data:match("^PAUSE\t(%d+)\t([01])\t([01])\t([01])\n$")
    if generation and generation~=state.generation then
        state.generation=generation;state.seen=os.time()
    end
    -- The host echoes both intents. A client waits for its release to be
    -- acknowledged, so a stale roster cannot undo a fresh local pause.
    state.remote=connected=="1" and os.time()-(state.seen or 0)<5 and (host=="1" or client=="1")
    local desired=state.requested or state.remote
    local utility=Context.gameplay()
    local paused=utility:IsGamePaused(pc)
    if desired and not paused then
        if utility:SetGamePaused(pc,true) then state.owned=true end
    elseif not desired and state.owned then
        if not paused or utility:SetGamePaused(pc,false) then state.owned=false end
    end
    if desired~=state.effective then
        state.effective=desired
        log((desired and "Shared simulation paused" or "Shared simulation resumed")
            .." menu="..tostring(menu).." native="..tostring(state.native_requested).." remote="..tostring(state.remote))
    end
end

function M.start()
    if StrayCoopPause and StrayCoopPause.running then return end
    local state={running=true,requested=false,owned=false,queued=false}
    StrayCoopPause=state
    if not StrayCoopPauseHooks then
        -- Native hooks are process-lifetime; never retain a level UObject.
        RegisterHook("/Script/Hk_project.GameStateManager:SuspendGame",function(context,reason)
            local s=StrayCoopPause
            if s and s.running and valid(context:get()) and reason:get()==2 then
                -- This pre-hook also owns the engine pause the native menu is
                -- about to install, so its final release cannot strand it.
                if not Context.gameplay():IsGamePaused(context:get()) then s.owned=true end
                s.native_requested=true;s.requested=true
                s.world=context:get():GetWorld():GetFullName()
                write_request(s)
            end
        end)
        RegisterHook("/Script/Hk_project.GameStateManager:UnsuspendGame",function()
            local s=StrayCoopPause
            if s and s.running then s.native_requested=false;s.requested=false;write_request(s) end
        end,function()
            local s=StrayCoopPause
            if s and s.running then
                local ok,err=pcall(tick,s)
                if not ok then log("Resume check: "..tostring(err)) end
            end
        end)
        StrayCoopPauseHooks=true
    end
    LoopAsync(50,function()
        if not state.running then return true end
        if state.queued then return false end
        state.queued=true
        ExecuteInGameThread(function()
            if state.running then
                local ok,err=pcall(tick,state)
                if not ok then
                    state.errors=(state.errors or 0)+1
                    if state.errors<=3 then log("ERROR: "..tostring(err)) end
                end
            end
            state.queued=false
        end)
        return false
    end)
    log("Shared pause ready")
end

function M.stop()
    local s=StrayCoopPause
    if not s then return end
    s.running=false
    local _,pc=Context.player()
    if pc and s.owned and not s.requested then Context.gameplay():SetGamePaused(pc,false) end
    s.requested=false;write_request(s)
end
return M
