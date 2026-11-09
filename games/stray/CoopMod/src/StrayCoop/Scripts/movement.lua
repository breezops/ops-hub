-- Remote cat visuals; the separate story module handles limited opening progress.
-- Only bounded numeric TSV state from bridge.py is accepted. No code evaluation.
local M = {}
local Animation = dofile("Mods/StrayCoop/Scripts/animation.lua")
local Interpolation = dofile("Mods/StrayCoop/Scripts/interpolation.lua")
local Context = dofile("Mods/StrayCoop/Scripts/context.lua")
local directory = "Mods/StrayCoop/"
local function log(s) print("[StrayCoop/Movement] " .. tostring(s) .. "\n") end
local function valid(o) return o ~= nil and o:IsValid() end
local function resolve(name)
    if not name then return nil end
    local o = StaticFindObject(name:match("^%S+ (.+)$") or name)
    if valid(o) then return o end
end

local function local_cat()
    return Context.player()
end

local function split(line)
    local parts = {}
    for part in (line .. "\t"):gmatch("(.-)\t") do parts[#parts+1] = part end
    return parts
end

local function finite(s, limit)
    local n = tonumber(s)
    if not n or n ~= n or math.abs(n) > limit then return nil end
    return n
end

local function publish(state, cat, pc)
    state.sequence = state.sequence + 1
    local row = {"STRAYCOOP", "6", tostring(state.sequence), "-", "-1", "-1"}
    if cat then
        local position = cat:K2_GetActorLocation()
        local rotation = cat:K2_GetActorRotation()
        local velocity = cat:GetVelocity()
        local world = cat:GetWorld()
        row[4] = world:GetFullName():match("^World (.-)%.") or "-"
        row[4] = Context.scene(row[4])
        local instance = Context.singleton("HKGameInstance")
        if valid(instance) then
            row[5] = tostring(instance:GetCurrentAdventureChapter())
            row[6] = tostring(instance:GetCurrentAdventureChapterState(tonumber(row[5])))
        end
        for _, v in ipairs({position.X, position.Y, position.Z, rotation.Pitch, rotation.Yaw, rotation.Roll,
                            velocity.X, velocity.Y, velocity.Z}) do
            row[#row+1] = string.format("%.6f", v)
        end
        state.local_name = cat:GetFullName()
        state.world_name = world:GetFullName()
    else
        for _=1,9 do row[#row+1] = "0" end
        state.local_name = nil
    end
    row[#row+1] = tostring(state.meows)
    if cat then
        local visual = cat.m_visual
        local p, r = visual:K2_GetComponentLocation(), visual:K2_GetComponentRotation()
        for _, v in ipairs({p.X,p.Y,p.Z,r.Pitch,r.Yaw,r.Roll}) do row[#row+1] = string.format("%.6f",v) end
        local animation = Animation.capture(cat)
        for _, value in ipairs(animation) do row[#row+1] = tostring(value) end
        state.animation_mode = animation[1]
        state.animation_asset = animation[2]
        state.animation_counts = state.animation_counts or {}
        state.animation_counts[animation[1]] = (state.animation_counts[animation[1]] or 0) + 1
    else
        for _=1,6 do row[#row+1] = "0" end
        for _, v in ipairs({0,"-",1,-1,1}) do row[#row+1] = tostring(v) end
    end
    row[#row+1]=string.format("%.6f",pc and Context.gameplay():GetRealTimeSeconds(pc) or 0)
    if os.time() >= (state.pause_publish_until or 0) then
        local f = io.open(directory .. "local.tsv", "w")
        if f then f:write(table.concat(row, "\t") .. "\n"); f:close() end
    end
    return row[4], tonumber(row[5])
end

local function destroy(state, id)
    local record = state.remotes[id]
    if record then
        local actor = resolve(record.name)
        if actor then actor:K2_DestroyActor() end
        state.remotes[id] = nil
        log("Removed remote avatar " .. id)
    end
end

local function clear(state)
    local ids = {}
    for id in pairs(state.remotes) do ids[#ids+1] = id end
    for _, id in ipairs(ids) do destroy(state, id) end
end

local function spawn(state, id, cat, position, rotation)
    if state.spawn_world ~= state.world_name then state.spawn_world=state.world_name; state.spawn_count=0 end
    state.spawn_count = (state.spawn_count or 0) + 1
    assert(state.spawn_count <= 8, "Remote spawn limit reached; stop and inspect object lookup")
    -- A decorative mesh must never register as a second gameplay CatPawn, spawn
    -- a camera/scene capture, possess a controller, or participate in streaming.
    local class=StaticFindObject("/Script/Engine.SkeletalMeshActor")
    assert(valid(class),"Native visual actor class unavailable")
    local actor = cat:GetWorld():SpawnActor(class, position, rotation)
    assert(valid(actor), "Unable to spawn remote cat")
    local name = actor:GetFullName()
    -- Disable triggers and simulation so a visual proxy cannot advance local story.
    actor:SetActorEnableCollision(false)
    actor:SetActorTickEnabled(false)
    actor.bShouldDoAnimNotifies=false
    local visual=actor.SkeletalMeshComponent
    visual:SetSkeletalMesh(cat.m_visual.SkeletalMesh,true)
    visual:SetCollisionEnabled(0)
    visual:SetGenerateOverlapEvents(false)
    visual:SetSimulatePhysics(false)
    visual.VisibilityBasedAnimTickOption=0
    visual.bEnableUpdateRateOptimizations=false
    for i=0,cat.m_visual:GetNumMaterials()-1 do visual:SetMaterial(i,cat.m_visual:GetMaterial(i)) end
    state.remotes[id] = {name=name, world=state.world_name, meows=nil}
    log("Spawned remote cat " .. id .. ": " .. name)
    return actor, state.remotes[id]
end

local function receive(state, cat, pc, map, chapter)
    -- A proxy is not a participant in native cinematics. Keeping one alive
    -- across their streaming/animation setup can leave its rendered mesh stale.
    -- Rebuild only at the transition back to stable gameplay, never every tick.
    if pc and pc:IsIngameCinematicModeEnabled() then
        if not state.cinematic_active then
            clear(state)
            log("Removed visual proxies for native cinematic")
        end
        state.cinematic_active=true
        state.cinematic_release_since=nil
        return
    elseif state.cinematic_active then
        state.cinematic_release_since=state.cinematic_release_since or os.time()
        if os.time()-state.cinematic_release_since<2 then return end
        state.cinematic_active=false
        -- This is an intentional lifecycle boundary, separate from lookup failure.
        state.spawn_count=0
        log("Rebuilding visual proxies after native cinematic")
    end
    local function missing_file()
        state.file_missing_since=state.file_missing_since or os.time()
        if os.time()-state.file_missing_since>=5 then clear(state) end
    end
    local f = io.open(directory .. "remote.tsv", "r")
    if not f then missing_file(); return end
    local data = f:read(4097)
    f:close()
    if not data or #data > 4096 or data:sub(-1) ~= "\n" then missing_file(); return end
    local lines = {}
    for line in data:gmatch("[^\r\n]+") do lines[#lines+1] = split(line) end
    local header = lines[1]
    if not header or #header ~= 5 or header[1] ~= "STRAYCOOP" or header[2] ~= "6" then missing_file(); return end
    local own = finite(header[3], 2)
    local generation = finite(header[4], 2^53-1)
    if not own or own < 1 or own % 1 ~= 0 or not generation then missing_file(); return end
    state.file_missing_since=nil
    if state.generation ~= generation then
        state.generation_seen_at = os.time()
        state.generation = generation
    end
    if header[5] ~= "1" or os.time()-(state.generation_seen_at or os.time())>5 or not cat then
        state.missing_since = state.missing_since or os.time()
        if os.time()-state.missing_since >= 5 then clear(state) end
        return
    end
    state.missing_since = nil
    local seen = {}
    for i=2,#lines do
        local p = lines[i]
        local id = finite(p[2], 2)
        -- A live peer may have no fresh game snapshot while paused/loading. Keep
        -- the existing proxy rather than destroy/recreate it on each brief gap.
        if #p==28 and p[1]=="PLAYER" and id and p[4]=="-" and state.remotes[id] then
            seen[id]=true
            local actor=resolve(state.remotes[id].name)
            state.remotes[id].hidden=true
            if actor then actor:SetActorHiddenInGame(true) end
        end
        if #p == 28 and p[1] == "PLAYER" and id and id >= 1 and id % 1 == 0 and id ~= own
            and p[4] == map and finite(p[5], 1000) == chapter then
            local values = {}
            local good = true
            for j=7,15 do
                values[j] = finite(p[j], j <= 9 and 1e7 or (j <= 12 and 36000 or 1e5))
                if not values[j] then good = false end
            end
            local meows = finite(p[16], 2^53-1)
            for j=17,22 do
                values[j] = finite(p[j], j<=19 and 1e7 or 36000)
                if not values[j] then good=false end
            end
            local mode, rate, phase = finite(p[23],6), finite(p[25],4), finite(p[26],1)
            local asset_ok = p[24] == "-" or (#p[24]<=240 and p[24]:match("^/Game/Animation/CAT/[%w_/%.-]+$"))
            if not mode or mode<0 or mode%1~=0 or not rate or rate<0.1 or not phase or phase< -1
                or not asset_ok or (p[27]~="0" and p[27]~="1") then good=false end
            if good and meows then
                seen[id] = true
                local position = {X=values[7], Y=values[8], Z=values[9]}
                local rotation = {Pitch=values[10], Yaw=values[11], Roll=values[12]}
                local record = state.remotes[id]
                local actor = record and record.world == state.world_name and resolve(record.name) or nil
                if not actor then
                    state.remotes[id] = nil
                    actor, record = spawn(state, id, cat, position, rotation)
                end
                record.hidden=false
                local pose={}
                for j=17,22 do pose[#pose+1]=values[j] end
                local sequence=finite(p[3],2^53-1)
                if sequence then
                    Interpolation.push(record, {t=Context.gameplay():GetRealTimeSeconds(pc),source_t=finite(p[28],1e9),
                        sequence=sequence,pose=pose,animation={mode,p[24],rate,phase,tonumber(p[27])}})
                end
                -- Meow counts remain in the protocol. This visual actor has no
                -- gameplay/audio Blueprint; do not call local-cat events here.
                record.meows = meows
            end
        end
    end
    local removed = {}
    for id, record in pairs(state.remotes) do
        if seen[id] then record.missing_since=nil
        else
            record.hidden=true
            local actor=resolve(record.name)
            if actor then actor:SetActorHiddenInGame(true) end
            record.missing_since=record.missing_since or os.time()
            if os.time()-record.missing_since>=2 then removed[#removed+1]=id end
        end
    end
    for _, id in ipairs(removed) do destroy(state, id) end
end

local function render(state)
    local cat,pc=local_cat()
    if not cat or not pc or state.cinematic_active or pc:IsIngameCinematicModeEnabled() then return end
    if Context.gameplay():IsGamePaused(pc) then return end
    local now=Context.gameplay():GetRealTimeSeconds(pc)
    local world=cat:GetWorld():GetFullName()
    for _,record in pairs(state.remotes) do
        if not record.hidden and record.world==world then
            local pose,animation=Interpolation.sample(record,now)
            if pose then pose,animation=Interpolation.display(record,pose,now,animation) end
            local actor=pose and resolve(record.name) or nil
            if actor then
                local visual=actor.SkeletalMeshComponent
                if valid(visual) then
                    actor:SetActorHiddenInGame(false)
                    visual:SetVisibility(true,false)
                    visual:SetHiddenInGame(false,false)
                    visual:K2_SetWorldLocationAndRotation(
                        {X=pose[1],Y=pose[2],Z=pose[3]},
                        {Pitch=pose[4],Yaw=pose[5],Roll=pose[6]},false,{},true)
                    Animation.apply(visual,animation,record)
                end
            end
        end
    end
end

function M.start()
    if StrayCoopMovement and StrayCoopMovement.running then return end
    local state = {running=true, queued=false, sequence=0, remotes={}, meows=0, stale_ticks=0, errors=0}
    StrayCoopMovement = state
    if not StrayCoopMeowHook then
        StrayCoopMeowHook = true
        RegisterHook("/Script/Hk_project.CatPawn:Meow", function(context)
            local current = StrayCoopMovement
            if current and current.running and valid(context:get())
                and context:get():GetFullName() == current.local_name then
                current.meows = current.meows + 1
            end
        end)
    end
    LoopAsync(50, function()
        if not state.running then return true end
        if state.queued then return false end
        state.queued = true
        ExecuteInGameThread(function()
            if state.running then
                local started=os.clock()
                local ok, err = pcall(function()
                    local cat, pc = local_cat()
                    local map, chapter = publish(state, cat, pc)
                    receive(state, cat, pc, map, chapter)
                    local dt=pc and Context.gameplay():GetWorldDeltaSeconds(pc) or 0
                    state.work=state.work or {};state.frames=state.frames or {}
                    state.work[#state.work+1]=(os.clock()-started)*1000
                    if dt>0 and dt<2 then state.frames[#state.frames+1]=dt*1000 end
                    if os.time()>=(state.next_metrics or 0) then
                        state.next_metrics=os.time()+5
                        table.sort(state.work);table.sort(state.frames)
                        local ms=state.work[math.max(1,math.ceil(#state.work/2))] or 0
                        local frame=state.frames[math.max(1,math.ceil(#state.frames/2))] or 0
                        local count=0;for _ in pairs(state.remotes) do count=count+1 end
                        local frame_count,real_time
                        if pc then frame_count,real_time=Context.frame_clock(pc) end
                        local fps=0
                        if frame_count and real_time and state.metric_frame and real_time>(state.metric_time or real_time) then
                            fps=(frame_count-state.metric_frame)/(real_time-state.metric_time)
                        end
                        state.metric_frame=frame_count;state.metric_time=real_time
                        local f=io.open(directory.."performance.tsv","w")
                        if f then
                            f:write(string.format("PERF\t2\t%d\t%.3f\t%.3f\t%d\t%d\t%s\t%.2f\n",os.time(),ms,frame,Context.scans,count,map,fps));f:close()
                        end
                        state.work={};state.frames={}
                    end
                end)
                if not ok then
                    state.errors = state.errors + 1
                    if state.errors <= 3 or state.errors % 100 == 0 then log("ERROR: " .. tostring(err)) end
                    if state.errors >= 10 then state.running = false; log("Stopped after repeated errors") end
                end
            end
            state.queued = false
        end)
        return false
    end)
    -- Poll faster than a 60-Hz frame; the queued guard permits at most one
    -- pending game-thread update and avoids a 16-ms timer/frame beat pattern.
    LoopAsync(8,function()
        if not state.running then return true end
        if state.render_queued then return false end
        state.render_queued=true
        ExecuteInGameThread(function()
            if state.running then
                local ok,err=pcall(render,state)
                if not ok then
                    state.render_errors=(state.render_errors or 0)+1
                    if state.render_errors<=3 then log("Render ERROR: "..tostring(err)) end
                    if state.render_errors>=10 then state.running=false;log("Stopped after repeated render errors") end
                end
            end
            state.render_queued=false
        end)
        return false
    end)
    log("Movement started (20 Hz snapshots, buffered frame-level visual updates).")
end

function M.stop()
    if StrayCoopMovement then
        StrayCoopMovement.running = false
        clear(StrayCoopMovement)
        local f = io.open(directory .. "local.tsv", "w")
        if f then f:close() end
    end
    log("Movement prototype stopped")
end
return M
