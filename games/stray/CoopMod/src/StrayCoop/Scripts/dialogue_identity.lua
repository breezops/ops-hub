-- Only the local backpack's generated external talkable needs an alias.
local M={}
local prefix='/Game/Map/_MainGame/BaseMap.BaseMap:PersistentLevel.CoopLocalBackpack.'
function M.encode(target,backpack,dialog)
    if target and backpack and dialog and target==backpack..'.externalTalkable' then
        local actor=dialog:match(':PersistentLevel%.([%w_]+)$')
        if actor then return prefix..actor..'.externalTalkable' end
    end
    return target
end
function M.resolve(target,backpack,dialog)
    if type(target)~='string' or target:sub(1,#prefix)~=prefix then return target end
    local actor=target:sub(#prefix+1):match('^([%w_]+)%.externalTalkable$')
    if actor and backpack and dialog and dialog:match(':PersistentLevel%.([%w_]+)$')==actor then
        return backpack..'.externalTalkable'
    end
end
function M.itemEncode(dialog,id)
    local actor=dialog and dialog:match(':PersistentLevel%.([%w_]+)$')
    if actor and type(id)=='string' and #id<=64 and id:match('^[%w_]+$') then
        return '/Game/Map/_MainGame/BaseMap.BaseMap:PersistentLevel.CoopShowItem.'..actor..'.'..id
    end
end
function M.itemResolve(target,dialog)
    if type(target)~='string' then return end
    local actor,id=target:match('^/Game/Map/_MainGame/BaseMap%.BaseMap:PersistentLevel%.CoopShowItem%.([%w_]+)%.([%w_]+)$')
    if actor and dialog and dialog:match(':PersistentLevel%.([%w_]+)$')==actor then return id end
end
return M
