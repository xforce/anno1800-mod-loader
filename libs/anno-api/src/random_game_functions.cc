#include "anno/random_game_functions.h"

namespace anno
{
static uintptr_t ADDRESSES[Address::SIZE] = {
    0x145394060, // GET_CONTAINER_BLOCK_INFO
    0x14004F840  // READ_FILE_FROM_CONTAINER
};

uintptr_t GetAddress(Address address)
{
    return ADDRESSES[address];
}
void SetAddress(Address address, uint64_t add)
{
    ADDRESSES[address] = add;
}

} // namespace anno