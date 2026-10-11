#include "LogWindow.h"

#include "Clock.h"
#include "../../Singletons.h"
#include "../../Theme.h"
#include "../../Notifications.h"

#include <imgui_stdlib.h>
#include <imgui_internal.h>

#include <algorithm>

namespace Atlas::Editor::UI {

    void LogWindow::Render() {

        if (!Begin())
            return;

        bool copyRequested = false;

        // Toolbar
        {
            RenderFilterToggle("Messages", messageCount, &showMessages, ImGui::GetStyleColorVec4(ImGuiCol_Text));
            ImGui::SameLine();
            RenderFilterToggle("Warnings", warningCount, &showWarnings, Theme::Warning());
            ImGui::SameLine();
            RenderFilterToggle("Errors", errorCount, &showErrors, Theme::Error());

            ImGui::SameLine();

            auto& style = ImGui::GetStyle();
            auto copyWidth = ImGui::CalcTextSize("Copy").x + 2.0f * style.FramePadding.x;
            auto clearWidth = ImGui::CalcTextSize("Clear").x + 2.0f * style.FramePadding.x;
            auto autoScrollWidth = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize("Auto-scroll").x;
            auto selectedWidth = selection.Size > 0 ? ImGui::CalcTextSize("00000 selected").x + style.ItemSpacing.x : 0.0f;
            auto searchWidth = ImGui::GetContentRegionAvail().x - copyWidth - clearWidth - autoScrollWidth -
                selectedWidth - 3.0f * style.ItemSpacing.x;

            ImGui::SetNextItemWidth(std::max(searchWidth, 50.0f));
            ImGui::InputTextWithHint("##Log search", "Search log...", &logSearch);

            ImGui::SameLine();
            copyRequested = ImGui::Button("Copy");
            ImGui::SetItemTooltip("Copies the selected lines, or all visible lines if nothing is selected (Ctrl+C)");

            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                clearEntryIdx = fetchedEntryCount;
                linesDirty = true;
            }

            ImGui::SameLine();
            ImGui::Checkbox("Auto-scroll", &autoScroll);

            if (selection.Size > 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("%d selected", selection.Size);
            }
        }

        ImGui::Separator();

        UpdateLines();

        if (copyRequested)
            CopyLines(selection.Size > 0);

