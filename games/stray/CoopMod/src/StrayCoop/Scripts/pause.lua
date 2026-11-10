-- Player pause intent is independent of the engine pause we apply for a peer.
local M={}
local root="Mods/StrayCoop/"
local Context=dofile(root.."Scripts/context.lua")
local schedule_loop=Context.loop or LoopAsync
local game_thread=Context.run or ExecuteInGameThread
local function valid(o) return o~=nil and o:IsValid() end
local function log(s) print("[StrayCoop/Pause] "..s.."\n") end
local function write_request(state)
    local f=io.open(root.."pause-local.tsv","w")
    if f then f:write("PAUSE\t"..(state.requested and "1" or "0").."\n");f:close() end
end
local function tick(state)
    local cat,pc=Context.player()
    if not pc then write_request(state);return end
    local settings=Context.settings()
    if valid(settings) then
        local name=settings:GetFullName()
        if state.settings_name~=name then
            state.settings_name=name;state.old_auto_pause=settings.AutoPauseEnabled
        end
        settings.AutoPauseEnabled=false
    end
    if not cat or pc:IsWaitingForStreamingAndLoading() then
        -- Loading owns its native suspension/fade lifecycle. A menu from that
        -- lifecycle must never be dismissed as a focus-loss pause.
        write_request(state);return
    end
    local world=pc:GetWorld():GetFullName()
    if state.world and state.world~=world then
        state.requested=false;state.native_requested=false;state.explicit=false;state.owned=false;state.menu_until=nil
    end
    state.world=world
    -- Native C++ callers can bypass a reflected-function hook. Read the
    -- local HUD stack as well, without searching global level objects.
    local menu=false
    local menus={}
    local hud=pc:GetHUD()
    if valid(hud) and hud:IsA("/Script/Hk_project.HKHUD") then
        -- Use the reflected array: this UE4SS build can return stale UObject
        -- wrappers from a temporary TArray returned by GetWidgets().
        hud.m_widgets:ForEach(function(_,entry)
            local widget=entry:get()
            if valid(widget) and widget:IsA("/Script/Hk_project.PauseMenuWidget")
                and widget:IsInViewport() then menu=true;menus[#menus+1]=widget end
        end)
    end
    local now=Context.gameplay():GetRealTimeSeconds(pc)
    -- A focus-loss/native menu is not an Escape vote. Only the native pause
    -- input hook arms intent. Clear unsolicited menus in the co-op session.
    if menu and not state.explicit then
        for _,widget in ipairs(menus) do widget:RemoveFromParent() end
        local manager=pc.m_gameStateManager
        if not valid(manager) then manager=Context.singleton("GameStateManager") end
        if valid(manager) and manager:IsSuspended() then manager:UnsuspendGame() end
        state.native_requested=false;state.owned=true;menu=false
    end
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
    local roster=io.open(root.."remote.tsv","r")
    local header=roster and roster:read("*l") or ""
    if roster then roster:close() end
    local own=header:match("^STRAYCOOP\t%d+\t([12])\t")
    local partner=own=="1" and client=="1" or own=="2" and host=="1"
    if partner and state.remote and now>=(state.notice_at or 0) then
        state.notice_at=now+1
        local ui=StaticFindObject("/Script/Engine.Default__KismetSystemLibrary")
        if valid(ui) then ui:PrintString(pc,"Partner is paused. Play resumes when both players unpause.",true,false,
            {R=1,G=.65,B=.2,A=1},1) end
    end
    local utility=Context.gameplay()
    local paused=utility:IsGamePaused(pc)
    state.observed_menu=menu;state.observed_pause=paused;state.desired=desired
    local cinematic=StrayCoopCinematics
    if paused and not desired and not state.explicit and cat and cat:IsInNormalLocomotion()
        and not pc:IsIngameCinematicModeEnabled() and not (cinematic and cinematic.waiting) then
        -- Native focus-loss pause can bypass SuspendGame's reflected hook.
        -- Reclaim it only during ordinary controllable gameplay.
        state.owned=true
    end
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
        RegisterHook("/Script/Hk_project.GameStateManager:_OnMenuStartPressed",function()
            local s=StrayCoopPause
            if s and s.running then s.explicit=true end
        end)
        -- Native hooks are process-lifetime; never retain a level UObject.
        RegisterHook("/Script/Hk_project.GameStateManager:SuspendGame",function(context,reason)
            local s=StrayCoopPause
            if s and s.running and valid(context:get()) and reason:get()==2 then
                -- This pre-hook also owns the engine pause the native menu is
                -- about to install, so its final release cannot strand it.
                s.owned=true
                s.native_requested=s.explicit or false;s.requested=s.native_requested
                s.world=context:get():GetWorld():GetFullName()
                write_request(s)
            end
        end)
        RegisterHook("/Script/Hk_project.GameStateManager:UnsuspendGame",function()
            local s=StrayCoopPause
            if s and s.running then s.explicit=false;s.native_requested=false;s.requested=false;write_request(s) end
        end,function()
            local s=StrayCoopPause
            if s and s.running then
                -- Do not carry a hook Lua-state UObject into the timer state.
                -- The next regular tick observes the resulting native menu.
            end
        end)
        StrayCoopPauseHooks=true
    end
    schedule_loop(50,function()
        if not state.running then return true end
        if state.queued then return false end
        state.queued=true
        game_thread(function()
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
    local settings=Context.settings()
    if valid(settings) and s.old_auto_pause~=nil then settings.AutoPauseEnabled=s.old_auto_pause end
    local _,pc=Context.player()
    if pc and s.owned and not s.requested then Context.gameplay():SetGamePaused(pc,false) end
    s.requested=false;write_request(s)
end
return M
