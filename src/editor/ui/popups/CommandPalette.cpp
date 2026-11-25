#include "CommandPalette.h"

#include "../../Singletons.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>

namespace Atlas::Editor::UI {

    void CommandPalette::Register(const Command& command) {

        commands.push_back(command);

    }

    const Command* CommandPalette::Find(const std::string& name) const {

        for (const auto& command : commands)
            if (command.name == name || GetName(command) == name)
                return &command;

        return nullptr;

    }

    void CommandPalette::HandleShortcuts() {

        // Without a new frame, e.g. while the window is covered, the keys are still the ones of the last one
        auto isNewFrame = handledFrame != ImGui::GetFrameCount();
        handledFrame = ImGui::GetFrameCount();

        const auto& io = ImGui::GetIO();
        if (!isNewFrame || recordedFrame == ImGui::GetFrameCount() || ImGui::GetTopMostPopupModal())
            return;
        recordedCommand.clear();

        for (const auto& command : commands) {
            if (io.WantTextInput && !command.whileTyping)
                continue;

            auto isPressed = [&](ImGuiKeyChord shortcut) {
                return shortcut && ImGui::IsKeyChordPressed(shortcut, command.repeat ? ImGuiInputFlags_Repeat : ImGuiInputFlags_None);
                };
            auto isCustom = Singletons::config->shortcuts.contains(GetName(command));
            if (!isPressed(GetShortcut(command)) && (isCustom || !isPressed(command.alternative)))
                continue;

            if ((command.isFocused && !command.isFocused()) || (command.isEnabled && !command.isEnabled()))
                continue;

            command.action();
            return;
        }

    }

    void CommandPalette::RenderShortcuts() {

        auto& shortcuts = Singletons::config->shortcuts;

        if (!recordedCommand.empty()) {
            recordedFrame = ImGui::GetFrameCount();
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
                recordedCommand.clear();
            }
            else {
                for (auto key = int32_t(ImGuiKey_Tab); key <= int32_t(ImGuiKey_KeypadEqual); key++) {
                    auto isModifier = key >= ImGuiKey_LeftCtrl && key <= ImGuiKey_RightSuper;
                    if (isModifier || !ImGui::IsKeyPressed(ImGuiKey(key), false))
                        continue;
                    shortcuts[recordedCommand] = ImGuiKeyChord(key) | ImGui::GetIO().KeyMods;
                    recordedCommand.clear();
                    break;
                }
            }
        }

        // Commands of a category aren't necessarily registered next to each other
        std::vector<std::string> categories;
        std::vector<const Command*> sortedCommands;
        for (const auto& command : commands) {
            if (std::find(categories.begin(), categories.end(), command.category) == categories.end())
                categories.push_back(command.category);
        }
        for (const auto& category : categories)
            for (const auto& command : commands)
                if (command.category == category)
                    sortedCommands.push_back(&command);