        selection.UserData = this;
        selection.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* self, int idx) {
            return static_cast<LogWindow*>(self->UserData)->lines[idx].id;
            };

        ImGui::BeginChild("Log entries", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

        // Ctrl+C while the log has the focus copies the selection
        if (selection.Size > 0 && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C))
            CopyLines(true);

        auto textColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        auto timeColor = Theme::TextMuted();
        auto warningColor = Theme::Warning();
        auto errorColor = Theme::Error();
        auto timeWidth = ImGui::CalcTextSize("00000.000").x;
        auto lineHeight = ImGui::GetTextLineHeight();

        char timeText[32];

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 2.0f));

        // Click, Shift/Ctrl + click, dragging over lines and Ctrl+A select lines, Escape clears the selection
        auto multiSelectFlags = ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_ClearOnClickVoid |
            ImGuiMultiSelectFlags_BoxSelect1d;
        auto multiSelectIO = ImGui::BeginMultiSelect(multiSelectFlags, selection.Size, int32_t(lines.size()));
        selection.ApplyRequests(multiSelectIO);

        // Only the visible lines are submitted, so the cost doesn't depend on the amount of log entries
        ImGuiListClipper clipper;
        clipper.Begin(int32_t(lines.size()));
        if (multiSelectIO->RangeSrcItem != -1)
            clipper.IncludeItemByIndex(int32_t(multiSelectIO->RangeSrcItem));
        while (clipper.Step()) {
            for (int32_t i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                const auto& line = lines[i];
                const auto& entry = GetEntry(line.entryIdx);

                ImGui::PushID(i);

                // Full width selectable in the background, the text is drawn on top of it
                auto rowPos = ImGui::GetCursorPos();
                ImGui::SetNextItemSelectionUserData(i);
                ImGui::Selectable("##Line", selection.Contains(line.id), ImGuiSelectableFlags_AllowOverlap,
                    ImVec2(0.0f, lineHeight));

                if (ImGui::BeginPopupContextItem("Log line context")) {
                    if (ImGui::MenuItem("Copy", "Ctrl+C"))
                        CopyLines(true);
                    if (ImGui::MenuItem("Copy message"))
                        ImGui::SetClipboardText(entry.message.c_str());
                    if (ImGui::MenuItem("Copy all visible lines"))
                        CopyLines(false);
                    ImGui::EndPopup();
                }

                ImGui::SetCursorPos(rowPos);

                if (line.first) {
                    std::snprintf(timeText, sizeof(timeText), "%9.3f", entry.time);
                    ImGui::TextColored(timeColor, "%s", timeText);
                }
                else {
                    ImGui::Dummy(ImVec2(timeWidth, lineHeight));
                }
                ImGui::SameLine();

                auto messageColor = entry.type == Atlas::Log::Type::TYPE_WARNING ? warningColor :
                    entry.type == Atlas::Log::Type::TYPE_ERROR ? errorColor : textColor;

                const char* text = entry.message.c_str() + line.offset;
                ImGui::PushStyleColor(ImGuiCol_Text, messageColor);
                ImGui::TextUnformatted(text, text + line.length);
                ImGui::PopStyleColor();

                ImGui::PopID();
            }
        }
        clipper.End();

        multiSelectIO = ImGui::EndMultiSelect();
        selection.ApplyRequests(multiSelectIO);

        ImGui::PopStyleVar();

        if (lines.empty()) {
            ImGui::TextDisabled("%s", logSearch.empty() ? "No log entries" : "No log entries match the search");
        }

        // Only follow new lines if enabled and the user didn't scroll up
        if (lastLineCount != lines.size()) {
            lastLineCount = lines.size();
            if (autoScroll)
                ImGui::SetScrollHereY(1.0f);
        }
        else if (ImGui::GetScrollY() < ImGui::GetScrollMaxY() && ImGui::IsWindowHovered() &&
            ImGui::GetIO().MouseWheel > 0.0f) {
            // Scrolling up disables auto-scroll, until the user enables it again
            autoScroll = false;
        }

        ImGui::EndChild();

        End();

    }

    void LogWindow::UpdateLines() {

        // A different filter shows different lines, so the lines and the selection have to be rebuilt
        auto filterState = logSearch + (showMessages ? "1" : "0") + (showWarnings ? "1" : "0") + (showErrors ? "1" : "0");
        if (filterState != lastFilterState) {
            lastFilterState = filterState;
            lowerSearch = logSearch;
            std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);
            linesDirty = true;
        }

        // Cheap check for new entries, only these are copied
        if (Atlas::Log::GetEntryCount() > fetchedEntryCount) {
            auto newEntries = Atlas::Log::GetEntriesSince(fetchedEntryCount);
            auto firstNewIdx = fetchedEntryCount;
            fetchedEntryCount += newEntries.size();

            // Skip what wouldn't be kept anyway, e.g. when the window is opened with a long log
            size_t skip = newEntries.size() > maxCachedEntries ? newEntries.size() - maxCachedEntries : 0;
            if (skip > 0) {
                cachedEntries.clear();
                cacheStartIdx = firstNewIdx + skip;
                linesDirty = true;
            }

            for (size_t i = skip; i < newEntries.size(); i++) {
                cachedEntries.push_back(std::move(newEntries[i]));
                if (!linesDirty)
                    AppendLines(firstNewIdx + i);
            }

            // Trim in batches, every trim needs a rebuild of the lines
            if (cachedEntries.size() > maxCachedEntries + trimBatchSize) {
                auto removeCount = cachedEntries.size() - maxCachedEntries;
                cachedEntries.erase(cachedEntries.begin(), cachedEntries.begin() + removeCount);
                cacheStartIdx += removeCount;
                linesDirty = true;
            }
        }

        if (linesDirty)
            RebuildLines();

    }

    void LogWindow::RebuildLines() {

        lines.clear();
        messageCount = warningCount = errorCount = 0;

        // The indices of the lines change, so the selection can't be kept
        selection.Clear();

        for (size_t i = 0; i < cachedEntries.size(); i++)
            AppendLines(cacheStartIdx + i);

        linesDirty = false;

    }

    void LogWindow::AppendLines(size_t entryIdx) {

        if (entryIdx < clearEntryIdx)
            return;

        const auto& entry = GetEntry(entryIdx);

        messageCount += entry.type == Atlas::Log::Type::TYPE_MESSAGE ? 1 : 0;
        warningCount += entry.type == Atlas::Log::Type::TYPE_WARNING ? 1 : 0;
        errorCount += entry.type == Atlas::Log::Type::TYPE_ERROR ? 1 : 0;

        if (entry.type == Atlas::Log::Type::TYPE_MESSAGE && !showMessages ||
            entry.type == Atlas::Log::Type::TYPE_WARNING && !showWarnings ||
            entry.type == Atlas::Log::Type::TYPE_ERROR && !showErrors)
            return;

        // Case-insensitive search without copying the message
        if (!lowerSearch.empty()) {
            auto it = std::search(entry.message.begin(), entry.message.end(), lowerSearch.begin(), lowerSearch.end(),
                [](char a, char b) { return char(std::tolower(static_cast<unsigned char>(a))) == b; });
            if (it == entry.message.end())
                return;
        }

        // Ids are derived from the entry index and the line within the message, which is stable and cheap
        uint32_t offset = 0;
        uint32_t lineInMessage = 0;
        const auto& message = entry.message;
        while (true) {
            auto newLine = message.find('\n', offset);
            auto end = newLine == std::string::npos ? message.size() : newLine;

            uint64_t key[2] = { uint64_t(entryIdx), uint64_t(lineInMessage) };
            lines.push_back({
                .entryIdx = entryIdx,
                .offset = offset,
                .length = uint32_t(end - offset),
                .first = lineInMessage == 0,
                .id = ImHashData(key, sizeof(key))
            });

            if (newLine == std::string::npos)
                break;

            offset = uint32_t(newLine + 1);
            lineInMessage++;
        }

    }

    void LogWindow::CopyLines(bool onlySelected) {

        // Lines are copied like they are displayed: time stamp and message
        std::string text;
        char timeText[32];
        int32_t lineCount = 0;

        for (const auto& line : lines) {
            if (onlySelected && !selection.Contains(line.id))
                continue;

            const auto& entry = GetEntry(line.entryIdx);
            if (line.first) {
                std::snprintf(timeText, sizeof(timeText), "%9.3f ", entry.time);
                text += timeText;
            }
            else {
                text += std::string(10, ' ');
            }
            text.append(entry.message, line.offset, line.length);
            text += '\n';
            lineCount++;
        }

        if (lineCount == 0)
            return;

        ImGui::SetClipboardText(text.c_str());
        Notifications::Push({ .message = "Copied " + std::to_string(lineCount) +
            (lineCount == 1 ? " log line" : " log lines"), .displayTime = 2.0f });

    }

    bool LogWindow::RenderFilterToggle(const char* label, int32_t count, bool* enabled, ImVec4 color) {

        auto text = std::string(label) + " " + std::to_string(count) + "##" + label;

        // Disabled filters are shown faded out
        color.w *= *enabled ? 1.0f : 0.4f;
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::PushStyleColor(ImGuiCol_Button, *enabled ? ImGui::GetStyleColorVec4(ImGuiCol_Button) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        bool pressed = ImGui::Button(text.c_str());
        if (pressed)
            *enabled = !*enabled;

        ImGui::PopStyleColor(2);

        ImGui::SetItemTooltip(*enabled ? "Hide %s" : "Show %s", label);

        return pressed;

    }

}
