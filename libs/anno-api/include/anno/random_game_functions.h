#pragma once

#include "meow_hook/util.h"

#include <cstdint>
#include <string>

namespace anno
{

enum Address {
    READ_FILE_FROM_CONTAINER,
    SOME_GLOBAL_STRUCTURE_ARCHIVE,
    // ToolOneDataHelper::ReloadData
    //
    TOOL_ONE_DATA_HELPER_RELOAD_DATA,

    FILE_GET_FILE_SIZE,

    READ_GAME_FILE,
    // NOTE(alexander): This function is now fully inlined starting with Game Update 7
    // So we can't use it anymore :(
    // FILE_READ_ALLOCATE_BUFFER,
    SOME_GLOBAL_STRUCT_TOOL_ONE_HELPER_MAYBE,
    READ_INT64_FROM_XML_NODE,
    SIZE
};

bool      FindAddresses();
uintptr_t GetAddress(Address address);
void      SetAddress(Address address, uint64_t add);

inline bool __fastcall ReadFileFromContainer(__int64             archive_file_map,
                                             const std::wstring& file_path,
                                             char** output_data_pointer, size_t* output_data_size)
{
    return ((decltype(ReadFileFromContainer)*)GetAddress(READ_FILE_FROM_CONTAINER))(
        archive_file_map, file_path, output_data_pointer, output_data_size);
}
namespace ToolOneDataHelper
{
    inline void ReloadData()
    {
        return ((decltype(ReloadData)*)GetAddress(TOOL_ONE_DATA_HELPER_RELOAD_DATA))();
    }
} // namespace ToolOneDataHelper
} // namespace anno
