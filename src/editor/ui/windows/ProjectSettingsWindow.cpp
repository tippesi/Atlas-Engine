#include "ProjectSettingsWindow.h"
#include "../Widgets.h"
#include "../../Theme.h"
#include "../../Singletons.h"

#include "resource/ResourceManager.h"
#include "texture/Texture2D.h"

#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>

namespace Atlas::Editor::UI {

    ProjectSettingsWindow::ProjectSettingsWindow(bool show) : Window("Project settings", show) {

        AddProjectSettings();
        AddStreamingSettings();

        for (const auto& setting : settings) {
            if (std::find(categories.begin(), categories.end(), setting.category) == categories.end())
                categories.push_back(setting.category);
        }

    }

    void ProjectSettingsWindow::RecordChange(const Ref<Project>& project, const Project& before, const std::string& name) {

        // The window lives as long as the editor, its history too
        auto apply = [this, project](const Project& state) {
            *project = state;
            hasUnsavedChanges = true;
            if (onChange)
                onChange(*project);
            };
        history.Push({ .name = "Change " + name, .undo = [apply, before]() { apply(before); },
            .redo = [apply, after = *project]() { apply(after); } });
        hasUnsavedChanges = true;
        if (onChange)
            onChange(*project);

    }

    void ProjectSettingsWindow::Save(const Ref<Project>& project) {

        if (project->Save())
            hasUnsavedChanges = false;

    }

