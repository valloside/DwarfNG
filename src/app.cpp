#include "app.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_internal.h"
#include "textselect/textselect.hpp"
#include <d3d11.h>
#include <tchar.h>
#include <iostream>
#include <vector>
#include <set>
#include <unordered_set>
#include <map>
#include <format>
#include <algorithm>
#undef max // nice job, microsoft
#include <cmath>
#include "CommDlg.h"

#include "dwarfng.hpp"

namespace
{
    dw::file f{};

    uint64_t selectedTreeNode = 0;
    const dw::die *selectedDie = nullptr;
    bool newFileOpening = false;

    // only contains the file name (path removed)
    std::vector<std::pair<uint64_t, std::string>> CU_name_list;

    // when open a new file
    void initCUlist(const char *path)
    {
        dw::file tmpNew(path);
        if (!tmpNew.isOpen())
        {
            MessageBox(GetActiveWindow(), _T("Failed to read dwarf info.\nMaybe no dwarf info in this file."), _T("Error"), MB_OK);
            return;
        }
        f = std::move(tmpNew);
        const std::vector<dw::CU> &CUlist = f.getCUs();
        CU_name_list.clear();

        for (auto &&cu : CUlist)
        {
            CU_name_list.emplace_back(cu.getOffset(), dw::splitPath(cu.getName()).second);
        }
        selectedTreeNode = 0;
        selectedDie = nullptr;
        newFileOpening = true;
    }

    void initCUlist(const wchar_t *path)
    {
        // wchar_t to char
        int iLen = WideCharToMultiByte(CP_ACP, 0, path, -1, NULL, 0, NULL, NULL);
        char *_char = new char[iLen * sizeof(char)];
        WideCharToMultiByte(CP_ACP, 0, path, -1, _char, iLen, NULL, NULL);
        f.open(_char);
        delete[] _char;
    }

    struct TreeClipper
    {
        // range: [mBegin, mEnd)
        uint64_t mBegin = 0;
        uint64_t mEnd = static_cast<uint64_t>(-1);

        float mBeforeGapSize = 0.0f;
        float mAfterGapSize = 0.0f;

        // number of invisible nodes before and after
        uint32_t mBeforeCount = 0;
        uint32_t mAfterCount = 0;

        // the first visible tree node might be a child node.
        // In this case, we have to record its parent nodes.
        uint32_t mFakeTreeDepth = 0;

        // 1 - before invisible
        // 2 - visible
        // 3 - after invisible
        uint8_t mStatue = 0;
        bool mNeedRefresh = true;
        bool mClicked = true;
        float mTreeNodeHeight = 0.0;

        void refresh()
        {
            float windowSizeY = ImGui::GetWindowSize().y;
            float y = ImGui::GetScrollY();
            // need refresh when scrolled/window resized/treeNode clicked/newfile opened
            this->mNeedRefresh = this->mLastScrollY != y ||
                                 this->mlastWindowSizeY != windowSizeY ||
                                 this->mClicked ||
                                 newFileOpening;
            this->mLastScrollY = y;
            this->mlastWindowSizeY = windowSizeY;
            this->mClicked = false;

            // std::cout << "Need update: " << this->mNeedRefresh;

            this->mBeforeGapSize = mTreeNodeHeight * (this->mBeforeCount - this->mFakeTreeDepth);
            this->mAfterGapSize = mTreeNodeHeight * this->mAfterCount;
        }

        void counter(uint64_t id, uint32_t depth)
        {
            bool visible = ImGui::IsItemVisible();
            if (this->mStatue == 0 && visible)
            {
                this->mBegin = id;
                this->mStatue = 1;
                this->mFakeTreeDepth = depth;
            }
            else if (this->mStatue == 1 && !visible)
            {
                this->mEnd = id;
                this->mStatue = 2;
                ++this->mAfterCount;
            }
            else if (this->mStatue == 0 && !visible)
            {
                ++this->mBeforeCount;
            }
            else if (this->mStatue == 2)
            {
                ++this->mAfterCount;
            }
        }

        void printInfo()
        {
            // std::cout << "\tBegin: " << this->mBegin << "\tEnd: " << this->mEnd
            //           << "\tBefore: " << this->mBeforeCount << " \tAfter: " << this->mAfterCount
            //           << std::endl;
        }

