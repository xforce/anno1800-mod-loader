#include "anno/random_game_functions.h"

#include "meow_hook/pattern_search.h"
#include "spdlog/spdlog.h"

#include <Windows.h>
#include <xmmintrin.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <locale>
#include <mutex>
#include <sstream>
#include <vector>

namespace anno
{
struct AddressInfo {
    std::function<uintptr_t(std::optional<std::string_view>)> pattern_lookup;
    uintptr_t                                                 address = 0;
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

static uintptr_t RebaseFileOffsetToMemoryAddess(uintptr_t file_offset)
{
    auto executable_address = GetModuleHandle(NULL);

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)(executable_address);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        throw std::runtime_error("Invalid DOS Signature");
    }

    PIMAGE_NT_HEADERS header =
        (PIMAGE_NT_HEADERS)(((char *)executable_address + (dosHeader->e_lfanew * sizeof(char))));
    if (header->Signature != IMAGE_NT_SIGNATURE) {
        throw std::runtime_error("Invalid NT Signature");
    }

    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(header);

    for (int32_t i = 0; i < header->FileHeader.NumberOfSections; i++, section++) {
        bool executable = (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        bool readable   = (section->Characteristics & IMAGE_SCN_MEM_READ) != 0;
        spdlog::debug("{}, {} -> {}", section->PointerToRawData,
                      section->PointerToRawData + section->SizeOfRawData);
        if (file_offset >= section->PointerToRawData
            && file_offset <= section->PointerToRawData + section->SizeOfRawData) {
            return (uintptr_t)executable_address + file_offset
                   + ((intptr_t)section->VirtualAddress - (intptr_t)section->PointerToRawData);
        }
    }
    return 0xDEAD;
}

bool FindAddresses()
{
    if (!initialized) {
        std::scoped_lock lk{initialization_mutex};
        if (initialized) {
            return true;
        }
        initialized = true;

        // Do a combined pre-search
        ADDRESSES[READ_FILE_FROM_CONTAINER] = {[](std::optional<std::string_view> game_file) {
            // Game Update 13 
            try {
               auto match = meow_hook::pattern("E8 ? ? ? ? 0F B6 D8 48 8D 4D C0", game_file)
                                .count(1)
                                .get(0);
               if (game_file) {
                   match = match.adjust(
                       RebaseFileOffsetToMemoryAddess(
                           match.as<uintptr_t>() - reinterpret_cast<intptr_t>(game_file->data()))
                       - match.as<uintptr_t>());
               }
               return match.extract_call();
            }
            catch (...) {
            }

            // Game Update 13 
            try {
                auto match = meow_hook::pattern("E8 ? ? ? ? 0F B6 D8 48 8D 4D B0", game_file)
                                 .count(1)
                                 .get(0);
                if (game_file) {
                    match = match.adjust(
                        RebaseFileOffsetToMemoryAddess(
                            match.as<uintptr_t>() - reinterpret_cast<intptr_t>(game_file->data()))
                        - match.as<uintptr_t>());
                }
                return match.extract_call();
            } catch (...) {
            }

            // Game Update 12
            try {
                auto match = meow_hook::pattern("E8 ? ? ? ? 0F B6 D8 48 8D 4D D0", game_file)
                                 .count(1)
                                 .get(0);
                if (game_file) {
                    match = match.adjust(
                        RebaseFileOffsetToMemoryAddess(
                            match.as<uintptr_t>() - reinterpret_cast<intptr_t>(game_file->data()))
                        - match.as<uintptr_t>());
                }
                return match.extract_call();
            } catch (...) {
            }

            // We dead
            return uintptr_t(0xDEAD);
        }};

        ADDRESSES[SOME_GLOBAL_STRUCTURE_ARCHIVE] = {[](std::optional<std::string_view> game_file) {
            // Game Update 13
            try {
                auto match =
                    meow_hook::pattern("48 83 3D ? ? ? ? ? 75 20 B9 10 01 00 00", game_file)
                        .count(1)
                        .get(0);
                if (game_file) {
                    match = match.adjust(
                        RebaseFileOffsetToMemoryAddess(
                            match.as<uintptr_t>() - reinterpret_cast<intptr_t>(game_file->data()))
                        - match.as<uintptr_t>());
                }
                return match.adjust(3).add_disp().adjust(5).as<uintptr_t>();
            } catch (...) {
            }

            // 48 89 3D ? ? ? ? 48 83 3D ? ? ? ? ?
            // Game Update 12
            try {
                auto match =
                    meow_hook::pattern("48 83 3D ? ? ? ? ? 75 20 B9 20 01 00 00", game_file)
                        .count(1)
                        .get(0);
                if (game_file) {
                    match = match.adjust(
                        RebaseFileOffsetToMemoryAddess(
                            match.as<uintptr_t>() - reinterpret_cast<intptr_t>(game_file->data()))
                        - match.as<uintptr_t>());
                }
                return match.adjust(3).add_disp().adjust(5).as<uintptr_t>();
            } catch (...) {
            }

            // We dead
            return uintptr_t(0xDEAD);
        }};
        ADDRESSES[TOOL_ONE_DATA_HELPER_RELOAD_DATA] = {[](std::optional<std::string_view>
                                                              game_file) {
            // Game Update 13
            try {
                auto match = meow_hook::pattern("40 55 53 56 57 41 54 41 56 41 57 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 8D 45 AF",
                                                game_file)
                                 .count(1)
                                 .get(0);
                if (game_file) {
                    match = match.adjust(
                        RebaseFileOffsetToMemoryAddess(
                            match.as<uintptr_t>() - reinterpret_cast<intptr_t>(game_file->data()))
                        - match.as<uintptr_t>());
                }
                return match.as<uintptr_t>();
            } catch (...) {
            }

            // Game Update 12
            try {
                auto match = meow_hook::pattern("40 55 53 56 57 41 54 41 56 41 57 48 8D 6C 24 ? 48 "
                                                "81 EC ? ? ? ? 48 8D 44 24 ?",
                                                game_file)
                                 .count(1)
                                 .get(0);
                if (game_file) {
                    match = match.adjust(
                        RebaseFileOffsetToMemoryAddess(
                            match.as<uintptr_t>() - reinterpret_cast<intptr_t>(game_file->data()))
                        - match.as<uintptr_t>());
                }
                return match.as<uintptr_t>();
            } catch (...) {
            }

            // We dead
            return uintptr_t(0xDEAD);
        }};
        ADDRESSES[FILE_GET_FILE_SIZE] = {[](std::optional<std::string_view> game_file) {
            // Game Update 13.2
            try {
                auto match = meow_hook::pattern("E8 ? ? ? ? 84 C0 74 18 4C 89 66 60", game_file)
                                .count(1)
                                .get(0);
                if (game_file) {
                    match = match.adjust(
                        RebaseFileOffsetToMemoryAddess(
                            match.as<uintptr_t>() - reinterpret_cast<intptr_t>(game_file->data()))
                        - match.as<uintptr_t>());
                }
                return match.extract_call();
            }
            catch (...) {
            }

            // Game Update 13
            try {
                auto match = meow_hook::pattern("E8 ? ? ? ? 0F B6 D8 48 8D 4D 80", game_file)
                                 .count(1)
                                 .get(0);
                if (game_file) {
                    match = match.adjust(
                        RebaseFileOffsetToMemoryAddess(
                            match.as<uintptr_t>() - reinterpret_cast<intptr_t>(game_file->data()))
                        - match.as<uintptr_t>());
                }
                return match.extract_call();
            } catch (...) {
            }

            // We dead
            return uintptr_t(0xDEAD);
        }};
        ADDRESSES[READ_GAME_FILE]     = {[](std::optional<std::string_view> game_file) {
            try {
                auto match =
                    meow_hook::pattern("E8 ? ? ? ? 48 83 F8 30", game_file).count(1).get(0);
                if (game_file) {
                    match = match.adjust(
                        RebaseFileOffsetToMemoryAddess(
                            match.as<uintptr_t>() - reinterpret_cast<intptr_t>(game_file->data()))
                        - match.as<uintptr_t>());
                }
                return match.extract_call();
            } catch (...) {
            }

            return uintptr_t(0xDEAD);
        }};
        ADDRESSES[SOME_GLOBAL_STRUCT_TOOL_ONE_HELPER_MAYBE] = {[](std::optional<std::string_view>
                                                                      game_file) {
            // Game Update 13
            try {
                auto match = meow_hook::pattern("C7 45 ? ? ? ? ? 48 8B 35 ? ? ? ? 41 BC ? ? ? ?",
                                                game_file)
                                 .count(1)
                                 .get(0);
                if (game_file) {
                    match = match.adjust(
                        RebaseFileOffsetToMemoryAddess(
                            match.as<uintptr_t>() - reinterpret_cast<intptr_t>(game_file->data()))
                        - match.as<uintptr_t>());
                }
                return match.adjust(7 + 3).add_disp().adjust(4).as<uintptr_t>();
            } catch (...) {
            }

            // Game Update 12
            try {
                auto match = meow_hook::pattern("C7 44 ? ? ? ? ? ? 4C 8B 35 ? ? ? ? 41 BC ? ? ? ?",
                                                game_file)
                                 .count(1)
                                 .get(0);
                if (game_file) {
                    match = match.adjust(
                        RebaseFileOffsetToMemoryAddess(
                            match.as<uintptr_t>() - reinterpret_cast<intptr_t>(game_file->data()))
                        - match.as<uintptr_t>());
                }
                return match.adjust(7 + 3).add_disp().adjust(4).as<uintptr_t>();
            } catch (...) {
            }

            // We dead
            return uintptr_t(0xDEAD);
        }};

        ADDRESSES[READ_INT64_FROM_XML_NODE] = {
            [](std::optional<std::string_view> game_file) {
				// Game Update 14
				try {
				   auto match =
					   meow_hook::pattern("E8 ? ? ? ? 66 41 83 FC ? 77 0A", game_file).count(1).get(0);
				   if (game_file) {
					   match = match.adjust(RebaseFileOffsetToMemoryAddess(
												match.as<uintptr_t>()
												- reinterpret_cast<intptr_t>(game_file->data()))
											- match.as<uintptr_t>());
				   }
				   return match.extract_call();
				}
				catch (...) {
				}

                // Game Update 13.2+
                try {
                   auto match =
                       meow_hook::pattern("E8 ? ? ? ? 4C 8D 76 40", game_file).count(1).get(0);
                   if (game_file) {
                       match = match.adjust(RebaseFileOffsetToMemoryAddess(
                                                match.as<uintptr_t>()
                                                - reinterpret_cast<intptr_t>(game_file->data()))
                                            - match.as<uintptr_t>());
                   }
                   return match.extract_call();
                }
                catch (...) {
                }

                // Game Update 12
                try {
                    auto match =
                        meow_hook::pattern("E8 ? ? ? ? 66 83 FE 12", game_file).count(1).get(0);
                    if (game_file) {
                        match = match.adjust(RebaseFileOffsetToMemoryAddess(
                                                 match.as<uintptr_t>()
                                                 - reinterpret_cast<intptr_t>(game_file->data()))
                                             - match.as<uintptr_t>());
                    }
                    return match.extract_call();
                } catch (...) {
                    return uintptr_t(0xDEAD);
                }
            }};

        std::filesystem::path process_file_path;

        {
            wchar_t process_name[1024] = {0};
            DWORD   process_name_size  = 1024;
            QueryFullProcessImageNameW(GetCurrentProcess(), 0, process_name, &process_name_size);
            process_file_path = process_name;
        }

        std::fstream is(process_file_path, std::ios::in | std::ios::binary);
        is.seekg(0, std::ios::end);
        size_t data_size = is.tellg();
        is.seekg(0, std::ios::beg);
        std::unique_ptr<char[]> data(new char[data_size]);
        is.read(data.get(), data_size);

        std::string_view game_file(data.get(), data_size);
        //
        bool any_address_failed = false;
        for (int attempt = 0; attempt < 5; ++attempt) {
            any_address_failed = false;
            int index          = 0;
            for (auto &address : ADDRESSES) {
                uintptr_t pattern_matched_address = 0;
                //
                try {
                    pattern_matched_address = address.pattern_lookup(game_file);
                } catch (...) {
                    pattern_matched_address = 0xDEAD;
                }

                if (pattern_matched_address == 0xDEAD || pattern_matched_address == 0) {
                    try {
                        spdlog::warn("Address search fall back to memory search");
                        pattern_matched_address = address.pattern_lookup({});
                    } catch (...) {
                        spdlog::error("Failed to find address in memory");
                        pattern_matched_address = 0xDEAD;
                    }
                }

                // If we fail to find an address, we are in trouble :)
                // Mod loader needs updating
                if (pattern_matched_address != 0xDEAD && pattern_matched_address != 0) {
                    address.address = pattern_matched_address;
                    spdlog::debug("Matched address {}", (void *)pattern_matched_address);
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
