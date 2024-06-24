#ifndef __DWARFNG_HPP
#define __DWARFNG_HPP

#ifndef LIBDWARF_STATIC
#define LIBDWARF_STATIC
#endif
#include "dwarf.h"
#include "libdwarf.h"
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include "attr.hpp"
#include "global.hpp"
#include "arange.hpp"
#include "linetable.hpp"
#include "utils.hpp"

namespace dw
{
    class file;

    /**
     * @brief debugging info entry
     */
    class die
    {
    protected:
        friend class file;

        std::vector<dw::die> mChildren;
        std::vector<dw::attr> mAttrs;

        uint64_t mOffset;
        uint16_t mTAG;
        bool mHasChildren;

    public:
        /**
         * @brief debugging info entry
         * @param raw_die a ptr to the raw die from `libdwarf`, must be dealloc right away
         * @param parent parent die, or nullptr if not exists
         * @param file which `dw::file` contain this die
         */
        die(Dwarf_Die raw_die, dw::die *parent, dw::file *file);
        die(const dw::die &die) = delete;
        die(dw::die &&die) noexcept;

        bool hasChild() const { return this->mHasChildren; }

        virtual bool isCompileUnit() const { return false; }

        // get value of `DW_AT_name`
        const std::string &getName() const;

        // get the value of `DW_TAG_xxx`
        uint16_t getTAG() const { return this->mTAG; }

        /**
         * @brief get the value of `DW_TAG_xxx` in C-style string form
         * @return `nullptr` if something went wrong
         * @warning do not modify or dealloc the returned value in any case
         */
        const char *getTAG_str() const;

        uint64_t getOffset() const { return this->mOffset; }
        const std::vector<dw::attr> &getAttrs() const { return this->mAttrs; }

        std::vector<dw::die> &getChildren(dw::file &dwFile);
        const std::vector<dw::die> &getChildren(dw::file &dwFile) const;

        const dw::attr *findAttrByOffset(uint64_t off) const;
        const dw::attr *finAttrByType(uint16_t type) const;
        const dw::attr *findAttrByName(const std::string &name) const;

    protected:
        void _initAttrs(Dwarf_Die raw_die);

        void _initChildren(Dwarf_Die raw_die, dw::file &dwFile);

        std::vector<dw::die>::iterator _findChildByOffset(uint64_t offset, dw::file &dwFile);
    };

    class CU : public die
    {
        dw::linetable mLineTable;
        std::vector<std::string> mSrcfiles;

    public:
        CU(Dwarf_Die raw_die, dw::die *parent, dw::file *file) : die(raw_die, parent, file) {}
        CU(dw::CU &&other) noexcept : die(std::move(other)),
                                      mLineTable(std::move(other.mLineTable)),
                                      mSrcfiles(std::move(other.mSrcfiles)) {}

        virtual bool isCompileUnit() const override { return true; }

        const std::vector<std::string> getSrcfiles(dw::file &dwFile);
        dw::linetable &getLineTable(dw::file &dwFile);
        dw::linetable &getLineTable(dw::file &dwFile) const;
    };

    class file
    {
        friend class die;
        friend class CU;
        std::string mFilePath;
        int mStatue = 1; // 0: success; 1: error; -1: no dwarf

        Dwarf_Debug mRawDbg;
        std::vector<dw::CU> mCompileUnits;

    public:
        file() {}

        /**
         * @brief open an executable with dwarf info
         * @param filePath path to the executable
         */
        file(const std::string &filePath);
        file(dw::file &&other) noexcept;
        dw::file &operator=(const dw::file &other) = delete;
        dw::file &operator=(dw::file &&other) noexcept;
        ~file();

        bool open(const std::string &filePath);

        bool isOpen() const { return this->mStatue == 0; }

        /**
         * @brief 0: success; \n 1: error; \n -1: no dwarf
         */
        int getStatue() const { return this->mStatue; }

        const std::vector<dw::CU> &getCUs() const { return this->mCompileUnits; }

