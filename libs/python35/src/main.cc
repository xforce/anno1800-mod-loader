#include <interface.h>

#if defined(INTERNAL_ENABLED)
#include <libs/internal/debuggable/include/debuggable.h>
#endif
#include <hooking.h>

#include <Windows.h>

#include <cstdio>
#include <thread>

Events events;

FARPROC GetProcAddress_S(HMODULE hModule, LPCSTR lpProcName)
{
    std::once_flag flag1;
    std::call_once(flag1, []() { events.DoHooking(); });
    return GetProcAddress(hModule, lpProcName);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
#if defined(INTERNAL_ENABLED)
            EnableDebugging(events);
#endif
            // TODO(alexander): Add code that can load other dll libraries here
            // that offer more features later on
            set_import("GetProcAddress", (uintptr_t)GetProcAddress_S);
        }
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}