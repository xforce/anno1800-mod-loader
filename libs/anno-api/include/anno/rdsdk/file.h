#pragma once

#include "anno/random_game_functions.h"

#include "meow_hook/util.h"

#include <Windows.h>

#include <cstdint>

namespace anno
{
namespace rdsdk
{
#pragma pack(push, 1)
    class IFileInterface
    {
      public:
        virtual ~IFileInterface()                                          = 0;
        virtual const std::wstring& GetFilePath() const                    = 0;
        virtual size_t              Write(const void* buffer, size_t size) = 0;
        virtual size_t              Read(void* buffer, size_t size)        = 0;
        virtual size_t              Seek(size_t offset, int seek_mode)     = 0;
        virtual size_t              GetPosition() const                    = 0;
        virtual size_t              GetLength() const                      = 0;
    };

    class CFile : public IFileInterface
    {
      public:
        virtual bool Open(void* unk, int flags) = 0;
        virtual bool Close()                    = 0;

        std::wstring file_path;    // 0x8
        char         pad[0x50];    //
        HANDLE*      file_handle;  // 0x78
        char         no_idea[0x8]; // 0x80
        size_t       size;         // 0x88
        size_t       offset;       // 0x90
        char         pad2[0x8];    // 0x98
        struct {
            char*  data;
            size_t size;
        } buffer; // 0xA0

        static std::string ReadFile(fs::path path)
        {
            std::string output;

            char*  buffer           = nullptr;
            size_t output_data_size = 0;
            if (anno::ReadFileFromContainer(
                    *(uintptr_t*)GetAddress(anno::SOME_GLOBAL_STRUCTURE_ARCHIVE),
                    path.wstring().c_str(), &buffer, &output_data_size)) {
                buffer = buffer;
                output = {buffer, output_data_size};

                // TODO(alexander): Move to anno api
                static auto game_free = (decltype(free)*)(GetProcAddress(
                    GetModuleHandleA("api-ms-win-crt-heap-l1-1-0.dll"), "free"));
                game_free(buffer);
            }
            return output;
        }

        /*static std::string ReadFile(fs::path path)
        {
            std::string output;

            auto size = GetFileSize(path);
            if (size <= 0) {
                return {};
            }
            output.resize(size);
            size_t output_data_size = size;
            if (!anno::ReadFileFromContainer(
                    *(uintptr_t*)GetAddress(anno::SOME_GLOBAL_STRUCTURE_ARCHIVE),
                    path.wstring().c_str(), &output.data(), &output_data_size)) {
                output.clear();
            }
            return output;
        }*/

        static size_t GetFileSize(fs::path path)
        {
            size_t size = 0;
            meow_hook::func_call<bool>(GetAddress(anno::FILE_GET_FILE_SIZE),
                                       *(uintptr_t*)GetAddress(anno::SOME_GLOBAL_STRUCTURE_ARCHIVE),
                                       path.wstring(), &size);
            return size;
        }

        static void SIZE_CHECK()
        {
            static_assert(offsetof(CFile, file_path) == 0x8);
            static_assert(offsetof(CFile, file_handle) == 0x78);
            static_assert(offsetof(CFile, buffer) == 0xA0);
        }
    };

    class CArchiveFile : public CFile
    {
      public:
    };

    class CFileDatabase
    {
      public:
        virtual ~CFileDatabase() = 0;
    };
#pragma pack(pop)

} // namespace rdsdk
} // namespace anno