    void ProjectSettingsWindow::Render(const Ref<Project>& project) {

        SetTitle(name, name);
        if (!Begin())
            return;

        inFocus = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        streamingStats = Streaming::StreamingManager::GetStats();

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##Settings search", "Search settings...", &search);

        // A change is one undo step once its field isn't edited anymore, e.g. after a drag. Combos and checkboxes
        // change while they aren't active
        if (!ImGui::IsAnyItemActive())
            editStart = *project;
        auto trackEdit = [&](const char* name) {
            if (!ImGui::IsItemDeactivatedAfterEdit() && !(ImGui::IsItemEdited() && !ImGui::IsItemActive()))
                return;

            RecordChange(project, editStart, name);
            };

        auto renderCategory = [&](const char* category) {
            // While searching all categories with matches are shown
            if (!search.empty())
                ImGui::SetNextItemOpen(true);
            if (!Widgets::SectionHeader(category, ImGuiTreeNodeFlags_DefaultOpen))
                return;

            // The name of a setting is its label, the widgets of the settings don't show their own
            ImGui::PushID(category);
            if (Widgets::BeginProperties("Settings")) {
                for (const auto& setting : settings) {
                    if (setting.category != category || !Matches(setting))
                        continue;
                    Widgets::PropertyLabel(setting.name);
                    ImGui::PushID(setting.name);
                    ImGui::PushTextWrapPos(0.0f);
                    setting.render(*project);
                    trackEdit(setting.name);
                    ImGui::PopTextWrapPos();
                    ImGui::PopID();
                }
                Widgets::EndProperties();
            }
            ImGui::PopID();
            };

        std::vector<std::pair<const char*, int32_t>> visibleCategories;
        int32_t totalRows = 0;
        for (auto category : categories) {
            auto rows = int32_t(std::count_if(settings.begin(), settings.end(), [&](const Setting& setting) {
                return setting.category == category && Matches(setting);
                }));
            if (rows == 0)
                continue;
            // The header counts as a row
            visibleCategories.push_back({ category, rows + 1 });
            totalRows += rows + 1;
        }

        // Wide windows get columns, the categories keep their order and fill them about evenly
        auto columnWidth = ImGui::GetFontSize() * 32.0f;
        auto columnCount = std::clamp(int32_t(ImGui::GetContentRegionAvail().x / columnWidth), 1,
            std::min(int32_t(visibleCategories.size()), 3));
        if (columnCount > 1 && ImGui::BeginTable("Columns", columnCount, ImGuiTableFlags_SizingStretchSame |
            ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableNextRow();
            int32_t column = -1, rowsBefore = 0;
            for (const auto& [category, rows] : visibleCategories) {
                auto categoryColumn = std::min((2 * rowsBefore + rows) * columnCount / (2 * totalRows), columnCount - 1);
                while (column < categoryColumn)
                    ImGui::TableSetColumnIndex(++column);
                renderCategory(category);
                rowsBefore += rows;
            }
            ImGui::EndTable();
        }
        else {
            for (const auto& [category, rows] : visibleCategories)
                renderCategory(category);
        }

        ImGui::Spacing();
        ImGui::TextColored(Theme::TextMuted(), "%s, engine version of the file: %s", project->path.c_str(),
            project->engineVersion.empty() ? "saved before versions were recorded" : project->engineVersion.c_str());

        End();

    }

    void ProjectSettingsWindow::AddProjectSettings() {

        const char* general = "Project";
        settings.push_back({ general, "Name", "title", [](Project& project) {
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##Name", "Name of the project", &project.name);
            } });
        settings.push_back({ general, "Main script", "lua main.lua game script", [](Project& project) {
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##Main script", "e.g. Main.lua", &project.mainScript);
            ImGui::SetItemTooltip("Lua script that runs while the project plays, relative to the asset directory");
            } });
        settings.push_back({ general, "Start scene", "first scene level", [](Project& project) {
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##Start scene", "e.g. scenes/Main.aescene", &project.startScene);
            ImGui::SetItemTooltip("Scene the project starts with, relative to the asset directory");
            } });

        const char* rendering = "Rendering";
        settings.push_back({ rendering, "Occlusion culling", "culling hidden depth hiz", [](Project& project) {
            ImGui::Checkbox("##Occlusion culling", &project.settings.occlusionCulling);
            ImGui::SetItemTooltip("Skips meshes hidden behind others");
            } });
        settings.push_back({ rendering, "Mesh LOD error scale", "level of detail lod quality pixel error", [](Project& project) {
            ImGui::DragFloat("##Mesh LOD error scale", &project.settings.meshLodErrorScale, 0.05f, 0.0f, 16.0f);
            ImGui::SetItemTooltip("Larger values switch meshes to coarser levels of detail earlier");
            } });

        settings.push_back({ rendering, "Upscaler", "fsr dlss taa anti aliasing resolution", [](Project& project) {
            const auto& names = PostProcessing::PostProcessing::upscalerNames;
            auto selected = int32_t(project.settings.upscaler);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##Upscaler", &selected, names, IM_ARRAYSIZE(names)))
                project.settings.upscaler = PostProcessing::Upscaler(selected);
            if (!Singletons::mainRenderer->IsUpscalerSupported(project.settings.upscaler))
                ImGui::TextDisabled("Not available on this device, FSR 2 is used instead");
            } });

        settings.push_back({ "Scene files", "Binary columns", "save format load speed", [](Project& project) {
            ImGui::Checkbox("##Binary columns", &project.settings.binarySceneColumns);
            ImGui::SetItemTooltip("Stores the common components as raw values in the binary scene files, they load about "
                "twice as fast. Takes effect when a scene is saved");
            } });

    }

    void ProjectSettingsWindow::AddStreamingSettings() {

        auto megabytes = [](size_t bytes) { return float(double(bytes) / (1024.0 * 1024.0)); };

        // Textures and impostors are streamed by their mip levels, each with its own budget
        using Levels = Streaming::StreamingSettings::Levels;
        auto addLevelSettings = [&](const char* category, const char* noun, Levels Streaming::StreamingSettings::* member,
            Streaming::ResidencyStats Streaming::StreamingManager::Stats::* levelStats, const char* editorNote) {
            settings.push_back({ category, "Enable", "", [member, editorNote](Project& project) {
                ImGui::Checkbox("##Enable", &(project.settings.streaming.*member).enable);
                ImGui::SetItemTooltip("Disabled, all of them get their full resolution%s", editorNote);
                } });
            settings.push_back({ category, "Budget (MB)", "memory vram limit megabytes", [member](Project& project) {
                ImGui::DragInt("##Budget (MB)", &(project.settings.streaming.*member).budget, 16.0f, 16, 65536);
                ImGui::SetItemTooltip("Above it all resolutions are lowered evenly");
                } });
            settings.push_back({ category, "Minimum resolution", "mip level quality", [member](Project& project) {
                ImGui::DragInt("##Minimum resolution", &(project.settings.streaming.*member).minResolution, 1.0f, 1, 4096);
                ImGui::SetItemTooltip("They don't go below this resolution and load with it first");
                } });
            settings.push_back({ category, "Resolution scale", "quality mip bias", [member](Project& project) {
                ImGui::SliderFloat("##Resolution scale", &(project.settings.streaming.*member).resolutionScale, 0.125f, 2.0f);
                ImGui::SetItemTooltip("Scales all requests, e.g. 0.5 for half the resolution");
                } });
            settings.push_back({ category, "Reduction delay (frames)", "frames hysteresis", [member](Project& project) {
                ImGui::DragInt("##Reduction delay (frames)", &(project.settings.streaming.*member).reductionDelay, 1.0f, 0, 10000);
                } });
            settings.push_back({ category, "Max pending loads", "jobs parallel", [member](Project& project) {
                ImGui::DragInt("##Max pending loads", &(project.settings.streaming.*member).maxPendingLoads, 0.1f, 1, 64);
                } });
            settings.push_back({ category, "Statistics", "memory vram resident requested", [this, noun, levelStats, megabytes](Project&) {
                const auto& stats = streamingStats.*levelStats;
                ImGui::TextDisabled("%d %s, %d on the GPU, %.0f MB resident, %.0f MB requested", stats.count, noun,
                    stats.residentCount, megabytes(stats.residentBytes), megabytes(stats.requestedBytes));
                ImGui::TextDisabled("Lowered by %d levels for the budget, %d loads pending", stats.budgetBias,
                    stats.pendingLoads);
                } });
            };

        addLevelSettings("Texture streaming", "textures", &Streaming::StreamingSettings::textures,
            &Streaming::StreamingManager::Stats::textures, "");
        settings.push_back({ "Texture streaming", "Not streamed", "png ktx2 mip level", [this](Project&) {
            auto textures = ResourceManager<Texture::Texture2D>::GetResources();
            auto loaded = std::count_if(textures.begin(), textures.end(), [](const auto& texture) { return texture.IsLoaded(); });
            ImGui::TextDisabled("%d other textures stay in full resolution, only ktx2 files with mip levels are streamed",
                std::max(int32_t(loaded) - streamingStats.textures.count, 0));
            } });
        // The editor bakes and saves impostors and meshes, it keeps them resident whatever the project sets
        const char* editorNote = ". The editor keeps them resident to bake and save them";
        addLevelSettings("Impostor streaming", "impostors", &Streaming::StreamingSettings::impostors,
            &Streaming::StreamingManager::Stats::impostors, editorNote);

        const char* meshCategory = "Mesh streaming";
        settings.push_back({ meshCategory, "Enable", "", [editorNote](Project& project) {
            ImGui::Checkbox("##Enable", &project.settings.streaming.meshes.enable);
            ImGui::SetItemTooltip("Disabled, all streamed meshes get their geometry back%s", editorNote);
            } });
        settings.push_back({ meshCategory, "Budget (MB)", "memory vram limit megabytes geometry", [](Project& project) {
            ImGui::DragInt("##Budget (MB)", &project.settings.streaming.meshes.budget, 16.0f, 64, 65536);
            ImGui::SetItemTooltip("Above it meshes far from their impostor distance are shown as impostors only");
            } });
        settings.push_back({ meshCategory, "Distance margin", "prefetch load", [](Project& project) {
            ImGui::DragFloat("##Distance margin", &project.settings.streaming.meshes.distanceMargin, 1.0f, 0.0f, 1000.0f);
            ImGui::SetItemTooltip("Meshes are loaded once their closest instance is this close to where they are needed");
            } });
        settings.push_back({ meshCategory, "Ray tracing distance", "rt blas geometry", [](Project& project) {
            ImGui::DragFloat("##Ray tracing distance", &project.settings.streaming.meshes.rayTracingDistance, 10.0f, 0.0f, 10000.0f);
            ImGui::SetItemTooltip("While ray tracing, meshes keep their geometry up to this distance at most");
            } });
        settings.push_back({ meshCategory, "Unload delay (frames)", "frames hysteresis", [](Project& project) {
            ImGui::DragInt("##Unload delay (frames)", &project.settings.streaming.meshes.reductionDelay, 1.0f, 0, 10000);
            } });
        settings.push_back({ meshCategory, "Max pending loads", "jobs parallel", [](Project& project) {
            ImGui::DragInt("##Max pending loads", &project.settings.streaming.meshes.maxPendingLoads, 0.1f, 1, 64);
            } });
        settings.push_back({ meshCategory, "Statistics", "memory vram resident requested impostors", [this, megabytes](Project&) {
            const auto& stats = streamingStats.meshes;
            ImGui::TextDisabled("%d meshes, %d with geometry, %d loads pending", stats.count, stats.residentCount,
                stats.pendingLoads);
            ImGui::TextDisabled("%.0f MB geometry resident, %.0f MB requested", megabytes(stats.residentBytes),
                megabytes(stats.requestedBytes));
            } });

    }

    bool ProjectSettingsWindow::Matches(const Setting& setting) const {

        if (search.empty())
            return true;

        auto toLower = [](std::string text) {
            std::transform(text.begin(), text.end(), text.begin(),
                [](unsigned char c) { return char(std::tolower(c)); });
            return text;
            };

        auto text = toLower(std::string(setting.category) + " " + setting.name + " " + setting.keywords);
        return text.find(toLower(search)) != std::string::npos;

    }

}
