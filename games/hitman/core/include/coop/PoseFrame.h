#pragma once
#include "Math.h"

namespace coop {
inline Quat Multiply(Quat a, Quat b) {
    return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
            a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
            a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
            a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
inline Quat InverseRotation(Quat q) {
    q = Normalize(q);
    return {-q.x, -q.y, -q.z, q.w};
}
inline Vec3 Rotate(Quat q, Vec3 v) {
    q = Normalize(q);
    const auto r = Multiply(Multiply(q, {v.x,v.y,v.z,0}), InverseRotation(q));
    return {r.x,r.y,r.z};
}

// DirectX row-vector affine matrix. Strip positive scale, reject invalid or
// sheared frames rather than applying a guessed orientation to a live rig.
inline bool PoseWorldRotation(const float* m, Quat& out) {
    for (int i = 0; i < 16; ++i)
        if (!std::isfinite(m[i])) return false;
    if (std::fabs(m[3]) > .001f || std::fabs(m[7]) > .001f ||
        std::fabs(m[11]) > .001f || std::fabs(m[15]-1.f) > .001f) return false;
    Vec3 rows[3] {{m[0],m[1],m[2]}, {m[4],m[5],m[6]}, {m[8],m[9],m[10]}};
    for (auto& row : rows) {
        const auto len = Length(row);
        if (len < 1e-5f || !std::isfinite(len)) return false;
        row = row * (1.f/len);
    }
    if (std::fabs(Dot(rows[0],rows[1])) > .01f ||
        std::fabs(Dot(rows[0],rows[2])) > .01f ||
        std::fabs(Dot(rows[1],rows[2])) > .01f) return false;
    const Vec3 cross {rows[0].y*rows[1].z-rows[0].z*rows[1].y,
                      rows[0].z*rows[1].x-rows[0].x*rows[1].z,
                      rows[0].x*rows[1].y-rows[0].y*rows[1].x};
    if (Dot(cross,rows[2]) < .99f) return false;
    const float a=rows[0].x, b=rows[0].y, c=rows[0].z;
    const float d=rows[1].x, e=rows[1].y, f=rows[1].z;
    const float g=rows[2].x, h=rows[2].y, i=rows[2].z;
    Quat q;
    if (a+e+i > 0) {
        const float s=2.f*std::sqrt(a+e+i+1.f);
        q={(f-h)/s,(g-c)/s,(b-d)/s,s*.25f};
    } else if (a>e && a>i) {
        const float s=2.f*std::sqrt(1.f+a-e-i);
        q={s*.25f,(b+d)/s,(c+g)/s,(f-h)/s};
    } else if (e>i) {
        const float s=2.f*std::sqrt(1.f+e-a-i);
        q={(b+d)/s,s*.25f,(f+h)/s,(g-c)/s};
    } else {
        const float s=2.f*std::sqrt(1.f+i-a-e);
        q={(c+g)/s,(f+h)/s,s*.25f,(b-d)/s};
    }
    out=Normalize(q);
    return true;
}
} // namespace coop
