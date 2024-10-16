#include "LogWindow.h"

#include "Log.h"
#include "../../Singletons.h"

namespace Atlas::Editor::UI {

    void LogWindow::Render() {

        if (!Begin())
            return;

        auto darkMode = Singletons::config->darkMode;

        static size_t entriesCount = 0;

        auto entries = Atlas::Log::GetLatestEntries(10000);
        for (auto& entry : entries) {
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

        if (entriesCount != entries.size()) {
            ImGui::SetScrollHereY(1.0f);
            entriesCount = entries.size();
        }

        End();

    }

}