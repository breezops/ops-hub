#include "HitmanCoop.h"

#if defined(COOP_PARTY_FREEFORM_PROBE) || defined(COOP_PARTY_GUEST_START)
#include "CoopText.h"
#include "Log.h"

#include "Functions.h"
#include "Globals.h"

#include <Glacier/EntityFactory.h>
#include <Glacier/SExternalReferences.h>
#include <Glacier/ZModule.h>
#include <Glacier/ZScene.h>
#include <Glacier/ZTypeRegistry.h>
#include <Windows.h>

#ifdef COOP_PARTY_FREEFORM_MENU_START
#include <Glacier/ZContract.h>

namespace {
using NativeStart=void (*)(void*);
constexpr uintptr_t kLaunchRoutine=0xB6E990;
constexpr uintptr_t kNativeWrapperCall=0xB6EE2D;
constexpr uintptr_t kStarterVtable=0x1EB4CA0;
constexpr uintptr_t kContractManager=0x3159900;
constexpr std::array<uint8_t,37> kLaunchEntry{
    0x48,0x8b,0xc4,0x48,0x89,0x48,0x08,0x55,0x41,0x55,0x48,0x8d,0x68,0xb8,
    0x48,0x81,0xec,0x38,0x01,0x00,0x00,0x80,0x3d,0x05,0xb9,0x5e,0x02,0x00,
    0x4c,0x8b,0xe9,0x0f,0x85,0x11,0x07,0x00,0x00
};
constexpr std::array<uint8_t,5> kWrapperCall{0xe8,0xae,0x8c,0xeb,0xff};

template<class T>
bool ReadNative(uintptr_t at,T& value) {
    SIZE_T size=0;
    return at && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(at),
        &value,sizeof(value),&size) && size==sizeof(value);
}

NativeStart ResolveMenuStart(void* starter) {
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto* manager=Globals::ContractsManager;
    std::array<uint8_t,kLaunchEntry.size()> entry{};
    std::array<uint8_t,kWrapperCall.size()> call{};
    uintptr_t vtable=0;
    if (!image || reinterpret_cast<uintptr_t>(manager)!=image+kContractManager ||
        !ReadNative(image+kLaunchRoutine,entry) || entry!=kLaunchEntry ||
        !ReadNative(image+kNativeWrapperCall,call) || call!=kWrapperCall ||
        !ReadNative(reinterpret_cast<uintptr_t>(starter),vtable) || vtable!=image+kStarterVtable) {
        CoopLog::Warn("launch","Native launch routine or entity layout did not match the Steam build.");
        return nullptr;
    }
    const auto& context=manager->m_contractContext;
    if (!context.m_sContractId.IsEmpty() || !context.m_sScene.IsEmpty() || !context.m_sLocationId.IsEmpty()) {
        CoopLog::Warn("launch","Native contract context is occupied. Start from a fresh main menu.");
        return nullptr;
    }
    std::array<uint8_t,2> state{};
    if (!ReadNative(reinterpret_cast<uintptr_t>(starter)+0x38,state) || state[0] || state[1]) {
        CoopLog::Warn("launch","Contract-start entity is already primed or has a pending request.");
        return nullptr;
    }
    return reinterpret_cast<NativeStart>(image+kLaunchRoutine);
}
}
#endif

#ifdef COOP_PARTY_GUEST_START
bool HitmanCoop::NativeFreeformContract() const {
    const auto* manager=Globals::ContractsManager;
    if (!manager || m_GameVersion!="3.290.1") return false;
    const auto& id=manager->m_contractContext.m_sContractId;
    return std::string_view(id.c_str(),id.size())==coop::kPartyFreeformContract;
}
#endif

