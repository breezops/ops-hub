-- Opening-scene synchronization only. No network-supplied function or path.
local M={}
local Context=dofile("Mods/StrayCoop/Scripts/context.lua")
local schedule_loop=Context.loop or LoopAsync
local game_thread=Context.run or ExecuteInGameThread
local root="Mods/StrayCoop/"
local fields={"WhiteAndRedCatButton","RedCatButton","BlackCatButton"}
local sleep_path="/Game/Map/_MainGame/00_Intro/Intro_GD.Intro_GD:PersistentLevel.PlayerSleepSpotMoveToAnim.COMP_CatButtonMoveToAnim"
local completion="BndEvt__BP_CatButton_MoveToAnim_COMP_CatButtonMoveToAnim_K2Node_ComponentBoundEvent_0_CatUsableDelegate__DelegateSignature"
local function valid(o) return o~=nil and o:IsValid() end
local function log(s) print("[StrayCoop/Story] "..s.."\n") end
local function bit(mask,index) return math.floor(mask/2^(index-1))%2==1 end
local function tick(state)
    local cat,pc=Context.player()
    if state.sleep and pc and pc:IsIngameCinematicModeEnabled() then state.finished=true end
    local seq=cat and not state.finished and Context.actor("SEQ_cat_familly_intro_C") or nil
    if state.sleep and cat and not valid(seq) then state.finished=true end
    if valid(seq) then state.had_opening=true end
    if cat and pc then
        local cinematic=pc:IsIngameCinematicModeEnabled()
        local locomotion=cat:IsInNormalLocomotion()
        local phase=tostring(cinematic)..":"..tostring(locomotion)
        if phase~=state.phase then
            state.phase=phase
            local p=cat:K2_GetActorLocation()
            log(string.format("Cinematic/locomotion=%s at %.2f,%.2f,%.2f",phase,p.X,p.Y,p.Z))
        end
    end
    local active=valid(seq) and cat~=nil and pc~=nil
    state.opening_active=active
    local mask=0
    local ready=true
    if active then
        for i,field in ipairs(fields) do
            local b=seq[field]
            if valid(b) and (b.MoveToStarted or b.DestinationReached) then mask=mask+2^(i-1) end
            if not valid(b) or not b.DestinationReached then ready=false end
        end
        local sleep=StaticFindObject(sleep_path)
        if mask==7 and valid(sleep) and sleep:IsBeingUsed() and not state.sleep then
            state.sleep=true
            log("Observed native opening sleep start")
        end
    end
    local f=io.open(root.."story-local.tsv","w")
    if f then
        f:write(string.format("OPENING\t%d\t%d\t%d\n",active and 1 or 0,active and mask or 0,active and state.sleep and 1 or 0))
        f:close()
    end
    if not active then return end
    local f=io.open(root.."story-remote.tsv","r")
    if not f then return end
    local text=f:read(128); f:close()
    local generation,connected,enabled,shared,do_sleep=text:match("^OPENING\t(%d+)\t([01])\t([01])\t([0-7])\t([01])\n$")
    if not generation then return end
    if state.generation~=generation then state.generation=generation;state.received=os.time() end
    if connected~="1" or enabled~="1" or os.time()-(state.received or 0)>2 then return end
    shared=tonumber(shared)
    for i,field in ipairs(fields) do
        local b=seq[field]
        if bit(shared,i) and not bit(mask,i) and not state.applied[i] and valid(b) then
            local component=b.COMP_CatButtonMoveToAnim
            -- Let an overlapping real interaction finish instead of interrupting it.
            if valid(component) and not component:IsBeingUsed() then
                b[completion](b,component,cat)
                state.applied[i]=true
                log("Applied shared completion: "..field)
            end
        end
    end
    if do_sleep=="1" and shared==7 and mask==7 and ready and not state.sleep
        and os.time()-(state.sleep_attempted_at or 0)>=2 and (state.sleep_attempts or 0)<3
        and not pc:IsIngameCinematicModeEnabled() and cat:IsInNormalLocomotion() then
        local sleep=StaticFindObject(sleep_path)
        if valid(sleep) and sleep:IsEnabled() and sleep:IsUseEnabled() and not sleep:IsBeingUsed() then
            cat:StartInteraction(sleep)
            state.sleep_attempted_at=os.time()
            state.sleep_attempts=(state.sleep_attempts or 0)+1
            log("Requested shared opening sleep transition")
        end
    end
    local status=tostring(mask)..":"..tostring(state.sleep)
    if state.status~=status then state.status=status;log("Opening mask/sleep="..status) end
end
function M.start(previous)
    if StrayCoopStory and StrayCoopStory.running then return end
    local state={running=true,queued=false,sleep=previous and previous.sleep or false,errors=0,
        applied=previous and previous.applied or {},finished=previous and previous.finished or false}
    StrayCoopStory=state
    schedule_loop(500,function()
        if not state.running then return true end
        if state.queued then return false end
        state.queued=true
        game_thread(function()
            local ok,err=pcall(tick,state)
            if not ok then
                state.errors=state.errors+1
                if state.errors<=5 then log("ERROR "..tostring(err)) end
                if state.errors>=5 then state.running=false end
            end
            state.queued=false
        end)
        return false
    end)
    log("Opening story synchronization enabled")
end
function M.stop() if StrayCoopStory then StrayCoopStory.running=false end end
return M
