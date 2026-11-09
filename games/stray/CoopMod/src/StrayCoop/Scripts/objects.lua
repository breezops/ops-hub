-- Shared native carryables. Never spawn classes or execute paths from packets.
local M={}
local root="Mods/StrayCoop/"
local Context=dofile(root.."Scripts/context.lua")
local Interpolation=dofile(root.."Scripts/interpolation.lua")
local function valid(o) return o~=nil and o:IsValid() end
local function log(s) print("[StrayCoop/Objects] "..s.."\n") end
local function path(o) return o:GetFullName():match("^%S+ (.+)$") end
local function grabbable(o)
    return valid(o) and o:GetFullName():match("^COMP_Grabbable_C ")~=nil
end
local function resolve(id)
    if not id or #id>360 or not id:match("^/Game/[%w_/]+%.[%w_]+:PersistentLevel%.[%w_]+%.COMP_Grabbable$") then return end
    local o=StaticFindObject(id)
    if grabbable(o) and valid(o:GetOwner()) then return o end
end
local function parts(comp)
    local result={}
    local seen={}
    local function add(o)
        if valid(o) and o:IsA("/Script/Engine.PrimitiveComponent") then
            local full=path(o)
            local name=full and full:match("%.([%w_]+)$")
            if name and not seen[name] then seen[name]=true;result[#result+1]={name=name,object=o} end
        end
    end
    add(comp.BodyComponent)
    local actor=comp:GetOwner()
    -- Buckets have a separately simulated handle and container. Sending only
    -- the grabbed handle leaves the container behind on the other PC.
    if valid(actor) and actor:GetFullName():match("^BP_Bucket_Grabbable") then add(actor.Anse);add(actor.bucket) end
    return result
end
local restore
local function snapshot(state)
    local cat=Context.player()
    if not cat then return end
    local instance=Context.singleton("HKGameInstance")
    local chapter=valid(instance) and instance:GetCurrentAdventureChapter() or -1
    if state.chapter and state.chapter~=chapter then
        for id,r in pairs(state.remotes) do restore(r,resolve(id)) end
        state.local_objects={};state.remotes={};state.effects={};state.applied_fans={}
        state.fan_unblocked={};state.fan_names={};state.last_held=nil;state.next_fans=0
    end
    state.chapter=chapter
    local held=cat.GrabbedObject
    -- The Blueprint can retain its last object reference after an interaction
    -- finishes. Native usable state determines whether it is still carried.
    if grabbable(held) and not held:IsBeingUsed() then held=nil end
    if grabbable(held) then
        local id=path(held)
        if resolve(id) then
            state.last_held=id
            local remote=state.remotes[id]
            if remote and remote.owner==0 then
                -- An optimistic pickup of a free item immediately returns
                -- native simulation to this player while its claim travels.
                remote.release_for_pickup=true
            end
            local record=state.local_objects[id]
            if not record then
                local count=0;local oldest
                for key,r in pairs(state.local_objects) do
                    count=count+1
                    if not r.held and (not oldest or r.t<state.local_objects[oldest].t) then oldest=key end
                end
                if count>=8 and oldest then state.local_objects[oldest]=nil;count=count-1 end
                if count<8 then state.local_objects[id]={t=os.time(),held=true,pending_until=Context.gameplay():GetRealTimeSeconds(cat)+0.5} end
            end
        end
    end
    state.sequence=state.sequence+1
    local rows={"OBJECTS\t1\n"}
    for id,record in pairs(state.local_objects) do
        local comp=resolve(id)
        record.held=comp and grabbable(held) and path(held)==id or false
        if comp then
            if record.held then record.t=os.time() end
            record.last_parts={}
            for _,p in ipairs(parts(comp)) do
                local body=p.object
                local pos,rot,scale=body:K2_GetComponentLocation(),body:K2_GetComponentRotation(),body:K2_GetComponentScale()
                local velocity=body:GetPhysicsLinearVelocity(FName("None"))
                local values={p.name}
                for _,n in ipairs({pos.X,pos.Y,pos.Z,rot.Pitch,rot.Yaw,rot.Roll,scale.X,scale.Y,scale.Z,velocity.X,velocity.Y,velocity.Z}) do
                    values[#values+1]=string.format("%.6f",n)
                end
                values[#values+1]=body:IsSimulatingPhysics(FName("None")) and "1" or "0"
                record.last_parts[#record.last_parts+1]=table.concat(values,"\t")
            end
        end
        for _,tail in ipairs(record.last_parts or {}) do
            rows[#rows+1]="OBJECT\t"..id.."\t"..state.sequence.."\t"..(record.held and "1" or "0").."\t"..tail.."\n"
        end
    end
    if os.time()>=(state.next_fans or 0) then
        state.next_fans=os.time()+5;state.fan_names={}
        for _,fan in ipairs(FindAllOf("BP_VentiloBlokable_C") or {}) do
            if valid(fan) then state.fan_names[#state.fan_names+1]=path(fan) end
        end
    end
    for _,name in ipairs(state.fan_names or {}) do
        local fan=StaticFindObject(name)
        if valid(fan) and not fan["is blocked"] then state.fan_unblocked[name]=true end
        if valid(fan) and fan["is blocked"] and state.fan_unblocked[name] and not state.applied_fans[name] and not state.effects[name] then
            local bucket=fan.bucket
            local id=valid(bucket) and path(bucket)..".COMP_Grabbable" or state.last_held
            if not id or not state.local_objects[id] then id=state.last_held end
            if id and state.local_objects[id] then state.effects[name]=id end
        end
    end
    for name,bucket in pairs(state.effects) do rows[#rows+1]="FAN\t"..name.."\t"..bucket.."\n" end
    local f=io.open(root.."objects-local.tsv","w")
    if f then f:write(table.concat(rows));f:close() end
end
restore=function(record,comp)
    if comp and record.original_use~=nil then comp:SetUseEnabled(record.original_use) end
    if comp and record.original_tick~=nil then comp:GetOwner():SetActorTickEnabled(record.original_tick) end
    if comp and record.component_tick~=nil then comp:SetComponentTickEnabled(record.component_tick) end
    for _,p in pairs(record.parts or {}) do
        local body=StaticFindObject(p.name)
        if valid(body) and valid(body:GetOwner()) and p.original_sim~=nil then body:SetSimulatePhysics(p.original_sim) end
    end
end
local function receive(state)
    local cat,pc=Context.player()
    if not cat or not pc then return end
    local f=io.open(root.."objects-remote.tsv","r")
    local data=f and f:read(96001) or ""
    if f then f:close() end
    local own,generation,connected=data:match("^OBJECTS\t1\t([12])\t(%d+)\t([01])\n")
    if not own or #data>96000 or data:sub(-1)~="\n" then return end
    if generation~=state.generation then state.generation=generation;state.seen=os.time() end
    state.own=tonumber(own)
    state.connected=connected=="1"
    local now=Context.gameplay():GetRealTimeSeconds(pc)
    if state.connected then
        for name,bucket_id in data:gmatch("FAN\t([^\t\r\n]+)\t([^\t\r\n]+)\n") do
            if #name<=340 and name:match("^/Game/[%w_/]+%.[%w_]+:PersistentLevel%.[%w_]+$") then
                local fan=StaticFindObject(name)
                if valid(fan) and fan:GetFullName():match("^BP_VentiloBlokable_C ") and not state.applied_fans[name] then
                    local bucket=resolve(bucket_id)
                    if not fan["is blocked"] and bucket then fan:ReceiveActorBeginOverlap(bucket:GetOwner()) end
                    if fan["is blocked"] then
                        state.applied_fans[name]=true
                        state.remotes[bucket_id]=nil
                        log("Applied shared bucket/fan puzzle result")
                    end
                end
            end
        end
    end
    local count=0
    for line in data:gmatch("[^\r\n]+") do
        if line:sub(1,7)=="OBJECT\t" then
            count=count+1;if count>96 then return end
            local v={};for value in (line.."\t"):gmatch("(.-)\t") do v[#v+1]=value end
            if #v==21 then
                local id,revision,owner,driver,seq,held=v[2],tonumber(v[3]),tonumber(v[4]),tonumber(v[5]),tonumber(v[6]),tonumber(v[7])
                local comp=resolve(id)
                local good=comp and revision and revision>0 and seq and (owner==0 or owner==1 or owner==2)
                    and (driver==1 or driver==2) and (held==0 or held==1) and ((owner~=0)==(held==1))
                    and v[8]:match("^[%w_]+$") and #v[8]<=80 and (v[21]=="0" or v[21]=="1")
                local numbers={}
                for j=9,20 do
                    local n=tonumber(v[j]);local limit=j<=11 and 1e7 or j<=14 and 36000 or j<=17 and 100 or 1e5
                    if not n or n~=n or math.abs(n)>limit then good=false else numbers[#numbers+1]=n end
                end
                local local_claim=state.local_objects[id]
                local pending=local_claim and local_claim.held and owner==0 and now<(local_claim.pending_until or 0)
                if good and driver~=state.own and not pending then
                    local actor=comp:GetOwner()
                    local body=StaticFindObject(path(actor).."."..v[8])
                    if valid(body) and body:IsA("/Script/Engine.PrimitiveComponent") and body:GetOwner():GetFullName()==actor:GetFullName() then
                        local cat=Context.player()
                        if cat and grabbable(cat.GrabbedObject) and path(cat.GrabbedObject)==id then
                            cat:StopInteraction(comp) -- Release a losing optimistic native pickup.
                        end
                        state.local_objects[id]=nil -- Never echo received transforms as local input.
                        local r=state.remotes[id]
                        if not r then
                            r={parts={},original_use=comp:IsUseEnabled(),original_tick=actor:IsActorTickEnabled(),component_tick=comp:IsComponentTickEnabled()}
                            state.remotes[id]=r
                        end
                        r.owner=owner;r.driver=driver;r.held=held
                        comp:SetUseEnabled(owner==0 and r.original_use)
                        comp:SetComponentTickEnabled(false)
                        actor:SetActorTickEnabled(false)
                        local p=r.parts[v[8]]
                        if not p then p={name=path(body),original_sim=body:IsSimulatingPhysics(FName("None"))};r.parts[v[8]]=p end
                        p.scale={X=numbers[7],Y=numbers[8],Z=numbers[9]}
                        p.velocity={X=numbers[10],Y=numbers[11],Z=numbers[12]}
                        p.sim=held==0 and v[21]=="1"
                        Interpolation.push(p,{t=now,sequence=revision,pose={table.unpack(numbers,1,6)},animation={0,"-",1,-1,1}})
                    end
                elseif good and driver==state.own and state.remotes[id] then
                    restore(state.remotes[id],comp);state.remotes[id]=nil
                end
            end
        end
    end
end
local function render(state)
    local cat,pc=Context.player()
    if not cat or not pc then return end
    local utility=Context.gameplay()
    local now=utility:GetRealTimeSeconds(pc)
    local stale=not state.connected or os.time()-(state.seen or 0)>=5
    for id,r in pairs(state.remotes) do
        local comp=resolve(id)
        if stale or r.release_for_pickup then restore(r,comp);state.remotes[id]=nil
        elseif comp and not utility:IsGamePaused(pc) and not pc:IsIngameCinematicModeEnabled() then
            for _,p in pairs(r.parts) do
                local body=StaticFindObject(p.name)
                local pose=Interpolation.sample(p,now)
                if valid(body) and valid(body:GetOwner()) and pose then
                    if p.applied_sim~=p.sim then body:SetSimulatePhysics(p.sim);p.applied_sim=p.sim end
                    body:K2_SetWorldLocationAndRotation({X=pose[1],Y=pose[2],Z=pose[3]},
                        {Pitch=pose[4],Yaw=pose[5],Roll=pose[6]},false,{},true)
                    body:SetWorldScale3D(p.scale)
                    if p.sim then body:SetPhysicsLinearVelocity(p.velocity,false,FName("None")) end
                end
            end
        end
    end
end
function M.start()
    if StrayCoopObjects and StrayCoopObjects.running then return end
    local s={running=true,sequence=0,local_objects={},remotes={},effects={},applied_fans={},fan_unblocked={}}
    StrayCoopObjects=s
    local function loop(ms,key,fn)
        LoopAsync(ms,function()
            if not s.running then return true end
            if s[key] then return false end
            s[key]=true
            ExecuteInGameThread(function()
                if s.running then
                    local ok,err=pcall(fn,s)
                    if not ok then s.errors=(s.errors or 0)+1;if s.errors<=3 then log("ERROR: "..tostring(err)) end end
                end
                s[key]=false
            end)
            return false
        end)
    end
    loop(50,"queued",function(state) snapshot(state);receive(state) end)
    loop(8,"render_queued",render)
    log("Shared carryable tracking started")
end
function M.stop()
    local s=StrayCoopObjects;if not s then return end
    s.running=false
    for id,r in pairs(s.remotes) do restore(r,resolve(id)) end
end
return M