coop::IPartyFreeformLaunch::Result HitmanCoop::PollFreeformLaunch(double now) {
    using Result=coop::IPartyFreeformLaunch::Result;
    if (m_GameVersion!="3.290.1" || !m_Unsupported.empty()) return Result::Failed;
    const auto scene=m_World.CurrentScene();
    const bool atMenu=(!m_Session || !m_Session->Active()) && !m_World.IsInScene() &&
        m_LevelState==static_cast<int>(ZLevelManager::EGameState::EGS_Playing) &&
        scene.scene=="assembly:/_PRO/Scenes/Frontend/MainMenu.entity";
    if (!atMenu) {
        CancelFreeformLaunch();
        return Result::Waiting;
    }

    if (m_FreeformLaunchDeadline<0.) {
        if (!Globals::ResourceManager || !Globals::EntityManager || !Functions::ZEntityManager_NewEntity->Exists()) {
            CoopLog::Warn("launch","Native resource or entity creation is unavailable.");
            return Result::Failed;
        }
        m_FreeformLaunchDeadline=now+10.;
        if (m_FreeformStartFactory.m_nResourceIndex.val<0)
            Globals::ResourceManager->LoadResource(m_FreeformStartFactory,ResId<"[modules:/zcontractstartentity.class].pc_entitytype">);
        CoopLog::Info("launch","Requested the native contract-start entity factory.");
    }
    const int resourceStatus=m_FreeformStartFactory.m_nResourceIndex.val<0 ? -1 : m_FreeformStartFactory.GetResourceInfo().status;
    if (resourceStatus!=RESOURCE_STATUS_VALID) {
        if (resourceStatus==RESOURCE_STATUS_FAILED || now>=m_FreeformLaunchDeadline) {
            CoopLog::Fmt(coop::LogLevel::Warn,"launch","Contract-start factory did not load, status=%d.",resourceStatus);
            CancelFreeformLaunch();
            return Result::Failed;
        }
        return Result::Waiting;
    }
    CancelFreeformLaunch();

    const auto* type=Globals::TypeRegistry && *Globals::TypeRegistry ?
        (*Globals::TypeRegistry)->GetTypeID(ZString("ZContractStartEntity")) : nullptr;
    const auto* info=type ? type->GetTypeInfo() : nullptr;
    constexpr uint32_t startPin=Hash::Crc32("Start",5);
    bool hasStart=false;
    if (info && info->IsClass()) {
        const auto* klass=reinterpret_cast<const IClassType*>(info);
        for (uint16_t i=0;i<klass->m_nInputPins;++i)
            if (klass->m_pInputPins[i].m_nPinID==startPin) hasStart=true;
    }
    const auto* module=Globals::Hitman5Module;
    const auto* context=module ? module->m_pEntitySceneContext : nullptr;
    if (!hasStart || !context || !context->m_pScene || !m_FreeformStartFactory) {
        CoopLog::Warn("launch","Contract-start pin, factory or live scene is unavailable.");
        return Result::Failed;
    }

    // The current scene owns the entity. A native completion may still need
    // it after Start returns, so let scene teardown retire it.
    ZEntityRef starter;
    SExternalReferences external;
    Functions::ZEntityManager_NewEntity->Call(Globals::EntityManager,starter,ZString("HitmanCoopFreeformStart"),
        m_FreeformStartFactory,context->m_pScene.m_entityRef,external,-1);
    const ZString contract{std::string_view(coop::kPartyFreeformContract)};
    auto* nativeStarter=starter ? starter.QueryInterface(type) : nullptr;
    if (!nativeStarter || !starter.SetProperty(ZString("m_sContractId"),contract)) {
        CoopLog::Warn("launch","Contract-start entity did not accept the local Freeform contract ID.");
        return Result::Failed;
    }
#ifdef COOP_PARTY_FREEFORM_MENU_START
    // Start queues until the previous contract ends. A fresh main menu has
    // no previous contract. Use the same continuation as the event handler.
    const auto start=ResolveMenuStart(nativeStarter);
    if (!start) return Result::Failed;
    CoopLog::Info("launch","Invoking the native contract continuation from a fresh main menu.");
    start(nativeStarter);
    CoopLog::Info("launch","The native continuation returned. Waiting for the scene and contract receipts.");
#else
    CoopLog::Info("launch","Invoking the native Start pin for Freeform Training.");
    starter.SignalInputPin(startPin);
    CoopLog::Info("launch","The native Start pin returned. Waiting for the scene and contract receipts.");
#endif
    return Result::Submitted;
}

#ifdef COOP_PARTY_FREEFORM_PROBE
void HitmanCoop::UpdateFreeformLaunchProbe(double now) {
    DWORD foreground=0;
    GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
    const bool key=foreground==GetCurrentProcessId() && (GetAsyncKeyState(VK_F8)&0x8000);
    const bool pressed=key && !m_FreeformProbeKey;
    m_FreeformProbeKey=key;
    if (m_PartyPreview || (m_Session && m_Session->Active())) {
        CancelFreeformLaunch();
        return;
    }
    if (m_FreeformLaunchDeadline<0. && (!pressed || m_FreeformProbeUsed)) return;
    const auto result=PollFreeformLaunch(now);
    if (result!=coop::IPartyFreeformLaunch::Result::Waiting) m_FreeformProbeUsed=true;
}
#endif
#endif
