#include <interface.h>

#if defined(INTERNAL_ENABLED)
#include <libs/internal/debuggable/include/debuggable.h>
#endif

#include <Windows.h>

#include <cstdio>

Events events;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
#if defined(INTERNAL_ENABLED)
            EnableDebugging(events);
#endif
            // TODO(alexander): Add code that can load other dll libraries here
            // that offer more features later on
        }
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}