        dw::die *findDIEbyOffset(uint64_t offset);
        const dw::die *findDIEbyOffset(uint64_t offset) const;

        dw::die *findDIEbyHashSignature();
        const dw::die *findDIEbyHashSignature() const;

        dw::global fastAccessToPubnames();
        dw::global fastAccessToPubtypes();

        std::vector<dw::arange> getAranges();

    private:
        void _init();

        void _clearAll();

        Dwarf_Die _getRawDieByOffset(const uint64_t &offset);
    };

} // namespace dw

/* ====================================================================================== */

inline dw::file::file(const std::string &filePath) : mFilePath(filePath)
{
    this->_init();
}

inline dw::file::file(dw::file &&other) noexcept
{
    this->mFilePath = std::move(other.mFilePath);
    this->mStatue = other.mStatue;
    if (this->mRawDbg)
        dwarf_finish(this->mRawDbg);
    this->mRawDbg = other.mRawDbg;
    other.mRawDbg = nullptr;
    this->mCompileUnits = std::move(other.mCompileUnits);
}

inline dw::file &dw::file::operator=(dw::file &&other) noexcept
{
    this->mFilePath = std::move(other.mFilePath);
    this->mStatue = other.mStatue;
    if (this->mRawDbg)
        dwarf_finish(this->mRawDbg);
    this->mRawDbg = other.mRawDbg;
    other.mRawDbg = nullptr;
    this->mCompileUnits = std::move(other.mCompileUnits);

    return *this;
}

inline dw::file::~file()
{
    if (this->mRawDbg)
        dwarf_finish(this->mRawDbg);
}

inline bool dw::file::open(const std::string &filePath)
{
    if (this->mRawDbg)
        this->_clearAll();

    this->mFilePath = filePath;
    this->_init();
    return this->isOpen();
}

inline dw::die *dw::file::findDIEbyOffset(uint64_t offset)
{
    auto it = std::upper_bound(this->mCompileUnits.begin(), this->mCompileUnits.end(),
                               offset, [](const uint64_t &a, const dw::die &b) -> bool
                               { return a < b.getOffset(); });
    --it;
    if (it->getOffset() == offset)
        return &*it;
    Dwarf_Die retDie = this->_getRawDieByOffset(it->getOffset());

    it->_initChildren(retDie, *this);
    dwarf_dealloc_die(retDie);

    auto found = it->_findChildByOffset(offset, *this);
    if (found == it->getChildren(*this).end())
        return nullptr;
    return &*found;
}

inline const dw::die *dw::file::findDIEbyOffset(uint64_t offset) const
{
    return const_cast<dw::file *>(this)->findDIEbyOffset(offset);
}

inline dw::die *dw::file::findDIEbyHashSignature()
{
    return nullptr; // todo
}

inline const dw::die *dw::file::findDIEbyHashSignature() const
{
    return nullptr; // todo
}

inline dw::global dw::file::fastAccessToPubnames()
{
    Dwarf_Global *globs = nullptr;
    Dwarf_Signed count = 0;
    Dwarf_Error error;
    int res = dwarf_get_globals(this->mRawDbg, &globs, &count, &error);
    if (res == DW_DLV_OK)
    {
        return dw::global{globs, this->mRawDbg, count};
    }

    return dw::global{nullptr, nullptr, 0};
}

inline dw::global dw::file::fastAccessToPubtypes()
{
    Dwarf_Global *globs = nullptr;
    Dwarf_Signed count = 0;
    Dwarf_Error error;
    int res = dwarf_get_pubtypes(this->mRawDbg, &globs, &count, &error);
    if (res == DW_DLV_OK)
    {
        return dw::global{globs, this->mRawDbg, count};
    }

    return dw::global{nullptr, nullptr, 0};
}

