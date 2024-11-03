#include "LogWindow.h"

#include "Log.h"
#include "../../Singletons.h"

#include <algorithm>

namespace Atlas::Editor::UI {

    void LogWindow::Render() {

        if (!Begin())
            return;

        auto darkMode = Singletons::config->darkMode;

        auto entries = Atlas::Log::GetLatestEntries(10000);
        for (auto& entry : entries) {
            
            bool hasNewLine = entry.message.find_first_of('\n') != std::string::npos;
            if (!hasNewLine) {
                ImGui::TextUnformatted("");
                if (!ImGui::IsItemVisible())
                    continue;
                ImGui::SameLine();
            }

            std::string logText;
            logText.append("[" + std::to_string(entry.time) + "] ");
            logText.append(entry.message);

            switch (entry.type) {
                case Atlas::Log::Type::TYPE_MESSAGE:
                    if (darkMode)
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    else
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
                    break;
                case Atlas::Log::Type::TYPE_WARNING:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
                    break;
                case Atlas::Log::Type::TYPE_ERROR:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                    break;
            }

            ImGui::TextUnformatted(logText.c_str());

            ImGui::PopStyleColor();
        }

        if (logEntryCount != entries.size()) {
            logEntryCount = entries.size();
            ImGui::SetScrollHereY(1.0f);
        }

        End();

    }

}