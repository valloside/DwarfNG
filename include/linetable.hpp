#ifndef __LINETABLE_HPP
#define __LINETABLE_HPP

#ifndef LIBDWARF_STATIC
#define LIBDWARF_STATIC
#endif
#include "dwarf.h"
#include "libdwarf.h"
#include <cstdint>
#include <vector>
#include <string>

namespace dw
{
    class linetable
    {
        Dwarf_Line_Context mRawLineContext = nullptr;
        uint64_t mVersion = static_cast<uint64_t>(-1);

    public:
        linetable() {}

        linetable(Dwarf_Line_Context rawLineContext, uint64_t version)
            : mRawLineContext(rawLineContext), mVersion(version) {}

        linetable(dw::linetable &&other) noexcept
            : mRawLineContext(std::move(other.mRawLineContext)), mVersion(mVersion) {}

        dw::linetable &operator=(const dw::linetable &other) = delete;
        dw::linetable &operator=(dw::linetable &&other) noexcept = default;

        ~linetable()
        {
            dwarf_srclines_dealloc_b(mRawLineContext);
        }

        struct line
        {
            // todo
        };

        std::vector<line> getSrclines()
        {
            Dwarf_Line *lines;
            Dwarf_Signed linecount;
            Dwarf_Error error;
            std::vector<line> ret;
            int res = dwarf_srclines_from_linecontext(this->mRawLineContext, &lines, &linecount, &error);
            if (res != DW_DLV_OK || linecount == 0)
                return ret;

            for (int i = 0; i < linecount; i++)
            {
                // todo
            }
        }

        std::vector<std::string> getIncludeList()
        {
            std::vector<std::string> ret;
            Dwarf_Signed count;
            Dwarf_Error error;
            int res = dwarf_srclines_include_dir_count(this->mRawLineContext, &count, &error);
            if (res != DW_DLV_OK || count == 0)
                return ret;

            ret.reserve(count);
            const char *data;
            for (size_t i = 0; i < count; i++)
            {
                res = dwarf_srclines_include_dir_data(this->mRawLineContext, i, &data, &error);
                if (res == DW_DLV_OK)
                    ret.emplace_back(data);
            }
            return ret;
        }
    };
} // namespace dw

#endif // __LINETABLE_HPP