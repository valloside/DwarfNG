#ifndef __GLOBAL_HPP
#define __GLOBAL_HPP

#ifndef LIBDWARF_STATIC
#define LIBDWARF_STATIC
#endif
#include "libdwarf.h"
#include "dwarf.h"
#include <cstdint>
#include <vector>
#include <string>

namespace dw
{
    // Dwarf_Global
    class global
    {
        Dwarf_Global *mRawGlobal;
        Dwarf_Debug mRawDebug;
        int64_t mCount;

    public:
        global(Dwarf_Global *glob, Dwarf_Debug dbg, int64_t count)
            : mRawGlobal(glob), mRawDebug(dbg), mCount(count) {}

        ~global()
        {
            dwarf_globals_dealloc(this->mRawDebug, this->mRawGlobal, this->mCount);
        }

        std::vector<std::string> getAllNames()
        {
            char *name = nullptr;
            Dwarf_Error error;
            std::vector<std::string> ret;
            ret.reserve(this->mCount);
            for (size_t i = 0; i < this->mCount; i++)
            {
                int res = dwarf_globname(this->mRawGlobal[i], &name, &error);
                if (res == DW_DLV_OK)
                {
                    ret.emplace_back(name);
                }
            }
            return ret;
        }

        uint64_t getDIEoffset()
        {
            uint64_t offset = UINT64_MAX;
            Dwarf_Error error;
            int res = dwarf_global_die_offset(*this->mRawGlobal, &offset, &error);
            if (res == DW_DLV_OK)
            {
                return offset;
            }
            return UINT64_MAX;
        }

        uint64_t getCUoffset()
        {
            uint64_t offset = UINT64_MAX;
            Dwarf_Error error;
            int res = dwarf_global_cu_offset(*this->mRawGlobal, &offset, &error);
            if (res == DW_DLV_OK)
            {
                return offset;
            }
            return UINT64_MAX;
        }

        char *operator[](int idx)
        {
            char *name = nullptr;
            if (idx > this->mCount)
                return name;

            Dwarf_Error error;
            dwarf_globname(this->mRawGlobal[idx], &name, &error);
            return name;
        }
    };
} // namespace dw

#endif // __GLOBAL_HPP