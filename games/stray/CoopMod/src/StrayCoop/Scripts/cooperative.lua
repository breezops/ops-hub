-- Native interaction intents and decorative backpack visuals. No packet code.
local M={}
local root="Mods/StrayCoop/"
local Context=dofile(root.."Scripts/context.lua")
local schedule_loop=Context.loop or LoopAsync
local game_thread=Context.run or ExecuteInGameThread
local Interpolation=dofile(root.."Scripts/interpolation.lua")
local Identity=dofile(root.."Scripts/dialogue_identity.lua")
local function valid(o) return o~=nil and o:IsValid() end
local function path(o) return valid(o) and o:GetFullName():match("^%S+ (.+)$") end
local function same(a,b) return valid(a) and valid(b) and a:GetFullName()==b:GetFullName() end
local function allowed(id)
    return type(id)=="string" and #id<=300 and id:match("^/Game/Map/[%w_/]+%.[%w_]+:PersistentLevel%.[%w_.]+$")
end
local function target_id(id)
    local state=StrayCoopCooperative
    return Identity.encode(id,state and state.backpack_path,state and state.dialog_path)
end
local function talkable(id)
    if not allowed(id) then return end
    local state=StrayCoopCooperative
    local resolved=Identity.resolve(id,state and state.backpack_path,state and state.dialog_path)
    local o=resolved and StaticFindObject(resolved)
    if valid(o) and o:IsA("/Script/Hk_project.TalkableComponent") then return o end
end
local guardian_path="/Game/Map/_MainGame/03_Slums/Slums_GD.Slums_GD:PersistentLevel.Seq_Slum_Meet_Guardian_2"
local alarm_path="/Game/Map/_MainGame/03_Slums/Slums_GD.Slums_GD:PersistentLevel.Seq_Slum_intro_Alarm"
local broom_path="/Game/Map/_MainGame/03_Slums/Slums_GD.Slums_GD:PersistentLevel.SEQ_IntroBroomDroid_2"
local cue_field={[guardian_path]="SeqPlayed",[alarm_path]="AlarmIsOn",[broom_path]="SeqDone"}
local ordered_cues={{broom_path,"SeqDone"},{alarm_path,"AlarmIsOn"},{guardian_path,"SeqPlayed"}}
local function guardian_sequence(id) return cue_field[id]~=nil end
local function backpack(cat)
    local b=cat and cat:GetBackpack()
    if valid(b) then return b end