    private:
        float mLastScrollY = 0.0f;
        float mlastWindowSizeY = 0.0f;
    };

    /**
     * @brief show children die tree of `current`
     * @tparam NEED_REFRESH if ture, recalculate the height of the invisible tree nodes
     * @param current dw::die
     * @param clipper TreeClipper, used to clip the invisible tree nodes
     * @param depth the depth of child
     */
    template <bool NEED_REFRESH>
    void showSubTree(const dw::die &current, TreeClipper &clipper, uint32_t depth = 1)
    {
        const std::vector<dw::die> &children = current.getChildren(f);
        for (size_t i = 0; i < children.size(); i++)
        {
            const dw::die &child = children[i];
            uint64_t childOffset = child.getOffset();
            if constexpr (!NEED_REFRESH)
            {
                if (children.size() > i + 1 && clipper.mBegin >= children[i + 1].getOffset())
                    continue;
                if (clipper.mEnd <= childOffset)
                    break;
            }

            ImGuiTreeNodeFlags flag = child.hasChild()
                                          ? (ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow)
                                          : ImGuiTreeNodeFlags_Leaf;
            flag |= childOffset == selectedTreeNode ? ImGuiTreeNodeFlags_Selected : 0;
            flag |= ImGuiTreeNodeFlags_SpanFullWidth;
            const std::string &name = child.getName();
            bool isExpanded = ImGui::TreeNodeEx((void *)childOffset, flag,
                                                "%s%s%s", child.getTAG_str(),
                                                name.empty() ? "" : ": ", name.data());
            if (ImGui::IsItemClicked() || (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                                           ImGui::IsMouseClicked(ImGuiMouseButton_Left, true)))
            {
                selectedDie = &child;
                selectedTreeNode = childOffset;
                clipper.mClicked = true;
            }
            if constexpr (NEED_REFRESH)
                clipper.counter(childOffset, depth);

            if (isExpanded)
            {
                if (child.hasChild())
                    showSubTree<NEED_REFRESH>(child, clipper, depth + 1);

                ImGui::TreePop();
            }
        }
    }

    /**
     * @brief
     * @tparam NEED_REFRESH if ture, recalculate the height of the invisible tree nodes
     * @param CUlist vector of `dw::CU`
     * @param clipper TreeClipper, used to clip the invisible tree nodes
     */
    template <bool NEED_REFRESH>
    void showCUtree(const std::vector<dw::CU> &CUlist, TreeClipper &clipper)
    {
        if constexpr (!NEED_REFRESH)
            ImGui::Dummy(ImVec2(0.0f, clipper.mBeforeGapSize));
        else
        {
            ImGui::Dummy(ImVec2(0.0f, 0.0f));

            // reset clipper data
            clipper.mBeforeCount = clipper.mAfterCount = clipper.mStatue = 0;
        }

        for (size_t i = 0; i < CU_name_list.size(); i++)
        {
            auto &&CU = CUlist[i];
            std::pair<uint64_t, std::string> &name = CU_name_list[i];
            if constexpr (!NEED_REFRESH)
            {
                if (CUlist.size() > i + 1 && clipper.mBegin >= CU_name_list[i + 1].first)
                    continue;
                if (clipper.mEnd <= name.first)
                    break;
            }

            ImGuiTreeNodeFlags flag = CU.hasChild()
                                          ? (ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow)
                                          : ImGuiTreeNodeFlags_Leaf;
            flag |= name.first == selectedTreeNode ? ImGuiTreeNodeFlags_Selected : 0;
            flag |= ImGuiTreeNodeFlags_SpanFullWidth;
            bool isExpanded = ImGui::TreeNodeEx((void *)name.first, flag, "%s", name.second.data());

            if (ImGui::IsItemClicked() || (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                                           ImGui::IsMouseClicked(ImGuiMouseButton_Left, true)))
            {
                selectedDie = &CU;
                selectedTreeNode = name.first;
                clipper.mClicked = true;
            }

            if constexpr (NEED_REFRESH)
                // update the clipper
                clipper.counter(name.first, 0);

            if (isExpanded)
            {
                if (CU.hasChild())
                    showSubTree<NEED_REFRESH>(CU, clipper);

                ImGui::TreePop();
            }
        }

        if constexpr (NEED_REFRESH)
        {
            if (clipper.mStatue == 1)
                clipper.mEnd = static_cast<uint64_t>(-1);
        }
        else
            ImGui::Dummy(ImVec2(0.0f, clipper.mAfterGapSize));
    }
} // namespace anonymous

