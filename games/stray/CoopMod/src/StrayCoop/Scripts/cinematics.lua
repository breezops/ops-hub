-- Scene-scoped shared skips. Scripted scenes finish through their own callbacks.
local M={}
local root="Mods/StrayCoop/"
local Context=dofile(root.."Scripts/context.lua")
local function valid(o) return o~=nil and o:IsValid() end
local function log(s) print("[StrayCoop/Cinematic] "..s.."\n") end
local function restore(s,pc)
    local world=pc:GetWorld():GetFullName()
    for name,old in pairs(s.sequences or {}) do
        if old.world==world then
            local p=StaticFindObject(name)
            if valid(p) then p:SetPlayRate(old.rate) end
        end
    end
    s.sequences={}
    if s.dilation then
        Context.gameplay():SetGlobalTimeDilation(pc,s.dilation)
        s.dilation=nil
        log("Restored normal cinematic clock")
    end
end
local function accelerate(s,pc,world,now)
    local utility=Context.gameplay()
    if not s.dilation then s.dilation=utility:GetGlobalTimeDilation(pc) end
    -- Some native scene setup resets the world clock after the first frame.
    if utility:GetGlobalTimeDilation(pc)<19 then utility:SetGlobalTimeDilation(pc,20) end
    if now<(s.scan_at or 0) then return end
    s.scan_at=now+.25;s.sequences=s.sequences or {}
    -- Sequencer can use an undilated clock. Its play rate must advance too;
    -- keeping it playing preserves OnFinished and all intermediate events.
    for _,p in ipairs(FindAllOf("LevelSequencePlayer") or {}) do
        if valid(p) and p:IsPlaying() and valid(p.Sequence) and p.PlaybackSettings.LoopCount.Value==0 then
            local owner=p:GetOuter()
            if valid(owner) and owner:IsA("/Script/Engine.Actor") and owner:GetWorld():GetFullName()==world then
                local name=p:GetFullName():match("^%S+ (.+)$")
                if not s.sequences[name] then s.sequences[name]={world=world,rate=p:GetPlayRate()} end
                p:SetPlayRate(20)
            end
        end
    end
end
local function release(s,pc)
    if s.waiting then
        local pause=StrayCoopPause
        if pause and (pause.requested or pause.remote) then pause.owned=true
        else Context.gameplay():SetGamePaused(pc,false) end
        s.waiting=nil
        log("Shared skip barrier released")
    end
end
local function tick(s)
    local _,pc=Context.player()
    if not pc then return end
    local utility=Context.gameplay()
    local now=utility:GetRealTimeSeconds(pc)
    local world=pc:GetWorld():GetFullName()
    if s.world and s.world~=world then restore(s,pc);release(s,pc) end
    s.world=world
    local cinematic=pc:IsIngameCinematicModeEnabled()
    if pc:IsWaitingForStreamingAndLoading() then
        restore(s,pc)
        return -- Never accelerate a streamed world before its controller is ready.
    end
    if cinematic and not s.active then
        release(s,pc)
        local instance=Context.singleton("HKGameInstance")
        local chapter=valid(instance) and instance:GetCurrentAdventureChapter() or -1
        local scope="/Game/StrayCoop/Cinematics/Chapter_"..tostring(chapter):gsub("-","M")
        s.counts[scope]=(s.counts[scope] or 0)+1
        s.active={asset=scope,count=s.counts[scope]}
        s.intent={scope,s.counts[scope],0,1};s.hold=nil;s.release_key=true
        log("Scene "..scope.." #"..s.counts[scope].." (hold Space to skip)")
    elseif not cinematic and s.active then
        local active=s.active
        restore(s,pc);s.active=nil;s.hold=nil;s.intent[4]=0
        if active.applied then
            s.barrier={asset=active.asset,count=active.count,since=now}
            if not utility:IsGamePaused(pc) then
                s.waiting=utility:SetGamePaused(pc,true)
            end
            log("Completed shared skip; waiting for peer completion")
        end
    end
    local active=s.active
    local down=pc:IsInputKeyDown({KeyName=FName("SpaceBar",EFindName.FNAME_Add)})
    local paused=utility:IsGamePaused(pc)
    if not down then s.release_key=false;s.hold=nil end
    if paused then s.hold=nil end
    if active and not s.release_key and down and not paused then
        s.hold=s.hold or now
        if now-s.hold>=.8 then s.intent[3]=1 end
    end
    -- Keep intent flowing during engine pause and after local completion.
    if now>=(s.io_at or 0) then
        s.io_at=now+.05
        local f=io.open(root.."cinematic-local.tsv","w")
        if f then f:write("CINEMATIC\t"..table.concat(s.intent,"\t").."\n");f:close() end
        f=io.open(root.."cinematic-remote.tsv","r")
        s.remote=f and f:read(1301) or "";if f then f:close() end
    end
    local text=s.remote or ""
    local generation,connected=text:match("^SKIPS\t(%d+)\t([01])\n")
    if generation and generation~=s.generation then s.generation=generation;s.seen=now end
    local live=connected=="1" and now-(s.seen or -100)<2
    local authorized,finished=false,false
    for line in text:gmatch("[^\n]+") do
        local a,c,phase=line:match("^SKIP\t(/Game/[%w_/%.-]+)\t(%d+)\t([12])$")
        local scope=active or s.barrier
        if scope and a==scope.asset and tonumber(c)==scope.count then
            authorized=true;finished=phase=="2"
        end
    end
    if s.barrier and (not live or finished or now-s.barrier.since>60) then
        release(s,pc);s.barrier=nil
    end
    if not active then return end
    if active.applied then
        if not live or paused or now-(active.started or now)>60 then
            restore(s,pc)
        elseif active.fast then
            accelerate(s,pc,world,now)
        end
        return
    end
    if not live or not authorized or paused then return end
    active.applied=true;active.started=now
    -- Stray mixes Sequencer, latent Blueprint actions and scripted rail motion.
    -- GoToEndAndStop stranded the morning scene in the native fixture: its
    -- normal completion delegate did not run. Keep every driver playing, and
    -- accelerate the cinematic clock so their natural callbacks still execute.
    active.fast=true
    accelerate(s,pc,world,now)
    log("Finishing shared cinematic through native callbacks")
end
function M.start()
    if StrayCoopCinematics and StrayCoopCinematics.running then return end
    local s={running=true,counts={},intent={"-",0,0,0},errors=0}
    StrayCoopCinematics=s
    LoopAsync(16,function()
        if not s.running then return true end
        if s.queued then return false end;s.queued=true
        ExecuteInGameThread(function()
            if s.running then
                local ok,err=pcall(tick,s)
                if not ok then
                    s.errors=s.errors+1
                    local _,pc=Context.player()
                    if pc then pcall(restore,s,pc);pcall(release,s,pc) end
                    if s.errors<=3 then log("ERROR: "..tostring(err)) end
                end
            end
            s.queued=false
        end)
        return false
    end)
end
function M.stop()
    local s=StrayCoopCinematics
    if not s then return end
    s.running=false
    local _,pc=Context.player()
    if pc then restore(s,pc);release(s,pc) end
end
return M
