-- Local development interface only. Never consumes network commands.
local M={}
local root="Mods/StrayCoop/"
local Context=dofile(root.."Scripts/context.lua")
local schedule_loop=Context.loop or LoopAsync
local game_thread=Context.run or ExecuteInGameThread
local function valid(o) return o~=nil and o:IsValid() end
local function log(s) print("[StrayCoop/Harness] "..s.."\n") end
local function pc()
    local _,current=Context.player()
    if valid(current) and current:IsLocalController() then return current end
    for _,o in ipairs(FindAllOf("PlayerController") or {}) do
        if valid(o) and o:IsLocalController() then return o end
    end
    error("Local controller unavailable")
end
local function guard()
    local f=assert(io.open(root.."harness-session.txt","r"),"Harness session is not enabled")
    local token=f:read("*a"):match("^([a-f0-9]+)\n?$");f:close()
    assert(token and #token==32,"Invalid harness session")
    -- Avoid FString return values in this UE4SS build; native boolean parsing
    -- verifies the per-process nonce without reading a temporary FString buffer.
    assert(StaticFindObject("/Script/Engine.Default__GameplayStatics"):HasLaunchOption("StrayCoopHarness="..token),
        "This process was not launched by the isolated harness")
end
local function clean(s) return tostring(s):gsub("[\t\r\n]"," ") end
local function resume()
    local p=pc()
    local settings=Context.settings()
    if valid(settings) then settings.AutoPauseEnabled=false end
    -- Unsuspending the engine alone can leave the pause menu in the viewport.
    -- Shared pause correctly treats that menu as an active player vote. Only
    -- this nonce-guarded development resume may dismiss it automatically.
    local hud=p:GetHUD()
    local menus={}
    if valid(hud) and hud:IsA("/Script/Hk_project.HKHUD") then
        hud.m_widgets:ForEach(function(_,entry)
            local widget=entry:get()
            if valid(widget) and widget:IsA("/Script/Hk_project.PauseMenuWidget")
                and widget:IsInViewport() then menus[#menus+1]=widget end
        end)
    end
    for _,widget in ipairs(menus) do widget:RemoveFromParent() end
    local manager=p.m_gameStateManager
    if valid(manager) and manager:IsSuspended() then manager:UnsuspendGame() end
    StaticFindObject("/Script/Engine.Default__GameplayStatics"):SetGamePaused(p,false)
end
local function snapshot()
    local p=pc()
    local cat=p.Pawn
    local utilities=StaticFindObject("/Script/Hk_project.Default__HKUtilities")
    local instance=FindFirstOf("HKGameInstance")
    local zone=FindFirstOf("ZoneManager")
    local save=valid(instance) and instance:GetSaveInstance() or nil
    local rows={"HARNESS\t1\t"..os.time(),"WORLD\t"..clean(p:GetWorld():GetFullName()),
        "CHAPTER\t"..tostring(utilities:GetChapter(p)),
        "ZONE\t"..(valid(zone) and clean(zone:GetCurrentZoneName():ToString()) or "-"),
        "SAVE\t"..tostring(valid(save) and valid(save.m_saveData)),
        "PAUSED\t"..tostring(StaticFindObject("/Script/Engine.Default__GameplayStatics"):IsGamePaused(p)),
        "WAITING\t"..tostring(p:IsA("/Script/Hk_project.HKPlayerController") and p:IsWaitingForStreamingAndLoading()),
        "CINEMATIC\t"..tostring(p:IsA("/Script/Hk_project.HKPlayerController") and p:IsIngameCinematicModeEnabled())}
    if StrayCoopPause then
        rows[#rows+1]="PAUSE_REQUEST\t"..tostring(StrayCoopPause.requested)
        rows[#rows+1]="PAUSE_REMOTE\t"..tostring(StrayCoopPause.remote)
    end
    local hud=p:GetHUD()
    if valid(hud) and hud:IsA("/Script/Hk_project.HKHUD") then
        hud.m_widgets:ForEach(function(_,entry)
            local widget=entry:get()
            if valid(widget) and widget:IsA("/Script/Hk_project.PauseMenuWidget") then
                rows[#rows+1]="PAUSE_WIDGET\t"..clean(widget:GetFullName()).."\t"
                    ..tostring(widget:IsInViewport()).."\t"..tostring(widget:IsVisible())
                    .."\t"..tostring(widget:GetVisibility())
            end
        end)
    end
    if valid(cat) and cat:IsA("/Script/Hk_project.CatPawn") then
        local v=cat:K2_GetActorLocation()
        rows[#rows+1]=string.format("PLAYER\t%s\t%.4f\t%.4f\t%.4f\t%s\t%s",clean(cat:GetFullName()),v.X,v.Y,v.Z,
            tostring(cat:IsInNormalLocomotion()),tostring(cat:IsDead()))
        rows[#rows+1]="GEAR\t"..tostring(valid(p:GetDrone())).."\t"..tostring(valid(p:GetBackpack()))
    end
    for _,s in ipairs(FindAllOf("Sequence") or {}) do
        if valid(s) then rows[#rows+1]="SEQUENCE\t"..clean(s:GetFullName()).."\t"..clean(s:GetCheckpoint():ToString()) end
    end
    local f=assert(io.open(root.."harness-state.tsv","w"));f:write(table.concat(rows,"\n").."\n");f:close()
end
local function catalog()
    local rows={"CATALOG\t1\t"..os.time()}
    for _,o in ipairs(FindAllOf("CatUsableComponentBase") or {}) do
        if valid(o) then
            local p=o:K2_GetComponentLocation()
            rows[#rows+1]=string.format("USABLE\t%s\t%s\t%s\t%.4f\t%.4f\t%.4f",clean(o:GetFullName()),
                tostring(o:IsEnabled()),tostring(o:IsBeingUsed()),p.X,p.Y,p.Z)
        end
    end
    local count=#rows-1
    rows[#rows+1]="END\t"..count
    local f=assert(io.open(root.."harness-catalog.tsv","w"));f:write(table.concat(rows,"\n").."\n");f:close()
    log("Cataloged "..count.." interaction components")
end
function M.start(restart)
    guard()
    if restart and StrayCoopHarness then StrayCoopHarness.running=false end
    if StrayCoopHarness and StrayCoopHarness.running then return end
    local state={running=true,queued=false,errors=0}
    StrayCoopHarness=state
    schedule_loop(250,function()
        if not state.running then return true end
        if state.queued then return false end
        state.queued=true
        game_thread(function()
            if state.running then
                local ok,err=pcall(snapshot)
                if not ok then
                    state.errors=state.errors+1
                    if state.errors<=3 then log("Snapshot unavailable: "..tostring(err)) end
                end
            end
            state.queued=false
        end)
        return false
    end)
    log("Isolated test harness ready")
end
function M.dispatch(command)
    guard()
    if command=="harness start" then M.start()
    elseif command=="harness stop" then if StrayCoopHarness then StrayCoopHarness.running=false end;log("Continuous harness telemetry stopped")
    elseif command=="harness restart" then M.start(true)
    elseif command=="harness inspect" then snapshot();log("Snapshot captured")
    elseif command=="harness frame-probe" then
        local p=pc();local address=p:GetAddress()
        p.bShouldPerformFullTickWhenPaused=true
        if not StrayCoopFrameProbe then
            local probe={address=address,count=0,seen=0};StrayCoopFrameProbe=probe
            RegisterHook("/Script/Engine.Actor:ReceiveTick",function(context)
                local current=StrayCoopFrameProbe
                if context:get():GetAddress()==current.address then
                    current.count=current.count+1
                    if os.time()~=current.seen then
                        current.seen=os.time()
                        local f=io.open(root.."frame-probe.tsv","w")
                        if f then f:write("FRAMES\t"..current.count.."\t"..current.seen.."\n");f:close() end
                    end
                end
            end)
        end
        log("Game-thread-only frame probe enabled")
    elseif command=="harness hud-probe" then
        if not StrayCoopHUDProbe then
            local probe={count=0,seen=0};StrayCoopHUDProbe=probe
            RegisterHook("/Script/Engine.HUD:ReceiveDrawHUD",function()
                probe.count=probe.count+1
                if os.time()~=probe.seen then
                    probe.seen=os.time()
                    local f=io.open(root.."hud-probe.tsv","w")
                    if f then f:write("FRAMES\t"..probe.count.."\t"..probe.seen.."\n");f:close() end
                end
            end)
        end
        log("HUD game-thread probe enabled")
    elseif command=="harness bp-frame-probe" then
        pc().bShouldPerformFullTickWhenPaused=true
        if not StrayCoopBPFrameProbe then
            local probe={count=0,seen=0};StrayCoopBPFrameProbe=probe
            RegisterHook("/Game/Technical/BP_HKPlayerController.BP_HKPlayerController_C:ReceiveTick",function()
                probe.count=probe.count+1
                if os.time()~=probe.seen then
                    probe.seen=os.time()
                    local f=io.open(root.."bp-frame-probe.tsv","w")
                    if f then f:write("FRAMES\t"..probe.count.."\t"..probe.seen.."\n");f:close() end
                end
            end)
        end
        log("Player controller game-thread probe enabled")
    elseif command=="harness coop-status" then
        local cat=pc().Pawn;local b=valid(cat) and cat:GetBackpack() or nil
        local d=valid(b) and b:GetCurrentDialog() or nil
        local s=StrayCoopCooperative
        local rows={"COOP_STATUS\t"..os.time(),"ERRORS\t"..tostring(s and s.errors or -1),
            "AUTO_PAUSE\t"..tostring(FindFirstOf("HKGameUserSettings").AutoPauseEnabled),
            "DIALOG\t"..(valid(d) and clean(d:GetFullName()) or "-"),
            "RUNNING\t"..tostring(valid(d) and d:IsDialogRunning() or false),
            "TEXT_READY\t"..tostring(valid(d) and d:IslineTextDisplayed() or false)}
        if valid(b) then
            rows[#rows+1]="CURRENT_INTERACTION\t"..(valid(b:GetCurrentInteraction()) and clean(b:GetCurrentInteraction():GetFullName()) or "-")
            rows[#rows+1]="WITHIN\t"..tostring(b:IsWithinDialog())
            rows[#rows+1]="INTERACTING\t"..tostring(b:IsInteracting())
            rows[#rows+1]="DRONE\t"..tostring(valid(b:GetDrone()))
        end
        if StrayCoopPause then
            for _,key in ipairs({"running","queued","errors","explicit","owned","observed_menu","observed_pause","desired","native_requested"}) do
                rows[#rows+1]="PAUSE_"..key.."\t"..tostring(StrayCoopPause[key])
            end
        end
        for _,o in ipairs(FindAllOf("TalkableComponent") or {}) do
            if valid(o) and valid(o:GetOwner()) then rows[#rows+1]="TALKABLE\t"..clean(o:GetFullName()).."\t"..clean(o:GetOwner():GetFullName()).."\t"..tostring(o:IsEnabled()).."\t"..tostring(o:IsUseEnabled()).."\t"..tostring(o:IsBeingUsed()) end
        end
        if s then for id,line in pairs(s.lines) do rows[#rows+1]="LINE\t"..id.."\t"..line end end
        if StrayCoopMovement then
            for id,r in pairs(StrayCoopMovement.remotes) do rows[#rows+1]="REMOTE_GEAR\t"..id.."\t"..tostring(r.gear_name or "-") end
        end
        local f=assert(io.open(root.."coop-status.tsv","w"));f:write(table.concat(rows,"\n").."\n");f:close()
    elseif command=="harness coop-talk" then
        local cat=pc().Pawn;local b=cat:GetBackpack();assert(valid(b),"Backpack unavailable")
        local chosen
        for _,o in ipairs(FindAllOf("TalkableComponent") or {}) do
            if valid(o) and valid(o:GetOwner()) and o:GetOwner():GetFullName():find("PersistentLevel.BP_Droid_Guardian",1,true) then chosen=o;break end
        end
        assert(valid(chosen),"Guardian talkable not loaded")
        if b:IsInteracting() then b:StopUse() end
        cat:StartInteraction(chosen);log("Native Guardian conversation requested")
    elseif command=="harness coop-start" then
        local cat=pc().Pawn;local b=cat:GetBackpack();local d=b:GetCurrentDialog()
        assert(valid(d) and valid(d:GetCat()) and d:GetCat():GetFullName()==cat:GetFullName(),"Dialogue is not bound to local test cat")
        if not d:IsDialogRunning() then d:StartDialog() end
        log("Native dialogue start fixture requested")
    elseif command=="harness coop-next" then
        local b=pc().Pawn:GetBackpack();local d=b:GetCurrentDialog();assert(valid(d),"No current dialog")
        d:RequestNextDialogLine();log("Native dialog advance requested")
    elseif command=="harness coop-end" then
        local b=pc().Pawn:GetBackpack();local d=b:GetCurrentDialog();assert(valid(d),"No current dialog")
        d:EndDialog();log("Native dialog end requested")
    elseif command=="harness catalog" then catalog()
    elseif command=="harness resume" then resume();log("Background test simulation resumed")
    elseif command=="harness pause-on" then
        local manager=FindFirstOf("GameStateManager")
        assert(valid(manager),"Game state manager unavailable")
        if StrayCoopPause then StrayCoopPause.explicit=true end
        manager:SuspendGame(2)
        log("Native player-requested pause invoked")
    elseif command=="harness pause-off" then
        local manager=FindFirstOf("GameStateManager")
        assert(valid(manager),"Game state manager unavailable")
        manager:UnsuspendGame()
        log("Native resume invoked")
    elseif command=="harness pause-toggle" then
        local manager=FindFirstOf("GameStateManager")
        assert(valid(manager),"Game state manager unavailable")
        manager:_OnMenuStartPressed()
        log("Native menu input handler invoked")
    elseif command:match("^harness open ") then
        local id=tonumber(command:match("^harness open (%d+)$"))
        assert(id and id>=1 and id<=12,"Chapter must be 1 through 12")
        local p=pc()
        assert(StaticFindObject("/Script/Hk_project.Default__HKUtilities"):GetChapter(p)==0,
            "Chapter jumps require a fresh menu session; restart the harness for the next case")
        resume()
        log("Opening native chapter "..id)
        StaticFindObject("/Script/Hk_project.Default__HKUtilities"):OpenChapter(p,id)
    else error("Unknown harness command") end
end
return M