        std::string category;
        auto tableOpen = false;
        for (auto sortedCommand : sortedCommands) {
            const auto& command = *sortedCommand;
            auto name = GetName(command);
            auto shortcut = GetShortcut(command);
            auto isCustom = shortcuts.contains(name);

            if (command.category != category) {
                if (tableOpen)
                    ImGui::EndTable();
                category = command.category;
                ImGui::SeparatorText(category.c_str());
                tableOpen = ImGui::BeginTable(category.c_str(), 3, ImGuiTableFlags_SizingFixedFit);
                if (tableOpen) {
                    ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 10.0f * ImGui::GetFontSize());
                    ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, 6.0f * ImGui::GetFontSize());
                }
            }
            if (!tableOpen)
                continue;

            ImGui::PushID(name.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(command.name.c_str());

            ImGui::TableNextColumn();
            auto isRecorded = recordedCommand == name;
            auto label = isRecorded ? std::string("Press keys...") : GetShortcutName(shortcut);
            if (!isRecorded && !isCustom && command.alternative)
                label += " / " + GetShortcutName(command.alternative);
            if (ImGui::Button((label + "##Shortcut").c_str(), ImVec2(-FLT_MIN, 0.0f))) {
                recordedCommand = isRecorded ? std::string() : name;
                recordedFrame = ImGui::GetFrameCount();
            }

            // The same keys in another window are fine
            std::string others;
            for (const auto& other : commands)
                if (shortcut && &other != &command && GetShortcut(other) == shortcut)
                    others += (others.empty() ? "" : "\n") + GetName(other);
            if (!others.empty())
                ImGui::SetItemTooltip("Also used by\n%s", others.c_str());

            ImGui::TableNextColumn();
            if (isCustom && ImGui::SmallButton("Reset"))
                shortcuts.erase(name);
            if (isCustom && shortcut)
                ImGui::SameLine();
            if (shortcut && ImGui::SmallButton("Clear"))
                shortcuts[name] = ImGuiKey_None;
            ImGui::PopID();
        }
        if (tableOpen)
            ImGui::EndTable();

    }

    std::string CommandPalette::GetShortcutText(const std::string& name) const {

        auto command = Find(name);
        return command ? GetShortcutName(GetShortcut(*command)) : std::string();

    }

    std::string CommandPalette::GetTooltip(const std::string& text, const std::string& name) const {

        auto shortcut = GetShortcutText(name);
        return shortcut.empty() ? text : text + " (" + shortcut + ")";

    }

    ImGuiKeyChord CommandPalette::GetShortcut(const Command& command) {

        const auto& shortcuts = Singletons::config->shortcuts;
        auto custom = shortcuts.find(GetName(command));
        return custom != shortcuts.end() ? custom->second : command.shortcut;

    }

    ImGuiKeyChord CommandPalette::ParseShortcut(const std::string& name) {

        for (auto key = int32_t(ImGuiKey_NamedKey_BEGIN); key < int32_t(ImGuiKey_NamedKey_END); key++) {
            if (!name.ends_with(ImGui::GetKeyName(ImGuiKey(key))))
                continue;
            // All combinations of the modifier bits, their names depend on the system
            for (auto modifiers = 0; modifiers <= ImGuiMod_Mask_; modifiers += ImGuiMod_Ctrl)
                if (GetShortcutName(key | modifiers) == name)
                    return key | modifiers;
        }

        return ImGuiKey_None;

    }

    std::string CommandPalette::GetShortcutName(ImGuiKeyChord shortcut) {

        return shortcut ? ImGui::GetKeyChordName(shortcut) : std::string();

    }

    void CommandPalette::Open() {

        openRequested = true;

    }

    void CommandPalette::Render() {

        const char* popupName = "Command palette";

        if (openRequested) {
            ImGui::OpenPopup(popupName);
            search.clear();
            selectedIdx = 0;
            wasJustOpened = true;
            openRequested = false;
        }

        // Top center of the main viewport, like in most editors
        auto viewport = ImGui::GetMainViewport();
        auto width = std::clamp(viewport->Size.x * 0.4f, 420.0f, 760.0f);
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.12f),
            ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(width, 0.0f));

        if (!ImGui::BeginPopup(popupName, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
            return;

        if (wasJustOpened) {
            ImGui::SetKeyboardFocusHere();
            wasJustOpened = false;
        }

        ImGui::SetNextItemWidth(-FLT_MIN);
        auto prevSearch = search;
        bool confirmed = ImGui::InputTextWithHint("##Command search", "Type a command...", &search,
            ImGuiInputTextFlags_EnterReturnsTrue);
        if (search != prevSearch)
            selectedIdx = 0;

        // Collect and rank the matches
        std::vector<Match> matches;
        for (const auto& command : commands) {
            auto score = Score(search, command.category + ": " + command.name);
            if (score >= 0)
                matches.push_back({ &command, score });
        }
        std::stable_sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
            return a.score > b.score;
            });

        auto matchCount = int32_t(matches.size());

        // Keyboard navigation, the text field keeps the focus
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true) && matchCount > 0)
            selectedIdx = (selectedIdx + 1) % matchCount;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true) && matchCount > 0)
            selectedIdx = (selectedIdx - 1 + matchCount) % matchCount;
        selectedIdx = std::clamp(selectedIdx, 0, std::max(matchCount - 1, 0));

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();

        const Command* executeCommand = nullptr;
        if (confirmed && matchCount > 0)
            executeCommand = matches[selectedIdx].command;

        ImGui::Spacing();

        // List with a fixed maximum height, which scrolls with the keyboard selection
        auto rowHeight = ImGui::GetTextLineHeightWithSpacing();
        auto listHeight = std::min(float(std::max(matchCount, 1)), 12.0f) * rowHeight + ImGui::GetStyle().WindowPadding.y;
        ImGui::BeginChild("Command list", ImVec2(0.0f, listHeight));

        if (matches.empty())
            ImGui::TextDisabled("No matching commands");

        for (int32_t i = 0; i < matchCount; i++) {
            const auto& command = *matches[i].command;
            bool enabled = !command.isEnabled || command.isEnabled();
            bool checked = command.isChecked && command.isChecked();

            ImGui::PushID(i);
            ImGui::BeginDisabled(!enabled);

            auto rowStart = ImGui::GetCursorPosX();
            bool selected = i == selectedIdx;
            if (ImGui::Selectable("##Command", selected, ImGuiSelectableFlags_AllowOverlap) && enabled)
                executeCommand = &command;
            if (ImGui::IsItemHovered() && ImGui::GetIO().MouseDelta.x != 0.0f)
                selectedIdx = i;
            if (selected && ImGui::IsKeyPressed(ImGuiKey_DownArrow, true) || selected && ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
                ImGui::SetScrollHereY();

            // Category muted, then the name, shortcut and check state right aligned
            ImGui::SameLine(rowStart);
            ImGui::TextDisabled("%s:", command.category.c_str());
            ImGui::SameLine();
            ImGui::TextUnformatted(command.name.c_str());

            auto rightText = GetShortcutName(GetShortcut(command));
            if (checked)
                rightText = rightText.empty() ? "On" : rightText + "   On";
            if (!rightText.empty()) {
                auto rightWidth = ImGui::CalcTextSize(rightText.c_str()).x;
                ImGui::SameLine(ImGui::GetContentRegionMax().x - rightWidth);
                ImGui::TextDisabled("%s", rightText.c_str());
            }

            ImGui::EndDisabled();
            ImGui::PopID();
        }

        ImGui::EndChild();

        ImGui::Separator();
        ImGui::TextDisabled("Up/Down to navigate, Enter to run, Esc to close");

        if (executeCommand) {
            bool enabled = !executeCommand->isEnabled || executeCommand->isEnabled();
            ImGui::CloseCurrentPopup();
            if (enabled && executeCommand->action)
                executeCommand->action();
        }

        ImGui::EndPopup();

    }

    int32_t CommandPalette::Score(const std::string& query, const std::string& text) {

        if (query.empty())
            return 0;

        auto lower = [](char c) { return char(std::tolower(static_cast<unsigned char>(c))); };

        int32_t score = 0;
        size_t textIdx = 0;
        size_t lastMatchIdx = std::string::npos;

        for (size_t queryIdx = 0; queryIdx < query.size(); queryIdx++) {
            auto queryChar = lower(query[queryIdx]);
            if (queryChar == ' ')
                continue;

            bool found = false;
            for (; textIdx < text.size(); textIdx++) {
                if (lower(text[textIdx]) != queryChar)
                    continue;

                score += 1;
                // Consecutive characters
                if (lastMatchIdx != std::string::npos && textIdx == lastMatchIdx + 1)
                    score += 5;
                // Start of a word
                if (textIdx == 0 || text[textIdx - 1] == ' ' || text[textIdx - 1] == ':')
                    score += 8;

                lastMatchIdx = textIdx;
                textIdx++;
                found = true;
                break;
            }

            if (!found)
                return -1;
        }

        // Contiguous matches beat scattered ones, especially at the start of a word
        std::string lowerText = text, lowerQuery = query;
        std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), lower);
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), lower);
        auto substringPos = lowerText.find(lowerQuery);
        if (substringPos != std::string::npos) {
            score += 50;
            if (substringPos == 0 || text[substringPos - 1] == ' ' || text[substringPos - 1] == ':')
                score += 25;
        }

        // Prefer shorter texts, which match the query more precisely
        return score * 100 - int32_t(text.size());

    }

}
