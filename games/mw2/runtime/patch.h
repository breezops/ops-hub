#pragma once
#include <cstdint>

using PatchLog = void(*)(const wchar_t*);
bool apply_storage_patch(std::uint8_t* module, PatchLog log);
bool remove_storage_patch(PatchLog log);
