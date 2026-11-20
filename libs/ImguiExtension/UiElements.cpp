#include "UiElements.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>

namespace Atlas::ImguiExtension {

    UIElements::PropertySearch UIElements::propertySearch;

    void UIElements::TexturePreview(Ref<ImguiWrapper>& wrapper, const Texture::Texture* texture) {

        auto lineHeight = ImGui::GetTextLineHeightWithSpacing();
        auto set = wrapper->GetTextureId(texture);
        ImGui::Image(set, ImVec2(lineHeight, lineHeight));

        if (ImGui::IsItemHovered() && ImGui::BeginItemTooltip()) {
            ImGui::Image(set, ImVec2(lineHeight * 8.0f, lineHeight * 8.0f));
            ImGui::EndTooltip();
        }

    }

    void UIElements::TextureView(Ref<ImguiWrapper>& wrapper, const Texture::Texture* texture, float maxTextureSize) {

        auto region = ImGui::GetContentRegionAvail();

        auto size = std::min(region.x > 0.0f ? region.x : region.y, region.y > 0.0f ? region.y : region.x);
        size = std::max(1.0f, size);

        if (maxTextureSize > 0)
            size = std::min(maxTextureSize, size);

        auto pos = (region.x - size) / 2.0f;
        ImGui::SetCursorPosX(pos);

        auto set = wrapper->GetTextureId(texture);
        ImGui::Image(set, ImVec2(size, size));

        /*
        if (ImGui::IsItemHovered() && ImGui::BeginItemTooltip()) {
            ImGui::Image(set, ImVec2(lineHeight * 8.0f, lineHeight * 8.0f));
            ImGui::EndTooltip();
        }
        */

    }

