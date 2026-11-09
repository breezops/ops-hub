local M={}
local Context=dofile("Mods/StrayCoop/Scripts/context.lua")
local function valid(o) return o~=nil and o:IsValid() end
local function log(s) print("[StrayCoop/StoryTrace] " .. s .. "\n") end
local function snapshot()
    local seq=Context.actor("SEQ_cat_familly_intro_C")
    if not valid(seq) then return "Opening sequence not loaded" end
    local values={"Flags="..tostring(seq.WhiteAndRedCatDone)..","..tostring(seq.RedCatDone)..","..tostring(seq.BlackCatDone),
        "PlayIntro="..tostring(seq.Play_cine_intro),"Thunder="..tostring(seq.ThunderStopped)}
    for _, name in ipairs({"WhiteAndRedCatButton","RedCatButton","BlackCatButton"}) do
        local b=seq[name]
        if valid(b) then
            values[#values+1]=name..":"..tostring(b.MoveToStarted)..","..tostring(b.DestinationReached)
                ..","..tostring(b.COMP_CatButtonMoveToAnim.AnimStarted)..","..tostring(b.COMP_CatButtonMoveToAnim:IsEnabled())
        end
    end
    return table.concat(values,"; ")
end
function M.start()
    if StrayCoopStoryTrace then return end
    StrayCoopStoryTrace={queued=false}
    local state=StrayCoopStoryTrace
    for _, name in ipairs({"SEQ_cat_familly_intro_C","BP_CatButton_MoveToAnim_FakeCatIntro_C","COMP_CatMoveToAnim_C"}) do
        local object=FindFirstOf(name)
        if valid(object) then
            log("Class="..object:GetClass():GetFullName())
            object:GetClass():ForEachFunction(function(fn)
                local name=fn:GetFName():ToString()
                if name:find("BndEvt",1,true) or name=="BedIdle" or name=="OnStartSequence"
                    or name=="_OnUseStarted" or name=="_OnBeforeUseDone" or name=="_OnAfterMoveUseStarted" then
                    local fn_name=fn:GetFullName()
                    local ok,err=pcall(function()
                        RegisterHook(fn_name,function(context)
                            log("EVENT "..name.." on "..context:get():GetFullName())
                        end)
                    end)
                    if not ok then log("Hook unavailable: "..tostring(err)) end
                end
            end)
        end
    end
    LoopAsync(200,function()
        if state.queued then return false end
        state.queued=true
        ExecuteInGameThread(function()
            local ok,value=pcall(snapshot)
            if ok and state.last~=value then log(value);state.last=value end
            state.queued=false
        end)
        return false
    end)
    log("Opening interaction trace enabled")
end
M.snapshot=snapshot
return M
