local M = {}
local Context=dofile("Mods/StrayCoop/Scripts/context.lua")
local function log(s) print("[StrayCoop] " .. tostring(s) .. "\n") end
local function valid(o) return o ~= nil and o:IsValid() end

local function local_controller()
    local _,current=Context.player()
    if valid(current) and current:IsLocalController() then return current end
    for _, pc in ipairs(FindAllOf("PlayerController") or {}) do
        if valid(pc) and pc:IsLocalController() then return pc end
    end
    error("Local player controller is not ready")
end

local function configure_ip()
    local engine = FindFirstOf("Engine")
    if not valid(engine) then error("Engine is not ready") end
    engine.NetDriverDefinitions:ForEach(function(_, value)
        local definition = value:get()
        log("NetDriverDefinition: " .. definition.DefName:ToString() .. " -> " .. definition.DriverClassName:ToString())
        if definition.DefName:ToString() == "GameNetDriver" then
            definition.DriverClassName = FName("OnlineSubsystemUtils.IpNetDriver", EFindName.FNAME_Add)
            definition.DriverClassNameFallback = FName("OnlineSubsystemUtils.IpNetDriver", EFindName.FNAME_Add)
            log("Updated driver: " .. definition.DriverClassName:ToString())
        end
    end)
    log("IP driver configured (no Steam matchmaking required)")
end