    void UIElements::Tooltip(const char* text, float size) {

        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * size);
            ImGui::TextUnformatted(text);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

    }

    bool UIElements::BeginProperties(const char* id) {

        // Content hidden by a search still shows the matching rows of its tables
        auto window = ImGui::GetCurrentWindow();
        if (IsContentHidden(window))
            window->SkipItems = false;

        auto start = ImGui::GetCursorScreenPos();
        if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
            UpdateContentVisibility();
            return false;
        }

        SetupPropertyLabelColumn("Label");
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        if (propertySearch.active)
            propertySearch.tables.push_back({ .start = start });
        return true;

    }

    void UIElements::PropertyLabel(const char* label, const char* tooltip) {

        auto labelEnd = ImGui::FindRenderedTextEnd(label);
        auto highlight = false;
        if (propertySearch.active) {
            auto& search = propertySearch;
            const auto& scope = search.scopes[search.depth - 1];
            if (!scope.matches && !scope.sectionMatches) {
                if (!MatchesSearchWords(std::string_view(label, labelEnd), tooltip ? tooltip : std::string_view())) {
                    // Like ImGui between rows, the value and everything up to the next row is skipped
                    HideItems(ImGui::GetCurrentWindow());
                    return;
                }
                highlight = true;
            }
            CountPropertyMatch();
            if (!search.tables.empty())
                search.tables.back().shownRows++;
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        // Muted and centered on the value, long labels are cut off with an ellipsis and shown in full in the tooltip
        auto width = ImGui::GetContentRegionAvail().x;
        auto pos = ImGui::GetCursorScreenPos();
        auto textSize = ImGui::CalcTextSize(label, nullptr, true);
        MeasurePropertyLabel(textSize.x);
        ImGui::Dummy(ImVec2(width, ImGui::GetFrameHeight()));
        auto textPos = ImVec2(pos.x, pos.y + ImGui::GetStyle().FramePadding.y);
        auto textEnd = ImVec2(pos.x + width, textPos.y + ImGui::GetTextLineHeight());
        // Matches of a search stand out
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(highlight ? ImGuiCol_Text : ImGuiCol_TextDisabled));
        ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), textPos, textEnd, textEnd.x, textEnd.x, label,
            labelEnd, &textSize);
        ImGui::PopStyleColor();
        if (tooltip)
            ImGui::SetItemTooltip("%s", tooltip);
        else if (textSize.x > width)
            ImGui::SetItemTooltip("%s", label);

        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);

    }

    void UIElements::EndProperties() {

        ImGui::EndTable();

        auto& search = propertySearch;
        if (!search.active || search.tables.empty())
            return;

        auto table = search.tables.back();
        search.tables.pop_back();
        if (table.shownRows == 0)
            ImGui::SetCursorScreenPos(table.start);
        UpdateContentVisibility();

    }

    void UIElements::SetupPropertyLabelColumn(const char* name) {

        // At most 40% of the width, which is also the width before the labels of the window were measured
        auto labelWidth = UpdatePropertyLabelWidth(0.0f);
        auto maxWidth = 0.4f * ImGui::GetContentRegionAvail().x;
        auto width = labelWidth > 0.0f ? std::min(labelWidth + 2.0f * ImGui::GetStyle().ItemSpacing.x, maxWidth) : maxWidth;

        ImGui::TableSetupColumn(name, ImGuiTableColumnFlags_WidthFixed, width);
        // A new table has no maximum width yet, which would clamp the width, the initial width applies instead
        auto table = ImGui::GetCurrentTable();
        if (!table->IsInitializing)
            ImGui::TableSetColumnWidth(table->DeclColumnsCount - 1, width);

    }

    void UIElements::MeasurePropertyLabel(float labelWidth) {

        UpdatePropertyLabelWidth(labelWidth);

    }

    void UIElements::PropertySection(const char* label) {

        auto& search = propertySearch;
        if (!search.active) {
            ImGui::SeparatorText(label);
            return;
        }

        auto& scope = search.scopes[search.depth - 1];
        auto labelEnd = ImGui::FindRenderedTextEnd(label);
        scope.section.clear();
        scope.sectionMatches = MatchesSearchWords(std::string_view(label, labelEnd));
        scope.section.assign(label, labelEnd);
        if (scope.sectionMatches)
            CountPropertyMatch();
        UpdateContentVisibility();

    }

    void UIElements::BeginPropertySearch(const std::string& text, uint32_t scope) {

        auto& search = propertySearch;
        AE_ASSERT(!search.active && "Property searches can't be nested");

        if (text != search.text) {
            search.text = text;
            search.words = SplitWords(text);
        }
        if (search.words.empty())
            return;

        search.active = true;
        search.anyMatch = false;
        search.depth = 0;
        search.blockDepth = 0;
        search.tables.clear();
        // Never 0, the value of groups which weren't counted yet
        search.key = ImHashData(&scope, sizeof(scope), ImHashStr(text.c_str())) | 1u;
        PushPropertyScope(0, {}, false);

    }

    bool UIElements::EndPropertySearch() {

        auto& search = propertySearch;
        if (!search.active)
            return true;

        AE_ASSERT(search.depth == 1 && search.blockDepth == 0 && search.tables.empty() &&
            "Property groups, blocks and tables have to end before the search");

        search.active = false;
        search.depth = 0;
        return search.anyMatch;

    }

    bool UIElements::PropertySearchBar(const char* id, std::string& text, bool focus) {

        if (focus)
            ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-FLT_MIN);
        return ImGui::InputTextWithHint(id, "Search properties...", &text, ImGuiInputTextFlags_EscapeClearsAll);

    }

    bool UIElements::BeginPropertyGroup(const char* name) {

        auto& search = propertySearch;
        if (!search.active)
            return true;

        // Nothing is counted in hidden windows or rows
        auto window = ImGui::GetCurrentWindow();
        auto contentHidden = IsContentHidden(window);
        if (window->SkipItems && !contentHidden)
            return false;

        const auto& parent = search.scopes[search.depth - 1];
        auto id = ImGui::GetID(name);
        auto nameView = std::string_view(name, ImGui::FindRenderedTextEnd(name));
        auto nameMatches = MatchesSearchWords(nameView);
        auto matches = nameMatches || parent.matches || parent.sectionMatches;

        // The matches of the last time the group was drawn with this search
        auto recount = false;
        auto& storage = window->StateStorage;
        if (!matches && storage.GetInt(ImHashStr("Search key", 0, id)) == int32_t(search.key) &&
            storage.GetInt(ImHashStr("Search matches", 0, id)) == 0) {
            if (ImGui::GetFrameCount() - storage.GetInt(ImHashStr("Search frame", 0, id)) < recountInterval)
                return false;
            recount = true;
        }

        auto& scope = PushPropertyScope(id, nameView, matches);
        if (nameMatches)
            CountPropertyMatch();

        // The header is shown in hidden content
        if (contentHidden)
            window->SkipItems = false;

        // Content can change without a change of the search, e.g. when a resource is loaded. The group is drawn
        // invisibly then and takes no space without matches
        if (recount) {
            scope.recount = true;
            scope.start = ImGui::GetCursorScreenPos();
            scope.maxPos = window->DC.CursorMaxPos;
            ImGui::PushClipRect(scope.start, scope.start, true);
        }

        return true;

    }

    void UIElements::EndPropertyGroup() {

        auto& search = propertySearch;
        if (!search.active)
            return;

        AE_ASSERT(search.depth > 1 && "EndPropertyGroup without a group");

        auto& scope = search.scopes[search.depth - 1];
        if (scope.contentWindow)
            EndPropertyContent();

        auto window = ImGui::GetCurrentWindow();
        if (scope.recount) {
            ImGui::PopClipRect();
            if (scope.matchCount == 0) {
                ImGui::SetCursorScreenPos(scope.start);
                window->DC.CursorMaxPos = scope.maxPos;
            }
        }

        if (scope.counted) {
            auto& storage = window->StateStorage;
            storage.SetInt(ImHashStr("Search key", 0, scope.id), int32_t(search.key));
            storage.SetInt(ImHashStr("Search matches", 0, scope.id), scope.matchCount);
            storage.SetInt(ImHashStr("Search frame", 0, scope.id), ImGui::GetFrameCount());
        }

        search.depth--;
        UpdateContentVisibility();

    }

    ImGuiTreeNodeFlags UIElements::PropertyGroupHeaderFlags() {

        // Leaf nodes are always open and keep the open state of the node
        return propertySearch.active ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None;

    }

    void UIElements::BeginPropertyContent() {

        auto& search = propertySearch;
        if (!search.active)
            return;

        auto& scope = search.scopes[search.depth - 1];
        AE_ASSERT(search.depth > 1 && !scope.contentWindow && "Property content needs a group and can't be nested");

        scope.contentWindow = ImGui::GetCurrentWindow();
        scope.contentSkipItems = scope.contentWindow->SkipItems;
        if (scope.contentSkipItems)
            scope.counted = false;
        UpdateContentVisibility();

    }

    void UIElements::EndPropertyContent() {

        auto& search = propertySearch;
        if (!search.active)
            return;

        auto& scope = search.scopes[search.depth - 1];
        AE_ASSERT(scope.contentWindow && "EndPropertyContent without BeginPropertyContent");

        scope.contentWindow->SkipItems = scope.contentSkipItems;
        scope.contentWindow = nullptr;

    }

    bool UIElements::BeginPropertyBlock(const char* label) {

        auto& search = propertySearch;
        if (!search.active)
            return true;

        // Blocks in hidden rows stay hidden
        auto window = ImGui::GetCurrentWindow();
        auto contentHidden = IsContentHidden(window);
        if (window->SkipItems && !contentHidden)
            return false;

        const auto& scope = search.scopes[search.depth - 1];
        if (!scope.matches && !scope.sectionMatches &&
            !MatchesSearchWords(std::string_view(label, ImGui::FindRenderedTextEnd(label))))
            return false;

        CountPropertyMatch();
        if (contentHidden)
            window->SkipItems = false;
        search.blockDepth++;
        return true;

    }

    void UIElements::EndPropertyBlock() {

        auto& search = propertySearch;
        if (!search.active)
            return;

        search.blockDepth--;
        UpdateContentVisibility();

    }

    bool UIElements::MatchesSearch(std::string_view text, std::string_view search) {

        size_t start = 0;
        while (start < search.size()) {
            auto end = search.find_first_of(" \t", start);
            if (end == std::string_view::npos)
                end = search.size();
            if (end > start && !ContainsIgnoreCase(text, search.substr(start, end - start)))
                return false;
            start = end + 1;
        }
        return true;

    }

    UIElements::PropertyScope& UIElements::PushPropertyScope(ImGuiID id, std::string_view name, bool matches) {

        auto& search = propertySearch;
        if (search.depth == search.scopes.size())
            search.scopes.emplace_back();

        auto& scope = search.scopes[search.depth++];
        scope.id = id;
        scope.name.assign(name);
        scope.matches = matches;
        scope.matchCount = 0;
        scope.counted = true;
        scope.recount = false;
        scope.section.clear();
        scope.sectionMatches = false;
        scope.contentWindow = nullptr;
        scope.contentSkipItems = false;
        return scope;

    }

    bool UIElements::MatchesSearchWords(std::string_view label, std::string_view tooltip) {

        const auto& search = propertySearch;
        for (const auto& word : search.words) {
            auto found = ContainsIgnoreCase(label, word) || ContainsIgnoreCase(tooltip, word);
            for (size_t i = 0; i < search.depth && !found; i++)
                found = ContainsIgnoreCase(search.scopes[i].name, word) || ContainsIgnoreCase(search.scopes[i].section, word);
            if (!found)
                return false;
        }
        return true;

    }

    bool UIElements::ContainsIgnoreCase(std::string_view text, std::string_view word) {

        auto iter = std::search(text.begin(), text.end(), word.begin(), word.end(), [](char character0, char character1) {
            return std::tolower(static_cast<unsigned char>(character0)) == std::tolower(static_cast<unsigned char>(character1));
            });
        return iter != text.end() || word.empty();

    }

    std::vector<std::string> UIElements::SplitWords(std::string_view text) {

        std::vector<std::string> words;
        size_t start = 0;
        while (start < text.size()) {
            auto end = text.find_first_of(" \t", start);
            if (end == std::string_view::npos)
                end = text.size();
            if (end > start)
                words.emplace_back(text.substr(start, end - start));
            start = end + 1;
        }
        return words;

    }

    void UIElements::CountPropertyMatch() {

        auto& search = propertySearch;
        search.anyMatch = true;
        for (size_t i = 0; i < search.depth; i++)
            search.scopes[i].matchCount++;

    }

    bool UIElements::IsContentHidden(ImGuiWindow* window) {

        const auto& search = propertySearch;
        if (!search.active || !search.tables.empty() || search.blockDepth > 0)
            return false;

        const auto& scope = search.scopes[search.depth - 1];
        return scope.contentWindow == window && !scope.contentSkipItems;

    }

    void UIElements::HideItems(ImGuiWindow* window) {

        // Item queries like IsItemHovered would otherwise refer to the last shown item
        window->SkipItems = true;
        ImGui::SetLastItemData(0, ImGuiItemFlags_None, ImGuiItemStatusFlags_None, ImRect());

    }

    void UIElements::UpdateContentVisibility() {

        const auto& search = propertySearch;
        if (!search.active || !search.tables.empty() || search.blockDepth > 0)
            return;

        const auto& scope = search.scopes[search.depth - 1];
        if (!scope.contentWindow)
            return;

        if (scope.contentSkipItems || (!scope.matches && !scope.sectionMatches))
            HideItems(scope.contentWindow);
        else
            scope.contentWindow->SkipItems = false;

    }

    float UIElements::UpdatePropertyLabelWidth(float labelWidth) {

        // The longest label of this frame is collected while the labels are drawn
        auto window = ImGui::GetCurrentWindow();
        auto& storage = window->StateStorage;
        auto lastWidthKey = ImHashStr("Property label width", 0, window->ID);
        auto widthKey = ImHashStr("Property label width of this frame", 0, window->ID);
        auto frameKey = ImHashStr("Property label frame", 0, window->ID);
        auto frame = ImGui::GetFrameCount();
        if (storage.GetInt(frameKey) != frame) {
            storage.SetFloat(lastWidthKey, storage.GetFloat(widthKey));
            storage.SetFloat(widthKey, 0.0f);
            storage.SetInt(frameKey, frame);
        }
        storage.SetFloat(widthKey, std::max(storage.GetFloat(widthKey), labelWidth));
        return storage.GetFloat(lastWidthKey);

    }

}