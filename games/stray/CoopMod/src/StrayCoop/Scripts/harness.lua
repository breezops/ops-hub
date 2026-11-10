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
    -- Resume startup menus through the actual UI. Removing a widget or
    -- invoking MenuStart can leave/open a native suspension state.
    if #menus>0 then log("Startup pause menu requires one real Resume action");return end
    local manager=p.m_gameStateManager
    if valid(manager) and manager:IsSuspended() then manager:UnsuspendGame() end
    Context.gameplay():SetGamePaused(p,false)
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
        "ADVENTURE_STATE\t"..tostring(instance:GetCurrentAdventureChapterState(4)),
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
        rows[#rows+1]="CAT_FROZEN\t"..tostring(cat:IsFrozen())
        rows[#rows+1]="CAT_PILOT_DISABLED\t"..tostring(cat:IsPilotDisabled())
        rows[#rows+1]="CAT_WALK_INTERP\t"..tostring(cat:IsWalkInterpolating())
        if valid(b) then
            rows[#rows+1]="CURRENT_INTERACTION\t"..(valid(b:GetCurrentInteraction()) and clean(b:GetCurrentInteraction():GetFullName()) or "-")
            rows[#rows+1]="WITHIN\t"..tostring(b:IsWithinDialog())
            rows[#rows+1]="INTERACTING\t"..tostring(b:IsInteracting())
            b.m_inventory:ForEach(function(_,entry)
                local item=entry:get()
                if valid(item) then rows[#rows+1]="INVENTORY\t"..clean(item.ID:ToString()).."\t"..clean(item:GetFullName()) end
            end)
            rows[#rows+1]="DRONE\t"..tostring(valid(b:GetDrone()))
            rows[#rows+1]="DIALOG_CAT\t"..(valid(d) and valid(d:GetCat()) and clean(d:GetCat():GetFullName()) or "-")
            local component=b:GetCurrentInteraction()
            if valid(component) and component:IsA("/Script/Hk_project.TalkableComponent") then
                rows[#rows+1]="WAITING_INTERLOCUTOR\t"..tostring(component:IsWaitingInterlocutor())
                rows[#rows+1]="WAITING_DIALOG\t"..tostring(component:IsWaitingDialog())
                rows[#rows+1]="DRONE_USING\t"..tostring(component:IsDroneUsing())
                local a,c=cat:K2_GetActorLocation(),component:GetOwner():K2_GetActorLocation()
                rows[#rows+1]="TARGET_DISTANCE\t"..tostring(math.sqrt((a.X-c.X)^2+(a.Y-c.Y)^2+(a.Z-c.Z)^2))
            end
        end
        for _,settings in ipairs(FindAllOf("HKGameUserSettings") or {}) do
            if valid(settings) then rows[#rows+1]="SETTINGS\t"..clean(settings:GetFullName()).."\t"..tostring(settings.AutoPauseEnabled) end
        end
        local settings=Context.settings()
        rows[#rows+1]="CONTEXT_SETTINGS\t"..(valid(settings) and clean(settings:GetFullName()) or "-")
        if StrayCoopPause then
            for _,key in ipairs({"running","queued","errors","explicit","owned","observed_menu","observed_pause","desired","native_requested"}) do
                rows[#rows+1]="PAUSE_"..key.."\t"..tostring(StrayCoopPause[key])
            end
        end
        for _,o in ipairs(FindAllOf("TalkableComponent") or {}) do
            if valid(o) and valid(o:GetOwner()) then rows[#rows+1]="TALKABLE\t"..clean(o:GetFullName()).."\t"..clean(o:GetOwner():GetFullName()).."\t"..tostring(o:IsEnabled()).."\t"..tostring(o:IsUseEnabled()).."\t"..tostring(o:IsBeingUsed()) end
        end
        local broom=StaticFindObject("/Game/Map/_MainGame/03_Slums/Slums_GD.Slums_GD:PersistentLevel.SEQ_IntroBroomDroid_2")
        local alarm=StaticFindObject("/Game/Map/_MainGame/03_Slums/Slums_GD.Slums_GD:PersistentLevel.Seq_Slum_intro_Alarm")
        rows[#rows+1]="BROOM_DONE\t"..tostring(valid(broom) and broom.SeqDone)
        rows[#rows+1]="ALARM_ON\t"..tostring(valid(alarm) and alarm.AlarmIsOn)
        local meet=StaticFindObject("/Game/Map/_MainGame/03_Slums/Slums_GD.Slums_GD:PersistentLevel.Seq_Slum_Meet_Guardian_2")
        if valid(meet) and valid(meet.MeetGuardian_Sequence) then
            local player=meet.MeetGuardian_Sequence:GetSequencePlayer()
            if valid(player) then
                rows[#rows+1]="MEET_PLAYING\t"..tostring(player:IsPlaying())
                rows[#rows+1]="MEET_PAUSED\t"..tostring(player:IsPaused())
                rows[#rows+1]="MEET_FRAME\t"..tostring(player:GetCurrentTime().Time.FrameNumber.Value)
                rows[#rows+1]="MEET_END\t"..tostring(player:GetEndTime().Time.FrameNumber.Value)
            end
        end
        local guardianDialog=StaticFindObject("/Game/Map/_MainGame/03_Slums/Slums_GD.Slums_GD:PersistentLevel.Dial_Guardian_2")
        if valid(guardianDialog) then
            for _,key in ipairs({"IntroTalkedFinished","PostCardShown","FirstTime","Untranslated"}) do
                rows[#rows+1]="GUARDIAN_"..key.."\t"..tostring(guardianDialog[key])
            end
        end
        if s then for id in pairs(s.sequence_applied or {}) do rows[#rows+1]="COOP_SEQUENCE_APPLIED\t"..id end end
        if s then for id in pairs(s.sequence_started or {}) do rows[#rows+1]="COOP_SEQUENCE\t"..id end end
        if s then for id,line in pairs(s.lines) do rows[#rows+1]="LINE\t"..id.."\t"..line end end
        if StrayCoopMovement then
            for id,r in pairs(StrayCoopMovement.remotes) do rows[#rows+1]="REMOTE_GEAR\t"..id.."\t"..tostring(r.gear_name or "-") end
        end
        local f=assert(io.open(root.."coop-status.tsv","w"));f:write(table.concat(rows,"\n").."\n");f:close()
    elseif command=="harness cooperative-refresh-idle" then
        local s=StrayCoopCooperative
        assert(s and s.counter==0 and s.applied==0 and not next(s.pending),"Refresh is allowed only before any shared interaction")
        s.running=false;dofile(root.."Scripts/cooperative.lua").start()
        log("Refreshed idle cooperative worker; native hooks unchanged")
    elseif command=="harness guardian-stage" then
        local cat=pc().Pawn;local guardian
        for _,o in ipairs(FindAllOf("TalkableComponent") or {}) do
            if valid(o) and valid(o:GetOwner()) and o:GetOwner():GetFullName():find("PersistentLevel.BP_Droid_Guardian",1,true) then guardian=o:GetOwner();break end
        end
        assert(valid(cat) and valid(guardian),"Guardian scene not loaded")
        local p=guardian:K2_GetActorLocation()
        cat:K2_SetActorLocation({X=p.X+220,Y=p.Y,Z=p.Z+70},false,{},true)
        log("Staged isolated cat near native Guardian; quest state unchanged")
    elseif command=="harness guardian-release-extra-lock" then
        local p=pc();local scene=StaticFindObject("/Game/Map/_MainGame/03_Slums/Slums_GD.Slums_GD:PersistentLevel.Seq_Slum_Meet_Guardian_2")
        assert(valid(scene) and scene.SeqPlayed and p:IsIngameCinematicModeEnabled(),"No Guardian cinematic lock to inspect")
        p:PopIngameCinematicModeEnabled(0)
        log("Released one extra Guardian cinematic entry in isolated test")
    elseif command=="harness broom-catchup" then
        local scene=StaticFindObject("/Game/Map/_MainGame/03_Slums/Slums_GD.Slums_GD:PersistentLevel.SEQ_IntroBroomDroid_2")
        assert(valid(scene),"Slums broom sequence unavailable")
        if not scene.SeqDone then scene:LogicTrigger() end
        log("Requested missing authored broom scene")
    elseif command=="harness guardian-controls" then
        local rows={}
        local input=pc().PlayerInput
        if valid(input) then input.ActionMappings:ForEach(function(_,entry)
            local v=entry:get();rows[#rows+1]=v.ActionName:ToString().."\t"..v.Key.KeyName:ToString()
        end) end
        local f=assert(io.open(root.."guardian-controls.tsv","w"));f:write(table.concat(rows,"\n").."\n");f:close()
    elseif command=="harness guardian-scene" then
        local scene=StaticFindObject("/Game/Map/_MainGame/03_Slums/Slums_GD.Slums_GD:PersistentLevel.Seq_Slum_Meet_Guardian_2")
        assert(valid(scene) and scene:IsA("/Script/Hk_project.Sequence"),"Guardian sequence unavailable")
        scene:LogicTrigger();log("Native Guardian first-meeting logic trigger requested")
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
    elseif command=="harness b12-help" then
        local b=pc().Pawn:GetBackpack();assert(valid(b) and b:IsB12HelpEnabled(),"B12 help unavailable")
        b:_OnHelpPressed();log("Native B12 help input requested")
    elseif command=="harness show-postcard" then
        local b=pc().Pawn:GetBackpack();local d=b:GetCurrentDialog()
        assert(valid(d) and d:GetFullName():find("Dial_Guardian_C ",1,true) and d:IsDialogRunning() and d:CanBeShownItems(),"Guardian item prompt unavailable")
        local chosen
        b.m_inventory:ForEach(function(_,entry) local item=entry:get();if valid(item) and item.ID:ToString()=="PostCard" then chosen=item end end)
        assert(valid(chosen),"Local postcard missing")
        d:BeginShowItem(chosen);log("Native Guardian postcard choice requested")
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