void App::HideTabBar()
{
    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
    ImGui::SetNextWindowClass(&window_class);
}

// CompileUnit list
void App::ShowTreeView()
{
    static TreeClipper clipper;
    App::HideTabBar();
    ImGui::Begin("CompileUnits");

    clipper.refresh();
    if (newFileOpening)
    {
        // collapse all TreeNode
        ImGuiStorage *storage = ImGui::GetStateStorage();
        storage->SetAllInt(0);
        newFileOpening = false;
    }

    const std::vector<dw::CU> &CUlist = f.getCUs();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    if (clipper.mNeedRefresh)
    {
        showCUtree<true>(CUlist, clipper);
        clipper.mNeedRefresh = false;
        clipper.mTreeNodeHeight = ImGui::GetItemRectSize().y;
    }
    else
    {
        showCUtree<false>(CUlist, clipper);
    }
    clipper.printInfo();

    ImGui::PopStyleVar();
    ImGui::End();
}

void App::ShowMainView()
{
    HideTabBar();
    ImGui::Begin("Details");

    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
    {
        static ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit |
                                       ImGuiTableFlags_RowBg |
                                       ImGuiTableFlags_Borders |
                                       ImGuiTableFlags_Resizable |
                                       ImGuiTableFlags_Reorderable |
                                       ImGuiTableFlags_Hideable;
        if (selectedDie && ImGui::BeginTabItem("Basic info"))
        {
            if (ImGui::BeginTable("Basic info table", 2, flags))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                static bool selected[6] = {0};
                static int prev = -1;
                int now = -1;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Selectable("Offset", &selected[0]))
                    now = 0;
                ImGui::TableSetColumnIndex(1);
                if (ImGui::Selectable(std::to_string(selectedDie->getOffset()).data(), &selected[1]))
                    now = 1;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Selectable("TAG", &selected[2]))
                    now = 2;
                ImGui::TableSetColumnIndex(1);
                if (ImGui::Selectable(selectedDie->getTAG_str(), &selected[3]))
                    now = 3;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Selectable("Has children", &selected[4]))
                    now = 4;
                ImGui::TableSetColumnIndex(1);
                if (ImGui::Selectable(selectedDie->hasChild() ? "true" : "false", &selected[5]))
                    now = 5;

                // Copy to clipboard
                if (ImGui::Shortcut(ImGuiMod_Shortcut | ImGuiKey_C, ImGuiInputFlags_None,
                                    ImGui::GetCurrentWindow()->ID))
                {
                    const char *text = "";
                    std::string temp = std::to_string(selectedDie->getOffset());
                    switch (now)
                    {
                    case 0:
                        text = "Offset";
                        break;
                    case 1:
                        text = temp.data();
                        break;
                    case 2:
                        text = "TAG";
                        break;
                    case 3:
                        text = selectedDie->getTAG_str();
                        break;
                    case 4:
                        text = "Has children";
                        break;
                    case 5:
                        text = selectedDie->hasChild() ? "true" : "false";
                        break;
                    default:
                        break;
                    }
                    ImGui::SetClipboardText(text);
                }
                if (now != -1 && now != prev)
                {
                    if (prev != -1)
                        selected[prev] = false;

                    prev = now;
                }

                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (selectedDie && ImGui::BeginTabItem("Attributes"))
        {
            if (ImGui::BeginTable("Main table", 3, flags))
            {
                ImGui::TableSetupColumn("Attribute", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Form", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                const std::vector<dw::attr> &attrs = selectedDie->getAttrs();

                // std::vector<bool> does not store boolean values
                // and converting `uint8_t*` to `bool*` is not safe.
                // So i defined this.
                struct Boolean_used_by_vector
                {
                    bool inner = false;
                };

                // recording the pos of selected item
                static std::vector<Boolean_used_by_vector> selectedPos;
                if (selectedPos.size() != attrs.size() * 3)
                    selectedPos.resize(attrs.size() * 3);
                static int prev = -1;
                int now = -1;
                for (size_t row = 0; row < attrs.size(); row++)
                {
                    const dw::attr &attr = attrs[row];
                    ImGui::TableNextRow();

                    // column: Attribute
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Selectable(attr.getAttrName(), &selectedPos[row * 3].inner))
                        now = row * 3;

                    // column: Form
                    ImGui::TableSetColumnIndex(1);
                    if (ImGui::Selectable(std::format("{}##{}", attr.getAttrForm_str(), row).data(),
                                          &selectedPos[row * 3 + 1].inner))
                        now = row * 3 + 1;

                    // column: Value
                    ImGui::TableSetColumnIndex(2);
                    if (ImGui::Selectable(std::format("{}##{}", attr.getAttrValue_str(), row).data(),
                                          &selectedPos[row * 3 + 2].inner))
                        now = row * 3 + 2;

                    // Copy to clipboard
                    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C, ImGuiInputFlags_None,
                                        ImGui::GetCurrentWindow()->ID))
                    {
                        if (selectedPos[row * 3].inner)
                            ImGui::SetClipboardText(attr.getAttrName());
                        else if (selectedPos[row * 3 + 1].inner)
                            ImGui::SetClipboardText(attr.getAttrForm_str());
                        else if (selectedPos[row * 3 + 2].inner)
                            ImGui::SetClipboardText(attr.getAttrValue_str().data());
                    }
                }

                // updating the pos of selected item
                if (now != -1 && now != prev)
                {
                    if (prev != -1)
                        selectedPos[prev].inner = false;

                    prev = now;
                }

                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (selectedDie && selectedDie->isCompileUnit() && ImGui::BeginTabItem("Line Table"))
        {
            // static const dw::die *old = selectedDie;
            // dw::linetable &cu = static_cast<const dw::CU *>(selectedDie)->getLineTable(f);
            // static std::vector<std::string> includeList = cu.getIncludeList();
            // if (old != selectedDie)
            // {
            //     includeList = cu.getIncludeList();
            //     old = selectedDie;
            // }
            // for (auto &&item : includeList)
            // {
            //     ImGui::Text("%s", item.data());
            // }

            ImGui::Text("to do");

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void App::RenderUI()
{
    bool *p_open = nullptr;
    static bool opt_fullscreen = true;
    static bool opt_padding = false;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        window_flags |= ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", p_open, window_flags);

    ImGui::PopStyleVar(3);

    ImGuiIO &io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        static auto is_first_time = true;
        if (is_first_time)
        {
            is_first_time = false;

            ImGui::DockBuilderRemoveNode(dockspace_id); // clear any previous layout
            ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            auto dock_id_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.20f, nullptr, &dockspace_id);
            ImGui::DockBuilderDockWindow("CompileUnits", dock_id_left);
            ImGui::DockBuilderDockWindow("Details", dockspace_id);
            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("文件（File）"))
        {
            if (ImGui::MenuItem("打开文件"))
            {
                TCHAR szBuffer[MAX_PATH] = {0};
                OPENFILENAME ofn = {0};
                ofn.hwndOwner = GetActiveWindow();
                ofn.lStructSize = sizeof(ofn);
                ofn.lpstrFile = szBuffer;
                ofn.nMaxFile = sizeof(szBuffer) / sizeof(*szBuffer);
                ofn.nFilterIndex = 0;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;
                if (GetOpenFileName(&ofn))
                    initCUlist(szBuffer);
            }
            ImGui::MenuItem("可选菜单项", nullptr, &opt_padding);
            ImGui::Separator();
            if (ImGui::MenuItem("item"))
            {
                // do something
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("选项（Options）"))
        {
            ImGui::MenuItem("不可选菜单项");
            ImGui::MenuItem("可选菜单项", nullptr, &opt_padding);
            ImGui::Separator();
            if (ImGui::MenuItem("item"))
            {
                // do something
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("主题（Theme）"))
        {
            if (ImGui::MenuItem("暗黑（Dark）"))
                ImGui::StyleColorsDark();

            if (ImGui::MenuItem("明亮（Light）"))
                ImGui::StyleColorsLight();

            if (ImGui::MenuItem("经典（Classic）"))
                ImGui::StyleColorsClassic();

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ShowTreeView();
    ShowMainView();

    ImGui::End();
}