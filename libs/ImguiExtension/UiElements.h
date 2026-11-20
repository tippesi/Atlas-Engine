#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <initializer_list>
#include <imgui.h>

#include "System.h"
#include "ImguiWrapper.h"
#include "texture/Texture2D.h"

struct ImGuiWindow;

namespace Atlas::ImguiExtension {

    class UIElements {

    public:
        static void TexturePreview(Ref<ImguiWrapper>& wrapper, const Texture::Texture* texture);

        static void TextureView(Ref<ImguiWrapper>& wrapper, const Texture::Texture* texture, float maxTextureSize = 0.0f);

        static void Tooltip(const char* text, float size = 35.0f);

        // Properties with a muted label on the left and the value filling the rest of the row. Call PropertyLabel
        // before each value, then EndProperties. Same layout as the property tables of the editor
        static bool BeginProperties(const char* id);

        // Starts a row, the next item is the value and fills its column
        static void PropertyLabel(const char* label, const char* tooltip = nullptr);

        static void EndProperties();

        // All label columns of a window are as wide as its longest label. Tables with their own columns set it up
        // with this and report each of their labels with MeasurePropertyLabel
        static void SetupPropertyLabelColumn(const char* name);

        static void MeasurePropertyLabel(float labelWidth);

        // Heading of the following properties, hidden while searching
        static void PropertySection(const char* label);

        // Filters the property rows drawn until EndPropertySearch. Rows are shown when every word of the text is in
        // their label, tooltip, or the name of a section or group they are in. Panels mark their parts with groups,
        // sections and blocks, everything else in the content of a group is hidden while searching. Tables in the
        // content have to be property tables. The scope changes when the searched content does, e.g. with the
        // selected entity. Does nothing for an empty text
        static void BeginPropertySearch(const std::string& text, uint32_t scope = 0);

        // Returns whether anything matched
        static bool EndPropertySearch();

        // Input for the text of a property search, Escape clears it
        static bool PropertySearchBar(const char* id, std::string& text, bool focus = false);

        // A group of properties below a header, e.g. a component or an element of a list. Groups can be nested. While
        // searching a group is hidden without matches, then it returns false and EndPropertyGroup isn't called.
        // Matches are counted while the group is drawn, hidden groups are drawn invisibly from time to time to find
        // matches in changed content
        static bool BeginPropertyGroup(const char* name);

        static void EndPropertyGroup();

        // Flags for the tree node or collapsing header of a group, which keep it open while searching
        static ImGuiTreeNodeFlags PropertyGroupHeaderFlags();

        // The content of a group below its header. While searching, everything in it that isn't a matching property
        // row, block, nested group or part of a matching section or group is hidden, e.g. buttons and lists
        static void BeginPropertyContent();

        static void EndPropertyContent();

        // Content outside of property rows, e.g. a button or a list, which is shown when the label matches the
        // search. Returns false when it's hidden, then EndPropertyBlock isn't called
        static bool BeginPropertyBlock(const char* label);

        static void EndPropertyBlock();

        // Case-insensitive, every word of the search has to be in the text
        static bool MatchesSearch(std::string_view text, std::string_view search);

    private:
        struct PropertyScope {
            ImGuiID id = 0;
            std::string name;
            bool matches = false;
            int32_t matchCount = 0;

            // Counts are only stored when the whole content was drawn
            bool counted = true;
            bool recount = false;
            ImVec2 start;
            ImVec2 maxPos;

            std::string section;
            bool sectionMatches = false;

            ImGuiWindow* contentWindow = nullptr;
            bool contentSkipItems = false;
        };

        struct PropertySearch {
            bool active = false;
            std::string text;
            std::vector<std::string> words;
            uint32_t key = 0;
            bool anyMatch = false;

            // The first scope is the search itself, scopes are kept to reuse their strings
            std::vector<PropertyScope> scopes;
            size_t depth = 0;
            int32_t blockDepth = 0;

            // Property tables of the search, tables without a shown row take no space
            struct Table {
                ImVec2 start;
                int32_t shownRows = 0;
            };
            std::vector<Table> tables;
        };

        // Returns the longest label of the current window in the last frame
        static float UpdatePropertyLabelWidth(float labelWidth);

        static PropertyScope& PushPropertyScope(ImGuiID id, std::string_view name, bool matches);

        // Every word of the search is in the label, the tooltip or the names and sections of the current scopes
        static bool MatchesSearchWords(std::string_view label, std::string_view tooltip = {});

        static bool ContainsIgnoreCase(std::string_view text, std::string_view word);

        static std::vector<std::string> SplitWords(std::string_view text);

        static void CountPropertyMatch();

        // Whether the window skips items only because the content of the current scope is hidden
        static bool IsContentHidden(ImGuiWindow* window);

        static void HideItems(ImGuiWindow* window);

        // Hides the content outside of property tables and blocks unless the section or the group matches
        static void UpdateContentVisibility();

        static constexpr int32_t recountInterval = 30;

        static PropertySearch propertySearch;

    };

}