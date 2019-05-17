#include "anno/random_game_functions.h"

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
    uintptr_t                  address;
    std::function<uintptr_t()> pattern_lookup;
};

struct PatternMatch {
    PatternMatch(void *address)
        : address_(address)
    {
    }

    PatternMatch &adjust(intptr_t offset)
    {
        offset_ = offset;
        return *this;
    }

    template <typename T> T *cast()
    {
        return reinterpret_cast<T *>(static_cast<char *>(address_) + offset_);
    }

    template <> void *cast()
    {
        return reinterpret_cast<void *>(static_cast<char *>(address_) + offset_);
    }

    intptr_t origaddr()
    {
        return reinterpret_cast<intptr_t>(address_);
    }

    intptr_t addr()
    {
        // return reinterpret_cast<uintptr_t>(_address);
        return reinterpret_cast<intptr_t>(static_cast<char *>(address_) + offset_);
    }

    bool operator==(const PatternMatch &rhs) const
    {
        return address_ == rhs.address_;
    }

  private:
    void *   address_ = nullptr;
    intptr_t offset_  = 0;
};

class PESectionInfo
{
  private:
    uintptr_t begin_;
    uintptr_t end_;

  public:
    decltype(auto) begin()
    {
        return begin_;
    }
    decltype(auto) end()
    {
        return end_;
    }

    PESectionInfo(uintptr_t begin, uintptr_t end)
        : begin_(begin)
        , end_(end)
    {
    }
};

std::vector<PESectionInfo> GetExecutableSections()
{
    static std::vector<PESectionInfo> sections;
    if (!sections.empty()) {
        return sections;
    }

    auto _executableAddress = GetModuleHandle(NULL);

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)(_executableAddress);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        throw std::runtime_error("Invalid DOS Signature");
    }

    PIMAGE_NT_HEADERS header =
        (PIMAGE_NT_HEADERS)(((char *)_executableAddress + (dosHeader->e_lfanew * sizeof(char))));
    if (header->Signature != IMAGE_NT_SIGNATURE) {
        throw std::runtime_error("Invalid NT Signature");
    }

    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(header);

    for (int32_t i = 0; i < header->FileHeader.NumberOfSections; i++, section++) {
        bool executable = (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        bool readable   = (section->Characteristics & IMAGE_SCN_MEM_READ) != 0;
        if (readable && executable) {
            auto     beg        = (header->OptionalHeader.ImageBase + section->VirtualAddress);
            uint32_t sizeOfData = std::min(section->SizeOfRawData, section->Misc.VirtualSize);
            sections.emplace_back(beg, beg + sizeOfData);
        }
    }
    return sections;
}

static void GenerateMaskAndData(const std::string &pattern, std::string &mask, std::string &data)
{
    const static std::locale loc;

    std::stringstream dataStream;
    std::stringstream maskStream;

    for (auto &&ch = pattern.begin(); ch != pattern.end(); ++ch) {
        if (*ch == '?') {
            dataStream << '\x00';
            maskStream << '?';
        } else if (std::isalnum(*ch, loc)) {
            auto ch1   = *ch;
            auto ch2   = *(++ch);
            char str[] = {ch1, ch2};
            char digit = static_cast<char>(strtol(str, nullptr, 16));
            dataStream << digit;
            maskStream << 'x';
        }
    }

    data = dataStream.str();
    mask = maskStream.str();
}

