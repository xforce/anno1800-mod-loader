#include "interface.h"

#include "version.h"

#if defined(INTERNAL_ENABLED)
#include "libs/internal/debuggable/include/debuggable.h"
#endif
#include "hooking.h"
#include "libs/external-file-loader/include/external-file-loader.h"

#include "nlohmann/json.hpp"

#include <WinInet.h>
#include <Windows.h>

#pragma comment(lib, "Wininet.lib")

#include <cstdio>
#include <thread>

Events events;

static std::string GetLatestVersion()
{
    char data[1024] = {0};

    // initialize WinInet
    HINTERNET hInternet =
        ::InternetOpen(TEXT("Anno 1800 Mod Loader"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (hInternet != nullptr) {
        // 5 second timeout for now
        DWORD timeout = 5 * 1000;
        auto  result  = InternetSetOption(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout,
                                        sizeof(timeout));
        InternetSetOption(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOption(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));

        // open HTTP session
        HINTERNET hConnect = ::InternetConnect(hInternet, "xforce.dev", INTERNET_DEFAULT_HTTPS_PORT,
                                               0, 0, INTERNET_SERVICE_HTTP, 0, 0);
        if (hConnect != nullptr) {
            HINTERNET hRequest = ::HttpOpenRequestA(
                hConnect, "GET", "/anno1800-mod-loader/latest.json", 0, 0, 0,
                INTERNET_FLAG_SECURE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD, 0);
            if (hRequest != nullptr) {
                // send request
                const auto was_sent = ::HttpSendRequest(hRequest, nullptr, 0, nullptr, 0);

                if (was_sent) {
                    DWORD status_code      = 0;
                    DWORD status_code_size = sizeof(status_code);
                    HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                                  &status_code, &status_code_size, 0);

                    if (status_code != 200) {
                        throw std::runtime_error("Failed to read version info");
                    }
                    for (;;) {
                        // reading data
                        DWORD      bytes_read;
                        const auto was_read =
                            ::InternetReadFile(hRequest, data, sizeof(data) - 1, &bytes_read);

                        // break cycle if error or end
                        if (was_read == FALSE || bytes_read == 0) {
                            if (was_read == FALSE) {
                                throw std::runtime_error("Failed to read version info");
                            }
                            break;
                        }

                        // saving result
                        data[bytes_read] = 0;
                    }
                }
                ::InternetCloseHandle(hRequest);
            }
            ::InternetCloseHandle(hConnect);
        }
        ::InternetCloseHandle(hInternet);
    }

    return data;
}

FARPROC GetProcAddress_S(HMODULE hModule, LPCSTR lpProcName)
{
    // A call to GetProcAddres indicates that all the copy protection is done
    // and we started loading all the dynamic imports
    // those would have usually been in the import table.
    // This means we are ready to do some hooking
    // But only do hooking once.
    static std::once_flag flag1;
    std::call_once(flag1, []() { events.DoHooking(); });
    return GetProcAddress(hModule, lpProcName);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);

#if defined(INTERNAL_ENABLED)
            EnableDebugging(events);
#endif
            // Version Check
            events.DoHooking.connect([]() {
                static int32_t current_version[3] = {VERSION_MAJOR, VERSION_MINOR,
                                                     VERSION_REVISION};

                try {
                    auto        body        = GetLatestVersion();
                    const auto& data        = nlohmann::json::parse(body);
                    const auto& version_str = data["version"].get<std::string>();

                    int32_t latest_version[3] = {0};
                    std::sscanf(version_str.c_str(), "%d.%d.%d", &latest_version[0],
                                &latest_version[1], &latest_version[2]);

                    if (std::lexicographical_compare(current_version, current_version + 3,
                                                     latest_version, latest_version + 3)) {
                        std::string msg =
                            "A new version (" + version_str + ") is available for download.\n\n";
                        msg.append("Do you want to go to the release page on GitHub?\n(THIS IS "
                                   "HIGHLY RECOMMENDED!!");

                        if (MessageBoxA(NULL, msg.c_str(), VER_FILE_DESCRIPTION_STR,
                                        MB_ICONQUESTION | MB_YESNO | MB_SYSTEMMODAL)
                            == IDYES) {
                            auto result = ShellExecuteA(
                                nullptr, "open",
                                "https://github.com/xforce/anno1800-mod-loader/releases/latest",
                                nullptr, nullptr, SW_SHOWNORMAL);
                            result = result;
                            TerminateProcess(GetCurrentProcess(), 0);
                        }
                    }
                } catch (...) {
                    // TODO(alexander): Logging
                }
            });

            EnableExtenalFileLoading(events);
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
