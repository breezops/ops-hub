-- Pure snapshot math. Network arrival and visual rendering run independently.
local M = {delay=0.15, max_extrapolation=0.10}
local function lerp(a,b,t) return a+(b-a)*t end
local function angle(a,b,t) return a+((b-a+180)%360-180)*t end

-- A monotone Hermite curve keeps velocity continuous across packet boundaries
-- without overshooting ledges or a jump apex. Uneven sender intervals matter:
-- arrival-time jitter must never become a velocity change in the curve.
local function tangent(left,right,left_dt,right_dt)
    if left*right<=0 then return 0 end
    local w1,w2=2*right_dt+left_dt,right_dt+2*left_dt
    return (w1+w2)/(w1/left+w2/right)
end
local function position(samples,ai,bi,axis,t)
    local a,b=samples[ai],samples[bi]
    local dt=b.t-a.t
    local x,y=a.pose[axis],b.pose[axis]
    local slope=(y-x)/dt
    local m1,m2=slope,slope
    local prior,following=samples[ai-1],samples[bi+1]
    if prior and a.t>prior.t then
        m1=tangent((x-prior.pose[axis])/(a.t-prior.t),slope,a.t-prior.t,dt)
    end
    if following and following.t>b.t then
        m2=tangent(slope,(following.pose[axis]-y)/(following.t-b.t),dt,following.t-b.t)
    end
    local t2,t3=t*t,t*t*t
    local value=(2*t3-3*t2+1)*x+(t3-2*t2+t)*dt*m1
        +(-2*t3+3*t2)*y+(t3-t2)*dt*m2
    return math.max(math.min(x,y),math.min(math.max(x,y),value))
end

function M.push(record, sample)
    local samples=record.samples or {}
    record.samples=samples
    local last=samples[#samples]
    if last and sample.sequence==last.sequence then return false end
    if sample.source_t then
        local arrival=sample.t
        sample.t=sample.source_t
        local offset=arrival-sample.source_t
        if not record.clock_offset or (last and sample.t<=last.t) or math.abs(offset-record.clock_offset)>2 then
            record.clock_offset=offset
        else
            -- Sender timestamps preserve the actual cadence through UDP/file
            -- jitter. Slew the clock estimate, never retime individual packets.
            record.clock_offset=record.clock_offset+math.max(-.002,math.min(.0002,offset-record.clock_offset))
        end
    end
    if last then
        local distance=0
        for i=1,3 do distance=distance+(sample.pose[i]-last.pose[i])^2 end
        if sample.t<=last.t or sample.t-last.t>0.5 or distance>1000^2 then
            samples={};record.samples=samples
        end
    end
    samples[#samples+1]=sample
    while #samples>12 do table.remove(samples,1) end
    return true
end

function M.sample(record, now)
    local samples=record.samples
    if not samples or #samples==0 then return nil end
    local target=now-(record.clock_offset or 0)-M.delay
    local a,b=samples[1],samples[1]
    local ai,bi=1,1
    for i=2,#samples do
        if target<samples[i].t then b=samples[i];bi=i;break end
        a=samples[i];b=a;ai=i;bi=i
    end
    local fraction=0
    if b.t>a.t then fraction=math.max(0,math.min(1,(target-a.t)/(b.t-a.t))) end
    local pose={}
    for i=1,6 do
        if i<=3 and b.t>a.t then pose[i]=position(samples,ai,bi,i,fraction)
        else pose[i]=(i<=3 and lerp or angle)(a.pose[i],b.pose[i],fraction) end
    end
    local anim={table.unpack(a.animation)}
    if a.animation[1]==b.animation[1] and a.animation[2]==b.animation[2] then
        anim[3]=lerp(a.animation[3],b.animation[3],fraction)
        if anim[1]==4 and a.animation[4]>=0 and b.animation[4]>=a.animation[4] then
            anim[4]=lerp(a.animation[4],b.animation[4],fraction)
        end
    end
    -- A short lost/late packet should not stall a walking cat. Bound prediction
    -- and never extend a jump trajectory past its last known airborne sample.
    if a==b and target>a.t and #samples>=2 and anim[1]>=1 and anim[1]<=3 then
        local previous=samples[#samples-1]
        local dt=a.t-previous.t
        if dt>0 and dt<=0.5 then
            local ahead=math.min(target-a.t,M.max_extrapolation)
            for i=1,3 do pose[i]=pose[i]+(a.pose[i]-previous.pose[i])/dt*ahead end
        end
    end
    return pose,anim
end

-- Smooth corrections when a packet arrives after bounded prediction. This
-- runs at render cadence; a real teleport still snaps rather than dragging.
function M.display(record,pose,now,animation)
    local prior=record.display_pose
    local dt=now-(record.display_time or now)
    local distance=0
    if prior then for i=1,3 do distance=distance+(pose[i]-prior[i])^2 end end
    local output={table.unpack(pose)}
    local displayed=animation and {table.unpack(animation)} or nil
    if prior and dt>0 and dt<.25 and distance<500^2 then
        local alpha=1-math.exp(-dt/.025)
        for i=1,6 do output[i]=(i<=3 and lerp or angle)(prior[i],pose[i],alpha) end
        local old=record.display_animation
        if displayed and old and displayed[1]==4 and old[1]==4 and old[2]==displayed[2]
            and old[4]>=0 and displayed[4]>=old[4] then
            displayed[4]=lerp(old[4],displayed[4],alpha)
        end
    end
    record.display_pose=output;record.display_time=now
    record.display_animation=displayed
    if displayed then return output,displayed end
    return output
end
return M
