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

uintptr_t GetAddress(Address address)
{
    // Check if we have to do pattern initialization
    if (!initialized) {
        std::scoped_lock lk{initialization_mutex};
        ADDRESSES[GET_CONTAINER_BLOCK_INFO] = {[]() {
            auto cont = meow_hook::pattern(
                            "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 83 79 78 00 44 89 C6")
                            .count(1)
                            .get(0)
                            .as<uintptr_t>();
            return cont;
        }}; //

        ADDRESSES[READ_FILE_FROM_CONTAINER] = {[]() {
            auto cont = meow_hook::pattern("E8 ? ? ? ? 0F B6 D8 48 8D 4D C0")
                            .count(1)
                            .get(0)
                            .extract_call();
            return cont;
        }};

        ADDRESSES[SOME_GLOBAL_STRUCTURE_ARCHIVE]            = {[]() {
            return meow_hook::pattern("75 3B B9 18 00 00 00")
                .count(1)
                .get(0)
                .adjust(-8)
                .adjust(3)
                .add_disp()
                .adjust(13)
                .as<uintptr_t>();
        }};
        ADDRESSES[TOOL_ONE_DATA_HELPER_RELOAD_DATA]         = {[]() {
            return meow_hook::pattern("40 55 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 C7 45 ? ? ? ? ? 48 "
                                      "89 9C 24 ? ? ? ? 48 8D 45 9F")
                .count(1)
                .get(0)
                .as<uintptr_t>();
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
        ADDRESSES[FILE_READ_ALLOCATE_BUFFER]                = {[]() {
            return meow_hook::pattern("48 89 5C 24 ? 57 48 83 EC 20 31 FF 48 89 6C 24 ?")
                .count(1)
                .get(0)
                .as<uintptr_t>();
        }};
        ADDRESSES[SOME_GLOBAL_STRUCT_TOOL_ONE_HELPER_MAYBE] = {[]() {
            return meow_hook::pattern("48 8B 0D ? ? ? ? E8 ? ? ? ? 90 48 8D 4D FF")
                .count(1)
                .get(0)
                .adjust(3)
                .add_disp()
                .adjust(4)
                .as<uintptr_t>();
        }};
        //
        if (!initialized) {
            for (auto &address : ADDRESSES) {
                //
                auto pattern_matched_address = address.pattern_lookup();
                // If we don't get a result, we just fallback to the hardcoded address
                // but adjust it to the actual loaded image base
                if (pattern_matched_address) {
                    address.address = pattern_matched_address;
                } else {
                    spdlog::error("Failed to find address, please create an issue on GitHub");
                }
            }
        }
        initialized = true;
    }
    return ADDRESSES[address].address;
}
void SetAddress(Address address, uint64_t add)
{
    ADDRESSES[address].address = add;
}

} // namespace anno
