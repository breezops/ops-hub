#include "SdkWorld.h"
#include "Globals.h"
#include "Log.h"
#include "TypeDump.h"
#include <Glacier/ZTypeRegistry.h>
#include <Glacier/ZActor.h>
#include <Glacier/ZRepository.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

namespace {
std::string LowerExit(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
}

void SdkWorld::QueueTeamExit(const coop::TeamExitRequest& request) {
    if (m_MissionComplete || m_TeamExitAttempted || request.scene != m_Scene.scene) return;
    if (m_TeamExitPending && m_TeamExit.repositoryId == request.repositoryId) return;
    m_TeamExit = request;
    m_TeamExitPending = true;
    m_TeamExitUntil = m_Now + 60.0;
    Notify("Your partner is extracting. Approach the same exit to finish together.");
    CoopLog::Info("exit", "received a same-mission exit request; waiting for a nearby, enabled native exit");
}

void SdkWorld::FindMissionExits() {
    m_NativeExits.clear();
    m_ExitsScanned = true;
    if (!Globals::TypeRegistry || !*Globals::TypeRegistry) return;
    // Local metadata remains useful even if this particular class is absent.
    TypeDump::Write("ZContractsExitEntity ZContractStateEntity", CoopLog::Path() + ".exit-types.txt");
    const STypeID* type = (*Globals::TypeRegistry)->GetTypeID(ZString("ZContractsExitEntity"));
    if (!type || !type->GetTypeInfo() || !type->GetTypeInfo()->IsClass()) {
        CoopLog::Warn("exit", "ZContractsExitEntity reflection not available; automatic native exit is unavailable");
        return;
    }
    const auto* klass = reinterpret_cast<const IClassType*>(type->GetTypeInfo());
    std::ostringstream pins;
    uint32_t exitPin = 0;
    for (uint16_t i = 0; i < klass->m_nInputPins; ++i) {
        const uint32_t id = klass->m_pInputPins[i].m_nPinID;
        pins << std::hex << id << ' ';
        for (const char* name : { "Exit", "TriggerExit", "ExitContract", "CompleteContract" }) {
            if (id == Hash::Crc32(name, std::strlen(name))) exitPin = id;
        }
    }
    CoopLog::Info("exit", "native exit input pins: " + pins.str());
    ForEachSceneEntity([&](uint64_t key, const ZEntityRef& ref) {
        if (!ref.QueryInterface(type)) return;
        const auto* entityType = ref->GetType();
        if (!entityType || !entityType->m_pPropertyData) return;
        NativeExit exit;
        exit.ref = ref;
        exit.pin = exitPin;
        std::unordered_set<std::string> identities;
        std::ostringstream properties;
        for (const auto& property : *entityType->m_pPropertyData) {
            const auto* info = property.GetPropertyInfo();
            if (!info || !info->m_pszPropertyName) continue;
            const auto* field = info->m_propertyInfo.m_Type ? info->m_propertyInfo.m_Type->GetTypeInfo() : nullptr;
            if (!field || !field->pszTypeName) continue;
            const std::string name = info->m_pszPropertyName;
            properties << name << ':' << field->pszTypeName << ' ';
            if (info->m_propertyInfo.m_Flags & EPropertyInfoFlags::E_HAS_GETTER_SETTER) continue;
            const auto* at = reinterpret_cast<const uint8_t*>(ref.m_pObj) + property.m_nPropertyOffset;
            if ((name == "m_bEnabled" || name == "m_bActive" || name == "m_bIsActive") &&
                field->m_nTypeSize == sizeof(bool)) {
                // Every reflected enable/active flag must agree before triggering.
                exit.enabled = (!exit.enabledKnown || exit.enabled) && (*at != 0);
                exit.enabledKnown = true;
            }
            if (field->m_nTypeSize == sizeof(ZRepositoryID) &&
                (LowerExit(name).find("repository") != std::string::npos || LowerExit(name).find("exitid") != std::string::npos ||
                 name == "m_sId" || name == "m_ID") &&
                (std::strcmp(field->pszTypeName, "ZRepositoryID") == 0 || std::strcmp(field->pszTypeName, "ZGuid") == 0)) {
                ZRepositoryID id;
                std::memcpy(&id, at, sizeof(id));
                const std::string value = id.ToString().c_str();
                if (value != "00000000-0000-0000-0000-000000000000") identities.insert(LowerExit(value));
            }
        }
        // Ambiguous identities cannot safely select a partner's exact exit.
        if (identities.size() == 1) exit.repositoryId = *identities.begin();
        CoopLog::Info("exit", "native exit " + std::to_string(key) + " repository " + exit.repositoryId +
            " enabled-known=" + std::to_string(exit.enabledKnown) + " enabled=" + std::to_string(exit.enabled) +
            " properties: " + properties.str());
        m_NativeExits.push_back(std::move(exit));
    });
    CoopLog::Info("exit", std::to_string(m_NativeExits.size()) + " native contract exit entities found");
}

void SdkWorld::UpdateTeamExit(bool synced) {
    if (!synced) m_TeamExitPending = false;
    if (!m_InScene || !IsMissionScene(m_Scene.scene) || m_MissionComplete) return;
    if (LowerExit(m_Scene.scene).find("_scene_mission_polarbear_module_002_b.entity") == std::string::npos) return;
    if (m_ExitScanAt == 0.0) m_ExitScanAt = m_Now + 6.0;
    if (!m_ExitsScanned && m_Now >= m_ExitScanAt) FindMissionExits();
    if (!m_TeamExitPending || m_TeamExitAttempted) return;
    if (m_Now > m_TeamExitUntil || m_TeamExit.scene != m_Scene.scene) {
        m_TeamExitPending = false;
        CoopLog::Warn("exit", "pending exit expired or the mission changed; no native exit was triggered");
        return;
    }
    coop::Transform local;
    if (!GetLocalAvatar(local) || !m_TeamExit.Near(local.pos)) return;
    // Refresh native enabled flags at the time of the request, not just at load.
    if (m_Now < m_ExitScanAt) return;
    m_ExitScanAt = m_Now + 2.0;
    FindMissionExits();
    for (const auto& exit : m_NativeExits) {
        if (!exit.pin || !exit.enabledKnown || !exit.enabled ||
            LowerExit(exit.repositoryId) != LowerExit(m_TeamExit.repositoryId)) continue;
        // Freeform Training is our sole current allowlisted contract. Do not
        // bypass arbitrary missions' additional objectives through a native pin.
        if (LowerExit(m_Scene.scene).find("_scene_mission_polarbear_module_002_b.entity") == std::string::npos) return;
        size_t targets = 0;
        std::vector<coop::NpcState> states;
        ListNpcs(states);
        for (const auto& [id, actor] : m_NpcsById) {
            if (!actor || !actor->m_bContractTarget) continue;
            ++targets;
            const auto state = std::find_if(states.begin(), states.end(), [id](const auto& npc) { return npc.id == id; });
            // An unconscious target is alive; never accept a knockout as a kill.
            if (state == states.end() || state->Alive()) return;
        }
        if (!targets) return;
        const ZEntityRef target = exit.ref;
        const uint32_t pin = exit.pin;
        m_TeamExitAttempted = true;
        m_TeamExitPending = false;
        CoopLog::Info("exit", "triggering the matching enabled native exit with the local player nearby; awaiting real ContractEnd");
        target.SignalInputPin(pin);
        return;
    }
}
