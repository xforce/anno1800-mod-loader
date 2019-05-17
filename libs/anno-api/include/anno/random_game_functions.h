#include "hooking.h"

#include <cstdint>
#include <string>

namespace anno
{

enum Address {
    // GetContainerBlockInfo
    // '13.10575.0.1471' === 140054880
    // '13.12551.0.1532' === 140054880
    GET_CONTAINER_BLOCK_INFO,
    // ReadFileFromContainer
    // '13.10575.0.1471' === 14004F840
    // '13.12551.0.1532' === 14004F840
    READ_FILE_FROM_CONTAINER,
    //// GetContainerBlockInfo
    //// '13.10575.0.1471' === 140054880
    //// '13.12551.0.1532' === 140054880
    //// GET_CONTAINER_BLOCK_INFO_JMP,
    // '13.12551.0.1532' === 144EE8DF8
    SOME_GLOBAL_STRUCTURE_ARCHIVE,
    SIZE
};

uintptr_t GetAddress(Address address);
void      SetAddress(Address address, uint64_t add);

inline bool GetContainerBlockInfo(uintptr_t* a1, const std::wstring& file_path, int a3)
{
    return func_call<bool>(GetAddress(GET_CONTAINER_BLOCK_INFO), a1, file_path, a3);
}
inline bool __fastcall ReadFileFromContainer(__int64             archive_file_map,
                                             const std::wstring& file_path,
                                             char** output_data_pointer, size_t* output_data_size)
{
    return ((decltype(ReadFileFromContainer)*)GetAddress(READ_FILE_FROM_CONTAINER))(
        archive_file_map, file_path, output_data_pointer, output_data_size);
}
} // namespace anno