inline std::vector<dw::arange> dw::file::getAranges()
{
    Dwarf_Signed count = 0;
    Dwarf_Arange *arange = 0;
    Dwarf_Error error;
    std::vector<dw::arange> ret;
    int res = dwarf_get_aranges(this->mRawDbg, &arange, &count, &error);
    if (res == DW_DLV_OK)
    {
        ret.reserve(count);
        for (int i = 0; i < count; ++i)
        {
            ret.emplace_back(arange[i], this->mRawDbg);
        }
    }
    return ret;
}

inline void dw::file::_init()
{
    // open the executable
    char true_pathbuf[FILENAME_MAX];
    Dwarf_Error error;
    this->mStatue = dwarf_init_path(mFilePath.c_str(), true_pathbuf,
                                    FILENAME_MAX, DW_GROUPNUMBER_ANY,
                                    nullptr, nullptr,
                                    &this->mRawDbg, &error);

    // get the compile units
    Dwarf_Unsigned abbrev_offset, typeoffset, next_cu_header;
    Dwarf_Half address_size, version_stamp, offset_size, extension_size, header_cu_type;
    Dwarf_Sig8 signature;
    Dwarf_Bool is_info = true;
    while (1)
    {
        Dwarf_Die raw_CU_die = 0;
        Dwarf_Unsigned cu_header_length = 0;
        int res = dwarf_next_cu_header_e(this->mRawDbg, is_info,
                                         &raw_CU_die, &cu_header_length,
                                         &version_stamp, &abbrev_offset,
                                         &address_size, &offset_size, &extension_size,
                                         &signature, &typeoffset,
                                         &next_cu_header, &header_cu_type, &error);
        if (res == DW_DLV_ERROR)
            return;

        // if no entry when is_info is true, set is_info to false and try again.
        if (res == DW_DLV_NO_ENTRY)
        {
            if (is_info == true)
            {
                is_info = false;
                continue;
            }
            return;
        }
        this->mCompileUnits.emplace_back(raw_CU_die, nullptr, this);
        dwarf_dealloc_die(raw_CU_die);
    }
}

inline void dw::file::_clearAll()
{
    dwarf_finish(this->mRawDbg);
    this->mRawDbg = nullptr;
    this->mFilePath.clear();
    this->mStatue = 1;
    this->mCompileUnits.clear();
}

inline Dwarf_Die dw::file::_getRawDieByOffset(const uint64_t &offset)
{
    Dwarf_Die retDie;
    Dwarf_Error err;
    int res = dwarf_offdie_b(this->mRawDbg, offset, 0, &retDie, &err);
    if (res != DW_DLV_OK)
        res = dwarf_offdie_b(this->mRawDbg, offset, 1, &retDie, &err);
    if (res != DW_DLV_OK)
        return nullptr;
    return retDie;
}

/* ====================================================================================== */

inline dw::die::die(Dwarf_Die raw_die, dw::die *parent, dw::file *file)
{
    // get the die offset
    Dwarf_Off off;
    dwarf_dieoffset(raw_die, &off, 0);
    this->mOffset = off;

    // get TAG info
    Dwarf_Half tagType = 0;
    dwarf_tag(raw_die, &tagType, 0);
    this->mTAG = tagType;

    this->_initAttrs(raw_die);
}

inline dw::die::die(dw::die &&die) noexcept
{
    this->mOffset = die.mOffset;
    this->mTAG = die.mTAG;
    this->mHasChildren = die.mHasChildren;
    this->mAttrs = std::move(die.mAttrs);
    this->mChildren = std::move(die.mChildren);
}

inline const std::string &dw::die::getName() const
{
    auto found = std::find(this->mAttrs.begin(), this->mAttrs.end(), (uint16_t)DW_AT_name);
    if (found != this->mAttrs.end())
        return std::get<std::string>(found->getAttrValue());
    else
        return EMPTY_STRING;
}

inline const char *dw::die::getTAG_str() const
{
    const char *tagName = nullptr;
    dwarf_get_TAG_name(mTAG, &tagName);
    return tagName;
}

