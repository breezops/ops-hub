-- Reuse process-lifetime engine singletons, never retain level actor pointers.
-- Negative actor lookups back off; completed opening lookups stop entirely.
local M=StrayCoopContext or {scans=0,retry={}}
M.actor_miss=M.actor_miss or {}
local function valid(o) return o~=nil and o:IsValid() end
local function discover(class)
    M.scans=M.scans+1
    return FindFirstOf(class)
end
function M.singleton(class)
    local o=M[class]
    if valid(o) then return o end
    if os.time()<(M.retry[class] or 0) then return nil end
    M.retry[class]=os.time()+2
    o=discover(class)
    if valid(o) then M[class]=o;return o end
end
function M.gameplay()
    if not valid(M.utility) then M.utility=StaticFindObject("/Script/Engine.Default__GameplayStatics") end
    return M.utility
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
    if not valid(M.settings_utility) then M.settings_utility=StaticFindObject("/Script/Engine.Default__GameUserSettings") end
    if valid(M.settings_utility) then
        local s=M.settings_utility:GetGameUserSettings()
        if valid(s) and s:IsA("/Script/Hk_project.HKGameUserSettings") then return s end
    end
end
function M.frame_clock(pc)
    if not valid(M.joy) then M.joy=StaticFindObject("/Script/JoyLibraryRuntime.Default__JoyUtilities") end
    if valid(M.joy) then return M.joy:GetCurrentFrameNumber(),M.gameplay():GetRealTimeSeconds(pc) end
end
StrayCoopContext=M
return M
