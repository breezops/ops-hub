#include "CrashGuards.h"
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

// Map the installed executable as non-executable data and run production
// resolver code. Never loads the game, patches it, or invokes any game code.
int wmain(int argc,wchar_t** argv) {
    if (argc!=2 && argc!=3) return 2;
    struct Handle { HANDLE value=nullptr; ~Handle() { if (value && value!=INVALID_HANDLE_VALUE) CloseHandle(value); } } process;
    uintptr_t liveBase=0;
    std::filesystem::path path(argv[1]);
    if (argc==3) {
        if (std::wcscmp(argv[1],L"--pid")) return 2;
        const auto pid=static_cast<DWORD>(std::wcstoul(argv[2],nullptr,10));
        Handle modules{CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid)};
        MODULEENTRY32W module{}; module.dwSize=sizeof(module);
        if (!Module32FirstW(modules.value,&module) || _wcsicmp(module.szModule,L"HITMAN3.exe")) return 2;
        path=module.szExePath; liveBase=reinterpret_cast<uintptr_t>(module.modBaseAddr);
        process.value=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid);
        if (!process.value) { std::printf("read-only process access failed: %lu\n",GetLastError()); return 2; }
    }
    std::ifstream input(path,std::ios::binary);
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(input)),{});
    auto fits=[](size_t at,size_t count,size_t limit) { return at<=limit && count<=limit-at; };
    if (file.size()<sizeof(IMAGE_DOS_HEADER)) return 2;
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(file.data());
    if (dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || !fits(dos->e_lfanew,sizeof(IMAGE_NT_HEADERS64),file.size())) return 2;
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(file.data()+dos->e_lfanew);
    if (nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
        nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt->OptionalHeader.SizeOfImage>1024u*1024u*1024u) return 2;
    std::vector<uint8_t> image(nt->OptionalHeader.SizeOfImage);
    const auto* sections=IMAGE_FIRST_SECTION(nt);
    if (!fits(reinterpret_cast<const uint8_t*>(sections)-file.data(),nt->FileHeader.NumberOfSections*sizeof(IMAGE_SECTION_HEADER),file.size())) return 2;
    uintptr_t from=0,to=0;
    for (unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        const auto& section=sections[i];
        if (!fits(section.PointerToRawData,section.SizeOfRawData,file.size()) || !fits(section.VirtualAddress,section.SizeOfRawData,image.size())) return 2;
        std::memcpy(image.data()+section.VirtualAddress,file.data()+section.PointerToRawData,section.SizeOfRawData);
        if (std::memcmp(section.Name,".text\0\0\0",8)==0) {
            from=reinterpret_cast<uintptr_t>(image.data()+section.VirtualAddress);
            to=from+section.SizeOfRawData;
            if (process.value) {
                SIZE_T copied=0;
                if (!ReadProcessMemory(process.value,reinterpret_cast<void*>(liveBase+section.VirtualAddress),
                    image.data()+section.VirtualAddress,section.SizeOfRawData,&copied) || copied!=section.SizeOfRawData) {
                    std::printf("read-only code copy failed: %lu\n",GetLastError()); return 2;
                }
            }
        }
    }
    if (!from) return 2;
    const auto job=CrashGuardsTest::ActionJobInText(from,to);
    const auto actions=CrashGuardsTest::ActionSitesInText(from,to);
    const auto behavior=CrashGuardsTest::BehaviorSitesInText(from,to);
    std::printf("job=%d action-sites=%d behavior-sites=%d\n",job?1:0,actions?1:0,behavior?1:0);
    if (behavior) {
        const auto base=reinterpret_cast<uintptr_t>(image.data());
        std::printf("RVAs: loop=%llx job=%llx control=%llx table=%llx\n",
            reinterpret_cast<uintptr_t>(behavior.actions.loop)-base,reinterpret_cast<uintptr_t>(behavior.actions.job)-base,
            reinterpret_cast<uintptr_t>(behavior.actorControl)-base,reinterpret_cast<uintptr_t>(behavior.table)-base);
    }
    return behavior?0:1;
}
