#include "ProjectHubWindow.h"
#include "../Widgets.h"
#include "../../Theme.h"

#include <common/SerializationHelper.h>

#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace Atlas::Editor::UI {

    ProjectHubWindow::ProjectHubWindow(const std::string& standardDataDirectory, const std::string& templatesDirectory)
        : standardDataDirectory(standardDataDirectory), templatesDirectory(templatesDirectory) {

        for (int32_t i = 0; i < 3; i++)
            dialogRequests[i] = { this, DialogType(i) };

        auto documents = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
        newLocation = documents ? fs::path(documents).generic_string() : fs::current_path().generic_string();
        if (newLocation.ends_with('/'))
            newLocation.pop_back();

        LoadEntries();
        LoadTemplates();

    }

    std::optional<std::string> ProjectHubWindow::Render(SDL_Window* window) {

        auto openPath = TakeDialogResult();

        // Laid out like the empty state of the editor: one column in the middle of the window
        auto viewport = ImGui::GetMainViewport();
        ImGui::GetBackgroundDrawList(viewport)->AddRectFilled(viewport->Pos,
            ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), ImGui::GetColorU32(Theme::Base()));
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("##Projects", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);

        // Heading and buttons are centered over the list
        const auto columnWidth = 560.0f;
        auto center = [&](float width) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (columnWidth - width) * 0.5f));
            };
        auto buttonWidth = [](const char* label) {
            return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
            };

        ImGui::Dummy(ImVec2(columnWidth, 0.0f));
        ImGui::SetWindowFontScale(1.5f);
        center(ImGui::CalcTextSize("Atlas Engine").x);
        ImGui::TextUnformatted("Atlas Engine");
        ImGui::SetWindowFontScale(1.0f);
        const char* subtitle = "Open a project or create a new one";
        center(ImGui::CalcTextSize(subtitle).x);
        ImGui::TextDisabled("%s", subtitle);

        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        center(buttonWidth("New project...") + buttonWidth("Open...") + buttonWidth("Add folder...") +
            ImGui::GetStyle().ItemSpacing.x * 2.0f);
        Theme::PushPrimaryButton();
        if (ImGui::Button("New project..."))
            OpenNewProjectPopup();
        Theme::PopPrimaryButton();
        ImGui::SameLine();
        if (ImGui::Button("Open..."))
            ShowDialog(window, DialogType::OpenProject);
        ImGui::SetItemTooltip("Opens a .aeproject file");
        ImGui::SameLine();
        if (ImGui::Button("Add folder..."))
            ShowDialog(window, DialogType::AddFolder);
        ImGui::SetItemTooltip("Adds a folder as a project, a project file is created if it has none");

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::SetNextItemWidth(columnWidth);
        ImGui::InputTextWithHint("##Project search", "Search projects...", &search);

        if (!error.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            Widgets::Callout(error.c_str(), Theme::Error());
        }

        auto matches = [&](const RecentProjects::Entry& entry) {
            auto lower = [](std::string text) {
                std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
                return text;
                };
            auto query = lower(search);
            return query.empty() || lower(entry.name).find(query) != std::string::npos ||
                lower(entry.path).find(query) != std::string::npos;
            };

        auto formatAge = [](int64_t lastOpened) {
            auto now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            auto seconds = std::max(now - lastOpened, int64_t(0));
            if (seconds < 60) return std::string("just now");
            if (seconds < 3600) return std::to_string(seconds / 60) + " min ago";
            if (seconds < 86400) return std::to_string(seconds / 3600) + " h ago";
            if (seconds < 2 * 86400) return std::string("yesterday");
            return std::to_string(seconds / 86400) + " days ago";
            };

        std::string removePath;
        if (!entries.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::SeparatorText("Recent projects");
        }

        for (size_t i = 0; i < entries.size(); i++) {
            const auto& entry = entries[i];
            if (!matches(entry))
                continue;

            ImGui::PushID(entry.path.c_str());
            auto start = ImGui::GetCursorPos();
            auto lineHeight = ImGui::GetTextLineHeightWithSpacing();
            ImGui::BeginDisabled(!entryExists[i]);
            if (ImGui::Selectable("##Project", false, ImGuiSelectableFlags_None, ImVec2(columnWidth, lineHeight * 2.0f)))
                openPath = entry.path;
            ImGui::EndDisabled();
            // Long paths are shortened, the window would otherwise grow wider than the column
            auto pathShortened = ImGui::CalcTextSize(entry.path.c_str()).x > columnWidth;
            if (!entryExists[i])
                ImGui::SetItemTooltip("The project file doesn't exist anymore");
            else if (pathShortened)
                ImGui::SetItemTooltip("%s", entry.path.c_str());
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Remove from list"))
                    removePath = entry.path;
                ImGui::EndPopup();
            }
            auto end = ImGui::GetCursorPos();

            auto age = formatAge(entry.lastOpened);
            auto ageWidth = ImGui::CalcTextSize(age.c_str()).x;
            ImGui::SetCursorPos(start);
            Widgets::TextEllipsis(entry.name.c_str(), nullptr, columnWidth - ageWidth - ImGui::GetStyle().ItemSpacing.x);
            ImGui::SetCursorPos(ImVec2(start.x + columnWidth - ageWidth, start.y));
            ImGui::TextDisabled("%s", age.c_str());
            ImGui::SetCursorPos(ImVec2(start.x, start.y + lineHeight));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            Widgets::TextEllipsis(entry.path.c_str(), nullptr, columnWidth);
            ImGui::PopStyleColor();
            ImGui::SetCursorPos(end);

            ImGui::PopID();
        }

        if (!removePath.empty()) {
            RecentProjects::Remove(removePath);
            LoadEntries();
        }

        if (auto created = RenderNewProjectPopup(window))
            openPath = created;

        ImGui::End();

        return openPath;

    }

    void ProjectHubWindow::OpenNewProjectPopup() {

        newProjectPopupRequested = true;
        error.clear();

    }

    std::optional<std::string> ProjectHubWindow::TakeDialogResult() {

        std::scoped_lock lock(dialogMutex);
        if (!dialogResult)
            return std::nullopt;

        auto [type, path] = *dialogResult;
        dialogResult.reset();
        if (type == DialogType::OpenProject)
            return path;
        if (type == DialogType::AddFolder) {
            auto projectPath = AddFolder(path);
            return projectPath.empty() ? std::nullopt : std::optional<std::string>(projectPath);
        }
        newLocation = path;
        return std::nullopt;

    }

    std::optional<std::string> ProjectHubWindow::RenderNewProjectPopup(SDL_Window* window) {

        // The popup also opens inside a project, without the hub around it
        TakeDialogResult();

        if (newProjectPopupRequested) {
            ImGui::OpenPopup("New project");
            newProjectPopupRequested = false;
            newName.clear();
        }

        auto viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(640.0f, 0.0f), ImGuiCond_Appearing);

        if (!ImGui::BeginPopupModal("New project", nullptr, ImGuiWindowFlags_NoSavedSettings))
            return std::nullopt;

        std::optional<std::string> created;

        if (Widgets::BeginProperties("New project")) {
            Widgets::PropertyLabel("Name");
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##Name", "Name of the project", &newName);

            Widgets::PropertyLabel("Location");
            ImGui::SetNextItemWidth(-ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);
            ImGui::InputText("##Location", &newLocation);
            ImGui::SameLine();
            if (ImGui::Button("...", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
                ShowDialog(window, DialogType::Location);
            Widgets::EndProperties();
        }

        ImGui::SeparatorText("Template");
        for (int32_t i = 0; i < int32_t(templates.size()); i++) {
            ImGui::PushID(i);
            if (ImGui::Selectable(templates[i].name.c_str(), templateIndex == i))
                templateIndex = i;
            if (!templates[i].description.empty()) {
                ImGui::Indent();
                ImGui::TextColored(Theme::TextMuted(), "%s", templates[i].description.c_str());
                ImGui::Unindent();
            }
            ImGui::PopID();
        }
        if (templates.empty())
            Widgets::Callout(("No templates found in " + templatesDirectory).c_str(), Theme::Warning());

        auto target = (fs::path(newLocation) / newName).generic_string();
        std::string validation;
        if (newName.empty())
            validation = "The project needs a name";
        else if (newName.find_first_of("<>:\"/\\|?*") != std::string::npos)
            validation = "The name can't contain < > : \" / \\ | ? *";
        else if (!fs::is_directory(newLocation))
            validation = "The location doesn't exist";
        else if (fs::exists(target) && !fs::is_empty(target))
            validation = target + " isn't empty";
        else if (templates.empty())
            validation = "There is no template";

        ImGui::Spacing();
        if (validation.empty())
            ImGui::TextColored(Theme::TextMuted(), "Creates %s/%s.aeproject", target.c_str(), newName.c_str());
        else
            ImGui::TextColored(Theme::Warning(), "%s", validation.c_str());
        if (!error.empty())
            Widgets::Callout(error.c_str(), Theme::Error());

        ImGui::Spacing();
        ImGui::BeginDisabled(!validation.empty());
        if (ImGui::Button("Create") || (validation.empty() && ImGui::IsKeyPressed(ImGuiKey_Enter, false))) {
            templateIndex = std::clamp(templateIndex, 0, int32_t(templates.size()) - 1);
            auto project = Project::Create(newLocation, newName, templates[templateIndex].directory, standardDataDirectory);
            if (project) {
                RecentProjects::Add(*project);
                LoadEntries();
                created = project->path;
                ImGui::CloseCurrentPopup();
            }
            else {
                error = "The project couldn't be created, the log has the details";
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();

        return created;

    }

    void ProjectHubWindow::LoadEntries() {

        // Projects might have been moved or deleted, which is checked once instead of every frame
        entries = RecentProjects::Load();
        entryExists.clear();
        for (const auto& entry : entries)
            entryExists.push_back(fs::exists(entry.path));

    }

    void ProjectHubWindow::LoadTemplates() {

        std::error_code errorCode;
        for (const auto& entry : fs::directory_iterator(templatesDirectory, errorCode)) {
            if (!entry.is_directory())
                continue;

            bool hasProject = false;
            for (const auto& file : fs::directory_iterator(entry.path(), errorCode))
                hasProject |= file.path().extension() == ".aeproject";
            if (!hasProject)
                continue;

            Template projectTemplate = {
                .directory = entry.path().generic_string(),
                .name = entry.path().filename().string()
            };

            std::ifstream stream(entry.path() / "template.json", std::ios::binary);
            if (stream.is_open()) {
                auto j = json::parse(stream, nullptr, false);
                if (!j.is_discarded())
                    try_get_json(j, "description", projectTemplate.description);
            }

            templates.push_back(projectTemplate);
        }

        // The empty template comes first, it's the one most projects start from
        std::stable_sort(templates.begin(), templates.end(), [](const Template& a, const Template& b) {
            return a.name == "Empty" && b.name != "Empty";
            });

    }

    std::string ProjectHubWindow::AddFolder(const std::string& folder) {

        std::error_code errorCode;
        for (const auto& entry : fs::directory_iterator(folder, errorCode)) {
            if (entry.path().extension() == ".aeproject")
                return entry.path().generic_string();
        }

        Project project;
        project.name = fs::path(folder).filename().string();
        project.assetDirectory = fs::is_directory(fs::path(folder) / "data") ? "data" : ".";
        project.path = (fs::path(folder) / (project.name + ".aeproject")).generic_string();
        if (!project.Save()) {
            error = "Couldn't create a project file in " + folder;
            return std::string();
        }

        return project.path;

    }

    void ProjectHubWindow::ShowDialog(SDL_Window* window, DialogType type) {

        auto request = &dialogRequests[int32_t(type)];
        if (type == DialogType::OpenProject) {
            static const SDL_DialogFileFilter filter = { "Atlas Engine project", "aeproject" };
            SDL_ShowOpenFileDialog(DialogCallback, request, window, &filter, 1, nullptr, false);
        }
        else {
            auto location = type == DialogType::Location && fs::is_directory(newLocation) ? newLocation.c_str() : nullptr;
            SDL_ShowOpenFolderDialog(DialogCallback, request, window, location, false);
        }

    }

    void ProjectHubWindow::DialogCallback(void* userData, const char* const* files, int32_t filter) {

        // No list on errors, an empty one if the user cancelled
        if (!files || !files[0])
            return;

        auto request = static_cast<DialogRequest*>(userData);
        std::scoped_lock lock(request->window->dialogMutex);
        request->window->dialogResult = std::make_pair(request->type, fs::path(files[0]).generic_string());

    }

}
