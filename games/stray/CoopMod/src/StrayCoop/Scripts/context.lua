-- Reuse process-lifetime engine singletons, never retain level actor pointers.
-- Negative actor lookups back off; completed opening lookups stop entirely.
local M=StrayCoopContext or {scans=0,retry={}}
-- UE4SS 3.0.1 may release its only registry anchor for a shared hook coroutine
-- when the first queued callback completes (upstream issue #1345). Retain the
-- executing coroutine in the shared Lua globals; leave normal GC enabled.
StrayCoopThreadRoots=StrayCoopThreadRoots or {}
local function keep_thread()
    local thread=coroutine.running()
    if thread then StrayCoopThreadRoots[thread]=true end
end
keep_thread()
M.actor_miss=M.actor_miss or {}
M.singleton_names=M.singleton_names or {}
local function valid(o) return o~=nil and o:IsValid() end
local function discover(class)
    M.scans=M.scans+1
    return FindFirstOf(class)
end
function M.singleton(class)
    keep_thread()
    local name=M.singleton_names[class]
    local o=name and StaticFindObject(name)
    if valid(o) then return o end
    if os.time()<(M.retry[class] or 0) then return nil end
    M.retry[class]=os.time()+2
    o=discover(class)
    if valid(o) then M.singleton_names[class]=o:GetFullName():match("^%S+ (.+)$");return o end
end
function M.gameplay()
    keep_thread()
    return StaticFindObject("/Script/Engine.Default__GameplayStatics")
end
function M.player()
    local instance=M.singleton("HKGameInstance")
    if not instance then return nil end
    local pc=M.gameplay():GetPlayerController(instance,0)
    if valid(pc) and pc:IsA("/Script/Hk_project.HKPlayerController") and pc:IsLocalController() then
        -- The currently possessed Pawn is updated by travel. GetCat can still
        -- reference the previous cat while the new zone is being initialized.
        if pc:IsWaitingForStreamingAndLoading() then return nil,pc end
        local cat=pc.Pawn
        if valid(cat) and cat:IsA("/Script/Hk_project.CatPawn") then return cat,pc end
        return nil,pc
    end
end
function M.actor(class)
    keep_thread()
    if class=="SEQ_cat_familly_intro_C" and StrayCoopStory and StrayCoopStory.finished then return nil end
    if os.time()<(M.actor_miss[class] or 0) then return nil end
    -- Do not retain even a level actor name as a validity cache across travel:
    -- a pending-destruction object can remain findable before collection.
    local o=discover(class)
    if valid(o) then return o end
    M.actor_miss[class]=os.time()+2
end
function M.scene(world)
    -- A logical matching key, never an asset loaded from a packet. Avoid the
    -- zone manager's travel-sensitive getter while levels are being replaced.
    return world..(StrayCoopStory and StrayCoopStory.opening_active and "/__CoopOpening" or "/__CoopCampaign")
end
function M.settings()
    keep_thread()
    local utility=StaticFindObject("/Script/Engine.Default__GameUserSettings")
    if valid(utility) then
        local s=utility:GetGameUserSettings()
        if valid(s) and s:IsA("/Script/Hk_project.HKGameUserSettings") then return s end
    end
end
function M.frame_clock(pc)
    keep_thread()
    local joy=StaticFindObject("/Script/JoyLibraryRuntime.Default__JoyUtilities")
    if valid(joy) then return joy:GetCurrentFrameNumber(),M.gameplay():GetRealTimeSeconds(pc) end
end

-- Batch all polling/render work onto one queued game-thread callback. The
-- async worker reuses this function and allocates no per-poll Lua closures.
-- Individual modules must not create competing async Lua update loops.
M.timers=M.timers or {}
function M.step(now)
    keep_thread()
    M.in_frame=true
    if M.last_step and now<M.last_step then
        for _,timer in ipairs(M.timers) do timer.due=now end
    end
    M.last_step=now
    local count=#M.timers
    for i=1,count do
        local timer=M.timers[i]
        if not timer.done and now>=timer.due then
            timer.due=now+timer.interval
            local ok,done=pcall(timer.callback)
            if not ok then
                timer.errors=(timer.errors or 0)+1
                if timer.errors<=3 then print("[StrayCoop/Scheduler] "..tostring(done).."\n") end
            elseif done then timer.done=true end
        end
    end
    for i=#M.timers,1,-1 do if M.timers[i].done then table.remove(M.timers,i) end end
    M.in_frame=false
end
function M.run(callback)
    if M.in_frame then return callback() end
    return ExecuteInGameThread(callback)
end
function M.loop(ms,callback)
    local timer={callback=callback,interval=ms/1000,due=0}
    M.timers[#M.timers+1]=timer
    if not M.worker then
        M.worker=true
        local queued=false
        local function frame()
            keep_thread()
            local ok,err=pcall(M.step,os.clock())
            M.in_frame=false;queued=false
            if not ok then print("[StrayCoop/Scheduler] Frame: "..tostring(err).."\n") end
        end
        LoopAsync(8,function()
            if not queued then queued=true;ExecuteInGameThread(frame) end
            return false
        end)
    end
    return timer
end
StrayCoopContext=M
return M
