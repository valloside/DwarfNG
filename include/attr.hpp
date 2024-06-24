#ifndef __ATTR_HPP
#define __ATTR_HPP

#ifndef LIBDWARF_STATIC
#define LIBDWARF_STATIC
#endif
#include "dwarf.h"
#include "libdwarf.h"
#include <cstdint>
#include <string>
#include <variant>
#include "constant.h"

namespace dw
{
    class attr
    {
    public:
        typedef std::variant<std::string, uint64_t, uint32_t, int64_t, int32_t> value;

    private:
        uint64_t mOffset = 0;
        uint16_t mType;
        uint16_t mForm;
        value mValue;

    public:
        attr(uint64_t offset, value attrValue, Dwarf_Half attrType, Dwarf_Half attrForm)
            : mOffset(offset), mValue(attrValue), mType(attrType), mForm(attrForm) {}

        uint64_t getOffset() const { return this->mOffset; }

        uint16_t getType() const { return this->mType; }

        /**
         * @return `nullptr` if something went wrong
         * @warning do not modify or dealloc the returned value in any case
         */
        const char *getAttrName() const
        {
            const char *tempStr = nullptr;
            dwarf_get_AT_name(this->mType, &tempStr);
            std::string a;
            return tempStr;
        }

        uint16_t getAttrForm() const { return this->mForm; }

        /**
         * @return `nullptr` if something went wrong
         * @warning do not modify or dealloc the returned value in any case
         */
        const char *getAttrForm_str() const
        {
            const char *tempStr = nullptr;
            dwarf_get_FORM_name(this->mForm, &tempStr);
            return tempStr;
        }

        const value &getAttrValue() const { return this->mValue; }

        std::string getAttrValue_str() const
        {
            return std::visit(
                [](const auto &var)
                {
                    if constexpr (std::is_same_v<std::decay_t<decltype(var)>, std::string>)
                    {
                        return var;
                    }
                    else if constexpr (std::is_same_v<std::decay_t<decltype(var)>, uint64_t>)
                    {
                        return std::to_string(var);
                    }
                    else if constexpr (std::is_same_v<std::decay_t<decltype(var)>, uint32_t>)
                    {
                        return std::to_string(var);
                    }
                    else if constexpr (std::is_same_v<std::decay_t<decltype(var)>, int64_t>)
                    {
                        return std::to_string(var);
                    }
                    else if constexpr (std::is_same_v<std::decay_t<decltype(var)>, int32_t>)
                    {
                        return std::to_string(var);
                    }
                },
                this->mValue);
        }

        bool operator==(const attr rhs) const
        {
            return this->mOffset == rhs.mOffset;
        }

        bool operator==(const uint64_t off) const
        {
            return this->mOffset == off;
        }

        bool operator==(const uint16_t type) const
        {
            return this->mType == type;
        }

        bool operator==(const std::string &name) const
        {
            return this->getAttrForm_str() == name;
        }
    };

} // namespace dw

#endif // __ATTR_HPP