#include "ContractLaunchTrace.h"

#ifdef COOP_PARTY_CONTRACT_TRACE
#include "Log.h"
#include "coop/PartyRoom.h"

#include <Windows.h>
#include <MinHook.h>
#include <intrin.h>

#include <array>
#include <atomic>
#include <cstring>
#include <string_view>

namespace {
constexpr uintptr_t kWrapperRva=0xA27AE0;
constexpr uintptr_t kManagerCallRva=0xA28994;
constexpr std::array<uint8_t,20> kWrapperEntry={
    0x4c,0x89,0x4c,0x24,0x20, 0x4c,0x89,0x44,0x24,0x18,
    0x48,0x89,0x54,0x24,0x10, 0x48,0x89,0x4c,0x24,0x08
};
constexpr std::array<uint8_t,5> kManagerCall={0xe8,0x87,0x12,0xf3,0xff};
constexpr std::string_view kContract=coop::kPartyFreeformContract;

using StartWrapper=void (*)(void*, const void*, void*, void*);
std::atomic<StartWrapper> g_Original{};
uintptr_t g_Image=0;
std::atomic<uint64_t> g_Receipt{};
std::atomic<unsigned> g_Calls{};

bool Read(uintptr_t address, void* output, size_t size) {
    SIZE_T bytes=0;
    return address && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),output,size,&bytes) &&
        bytes==size;
}

bool IsFreeform(const void* id) {
    struct StringView { uint32_t length; uint32_t padding; uintptr_t data; } value{};
    std::array<char,kContract.size()> text{};
    return Read(reinterpret_cast<uintptr_t>(id),&value,sizeof(value)) &&
        (value.length & 0x3fffffff)==text.size() && Read(value.data,text.data(),text.size()) &&
        std::string_view(text.data(),text.size())==kContract;
}

bool HasCallback(const void* slot) {
    uintptr_t callback=0;
    return slot && Read(reinterpret_cast<uintptr_t>(slot)+0x38,&callback,sizeof(callback)) && callback;
}

void Observe(void* context,const void* id,void* success,void* failure) {
    ++g_Calls;
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-g_Image;
    uint64_t receipt=0;
    if (caller<=UINT32_MAX && IsFreeform(id)) {
        const uint64_t flags=1u | (HasCallback(success)?2u:0u) | (HasCallback(failure)?4u:0u);
        receipt=(flags<<32) | caller;
    }
    g_Original.load(std::memory_order_acquire)(context,id,success,failure);
    if (receipt) g_Receipt.store(receipt,std::memory_order_release);
}
}

void ContractLaunchTrace::Install() {
    g_Image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    decltype(kWrapperEntry)::value_type entry[kWrapperEntry.size()]{};
    decltype(kManagerCall)::value_type call[kManagerCall.size()]{};
    const auto address=g_Image+kWrapperRva;
    if (!g_Image || !Read(address,entry,sizeof(entry)) || std::memcmp(entry,kWrapperEntry.data(),sizeof(entry)) ||
        !Read(g_Image+kManagerCallRva,call,sizeof(call)) || std::memcmp(call,kManagerCall.data(),sizeof(call))) {
        CoopLog::Warn("launch","Contract wrapper bytes differ. Observation disabled.");
        return;
    }
    const auto init=MH_Initialize();
    if (init!=MH_OK && init!=MH_ERROR_ALREADY_INITIALIZED) {
        CoopLog::Fmt(coop::LogLevel::Warn,"launch","Contract observer initialization failed: %d.",init);
        return;
    }
    // The engine may still return through the observer after plugin teardown.
    HMODULE module=nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&Observe),&module)) {
        CoopLog::Warn("launch","Could not retain contract observer code.");
        return;
    }
    auto* target=reinterpret_cast<void*>(address);
    void* original=nullptr;
    const auto created=MH_CreateHook(target,reinterpret_cast<void*>(&Observe),&original);
    if (created!=MH_OK || !original) {
        CoopLog::Fmt(coop::LogLevel::Warn,"launch","Contract observer creation failed: %d.",created);
        return;
    }
    g_Original.store(reinterpret_cast<StartWrapper>(original),std::memory_order_release);
    const auto enabled=MH_EnableHook(target);
    if (enabled==MH_OK) CoopLog::Info("launch","Contract wrapper observer attached.");
    else CoopLog::Fmt(coop::LogLevel::Warn,"launch","Contract observer enable failed: %d.",enabled);
}

void ContractLaunchTrace::Drain() {
    const auto receipt=g_Receipt.exchange(0,std::memory_order_acq_rel);
    if (receipt) {
        const auto flags=receipt>>32;
        CoopLog::Fmt(coop::LogLevel::Info,"launch",
            "Freeform wrapper returned, caller RVA=0x%X, callback slot 1=%s, slot 2=%s.",
            static_cast<unsigned>(receipt),flags&2?"set":"empty",flags&4?"set":"empty");
    }
    const auto calls=g_Calls.exchange(0,std::memory_order_relaxed);
    if (calls) CoopLog::Fmt(coop::LogLevel::Info,"launch","Contract wrapper calls since last update: %u.",calls);
}
#endif
