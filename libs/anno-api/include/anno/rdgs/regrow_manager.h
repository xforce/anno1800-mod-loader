#pragma once

#include "meow_hook/util.h"

namespace anno::rdgs
{
#pragma pack(push, 1)
class CRegrowManager
{
  public:
    [[nodiscard]] static CRegrowManager& Instance()
    {
        return *meow_hook::func_call<CRegrowManager*>(0x1403B4A30);
    }

    bool HasTreeAtPos(float pos[2]) const
    {
        return meow_hook::func_call<bool>(0x14035E020, this, pos);
    }

    struct StrangePlantTreeConfig {
        char pad[0x20];
        uint64_t
            growth_time; // 0x20 NOTE(alexander): Must be at least 2 and should be divisible by 2
    };
    void PlantTree(int x, int y, const StrangePlantTreeConfig& conf, float unk)
    {
        meow_hook::func_call<void>(0x14035E170, this, x, y, conf, unk);
    }
};
#pragma pack(pop)

} // namespace anno::rdgs
