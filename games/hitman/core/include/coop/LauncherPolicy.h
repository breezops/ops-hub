#pragma once
#include <algorithm>
#include <cctype>
#include <string>

namespace coop {
inline bool IsFreeformTrainingScene(std::string scene) {
    std::transform(scene.begin(), scene.end(), scene.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return scene == "assembly:/_pro/scenes/missions/thefacility/_scene_mission_polarbear_module_002_b.entity";
}
inline bool BlockLauncherFocusPause(bool windowActive, bool launcherRecent, bool pauseMenu) {
    return !windowActive && launcherRecent && pauseMenu;
}
inline const char* LauncherReadyState(bool synced, double now, double asked, double incoming, double acknowledged, double replied) {
    if (!synced) return "unavailable";
    if (now - incoming < 60.0) return "incoming";
    if (now - asked < 60.0) return "waiting";
    if (now - acknowledged < 60.0) return "acknowledged";
    if (now - replied < 60.0) return "replied";
    if (asked >= 0) return "expired";
    return "idle";
}
}
