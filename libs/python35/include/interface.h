// TODO(alexander): rename this file

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef WIN32_NO_STATUS
#define WIN32_NO_STATUS
#endif
#include <Windows.h>
#undef WIN32_NO_STATUS

#include <ksignals/ksignals.h>

struct Events {
    ksignals::Event<void()>                 DoHooking;
    ksignals::Event<uintptr_t(std::string)> GetProcAddress;
    ksignals::Event<void()>                 IHateEverything;
    ksignals::Event<HANDLE (LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
        LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
        DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)> CreateFileW;
};