function M.dispatch(command)
    if command=="cinematic-reload" then
        local m=dofile("Mods/StrayCoop/Scripts/cinematics.lua");m.stop();m.start()
    elseif command=="cinematic-status" then
        local pc=local_controller()
        local u=StaticFindObject("/Script/Engine.Default__GameplayStatics")
        local s=StrayCoopCinematics
        local f=assert(io.open("Mods/StrayCoop/cinematic-status.tsv","w"))
        f:write(string.format("STATUS\t%s\t%s\t%.6f\t%s\t%d\n",tostring(pc:IsIngameCinematicModeEnabled()),
            tostring(u:IsGamePaused(pc)),u:GetGlobalTimeDilation(pc),tostring(s and s.waiting or false),s and s.errors or -1))
        f:close()
    elseif command=="cinematic-test-recover" then
        assert(io.open("Mods/StrayCoop/harness-session.txt","r"),"Isolated fixture required")
        local pc=local_controller()
        assert(pc:IsIngameCinematicModeEnabled(),"Only an owned cinematic fixture can recover")
        for _,p in ipairs(FindAllOf("LevelSequencePlayer") or {}) do
            if valid(p) and valid(p.Sequence) and not p:IsPlaying() then p:Play() end
        end
    elseif command=="cinematic-test-replay" then
        assert(io.open("Mods/StrayCoop/harness-session.txt","r"),"Isolated fixture required")
        local pc=local_controller()
        assert(not pc:IsIngameCinematicModeEnabled(),"Existing cinematic left untouched")
        local seq=FindFirstOf("Seq_FirstCine_C")
        assert(valid(seq),"Morning sequence is not loaded")
        seq.Finished=false;seq.Active=false;seq.Timer=0
        seq:OnStartSequence()
        log("Replaying the native morning fixture")
    elseif command=="visual-probe" then
        local cat=local_controller().Pawn
        local visual=cat.m_visual
        local rows={"BONES\t"..visual:GetNumBones()}
        for _,name in ipairs({"CAT_StandBreathing_Idle","CAT_Stand_Idle"}) do
            local a=StaticFindObject("/Game/Animation/CAT/Idle/"..name.."."..name)
            rows[#rows+1]=name.."\t"..(valid(a) and tostring(a.AdditiveAnimType) or "missing")
        end
        local t=os.clock()
        for i=0,visual:GetNumBones()-1 do
            local name=visual:GetBoneName(i)
            local transform=visual:GetSocketTransform(name,2)
            if i<3 then rows[#rows+1]=name:ToString().."\t"..tostring(transform.Translation.X) end
        end
        rows[#rows+1]="CAPTURE_MS\t"..(os.clock()-t)*1000
        local pc=local_controller()
        rows[#rows+1]="CINEMATIC\t"..tostring(pc:IsIngameCinematicModeEnabled())
        for _,p in ipairs(FindAllOf("LevelSequencePlayer") or {}) do
            if valid(p) then rows[#rows+1]="SEQUENCE\t"..p:GetFullName().."\t"..tostring(p:IsPlaying()).."\t"..(valid(p.Sequence) and p.Sequence:GetFullName() or "-") end
        end
        for _,p in ipairs(FindAllOf("TimelineComponent") or {}) do
            if valid(p) and p:IsPlaying() then rows[#rows+1]="TIMELINE\t"..p:GetFullName().."\t"..p:GetTimelineLength().."\t"..p:GetPlaybackPosition() end
        end
        for _,p in ipairs(FindAllOf("SEQ_cat_familly_intro_C") or {}) do
            if valid(p) then rows[#rows+1]="INTRO\t"..tostring(p.Play_cine_intro).."\t"..p.Timeline_0:GetPlaybackPosition().."\t"..p.Timeline_0:GetTimelineLength() end
        end
        for _,p in ipairs(FindAllOf("Seq_FirstCine_C") or {}) do
            if valid(p) then rows[#rows+1]="FIRST\t"..tostring(p.Active).."\t"..tostring(p.Timer).."\t"..tostring(p.Finished) end
        end
        local f=assert(io.open("Mods/StrayCoop/visual-probe.tsv","w"));f:write(table.concat(rows,"\n"));f:close()
    elseif command=="objects-start" then
        dofile("Mods/StrayCoop/Scripts/objects.lua").start()
    elseif command=="objects-inspect" then
        local cat=local_controller().Pawn
        local held=cat.GrabbedObject
        log("HELD "..(valid(held) and held:GetFullName() or "none"))
        for _,comp in ipairs(FindAllOf("COMP_Grabbable_C") or {}) do
            if valid(comp) then
                local b=comp.BodyComponent
                log("GRABBABLE "..comp:GetFullName().." body="..(valid(b) and b:GetFullName() or "none").." state="..tostring(comp.GrabbedState))
            end
        end
        for _,fan in ipairs(FindAllOf("BP_VentiloBlokable_C") or {}) do
            if valid(fan) then log("FAN "..fan:GetFullName().." blocked="..tostring(fan["is blocked"]).." running="..tostring(fan.Running)) end
        end
    elseif command=="objects-probe" then
        local cat=local_controller().Pawn
        local rows={"HELD\t"..(valid(cat.GrabbedObject) and (cat.GrabbedObject:GetFullName():match("^%S+ (.+)$") or "-") or "-")}
        for _,comp in ipairs(FindAllOf("COMP_Grabbable_C") or {}) do
            if valid(comp) and valid(comp:GetOwner()) then
                local actor=comp:GetOwner()
                if actor:GetFullName():match("^BP_Bucket_Grabbable") then
                    for _,body in ipairs({actor.Anse,actor.bucket}) do
                        if valid(body) then
                            local p=body:K2_GetComponentLocation()
                            rows[#rows+1]=string.format("PART\t%s\t%.4f\t%.4f\t%.4f\t%s\t%s",body:GetFullName():match("^%S+ (.+)$"),p.X,p.Y,p.Z,tostring(comp:IsUseEnabled()),tostring(actor:IsActorTickEnabled()))
                        end
                    end
                end
            end
        end
        for _,fan in ipairs(FindAllOf("BP_VentiloBlokable_C") or {}) do
            if valid(fan) then rows[#rows+1]="FAN\t"..fan:GetFullName():match("^%S+ (.+)$").."\t"..tostring(fan["is blocked"]) end
        end
        local f=assert(io.open("Mods/StrayCoop/objects-probe.tsv","w"));f:write(table.concat(rows,"\n").."\n");f:close()
    elseif command=="objects-test-fan" then
        local fan=FindFirstOf("BP_VentiloBlokable_C")
        local cat=local_controller().Pawn
        assert(valid(fan) and valid(cat.GrabbedObject),"Native fan and held bucket required")
        local actor=cat.GrabbedObject:GetOwner()
        fan:ReceiveActorBeginOverlap(actor)
        log("Native fan overlap: blocked="..tostring(fan["is blocked"]).." running="..tostring(fan.Running))
    elseif command:match("^objects%-test%-pickup ") or command:match("^objects%-test%-near ") then
        local id=command:match("^objects%-test%-%w+ (%d+)$")
        local found={}
        for _,comp in ipairs(FindAllOf("COMP_Grabbable_C") or {}) do
            if valid(comp) and valid(comp:GetOwner()) and comp:GetOwner():GetFullName():match("^BP_Bucket_Grabbable") then found[#found+1]=comp:GetFullName():match("^%S+ (.+)$") end
        end
        table.sort(found)
        local comp=StaticFindObject(assert(found[tonumber(id)],"Bucket index unavailable"))
        local cat=local_controller().Pawn
        local p=comp:K2_GetComponentLocation()
        if command:match("^objects%-test%-near ") then
            assert(cat:Teleport({X=p.X-70,Y=p.Y,Z=p.Z+20},cat:K2_GetActorRotation(),false,true),"Native teleport rejected")
            log("Native bucket fixture teleport requested");return
        end
        cat:StartInteraction(comp)
        log("Native bucket pickup requested: "..comp:GetFullName())
    elseif command=="objects-test-drop" then
        local cat=local_controller().Pawn
        if valid(cat.GrabbedObject) then
            local comp=cat.GrabbedObject
            log("Before drop state="..tostring(comp.GrabbedState).." anim="..tostring(comp.AnimationState).." using="..tostring(comp:IsBeingUsed()))
            cat:StopInteraction(comp)
            log("After drop state="..tostring(comp.GrabbedState).." anim="..tostring(comp.AnimationState).." using="..tostring(comp:IsBeingUsed()))
        end
    elseif command=="pause-reload" then
        local module=dofile("Mods/StrayCoop/Scripts/pause.lua")
        module.stop();module.start()
    elseif command=="pause-inspect" then
        local pc=local_controller()
        log("Pause engine="..tostring(StaticFindObject("/Script/Engine.Default__GameplayStatics"):IsGamePaused(pc)))
        local manager=pc.m_gameStateManager
        log("Pause manager="..tostring(valid(manager) and manager:IsSuspended()))
        local hud=pc:GetHUD()
        log("Pause HUD="..(valid(hud) and hud:GetFullName() or "none"))
        if valid(hud) then for _,w in ipairs(hud:GetWidgets()) do
            if valid(w) then log("Widget "..w:GetFullName().." viewport="..tostring(w:IsInViewport()).." visible="..tostring(w:IsVisible())) end
        end end
    elseif command:match("^harness ") then
        dofile("Mods/StrayCoop/Scripts/harness.lua").dispatch(command)
    elseif command == "avatar-refresh" then
        local movement=dofile("Mods/StrayCoop/Scripts/movement.lua")
        movement.stop()
        movement.start()
    elseif command == "render-status" then
        local pc=local_controller()
        pc.HiddenActors:ForEach(function(_,v) local o=v:get();if valid(o) then log("CAMERA HIDDEN "..o:GetFullName()) end end)
        local cats={local_controller().Pawn}
        for _,r in pairs(StrayCoopMovement.remotes) do
            local a=StaticFindObject(r.name:match("^%S+ (.+)$") or r.name)
            if valid(a) then cats[#cats+1]=a end
        end
        for _,a in ipairs(cats) do
            log("RENDER "..a:GetFullName().." hiddenGameplay="..tostring(a.HiddenInGameplay).." rendered="..tostring(a:WasRecentlyRendered(1)))
            local m=a.m_visual
            log(" mainPass="..tostring(m.bRenderInMainPass).." depthPass="..tostring(m.bRenderInDepthPass).." captureOnly="..tostring(m.bVisibleInSceneCaptureOnly))
            local anim=m:GetAnimInstance()
            if valid(anim) and anim:IsA("/Script/Engine.AnimSingleNodeInstance") then
                local asset=anim:GetAnimationAsset()
                log(" single asset="..(valid(asset) and asset:GetFullName() or "none"))
            end
            for _,i in ipairs({0,1,5}) do
                local bone=m:GetBoneName(i);local p=m:GetSocketLocation(bone)
                log(string.format(" bone %s hidden=%s at %.2f,%.2f,%.2f",bone:ToString(),tostring(m:IsBoneHiddenByName(bone)),p.X,p.Y,p.Z))
            end
            log(" animation mode="..tostring(m.AnimationMode).." instance="..(valid(anim) and anim:GetFullName() or "none")
                .." asset="..(valid(m.AnimationData.AnimToPlay) and m.AnimationData.AnimToPlay:GetFullName() or "none"))
            log(" meshRendered="..tostring(m:WasRecentlyRendered(1)).." tick="..tostring(m:IsComponentTickEnabled()).." animTick="..tostring(m.VisibilityBasedAnimTickOption)
                .." hideSkin="..tostring(m.bHideSkin).." parentBounds="..tostring(m.bUseAttachParentBound)
                .." fixedBounds="..tostring(m.bComponentUseFixedSkelBounds).." updateRate="..tostring(m.bEnableUpdateRateOptimizations)
                .." paused="..tostring(m.bPauseAnims).." noSkeleton="..tostring(m.bNoSkeletonUpdate).." boundsScale="..tostring(m.BoundsScale))
            if valid(a.GFur) then
                local f=a.GFur;local p=f:K2_GetComponentLocation()
                log(string.format(" fur visible=%s hidden=%s tick=%s rendered=%s ownerOnly=%s ownerNoSee=%s position=%.2f,%.2f,%.2f",tostring(f:IsVisible()),tostring(f.bHiddenInGame),tostring(f:IsComponentTickEnabled()),tostring(f:WasRecentlyRendered(1)),tostring(f.bOnlyOwnerSee),tostring(f.bOwnerNoSee),p.X,p.Y,p.Z))
            end
        end
    elseif command == "test-separate" then
        local cat=local_controller().Pawn
        assert(valid(cat) and cat:IsInNormalLocomotion(),"Playable cat required")
        local p=cat:K2_GetActorLocation();p.X=p.X+90;p.Y=p.Y+90
        log("Native test teleport="..tostring(cat:Teleport(p,cat:K2_GetActorRotation(),true,true)))
    elseif command == "story-status" then
        local pc=local_controller()
        local p=pc.Pawn:K2_GetActorLocation()
        log(string.format("Story status: cinematic=%s; locomotion=%s; world=%s; position=%.2f,%.2f,%.2f; errors=%s",
            tostring(pc:IsIngameCinematicModeEnabled()),tostring(pc.Pawn:IsInNormalLocomotion()),
            pc.Pawn:GetWorld():GetFullName(),p.X,p.Y,p.Z,tostring(StrayCoopStory and StrayCoopStory.errors)))
        log(dofile("Mods/StrayCoop/Scripts/story_trace.lua").snapshot())
    elseif command == "story-test-sleep" then
        local component=StaticFindObject("/Game/Map/_MainGame/00_Intro/Intro_GD.Intro_GD:PersistentLevel.PlayerSleepSpotMoveToAnim.COMP_CatButtonMoveToAnim")
        assert(valid(component) and component:IsEnabled() and component:IsUseEnabled(),"Sleep unavailable")
        local_controller().Pawn:StartInteraction(component)
        log("Started native sleep interaction")
    elseif command:match("^story%-test%-complete ") then
        local names={white="WhiteAndRedCatButton",red="RedCatButton",black="BlackCatButton"}
        local field=names[command:match("^story%-test%-complete (%w+)$")]
        assert(field,"Expected white, red, or black")
        local seq=FindFirstOf("SEQ_cat_familly_intro_C")
        local b=seq[field]
        b:BndEvt__BP_CatButton_MoveToAnim_COMP_CatButtonMoveToAnim_K2Node_ComponentBoundEvent_0_CatUsableDelegate__DelegateSignature(b.COMP_CatButtonMoveToAnim,local_controller().Pawn)
        log("Replayed opening completion: "..field)
    elseif command:match("^story%-test%-interact ") then
        local names={white="WhiteAndRedCatButton",red="RedCatButton",black="BlackCatButton"}
        local field=names[command:match("^story%-test%-interact (%w+)$")]
        assert(field,"Expected white, red, or black")
        local seq=FindFirstOf("SEQ_cat_familly_intro_C")
        local pc=local_controller()
        assert(valid(seq) and valid(pc.Pawn) and not pc:IsIngameCinematicModeEnabled(),"Opening scene not ready")
        local component=seq[field].COMP_CatButtonMoveToAnim
        assert(component:IsEnabled() and component:IsUseEnabled(),"Interaction is unavailable")
        pc.Pawn:StartInteraction(component)
        log("Started native opening interaction: "..field)
    elseif command == "story-reload" then
        local previous=StrayCoopStory
        local story=dofile("Mods/StrayCoop/Scripts/story.lua")
        story.stop()
        story.start(previous)
    elseif command == "story-trace" then
        dofile("Mods/StrayCoop/Scripts/story_trace.lua").start()
    elseif command == "story-inspect" then
        for _, actor in ipairs(FindAllOf("Actor") or {}) do
            if valid(actor) and actor:GetFullName():find("/00_Intro/",1,true) then log("INTRO ACTOR " .. actor:GetFullName()) end
        end
        for _, class in ipairs({"LevelScriptActor","CatUsableComponentBase","HKGameInstance","LevelStreaming"}) do
            for _, object in ipairs(FindAllOf(class) or {}) do
                if valid(object) then
                    log("STORY " .. class .. " " .. object:GetFullName())
                    if class=="CatUsableComponentBase" then
                        log("  enabled=" .. tostring(object:IsEnabled()) .. "; used=" .. tostring(object:IsBeingUsed()))
                    end
                end
            end
        end
    elseif command == "jump-status" then
        local pc=local_controller()
        local cat=pc.Pawn
        assert(valid(cat) and cat:IsA("/Script/Hk_project.CatPawn"), "Playable cat required")
        log("Cinematic=" .. tostring(pc:IsIngameCinematicModeEnabled()) .. "; locomotion=" .. tostring(cat:IsInNormalLocomotion())
            .. "; jump enabled=" .. tostring(cat:IsJumpEnabled()) .. "; jump input=" .. tostring(cat:IsJumpInputEnabled()))
        local function count(array) if type(array)=="table" then return #array end; return array:GetArrayNum() end
        log("Up targets=" .. count(cat:GetUpJumpTargets()) .. "; down targets=" .. count(cat:GetDownJumpTargets()))
    elseif command == "test-snapshot-gap" then
        assert(StrayCoopMovement and StrayCoopMovement.running, "Start movement first")
        StrayCoopMovement.pause_publish_until = os.time()+4
        log("Simulating a four-second game snapshot gap")
    elseif command == "animation-status" then
        local state = StrayCoopMovement
        assert(state, "Movement prototype is not started")
        log("Local animation mode=" .. tostring(state.animation_mode) .. "; asset=" .. tostring(state.animation_asset))
        for mode, count in pairs(state.animation_counts or {}) do log("Sample count mode " .. mode .. "=" .. count) end
        local pc = local_controller()
        if valid(pc.Pawn) and pc.Pawn:IsA("/Script/Hk_project.CatPawn") then
            local anim = pc.Pawn:GetAnimInstance()
            log("Local anim instance=" .. (valid(anim) and anim:GetFullName() or "<none>"))
            if valid(anim) then
                log("IsMoving=" .. tostring(anim.IsMoving) .. "; running=" .. tostring(anim.IsRunning) .. "; speed=" .. tostring(anim["Cat Speed"]))
            end
        end
        for id, record in pairs(state.remotes) do
            local actor=StaticFindObject(record.name:match("^%S+ (.+)$") or record.name)
            if valid(actor) then
                log("Remote " .. id .. " animation=" .. tostring(record.animation) .. "; mode=" .. tostring(record.animation_mode)
                    .. "; switches=" .. tostring(record.animation_switches) .. "; rate=" .. tostring(record.animation_rate)
                    .. "; phase=" .. tostring(record.animation_phase) .. "; playback=" .. actor.SkeletalMeshComponent:GetPosition())
            end
        end
    elseif command == "test-camera" then
        local pc = local_controller()
        local camera = pc.Pawn:GetCameraThirdPerson()
        assert(valid(camera), "Cat camera unavailable")
        local manager = FindFirstOf("GameStateManager")
        if valid(manager) then manager:UnsuspendGame() end
        pc:SetViewTargetWithBlend(camera, 0, 0, 0, false)
        log("Test camera focused on local cat")
    elseif command == "test-move" or command == "test-restore" then
        local cat = local_controller().Pawn
        assert(valid(cat) and cat:IsA("/Script/Hk_project.CatPawn"), "Playable cat required")
        local p, r = cat:K2_GetActorLocation(), cat:K2_GetActorRotation()
        if command == "test-move" then
            assert(not StrayCoopTestOrigin, "Restore the previous test move first")
            StrayCoopTestOrigin = {X=p.X,Y=p.Y,Z=p.Z}
            cat.m_movement:SetComponentTickEnabled(false)
            cat:SetActorTickEnabled(false)
            cat:K2_SetActorLocationAndRotation({X=p.X+80,Y=p.Y+80,Z=p.Z}, {Pitch=r.Pitch,Yaw=r.Yaw,Roll=r.Roll}, false, {}, true)
            cat:Meow(false)
        elseif StrayCoopTestOrigin then
            cat:K2_SetActorLocationAndRotation(StrayCoopTestOrigin, {Pitch=r.Pitch,Yaw=r.Yaw,Roll=r.Roll}, false, {}, true)
            StrayCoopTestOrigin = nil
            cat.m_movement:SetComponentTickEnabled(true)
            cat:SetActorTickEnabled(true)
        end
        p = cat:K2_GetActorLocation()
        log(string.format("%s: local cat at %.2f %.2f %.2f",command,p.X,p.Y,p.Z))
    elseif command == "test-background" then
        local settings = FindFirstOf("HKGameUserSettings")
        if valid(settings) then settings.AutoPauseEnabled = false end
        local pc = local_controller()
        StaticFindObject("/Script/Engine.Default__GameplayStatics"):SetGamePaused(pc, false)
        log("Background play enabled for this test session")
    elseif command == "movement-cleanup" then
        local f = io.open("Mods/StrayCoop/cleanup.txt", "r")
        if f then
            for name in f:lines() do
                if name:match("^/Game/Map/.*:PersistentLevel%.BP_CatPawn_C_%d+$") then
                    local actor = StaticFindObject(name)
                    if valid(actor) and not actor:IsPlayerControlled() then actor:K2_DestroyActor() end
                end
            end
            f:close()
        end
        log("Test avatars cleaned up")
    elseif command == "movement-status" then
        local state = StrayCoopMovement
        assert(state, "Movement prototype is not started")
        log("Movement running=" .. tostring(state.running) .. "; sequence=" .. state.sequence .. "; errors=" .. state.errors)
        log("Local cat=" .. tostring(state.local_name))
        for id, record in pairs(state.remotes) do
            local actor = StaticFindObject(record.name:match("^%S+ (.+)$") or record.name)
            if valid(actor) then
                local p = actor:K2_GetActorLocation()
                log(string.format("Remote %d at %.2f %.2f %.2f; meows=%s; %s", id, p.X,p.Y,p.Z,tostring(record.meows),record.name))
                local mesh = actor.SkeletalMeshComponent
                log("Hidden=" .. tostring(actor.bHidden) .. "; visual=" .. tostring(valid(mesh)))
                if valid(mesh) then
                    log("Visible=" .. tostring(mesh:IsVisible()) .. "; mesh=" .. (valid(mesh.SkeletalMesh) and mesh.SkeletalMesh:GetFullName() or "<none>"))
                    log("Owner visibility=" .. tostring(mesh.bOnlyOwnerSee) .. "," .. tostring(mesh.bOwnerNoSee))
                    local q, s = mesh:K2_GetComponentLocation(), mesh.RelativeScale3D
                    log(string.format("Visual location %.2f %.2f %.2f; scale %.2f %.2f %.2f", q.X,q.Y,q.Z,s.X,s.Y,s.Z))
                end
            end
        end
    elseif command == "movement-start" then
        dofile("Mods/StrayCoop/Scripts/cooperative.lua").start()
        dofile("Mods/StrayCoop/Scripts/cinematics.lua").start()
        dofile("Mods/StrayCoop/Scripts/pause.lua").start()
        dofile("Mods/StrayCoop/Scripts/objects.lua").start()
        dofile("Mods/StrayCoop/Scripts/movement.lua").start()
        dofile("Mods/StrayCoop/Scripts/story.lua").start()
    elseif command == "movement-stop" then
        dofile("Mods/StrayCoop/Scripts/cooperative.lua").stop()
        dofile("Mods/StrayCoop/Scripts/cinematics.lua").stop()
        dofile("Mods/StrayCoop/Scripts/pause.lua").stop()
        dofile("Mods/StrayCoop/Scripts/objects.lua").stop()
        dofile("Mods/StrayCoop/Scripts/movement.lua").stop()
        dofile("Mods/StrayCoop/Scripts/story.lua").stop()
    elseif command == "observe" then
        for _, name in ipairs({"HandleNetworkError", "HandleTravelError"}) do
            local ok, err = pcall(function()
                RegisterHook("/Script/Engine.GameInstance:" .. name, function(_, value)
                    log(name .. ": " .. tostring(value:get()))
                end)
            end)
            log("Error hook " .. name .. ": " .. tostring(ok) .. "; " .. tostring(err))
        end
    elseif command == "host" or command == "probe-listen" then
        local pc = local_controller()
        if command == "host" and (not valid(pc.Pawn) or not pc.Pawn:IsA("/Script/Hk_project.CatPawn")) then
            error("Load a playable chapter before hosting")
        end
        configure_ip()
        local gameplay = StaticFindObject("/Script/Engine.Default__GameplayStatics")
        assert(valid(gameplay), "GameplayStatics default object is unavailable")
        log("GameplayStatics ready")
        local level = pc:GetWorld():GetFullName():match("^World (.-)%.")
        assert(level, "Unable to identify current world package")
        log("Opening listen server on current map: " .. level)
        gameplay:OpenLevel(pc, FName(level, EFindName.FNAME_Add), true, "listen?Port=7777")
    elseif command:match("^join ") then
        local target = command:match("^join ([%d%.]+:%d+)$")
        if not target then error("Expected join IPv4:port") end
        configure_ip()
        local pc = local_controller()
        log("Client travel to " .. target)
        pc:ClientTravel(target, 0, false, {A=0, B=0, C=0, D=0})
    elseif command == "replicate" then
        for _, cat in ipairs(FindAllOf("CatPawn") or {}) do
            if valid(cat) and cat:HasAuthority() then
                cat:SetReplicates(true)
                cat:SetReplicateMovement(true)
                cat.bAlwaysRelevant = true
                cat.NetUpdateFrequency = 30
                log("Replication enabled for " .. cat:GetFullName())
            end
        end
    elseif command == "network" then
        local pc = local_controller()
        local world = pc:GetWorld()
        log("World=" .. world:GetFullName())
        log("Authority=" .. tostring(pc:HasAuthority()) .. "; local=" .. tostring(pc:IsLocalController()))
        local driver = world.NetDriver
        log("NetDriver=" .. (valid(driver) and driver:GetFullName() or "<none>"))
        for _, d in ipairs(FindAllOf("NetDriver") or {}) do log("Driver instance=" .. d:GetFullName()) end
        FindFirstOf("Engine").NetDriverDefinitions:ForEach(function(_, value)
            local d = value:get()
            log("Configured driver=" .. d.DefName:ToString() .. " -> " .. d.DriverClassName:ToString())
        end)
        local ip = StaticFindObject("/Script/OnlineSubsystemUtils.Default__IpNetDriver")
        if valid(ip) then
            log("IP connection class=" .. ip.NetConnectionClassName:ToString())
        end
        for _, gm in ipairs(FindAllOf("GameModeBase") or {}) do
            log("GameMode options=" .. gm.OptionsString:ToString())
        end
        for _, item in ipairs(FindAllOf("PlayerController") or {}) do
            log("Controller=" .. item:GetFullName() .. "; local=" .. tostring(item:IsLocalController()) .. "; pawn=" .. (valid(item.Pawn) and item.Pawn:GetFullName() or "<none>"))
        end
    elseif command == "quit" then
        local pc = local_controller()
        StaticFindObject("/Script/Engine.Default__KismetSystemLibrary"):QuitGame(pc, pc, 0, true)
    else
        error("Unknown command: " .. command)
    end
end
return M
