-- Visual-only animation snapshot. Assets must already be loaded by the game.
local M = {}
local function valid(o) return o ~= nil and o:IsValid() end
local function path(o)
    if not valid(o) or o.AdditiveAnimType~=0 then return "-" end
    return o:GetFullName():match("^AnimSequence (/Game/Animation/CAT/[%w_/%.-]+)$") or "-"
end
local function asset(folder, name) return "/Game/Animation/CAT/" .. folder .. "/" .. name .. "." .. name end
-- StandBreathing is additive (native AdditiveAnimType=1); using it alone
-- produces a reference body/tail and an open mouth. Stand_Idle is full body.
local idle = asset("Idle", "CAT_Stand_Idle")
local walk = asset("Walk", "CAT_WALK_NEUTRAL_SPEEDNORMAL_LOOP_L")
local trot = asset("Walk", "CAT_TROT_NEUTRAL_NORMAL_LOOP_R")
local run = asset("Run", "CAT_RUN_NEUTRAK_NORMAL_LOOP_FL")
local function clamp(n, a, b) return math.max(a, math.min(b, n)) end

function M.capture(cat)
    local result = {0, idle, 1, -1, 1}
    local anim = cat:GetAnimInstance()
    if not valid(anim) or not anim:IsA("/Game/Character/Cat/BP/ABP_Cat_New.ABP_Cat_New_C") then return result end
    if anim.IsInScriptedJump then
        local p = path(anim.ScriptedJumpAnimation)
        if p ~= "-" then return {4,p,1,clamp(cat:GetJumpTrajectoryMotionRatio(),0,1),0} end
    elseif anim.IsJumping then
        local block = anim.IsJumpAnimA and anim.JumpBlockA or anim.JumpBlockB
        local p = path(block.Data.JumpAnimation)
        if p ~= "-" then return {4,p,clamp(anim.JumpPlayRate,0.1,4),clamp(cat:GetJumpTrajectoryMotionRatio(),0,1),0} end
    elseif anim.IsCustomAnimationEnabled then
        local custom = anim.IsInCustomAnimationA and anim.CustomAnimationA or anim.CustomAnimationB
        local p = path(custom.Animation)
        if p ~= "-" then return {5,p,clamp(custom.PlayRate,0.1,4),-1,custom.bIsLooping and 1 or 0} end
    end
    local speed = math.abs(anim["Cat Speed"])
    if anim.IsMoving or speed > 5 then
        if anim.IsRunning then result = {3,run,clamp(speed/400,0.5,2),-1,1}
        elseif speed > 150 then result = {2,trot,clamp(speed/180,0.5,2),-1,1}
        else result = {1,walk,clamp(speed/100,0.5,2),-1,1} end
    elseif anim.IsPerched then result = {6,asset("Idle","CAT_Perch_Idle"),1,-1,1}
    end
    return result
end

function M.apply(visual, snapshot, record)
    local mode, name, rate, phase, looping = table.unpack(snapshot)
    if name == "-" then return end
    local changed=record.animation~=name or record.animation_loop~=looping or visual:GetAnimationMode()~=1
    local sequence=changed and StaticFindObject(name) or nil
    if changed and (not valid(sequence) or not sequence:IsA("/Script/Engine.AnimSequence")) then
        if record.missing_animation ~= name then
            print("[StrayCoop/Animation] Asset not loaded: " .. name .. "\n")
            record.missing_animation = name
        end
        return
    end
    if changed then
        visual:PlayAnimation(sequence, looping == 1)
        record.animation, record.animation_loop = name, looping
        record.animation_switches = (record.animation_switches or 0) + 1
        record.sequence_length=sequence.SequenceLength
    end
    local visual_rate=(mode==4 and phase>=0) and 0 or rate
    if changed or not record.applied_rate or math.abs(record.applied_rate-visual_rate)>.02 then
        visual:SetPlayRate(visual_rate)
        record.applied_rate=visual_rate
    end
    -- Jump trajectories are sampled from the owning cat. Do not execute anim notifies.
    if mode == 4 and phase >= 0 then visual:SetPosition(phase * (record.sequence_length or 0), false) end
    record.animation_mode, record.animation_rate = mode, rate
    record.animation_phase = phase
end
return M