inline std::vector<dw::die> &dw::die::getChildren(dw::file &dwFile)
{
    if (this->mHasChildren && this->mChildren.empty())
    {
        Dwarf_Die raw_die = dwFile._getRawDieByOffset(this->mOffset);
        this->_initChildren(raw_die, dwFile);
        dwarf_dealloc_die(raw_die);
    }
    return this->mChildren;
}

inline const std::vector<dw::die> &dw::die::getChildren(dw::file &dwFile) const
{
    return const_cast<dw::die *>(this)->getChildren(dwFile);
}

inline const std::vector<std::string> dw::CU::getSrcfiles(dw::file &dwFile)
{
    char **declFiles = nullptr;
    if (!this->mSrcfiles.empty())
        return this->mSrcfiles;

    std::vector<std::string> files;
    Dwarf_Signed fileCount;
    int res = dwarf_srcfiles(dwFile._getRawDieByOffset(this->getOffset()),
                             &declFiles, &fileCount, nullptr);
    if (res == DW_DLV_OK)
    {
        this->mSrcfiles.reserve(fileCount);
        for (Dwarf_Signed i = 0; i < fileCount; i++)
        {
            this->mSrcfiles.push_back(declFiles[i]);
            dwarf_dealloc(dwFile.mRawDbg, declFiles[i], DW_DLA_STRING);
            declFiles[i] = nullptr;
        }
    }
    return this->mSrcfiles;
}

inline dw::linetable &dw::CU::getLineTable(dw::file &dwFile)
{
    Dwarf_Unsigned version;
    Dwarf_Small count;
    Dwarf_Line_Context context;
    Dwarf_Error error;
    int res = dwarf_srclines_b(dwFile._getRawDieByOffset(this->getOffset()),
                               &version, &count, &context, &error);
    this->mLineTable = dw::linetable(context, version);
    return this->mLineTable;
}

inline dw::linetable &dw::CU::getLineTable(dw::file &dwFile) const
{
    return const_cast<dw::CU *>(this)->getLineTable(dwFile);
}

inline const dw::attr *dw::die::findAttrByOffset(uint64_t off) const
{
    auto found = std::find(this->mAttrs.begin(), this->mAttrs.end(), off);
    return found == this->mAttrs.end() ? nullptr : &*found;
}

inline const dw::attr *dw::die::finAttrByType(uint16_t type) const
{
    auto found = std::find(this->mAttrs.begin(), this->mAttrs.end(), type);
    return found == this->mAttrs.end() ? nullptr : &*found;
}

inline const dw::attr *dw::die::findAttrByName(const std::string &name) const
{
    auto found = std::find(this->mAttrs.begin(), this->mAttrs.end(), name);
    return found == this->mAttrs.end() ? nullptr : &*found;
}

inline std::vector<dw::die>::iterator dw::die::_findChildByOffset(uint64_t offset, dw::file &dwFile)
{

    auto it = std::upper_bound(this->mChildren.begin(), this->mChildren.end(),
                               offset, [](const uint64_t &a, const dw::die &b) -> bool
                               { return a < b.getOffset(); });
    --it;
    if (it->getOffset() == offset)
        return it;
    uint64_t it_offset = it->getOffset();
    Dwarf_Die retDie;
    Dwarf_Error err;
    int res = dwarf_offdie_b(dwFile.mRawDbg, it_offset, false, &retDie, &err);
    if (res != DW_DLV_OK)
        res = dwarf_offdie_b(dwFile.mRawDbg, it_offset, true, &retDie, &err);
    if (res != DW_DLV_OK)
        return this->mChildren.end();

    it->_initChildren(retDie, dwFile);
    dwarf_dealloc_die(retDie);
    std::vector<dw::die> &childlist = it->getChildren(dwFile);

    auto found = it->_findChildByOffset(offset, dwFile);
    if (found == childlist.end())
        return this->mChildren.end();
    return found;
}

