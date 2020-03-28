#include "anno/random_game_functions.h"

#include "meow_hook/pattern_search.h"
#include "spdlog/spdlog.h"

#include <Windows.h>
#include <xmmintrin.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <functional>
#include <future>
#include <locale>
#include <mutex>
#include <sstream>
#include <vector>

namespace anno
{
struct AddressInfo {
    std::function<uintptr_t()> pattern_lookup;
    uintptr_t                  address = 0;
};

static AddressInfo ADDRESSES[Address::SIZE] = {};

static std::atomic_bool initialized = false;
static std::mutex       initialization_mutex;

static uintptr_t base_address = 0;
inline uint64_t  adjust_address(uint64_t address)
{
    if (base_address == 0) {
        base_address = uintptr_t(GetModuleHandle(NULL));
    }
    const auto offset = address - uint64_t(0x140000000);
    return base_address + offset;
}

bool FindAddresses() {
    if (!initialized) {
        std::scoped_lock lk{initialization_mutex};
        if (initialized) {
            return true;
        }
        initialized                         = true;

        ADDRESSES[GET_CONTAINER_BLOCK_INFO] = {[]() {
            auto cont = meow_hook::pattern(
                            "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 83 79 78 00 44 89 C6")
                            .count(1)
                            .get(0)
                            .as<uintptr_t>();
            return cont;
        }}; //

        ADDRESSES[READ_FILE_FROM_CONTAINER] = {[]() {
            // Post Game Update 7
            if (auto pattern = meow_hook::pattern("E8 ? ? ? ? 0F B6 F8 48 8D 4D C0");
                pattern.matches()) {
                auto cont = pattern.count(1).get(0).extract_call();
                return cont;
            }

            // Pre Game Update 7
            if (auto pattern = meow_hook::pattern("E8 ? ? ? ? 0F B6 D8 48 8D 4D C0");
                pattern.matches()) {
                auto cont = pattern.count(1).get(0).extract_call();
                return cont;
            }

            return uintptr_t(0xDEAD);
        }};

        ADDRESSES[SOME_GLOBAL_STRUCTURE_ARCHIVE]            = {[]() {
            // Post Game Update 7
            if (auto pattern = meow_hook::pattern("75 4F B9 18 00 00 00"); pattern.matches()) {
                return pattern.count(1)
                    .get(0)
                    .adjust(-8)
                    .adjust(3)
                    .add_disp()
                    .adjust(13)
                    .as<uintptr_t>();
            }

            // Pre Game Update 7
            if (auto pattern = meow_hook::pattern("75 3B B9 18 00 00 00"); pattern.matches()) {
                return pattern.count(1)
                    .get(0)
                    .adjust(-8)
                    .adjust(3)
                    .add_disp()
                    .adjust(13)
                    .as<uintptr_t>();
            }
            return uintptr_t(0xDEAD);
        }};
        ADDRESSES[TOOL_ONE_DATA_HELPER_RELOAD_DATA]         = {[]() {
            // 
            // Post Game Update 7
            if (auto pattern =
                    meow_hook::pattern("48 8B C4 55 48 8D 68 A1 48 81 EC ? ? ? ? 48 C7 45 ? ? ? ? ? 48 89 58 08 48 89 70 18 48 89 78 20 48 8D 45 9F");
                pattern.matches()) {
                return pattern.count(1).get(0).as<uintptr_t>();
            }
            
            // Pre Game Update 7
            if (auto pattern =
                    meow_hook::pattern("40 55 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 C7 45 ? ? ? ? ? 48 "
                                       "89 9C 24 ? ? ? ? 48 8D 45 9F");
                pattern.matches()) {
                return pattern.count(1).get(0).as<uintptr_t>();
            }
           
            return uintptr_t(0xDEAD);
        }};
        ADDRESSES[FILE_GET_FILE_SIZE]                       = {[]() {
            return meow_hook::pattern("E8 ? ? ? ? 0F B6 D8 48 8D 4D B0")
                .count(1)
                .get(0)
                .extract_call();
        }};
        ADDRESSES[READ_GAME_FILE]                           = {[]() {
            return meow_hook::pattern("48 89 5C 24 ? 48 89 6C 24 ? 56 48 83 EC 30 48 8B 81 ? ? ? ?")
                .count(1)
                .get(0)
                .as<uintptr_t>();
        }};
        // NOTE(alexander): This function is now fully inlined starting with Game Update 7
        // So we can't use it anymore :(
        //ADDRESSES[FILE_READ_ALLOCATE_BUFFER]                = {[]() {
        // 
        //    return meow_hook::pattern("48 89 5C 24 ? 57 48 83 EC 20 31 FF 48 89 6C 24 ?") // BROKEN
        //        .count(1)
        //        .get(0)
        //        .as<uintptr_t>();
        //}};
        ADDRESSES[SOME_GLOBAL_STRUCT_TOOL_ONE_HELPER_MAYBE] = {[]() {
            // Pre Game Update 7
            if (auto pattern = meow_hook::pattern("48 8B 0D ? ? ? ? E8 ? ? ? ? 90 48 8D 4D FF");
                pattern.matches()) {
                return pattern.count(1).get(0).adjust(3).add_disp().adjust(4).as<uintptr_t>();
            }

            // Post Game Update 7
            if (auto pattern = meow_hook::pattern("48 8B 3D ? ? ? ? 4C 8B 5D DF");
                pattern.matches()) {
                return pattern.count(1).get(0).adjust(3).add_disp().adjust(4).as<uintptr_t>();
            }
            return uintptr_t(0xDEAD);
        }};
        //
        bool any_address_failed = false;
        for (int attempt = 0; attempt < 5; ++attempt) {
            any_address_failed = false;
            int  index              = 0;
            for (auto &address : ADDRESSES) {
                //
                auto pattern_matched_address = address.pattern_lookup();

                // If we fail to find an address, we are in trouble :)
                // Mod loader needs updating
                if (pattern_matched_address != 0xDEAD && pattern_matched_address != 0) {
                    address.address = pattern_matched_address;
                    spdlog::debug("Matched address {}", pattern_matched_address);
                } else {
                    any_address_failed = true;
                    spdlog::error("Failed to find address, please create an issue on GitHub {}",
                                  index);
                }
                ++index;
            }
            if (!any_address_failed) {
                return true;
            }
        }
        return !any_address_failed;
    }
    return true;
}

uintptr_t GetAddress(Address address)
{
    return ADDRESSES[address].address;
}
void SetAddress(Address address, uint64_t add)
{
    ADDRESSES[address].address = add;
}

} // namespace anno
