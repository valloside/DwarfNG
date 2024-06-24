#ifndef __UTILS_HPP
#define __UTILS_HPP

#ifndef LIBDWARF_STATIC
#define LIBDWARF_STATIC
#endif
#include "dwarf.h"
#include "libdwarf.h"
#include <string>

namespace dw
{
    // spit by the last "/"
    std::pair<std::string, std::string> splitPath(const std::string &fullPath)
    {
        std::string directory;
        std::string fileName;
        size_t lastSlashPos = fullPath.find_last_of("/\\");
        if (lastSlashPos != std::string::npos)
            return {fullPath.substr(0, lastSlashPos), fullPath.substr(lastSlashPos + 1)};
        else
            return {"", fullPath};
    }

    // spit by the last "."
    std::pair<std::string, std::string> splitExtension(const std::string &path)
    {
        std::string base;
        std::string extension;
        size_t lastDotPos = path.find_last_of('.');
        if (lastDotPos != std::string::npos && lastDotPos != path.length() - 1)
            return std::make_pair(path.substr(0, lastDotPos), path.substr(lastDotPos + 1));
        else
            return std::make_pair(path, "");
    }

    /**
     * @brief return a C-style string of the DW_TAG_xxx value, do not modify returned value
     * @param tag DW_TAG_xxx value
     * @return C-style string
     */
    const char *getStringOfTAG(uint16_t tag)
    {
        const char *tempStr = nullptr;
        dwarf_get_AT_name(tag, &tempStr);
        return tempStr;
    }

    // std::string cxx_demangler(const std::string &symbol)
    // {
    //     char *demangled;
    //     int status = 0;
    //     demangled = abi::__cxa_demangle(symbol.c_str(), nullptr, nullptr, &status);
    //     if (!demangled)
    //         return symbol;
    //     std::string result(demangled);
    //     free(demangled);
    //     return result;
    // }

}

#endif // __UTILS_HPP