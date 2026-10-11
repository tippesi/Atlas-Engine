#include "CommandPalette.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>

namespace Atlas::Editor::UI {

    void CommandPalette::Register(const Command& command) {

        commands.push_back(command);

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

            std::string rightText = command.shortcut;
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
