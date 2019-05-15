#pragma once

#include <Windows.h>

#include <cstdint>
#include <string>

#include "udis86.h"

bool         set_import(const std::string& name, uintptr_t func);
inline void* AllocateFunctionStub(void* function)
{
    char* code              = reinterpret_cast<char*>(malloc(20));
    *(uint8_t*)code         = 0x48;
    *(uint8_t*)(code + 1)   = 0xb8;
    *(uint64_t*)(code + 2)  = (uint64_t)function;
    *(uint16_t*)(code + 10) = 0xE0FF;
    *(uint64_t*)(code + 12) = 0xCCCCCCCCCCCCCCCC;
    return code;
}

static uintptr_t base_address = 0;
inline uint64_t  adjust_address(uint64_t address)
{
    if (base_address == 0) {
        base_address = uintptr_t(GetModuleHandle(NULL));
    }
    const auto offset = address - uint64_t(0x140000000);
    return base_address + offset;
}

template <typename AddressType> inline void nop(AddressType address, size_t length)
{
    const auto offset = address - 0x140000000;
    DWORD      oldProtect;
    VirtualProtect((void*)(base_address + offset), length, PAGE_EXECUTE_READWRITE, &oldProtect);

    memset((void*)(base_address + offset), 0x90, length);

    VirtualProtect((void*)(base_address + offset), length, oldProtect, &oldProtect);
}

template <typename ValueType, typename AddressType>
inline void put(AddressType address, ValueType value)
{
    address = adjust_address(address);
    DWORD oldProtect;
    VirtualProtect((void*)(address), sizeof(value), PAGE_EXECUTE_READWRITE, &oldProtect);

    memcpy((void*)(address), &value, sizeof(value));

    VirtualProtect((void*)(address), sizeof(value), oldProtect, &oldProtect);
}

template <typename AddressType> inline void retn(AddressType address, uint16_t stackSize = 0)
{
    if (stackSize == 0) {
        put<uint8_t>(address, 0xC3);
    } else {
        put<uint8_t>(address, 0xC2);
        put<uint16_t>(address + 1, stackSize);
    }
}

template <typename T, typename AT> inline void jump_abs(AT address, T func)
{
#pragma pack(push, 1)
    struct PatchCode {
        // This struct contains roughly the following code:
        uint16_t mov_rax;
        uint64_t ptr;
        uint16_t jmp_rax;
    };
#pragma pack(pop)
    address = adjust_address(address);
    PatchCode patch;
    patch.mov_rax = 0xB848;
    patch.ptr     = (uint64_t)func;
    patch.jmp_rax = 0xE0FF;
    DWORD oldProtect;
    VirtualProtect((void*)(address), sizeof(PatchCode), PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)address, &patch, sizeof(PatchCode));
    VirtualProtect((void*)(address), sizeof(PatchCode), oldProtect, &oldProtect);
}

template <typename ValueType, typename AddressType>
inline uintptr_t* detour_func(AddressType address, ValueType target)
{
    auto* code = reinterpret_cast<char*>(AllocateFunctionStub(target));
    address    = adjust_address(address);
    ud_t ud;
    ud_init(&ud);

    ud_set_mode(&ud, 64);

    uint64_t k = address;
    ud_set_pc(&ud, k);
    ud_set_input_buffer(&ud, reinterpret_cast<uint8_t*>(address), INT64_MAX);

    auto opsize = ud_disassemble(&ud);
    while (opsize <= 12) {
        opsize += ud_disassemble(&ud);
    }

    opsize += 12;

    auto orig_code = reinterpret_cast<char*>(malloc(opsize));

    opsize -= 12;

    memcpy(orig_code, (void*)address, opsize);
    auto code2               = orig_code + opsize;
    *(uint8_t*)code2         = 0x48;
    *(uint8_t*)(code2 + 1)   = 0xb8;
    *(uint64_t*)(code2 + 2)  = (uint64_t)(address + opsize);
    *(uint16_t*)(code2 + 10) = 0xE0FF;

    DWORD oldProtect;
    VirtualProtect((void*)address, 12, PAGE_EXECUTE_READWRITE, &oldProtect);

    memcpy((void*)address, code, 12);

    VirtualProtect((void*)address, 12, oldProtect, &oldProtect);
    VirtualProtect((void*)orig_code, opsize, PAGE_EXECUTE_READWRITE, &oldProtect);
    return (uintptr_t*)orig_code;
}

template <typename R> static R __thiscall func_call(uint64_t addr)
{
    return ((R(*)())(adjust_address(addr)))();
}

template <typename R, typename... Args> struct func_call_member_helper {
    using func_t = R (func_call_member_helper::*)(Args...) const;

    R operator()(func_t func_ptr, Args... args) const
    {
        return (this->*func_ptr)(args...);
    }
};

template <typename R, class T, typename... Args>
R func_call_member(uint64_t addr, T _this, Args... args)
{
    const func_call_member_helper<R, Args...>* helper =
        reinterpret_cast<const func_call_member_helper<R, Args...>*>(_this);
    auto           func_ptr = *(typename std::remove_pointer_t<decltype(helper)>::func_t*)(&addr);
    return helper->operator()(func_ptr, args...);
}

template <typename R, class T, typename... Args>
inline std::enable_if_t<!std::is_pointer_v<T> || !std::is_class_v<std::remove_pointer_t<T>>, R>
func_call(uint64_t addr, T _this, Args... args)
{
    addr = adjust_address(addr);
    return ((R(*)(T, Args...))(addr))(_this, args...);
}

template <typename R, class T, typename... Args>
inline std::enable_if_t<std::is_pointer_v<T> && std::is_class_v<std::remove_pointer_t<T>>, R>
func_call(uint64_t addr, T _this, Args... args)
{
    addr = adjust_address(addr);
    return func_call_member<R, T, Args...>(addr, _this, args...);
}