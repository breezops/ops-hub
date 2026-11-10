#pragma once

namespace coop {
// An already-presented avatar may remain visible in the engine's native pose
// while an outfit rebuilds. Remote bone writes still require a validated rig.
// Initial spawning and a deliberate stop/leave keep the usual visibility gate.
class AvatarAppearance {
public:
    void BeginOutfitChange(bool alreadyVisible) { m_KeepVisible = m_KeepVisible || alreadyVisible; }
    bool KeepVisible() const { return m_KeepVisible; }
    void RigStable(double now, double matchedAt) {
        // Bodies can rebuild more than once as NPC geometry becomes hero
        // geometry. Keep continuity until the latest match has stayed stable.
        if (now - matchedAt >= 0.5) m_KeepVisible = false;
    }
    void Hide() { m_KeepVisible = false; }
private:
    bool m_KeepVisible = false;
};
}