inline void dw::die::_initChildren(Dwarf_Die raw_die, dw::file &dwFile)
{
    if (!this->mHasChildren || !this->mChildren.empty())
        return;

    Dwarf_Die raw_iter_child, raw_siblingdie;
    for (int res = dwarf_child(raw_die, &raw_iter_child, nullptr); res == DW_DLV_OK;)
    {
        this->mChildren.emplace_back(raw_iter_child, this, &dwFile);
        res = dwarf_siblingof_c(raw_iter_child, &raw_siblingdie, 0);
        dwarf_dealloc_die(raw_iter_child);
        raw_iter_child = raw_siblingdie;
    }
}

inline void dw::die::_initAttrs(Dwarf_Die raw_die)
{
    Dwarf_Attribute *attrList;
    Dwarf_Signed attrCount;
    Dwarf_Error err = 0;
    Dwarf_Half hasChildren = 0;
    dwarf_die_abbrev_children_flag(raw_die, &hasChildren);
    this->mHasChildren = hasChildren != 0;
    int res = dwarf_attrlist(raw_die, &attrList, &attrCount, &err);
    if (res != DW_DLV_OK || !attrCount)
        return;

    for (int i = 0; i < attrCount; i++)
    {
        Dwarf_Half attrType;
        Dwarf_Half attrForm;
        Dwarf_Off attrOffset;
        dwarf_whatattr(attrList[i], &attrType, nullptr);
        dwarf_attr_offset(raw_die, attrList[i], &attrOffset, nullptr);
        dwarf_whatform(attrList[i], &attrForm, &err);
        switch (attrType)
        {
        case DW_AT_name: // DW_FORM_strp
        case DW_AT_comp_dir:
        case DW_AT_producer:
        case DW_AT_linkage_name:
        {
            char *value;
            res = dwarf_formstring(attrList[i], &value, &err);
            if (res != DW_DLV_OK)
                break;
            this->mAttrs.emplace_back(attrOffset, value, attrType, attrForm);
            break;
        }
        case DW_AT_accessibility: // DW_FORM_data1 DW_FORM_data2
        case DW_AT_decl_line:
        case DW_AT_decl_column:
        case DW_AT_encoding:
        case DW_AT_upper_bound:
        case DW_AT_count:
        case DW_AT_defaulted:
        case DW_AT_inline:
        case DW_AT_data_member_location:
        case DW_AT_virtuality:
        {
            Dwarf_Unsigned data;
            res = dwarf_formudata(attrList[i], &data, &err);
            if (res != DW_DLV_OK)
                break;
            this->mAttrs.emplace_back(attrOffset, data, attrType, attrForm);
            break;
        }
        case DW_AT_type: // DW_FORM_ref4
        case DW_AT_import:
        case DW_AT_specification:
        case DW_AT_object_pointer:
        case DW_AT_sibling:
        case DW_AT_containing_type:
        {
            Dwarf_Off data;
            Dwarf_Bool dw_is_info;
            res = dwarf_global_formref_b(attrList[i], &data, &dw_is_info, &err);
            if (res != DW_DLV_OK)
                break;
            this->mAttrs.emplace_back(attrOffset, data, attrType, attrForm);
            break;
        }
        case DW_AT_external: // DW_FORM_flag_present
        case DW_AT_declaration:
        case DW_AT_enum_class:
        case DW_AT_artificial:
        case DW_AT_GNU_all_call_sites:
        case DW_AT_noreturn:
        {
            Dwarf_Bool data;
            res = dwarf_formflag(attrList[i], &data, &err);
            if (res != DW_DLV_OK)
                break;
            this->mAttrs.emplace_back(attrOffset, data, attrType, attrForm);
            break;
        }
        case DW_AT_const_value: // DW_FORM_sdata udata data1
        {
            if (attrForm == DW_FORM_sdata)
            {
                Dwarf_Signed data;
                res = dwarf_formsdata(attrList[i], &data, &err);
                if (res != DW_DLV_OK)
                    break;
                this->mAttrs.emplace_back(attrOffset, data, attrType, attrForm);
            }
            else
            {
                Dwarf_Unsigned udata;
                res = dwarf_formudata(attrList[i], &udata, &err);
                if (res != DW_DLV_OK)
                    break;
                this->mAttrs.emplace_back(attrOffset, udata, attrType, attrForm);
            }

            break;
        }
        case DW_AT_language:
        {
            Dwarf_Unsigned lang;
            res = dwarf_srclang(raw_die, &lang, &err);
            if (res != DW_DLV_OK)
                break;
            this->mAttrs.emplace_back(attrOffset, lang, attrType, attrForm);
            break;
        }
        case DW_AT_low_pc:
        {
            Dwarf_Addr addr;
            res = dwarf_lowpc(raw_die, &addr, &err);
            if (res != DW_DLV_OK)
                break;
            this->mAttrs.emplace_back(attrOffset, addr, attrType, attrForm);
            break;
        }
        case DW_AT_high_pc:
        {
            Dwarf_Addr addr;
            Dwarf_Half form;
            Dwarf_Form_Class formClass;
            res = dwarf_highpc_b(raw_die, &addr, &form, &formClass, &err);
            if (res != DW_DLV_OK)
                break;
            this->mAttrs.emplace_back(attrOffset, addr, attrType, form);
            break;
        }
        case DW_AT_byte_size:
        {
            Dwarf_Unsigned size;
            res = dwarf_bytesize(raw_die, &size, &err);
            if (res != DW_DLV_OK)
                break;
            this->mAttrs.emplace_back(attrOffset, size, attrType, attrForm);
            break;
        }
        case DW_AT_decl_file:
        {
            Dwarf_Unsigned fileIdx;
            dwarf_formudata(attrList[i], &fileIdx, &err);
            if (res != DW_DLV_OK)
                break;
            this->mAttrs.emplace_back(attrOffset, fileIdx, attrType, attrForm);
            break;
        }
        case DW_AT_vtable_elem_location:
        {
            Dwarf_Unsigned returnExprlen;
            Dwarf_Ptr blockPtr;
            Dwarf_Loc_Head_c loclist_head;
            Dwarf_Unsigned dw_locentry_count;
            res = dwarf_get_loclist_c(attrList[i], &loclist_head, &dw_locentry_count, &err);
            if (res != DW_DLV_OK)
            {
                break;
            }

            Dwarf_Small loclist_lkind = 0;
            Dwarf_Small lle_value = 0;
            Dwarf_Unsigned rawval1 = 0;
            Dwarf_Unsigned rawval2 = 0;
            Dwarf_Bool debug_addr_unavailable = false;
            Dwarf_Addr lopc = 0;
            Dwarf_Addr hipc = 0;
            Dwarf_Unsigned loclist_expr_op_count = 0;
            Dwarf_Locdesc_c locdesc_entry = 0;
            Dwarf_Unsigned expression_offset = 0;
            Dwarf_Unsigned locdesc_offset = 0;
            res = dwarf_get_locdesc_entry_d(loclist_head, 0, &lle_value, &rawval1, &rawval2,
                                            &debug_addr_unavailable, &lopc, &hipc,
                                            &loclist_expr_op_count, &locdesc_entry,
                                            &loclist_lkind, &expression_offset,
                                            &locdesc_offset, &err);
            if (res != DW_DLV_OK)
            {
                break;
            }
            int opres = 0;
            Dwarf_Small op = 0;
            Dwarf_Unsigned opd1 = 0;
            Dwarf_Unsigned opd2 = 0;
            Dwarf_Unsigned opd3 = 0;
            Dwarf_Unsigned offsetforbranch = 0;
            res = dwarf_get_location_op_value_c(locdesc_entry, 0, &op, &opd1, &opd2, &opd3,
                                                &offsetforbranch, &err);
            if (res != DW_DLV_OK)
            {
                break;
            }
            this->mAttrs.emplace_back(attrOffset, opd1, attrType, attrForm);
            dwarf_dealloc_loc_head_c(loclist_head);
            break;
        }
        default:
            break;
        }
        dwarf_dealloc_attribute(attrList[i]);
    }
}

#endif // __DWARFNG_HPP