end
local function emit(kind,target,line)
    local s=StrayCoopCooperative
    if not s or not s.running or s.applying or not allowed(target) then return end
    s.counter=s.counter+1
    s.events[#s.events+1]={s.counter,kind,target,line or 0,s.now or 0}
    while #s.events>4 do table.remove(s.events,1) end
end
local function current_dialog()
    local cat=Context.player();local b=backpack(cat)
    local d=b and b:GetCurrentDialog()
    if valid(d) then return d,b end
end
local function tick(s)
    local cat,pc=Context.player()
    if not cat or not pc then return end
    local now=Context.gameplay():GetRealTimeSeconds(pc)
    s.now=now;s.cat_path=path(cat)
    local b=backpack(cat)
    s.backpack_path=path(b)
    local dialog=b and b:GetCurrentDialog()
    s.dialog_path=path(dialog)
    -- Native C++ calls can bypass reflected hooks. Confirm entry from the
    -- running dialogue as well; never broadcast merely approaching an NPC.
    if valid(dialog) and dialog:IsDialogRunning() and b:IsWithinDialog() then
        local component=b:GetCurrentInteraction()
        local target=target_id(path(component))
        if target and allowed(target) and component:IsA("/Script/Hk_project.TalkableComponent")
            and s.observed_dialog~=s.dialog_path then
            s.observed_dialog=s.dialog_path;s.active_target=target
            s.lines[target]=s.lines[target] or 0
            if s.remote_start==target then s.remote_start=nil else emit("use",target,0) end
        end
    else s.observed_dialog=nil end
    s.cue_previous=s.cue_previous or {};s.cue_remote=s.cue_remote or {}
    for _,cue in ipairs(ordered_cues) do
        local id,field=cue[1],cue[2]
        local actor=StaticFindObject(id)
        if valid(actor) then
            local played=actor[field]
            if played and s.cue_previous[id]~=true then
                s.sequence_started[id]=true
                if s.cue_remote[id] then s.cue_remote[id]=nil else emit("sequence",id,0) end
            end
            s.cue_previous[id]=played
        else s.cue_previous[id]=nil end
    end
    s.cinematic=pc:IsIngameCinematicModeEnabled()
    local gear=0;local pose={0,0,0,0,0,0}
    if b and b:IsBackpackMeshVisible() and valid(b:GetMesh()) then
        local mesh=b:GetMesh();local p,r=mesh:K2_GetComponentLocation(),mesh:K2_GetComponentRotation()
        gear=1;pose={p.X,p.Y,p.Z,r.Pitch,r.Yaw,r.Roll}
    end
    local f=io.open(root.."coop-local.tsv","w")
    if f then
        f:write("GEAR\t"..gear.."\t"..table.concat(pose,"\t").."\n")
        for _,e in ipairs(s.events) do
            if now-e[5]<8 then f:write("EVENT\t"..table.concat({e[1],e[2],e[3],e[4]},"\t").."\n") end
        end
        f:close()
    end
    f=io.open(root.."coop-remote.tsv","r");local text=f and f:read(2001) or "";if f then f:close() end
    if #text>2000 or text:sub(-1)~="\n" then return end
    local generation,connected,own,epoch=text:match("^COOP\t(%d+)\t([01])\t([12])\t([%w%-]+)\n")
    if not generation then return end
    if generation~=s.generation then s.generation=generation;s.received=now end
    if connected~="1" or now-(s.received or 0)>2 then s.gear=nil;return end
    s.own=tonumber(own)
    if s.epoch~=epoch then s.epoch=epoch;s.applied=0;s.pending={} end
    local flag,coords=text:match("\nGEAR\t([01])\t([^\n]+)\n")
    local values={}
    for v in (coords or ""):gmatch("[^\t]+") do values[#values+1]=tonumber(v) end
    if #values==6 then
        local good=true
        for i,v in ipairs(values) do if not v or v~=v or math.abs(v)>(i<=3 and 1e7 or 36000) then good=false end end
        if good then s.gear={visible=flag=="1",pose=values,sequence=tonumber(generation),time=now} end
    end
    for id,kind,target,line in text:gmatch("\nEVENT\t(%d+)\t(%a+)\t([^\t\n]+)\t(%d+)") do
        id,line=tonumber(id),tonumber(line)
        if id>s.applied and not s.pending[id] and allowed(target) and (kind=="use" or kind=="advance" or kind=="unroll" or kind=="end" or kind=="interaction" or kind=="scene" or kind=="sequence" or kind=="help" or kind=="item") then
            s.pending[id]={kind=kind,target=target,line=line,since=now}
        end
    end
    local first
    for id in pairs(s.pending) do if not first or id<first then first=id end end
    if not first then return end
    local e=s.pending[first];local component=talkable(e.target)
    local native=component or (allowed(e.target) and StaticFindObject(e.target) or nil)
    if now-e.since>10 then
        print("[StrayCoop/Cooperative] Interaction timed out: "..e.kind.."\n")
        s.pending[first]=nil;s.applied=first;return
    end
    local item_id=e.kind=="item" and Identity.itemResolve(e.target,s.dialog_path)
    if item_id and b then native=b:GetCurrentDialog() end
    if not valid(native) then return end
    if (e.kind=="use" or e.kind=="advance" or e.kind=="unroll" or e.kind=="end") and (not component or not b) then return end
    local done=false
    s.applying=true
    local ok,err=pcall(function()
        local d=b and b:GetCurrentDialog()
        if e.kind=="item" then
            if item_id and valid(d) and d:IsDialogRunning() and d:CanBeShownItems() then
                b.m_inventory:ForEach(function(_,entry)
                    local item=entry:get()
                    if not done and valid(item) and item.ID:ToString()==item_id then d:BeginShowItem(item);done=true end
                end)
            end
        elseif e.kind=="help" then
            if valid(d) and d:IsDialogRunning() then done=true
            elseif not e.requested_start and b:IsB12HelpEnabled() then
                e.requested_start=true;s.remote_start=e.target;b:_OnHelpPressed()
            end
        elseif e.kind=="sequence" then
            if guardian_sequence(e.target) and native:IsA("/Script/Hk_project.Sequence") then
                if native[cue_field[e.target]] then s.sequence_applied[e.target]=true;done=true
                elseif not e.requested_start then
                    e.requested_start=true;s.cue_remote[e.target]=true
                    -- The native proximity caller owns this entry guard. Mirror
                    -- its confirmed value before invoking the graph, preventing
                    -- the receiver's proximity tick from entering a second time.
                    if e.target==guardian_path then native.SeqPlayed=true end
                    native:LogicTrigger()
                end
            else done=true end
        elseif e.kind=="scene" then
            if native:IsA("/Script/LevelSequence.LevelSequenceActor") then
                local player=native:GetSequencePlayer()
                if valid(player) then if not player:IsPlaying() then player:Play() end;done=true end
            else done=true end
        elseif e.kind=="interaction" then
            if native:IsA("/Script/Hk_project.CatMoveToUsableComponent") or native:IsA("/Script/Hk_project.CatPlayAnimationComponent") then
                if native:IsBeingUsed() then done=true
                elseif native:IsEnabled() and native:IsUseEnabled() then cat:StartInteraction(native);done=true end
            else done=true end
        elseif e.kind=="use" then
            if e.target:find(":PersistentLevel.CoopLocalBackpack.",1,true) then
                if valid(d) and d:IsDialogRunning() and same(b:GetCurrentInteraction(),component) then done=true
                elseif not e.requested_start and b:IsB12HelpEnabled() then
                    e.requested_start=true;s.remote_start=e.target;b:_OnHelpPressed()
                end
                return
            end
            if b:IsWithinDialog() and valid(d) and d:IsDialogRunning() and s.active_target~=e.target then
                if s.own==1 then done=true;return end
                -- Resolve simultaneous starts consistently in favor of host.
                b:StopUse()
            end
            if same(b:GetCurrentInteraction(),component) and b:IsWithinDialog() then
                if valid(d) and d:IsDialogRunning() then done=true
                elseif valid(d) and e.started and now-e.started>1 and not e.requested_start
                    and same(d:GetCat(),cat) then
                    -- StartInteraction has established this dialogue for our cat.
                    -- The peer already confirmed it is running; request the same
                    -- public lifecycle entry once, without internal state writes.
                    e.requested_start=true;d:StartDialog()
                end
            elseif not b:IsWithinDialog() and not pc:IsIngameCinematicModeEnabled() then
                s.remote_start=e.target;e.started=now
                -- The initiator has actually entered this native conversation.
                -- Match its usable state rather than waiting for a local prompt.
                component:SetEnabled(true);component:SetUseEnabled(true)
                cat:StartInteraction(component)
            end
        elseif e.kind=="end" and not valid(d) then done=true
        elseif valid(d) and s.active_target==e.target then
            if e.kind=="end" then
                if (s.lines[e.target] or 0)>=e.line then d:EndDialog();done=true end
            elseif e.kind=="unroll" then
                if (s.lines[e.target] or 0)>e.line then done=true
                elseif (s.lines[e.target] or 0)==e.line then
                    if not d:IslineTextDisplayed() then d:RequestNextDialogLine() end;done=true
                end
            elseif (s.lines[e.target] or 0)>e.line then done=true
            elseif (s.lines[e.target] or 0)==e.line and d:IslineTextDisplayed() then
                d:RequestNextDialogLine();done=true
            end
        end
    end)
    s.applying=false
    if not ok then error(err) end
    if done then s.pending[first]=nil;s.applied=first end
end

function M.render(record,cat,pc,now)
    local s=StrayCoopCooperative
    local info=s and s.gear
    local actor=record.gear_name and StaticFindObject(record.gear_name)
    if not info or not info.visible or now-info.time>2 then
        if valid(actor) then actor:SetActorHiddenInGame(true) end
        return
    end
    local local_b=backpack(cat)
    local mesh=local_b and local_b:GetMesh()
    if not valid(mesh) then return end
    if not valid(actor) then
        actor=cat:GetWorld():SpawnActor(StaticFindObject("/Script/Engine.SkeletalMeshActor"),{X=info.pose[1],Y=info.pose[2],Z=info.pose[3]},{Pitch=0,Yaw=0,Roll=0})
        if not valid(actor) then return end
        record.gear_name=path(actor);record.gear_history={}
        actor:SetActorEnableCollision(false);actor:SetActorTickEnabled(false)
        local v=actor.SkeletalMeshComponent
        v:SetSkeletalMesh(mesh.SkeletalMesh,true);v:SetCollisionEnabled(0);v:SetGenerateOverlapEvents(false);v:SetSimulatePhysics(false)
        for i=0,mesh:GetNumMaterials()-1 do v:SetMaterial(i,mesh:GetMaterial(i)) end
    end
    local history=record.gear_history
    Interpolation.push(history,{t=info.time,sequence=info.sequence,pose=info.pose,animation={0,"-",1,-1,1}})
    local p=Interpolation.sample(history,now)
    if p then
        p=Interpolation.display(history,p,now)
        actor:SetActorHiddenInGame(false)
        actor.SkeletalMeshComponent:K2_SetWorldLocationAndRotation({X=p[1],Y=p[2],Z=p[3]},{Pitch=p[4],Yaw=p[5],Roll=p[6]},false,{},true)
    end
end
function M.destroy(record)
    local actor=record.gear_name and StaticFindObject(record.gear_name)
    if valid(actor) then actor:K2_DestroyActor() end
    record.gear_name=nil;record.gear_history=nil
end
function M.start()
    if StrayCoopCooperative and StrayCoopCooperative.running then return end
    local s={running=true,counter=0,events={},pending={},applied=0,lines={},sequence_started={},sequence_applied={},errors=0}
    StrayCoopCooperative=s
    if not StrayCoopCooperativeHooks then
        RegisterHook("/Script/Hk_project.Backpack:Use",function(context,usable)
            local state=StrayCoopCooperative;local target=usable:get()
            if state and path(context:get())==state.backpack_path and valid(target)
                and target:IsA("/Script/Hk_project.TalkableComponent") and allowed(path(target)) then
                local id=target_id(path(target));state.active_target=id;state.lines[id]=0
                -- Wait for native DialogBegan before asking the peer to join.
            end
        end)
        RegisterHook("/Script/Hk_project.Backpack:_OnDialogBegan",function(context,component)
            local state=StrayCoopCooperative;local target=component:get()
            if state and path(context:get())==state.backpack_path and valid(target) and allowed(path(target)) then
                local d=context:get():GetCurrentDialog();state.dialog_path=path(d)
                local id=target_id(path(target));state.active_target=id;state.lines[id]=0
                state.observed_dialog=path(d)
                if state.remote_start==id then state.remote_start=nil else emit("use",id,0) end
            end
        end)
        RegisterHook("/Script/Hk_project.Backpack:_OnDialogLineBegan",function(context)
            local state=StrayCoopCooperative
            if state and path(context:get())==state.backpack_path and state.active_target then
                local id=state.active_target;state.lines[id]=(state.lines[id] or 0)+1
            end
        end)
        RegisterHook("/Script/Hk_project.Dialog:RequestNextDialogLine",function(context)
            local state=StrayCoopCooperative
            if state and path(context:get())==state.dialog_path then
                emit(context:get():IslineTextDisplayed() and "advance" or "unroll",state.active_target,state.lines[state.active_target] or 0)
            end
        end)
        RegisterHook("/Script/Hk_project.Backpack:_OnHelpPressed",function(context)
            local state=StrayCoopCooperative;local b=context:get()
            if state and path(b)==state.backpack_path then
                local d=b:GetCurrentDialog();local raw=path(b:GetCurrentInteraction())
                local id=Identity.encode(raw,path(b),path(d))
                if id and id~=raw then emit("help",id,0) end
            end
        end)
        RegisterHook("/Script/Hk_project.Dialog:BeginShowItem",function(context,lootable)
            local state=StrayCoopCooperative;local item=lootable:get()
            if state and path(context:get())==state.dialog_path and valid(item) and item:IsA("/Script/Hk_project.LootableComponent") then
                emit("item",Identity.itemEncode(state.dialog_path,item.ID:ToString()),state.lines[state.active_target] or 0)
            end
        end)
        RegisterHook("/Script/Hk_project.Dialog:EndDialog",function(context)
            local state=StrayCoopCooperative
            if state and path(context:get())==state.dialog_path then emit("end",state.active_target,state.lines[state.active_target] or 0) end
        end)
        RegisterHook("/Script/Hk_project.CatPawn:StartInteraction",function(context,component)
            local state=StrayCoopCooperative;local target=component:get()
            if state and path(context:get())==state.cat_path and valid(target) and
                (target:IsA("/Script/Hk_project.CatMoveToUsableComponent") or target:IsA("/Script/Hk_project.CatPlayAnimationComponent")) then
                emit("interaction",path(target),0)
            end
        end)
        StrayCoopCooperativeHooks=true
    end
    schedule_loop(50,function()
        if not s.running then return true end
        if s.queued then return false end;s.queued=true
        game_thread(function()
            if s.running then
                local ok,err=pcall(tick,s)
                if not ok then s.errors=s.errors+1;if s.errors<=3 then print("[StrayCoop/Cooperative] ERROR: "..tostring(err).."\n") end end
            end
            s.queued=false
        end)
        return false
    end)
end
function M.stop() if StrayCoopCooperative then StrayCoopCooperative.running=false end end
return M
