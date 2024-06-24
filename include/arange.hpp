#ifndef __ARANGE_HPP
#define __ARANGE_HPP

#ifndef LIBDWARF_STATIC
#define LIBDWARF_STATIC
#endif
#include "libdwarf.h"
#include "dwarf.h"
#include <cstdint>

namespace dw
{
    class arange
    {
        Dwarf_Arange mRawArange;
        Dwarf_Debug mRawDebug;

    public:
        arange(Dwarf_Arange rawArange, Dwarf_Debug rawDebug)
            : mRawArange(rawArange), mRawDebug(rawDebug) {}

        ~arange()
        {
            dwarf_dealloc(this->mRawDebug, mRawArange, DW_DLA_ARANGE);
        }

        uint64_t getCUoffset()
        {
            uint64_t offset = UINT64_MAX;
            Dwarf_Error err;
            int res = dwarf_get_cu_die_offset(this->mRawArange, &offset, &err);
            if (res == DW_DLV_OK)
            {
                return offset;
            }
            return UINT64_MAX;
        }

        uint64_t getCUheaderOffset()
        {
            uint64_t offset = UINT64_MAX;
            Dwarf_Error err;
            int res = dwarf_get_arange_cu_header_offset(this->mRawArange, &offset, &err);
            if (res == DW_DLV_OK)
            {
                return offset;
            }
            return UINT64_MAX;
        }

        // int getData()
        // {
        //     Dwarf_Unsigned segment = 0, segment_entry_size = 0, length = 0;
        //     Dwarf_Addr start = 0;
        //     Dwarf_Off cu_die_offset = 0;
        //     Dwarf_Error error;
        //     int res = dwarf_get_arange_info_b(this->mRawArange, &segment, &segment_entry_size,
        //                                       &start, &length, &cu_die_offset, &error);
        //     // todo
        // }
    };
} // namespace arange

#endif // __ARANGE_HPP