std::vector<PatternMatch> SearchPattern(std::string pattern)
{
    std::vector<PatternMatch> _matches;

    std::string mask;
    std::string data;

    GenerateMaskAndData(pattern, mask, data);

    auto doMatch = [&](uintptr_t offset) {
        char *ptr = reinterpret_cast<char *>(offset);

        for (size_t i = 0; i < mask.size(); i++) {
            if (mask[i] == '?') {
                continue;
            }

            if (data.length() < i || data[i] != ptr[i]) {
                return false;
            }
        }

        _matches.emplace_back(ptr);

        return true;
    };

    // check if SSE 4.2 is supported
    int32_t cpuid[4];
    __cpuid(cpuid, 0);

    bool sse42 = false;

    if (mask.size() <= 16) {
        if (cpuid[0] >= 1) {
            __cpuidex(cpuid, 1, 0);

            sse42 = (cpuid[2] & (1 << 20));
        }
    }

    std::vector<std::future<std::vector<PatternMatch>>> futureHandles = {};

    if (!sse42) {
        auto exe_sections = GetExecutableSections();
        for (auto &section : exe_sections) {
            auto secSize = section.end() - section.begin();
            if (secSize > 1) {
                auto partSize = secSize / 8;
                auto rest     = secSize % partSize;
                for (uintptr_t i = section.begin(); i < section.end() - rest; i += partSize) {
                    auto handle = std::async(
                        std::launch::async,
                        [&](uintptr_t start, uintptr_t end) -> std::vector<PatternMatch> {
                            std::vector<PatternMatch> vecMatches;
                            for (uintptr_t offset = start; offset < end; ++offset) {
                                if (doMatch(offset)) {
                                    vecMatches.push_back(
                                        PatternMatch(reinterpret_cast<char *>(offset)));
                                }
                            }
                            return vecMatches;
                        },
                        i, i + partSize);

                    futureHandles.push_back(std::move(handle));
                }
            }
        }
    } else {
        __declspec(align(16)) char desiredMask[16] = {0};

        for (int32_t i = 0; i < mask.size(); i++) {
            desiredMask[i / 8] |= ((mask[i] == '?') ? 0 : 1) << (i % 8);
        }

        __m128i mask      = _mm_load_si128(reinterpret_cast<const __m128i *>(desiredMask));
        __m128i comparand = _mm_loadu_si128(reinterpret_cast<const __m128i *>(data.c_str()));

        // We ignore onlyFirst here, as we try to optimize it using threads :D

        auto exe_sections = GetExecutableSections();
        for (auto &section : exe_sections) {
            auto secSize = section.end() - section.begin();

            auto partSize = secSize / 8;
            auto rest     = secSize % partSize;
            for (uintptr_t i = section.begin(); i < (section.end() - rest); i += partSize) {
                auto _end = i + partSize;
                if (_end > (section.end() - 16))
                    _end = section.end() - 16;

                auto handle = std::async(
                    std::launch::async,
                    [&](uintptr_t start, uintptr_t end) -> std::vector<PatternMatch> {
                        std::vector<PatternMatch> vecMatches;
                        for (uintptr_t offset = start; offset < end; ++offset) {
                            __m128i value =
                                _mm_loadu_si128(reinterpret_cast<const __m128i *>(offset));
                            __m128i result =
                                _mm_cmpestrm(value, 16, comparand, static_cast<int>(data.size()),
                                             _SIDD_CMP_EQUAL_EACH);

                            // as the result can match more bits than the mask contains
                            __m128i matches     = _mm_and_si128(mask, result);
                            __m128i equivalence = _mm_xor_si128(mask, matches);

                            if (_mm_test_all_zeros(equivalence, equivalence)) {
                                // PatternSaveHint(_hash, offset);
                                vecMatches.push_back(
                                    PatternMatch(reinterpret_cast<char *>(offset)));
                            }
                        }

                        return vecMatches;
                    },
                    i, _end);

                futureHandles.push_back(std::move(handle));
            }
        }
    }

    for (auto &handle : futureHandles) {
        auto matches = handle.get();

        if (!matches.empty()) {
            _matches.insert(_matches.end(), matches.begin(), matches.end());
        }
    }

    auto end = _matches.end();
    for (auto it = _matches.begin(); it != end; ++it) {
        end = std::remove(it + 1, end, *it);
    }
    _matches.erase(end, _matches.end());

    return _matches;
}

static AddressInfo ADDRESSES[Address::SIZE] = {
    {0x145394060,
     []() {
         auto matches =
             SearchPattern("48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 83 79 78 00 44 89 C6");
         if (matches.empty() || matches.size() > 1) {
             // TODO(alexander): Notify user
         }
         return matches[0].addr();
     }}, // GET_CONTAINER_BLOCK_INFO
    {0x14004F840,
     []() {
         auto matches =
             SearchPattern("4C 89 4C 24 20 4C 89 44 24 18 48 89 54 24 10 48 89 4C 24 08 55 53 "
                           "56 57 48 8D AC 24 C8 FA FF FF");
         if (matches.empty() || matches.size() > 1) {
             // TODO(alexander): Notify user
         }
         return matches[0].addr();
     }}, // READ_FILE_FROM_CONTAINER
    {0x144EE8DF0,
     []() {
         auto matches = SearchPattern("75 3B B9 18 00 00 00");
         if (matches.empty() || matches.size() > 1) {
             // TODO(alexander): Notify user
         }
         auto address = matches[0].addr();
         address -= 8; // "48 83 3D B1 D9 E6 04 00"
         address += 3;
         address += *(uint32_t *)(address);
         address += 13;
         return address;
     }}, // SOME_GLOBAL_STRUCTURE_ARCHIVE
};       // namespace anno

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
        if (!initialized) {
            for (auto &address : ADDRESSES) {
                //
                auto pattern_matched_address = address.pattern_lookup();
                // If we don't get a result, we just fallback to the hardcoded address
                // but adjust it to the actual loaded image base
                if (pattern_matched_address) {
                    address.address = pattern_matched_address;
                } else {
                    address.address = adjust_address(address.address);
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
