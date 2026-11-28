#include "ProjectSettingsWindow.h"
#include "../Widgets.h"
#include "../../Theme.h"
#include "../../Singletons.h"

#include "resource/ResourceManager.h"
#include "texture/Texture2D.h"

#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace Atlas::Editor::UI {

    ProjectSettingsWindow::ProjectSettingsWindow(bool show) : Window("Project settings", show) {

        pages = { { "Project", IconType::Document }, { "Rendering", IconType::PostProcessing }, { "Streaming", IconType::Mesh } };

        AddProjectSettings();
        AddStreamingSettings();

        for (const auto& setting : settings) {
            auto page = std::find_if(pages.begin(), pages.end(), [&](const Page& page) { return std::strcmp(page.name, setting.page) == 0; });
            AE_ASSERT(page != pages.end() && "Settings need a page");
            if (std::none_of(page->sections.begin(), page->sections.end(), [&](const char* section) { return std::strcmp(section, setting.section) == 0; }))
                page->sections.push_back(setting.section);
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

        auto countMatches = [&](const char* page, const char* section) {
            return std::count_if(settings.begin(), settings.end(), [&](const Setting& setting) {
                return std::strcmp(setting.page, page) == 0 && (!section || std::strcmp(setting.section, section) == 0) && Matches(setting);
                });
            };

        auto renderSection = [&](const Page& page, const char* section) {
            if (!countMatches(page.name, section))
                return;

            // A page with one section only has its heading
            ImGui::PushID(section);
            if (page.sections.size() > 1)
                Widgets::PropertySection(section);

            // The name of a setting is its label, the widgets of the settings don't show their own
            if (Widgets::BeginProperties("Settings")) {
                for (const auto& setting : settings) {
                    if (std::strcmp(setting.page, page.name) != 0 || std::strcmp(setting.section, section) != 0 || !Matches(setting))
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

        auto footerHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        auto pagesWidth = ImGui::GetFontSize() * 10.0f;

        // Pages without a match of the search can't be picked, picking one ends the search
        ImGui::BeginChild("Pages", ImVec2(pagesWidth, -footerHeight));
        for (int32_t i = 0; i < int32_t(pages.size()); i++) {
            const auto& page = pages[i];
            ImGui::BeginDisabled(!countMatches(page.name, nullptr));
            if (Widgets::IconListItem(page.name, Singletons::icons->Get(page.icon), search.empty() && i == selectedPage)) {
                selectedPage = i;
                search.clear();
            }
            ImGui::EndDisabled();
        }
        ImGui::EndChild();

        auto spacing = ImGui::GetStyle().ItemSpacing.x;
        auto lineX = ImGui::GetItemRectMax().x + spacing;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(lineX, ImGui::GetItemRectMin().y), ImVec2(lineX, ImGui::GetItemRectMax().y),
            ImGui::GetColorU32(ImGuiCol_Separator));

        // Fields as wide as the window would be hard to read
        ImGui::SameLine(0.0f, 2.0f * spacing);
        ImGui::BeginChild("Settings", ImVec2(std::min(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 48.0f), -footerHeight));
        // While searching all pages with matches are shown
        for (int32_t i = 0; i < int32_t(pages.size()); i++) {
            const auto& page = pages[i];
            if (search.empty() ? i != selectedPage : !countMatches(page.name, nullptr))
                continue;
            ImGui::PushID(page.name);
            Widgets::Heading(page.name);
            for (auto section : page.sections)
                renderSection(page, section);
            ImGui::Spacing();
            ImGui::PopID();
        }
        if (!search.empty() && std::none_of(pages.begin(), pages.end(), [&](const Page& page) { return countMatches(page.name, nullptr); }))
            Widgets::EmptyState("No setting matches the search");
        ImGui::EndChild();

        ImGui::TextColored(Theme::TextMuted(), "%s, engine version of the file: %s", project->path.c_str(),
            project->engineVersion.empty() ? "saved before versions were recorded" : project->engineVersion.c_str());

        End();

    }

    void ProjectSettingsWindow::AddProjectSettings() {

        const char* general = "Project";
        settings.push_back({ general, "General", "Name", "title", [](Project& project) {
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##Name", "Name of the project", &project.name);
            } });
        settings.push_back({ general, "General", "Main script", "lua main.lua game script", [](Project& project) {
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##Main script", "e.g. Main.lua", &project.mainScript);
            ImGui::SetItemTooltip("Lua script that runs while the project plays, relative to the asset directory");
            } });
        settings.push_back({ general, "General", "Start scene", "first scene level", [](Project& project) {
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##Start scene", "e.g. scenes/Main.aescene", &project.startScene);
            ImGui::SetItemTooltip("Scene the project starts with, relative to the asset directory");
            } });

        const char* rendering = "Rendering";
        settings.push_back({ rendering, rendering, "Occlusion culling", "culling hidden depth hiz", [](Project& project) {
            ImGui::Checkbox("##Occlusion culling", &project.settings.occlusionCulling);
            ImGui::SetItemTooltip("Skips meshes hidden behind others");
            } });
        settings.push_back({ rendering, rendering, "Mesh LOD error scale", "level of detail lod quality pixel error", [](Project& project) {
            ImGui::DragFloat("##Mesh LOD error scale", &project.settings.meshLodErrorScale, 0.05f, 0.0f, 16.0f);
            ImGui::SetItemTooltip("Larger values switch meshes to coarser levels of detail earlier");
            } });

        settings.push_back({ rendering, rendering, "Upscaler", "fsr dlss taa anti aliasing resolution", [](Project& project) {
            const auto& names = PostProcessing::PostProcessing::upscalerNames;
            auto selected = int32_t(project.settings.upscaler);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##Upscaler", &selected, names, IM_ARRAYSIZE(names)))
                project.settings.upscaler = PostProcessing::Upscaler(selected);
            if (!Singletons::mainRenderer->IsUpscalerSupported(project.settings.upscaler))
                ImGui::TextDisabled("Not available on this device, FSR 2 is used instead");
            } });

        settings.push_back({ general, "Scene files", "Binary columns", "save format load speed", [](Project& project) {
            ImGui::Checkbox("##Binary columns", &project.settings.binarySceneColumns);
            ImGui::SetItemTooltip("Stores the common components as raw values in the binary scene files, they load about "
                "twice as fast. Takes effect when a scene is saved");
            } });

    }

    void ProjectSettingsWindow::AddStreamingSettings() {

        const char* streaming = "Streaming";
        auto megabytes = [](size_t bytes) { return float(double(bytes) / (1024.0 * 1024.0)); };

        // Textures and impostors are streamed by their mip levels, each with its own budget
        using Levels = Streaming::StreamingSettings::Levels;
        auto addLevelSettings = [&](const char* section, const char* noun, Levels Streaming::StreamingSettings::* member,
            Streaming::ResidencyStats Streaming::StreamingManager::Stats::* levelStats, const char* editorNote) {
            settings.push_back({ streaming, section, "Enable", "", [member, editorNote](Project& project) {
                ImGui::Checkbox("##Enable", &(project.settings.streaming.*member).enable);
                ImGui::SetItemTooltip("Disabled, all of them get their full resolution%s", editorNote);
                } });
            settings.push_back({ streaming, section, "Budget (MB)", "memory vram limit megabytes", [member](Project& project) {
                ImGui::DragInt("##Budget (MB)", &(project.settings.streaming.*member).budget, 16.0f, 16, 65536);
                ImGui::SetItemTooltip("Above it all resolutions are lowered evenly");
                } });
            settings.push_back({ streaming, section, "Minimum resolution", "mip level quality", [member](Project& project) {
                ImGui::DragInt("##Minimum resolution", &(project.settings.streaming.*member).minResolution, 1.0f, 1, 4096);
                ImGui::SetItemTooltip("They don't go below this resolution and load with it first");
                } });
            settings.push_back({ streaming, section, "Resolution scale", "quality mip bias", [member](Project& project) {
                ImGui::SliderFloat("##Resolution scale", &(project.settings.streaming.*member).resolutionScale, 0.125f, 2.0f);
                ImGui::SetItemTooltip("Scales all requests, e.g. 0.5 for half the resolution");
                } });
            settings.push_back({ streaming, section, "Reduction delay (frames)", "frames hysteresis", [member](Project& project) {
                ImGui::DragInt("##Reduction delay (frames)", &(project.settings.streaming.*member).reductionDelay, 1.0f, 0, 10000);
                } });
            settings.push_back({ streaming, section, "Max pending loads", "jobs parallel", [member](Project& project) {
                ImGui::DragInt("##Max pending loads", &(project.settings.streaming.*member).maxPendingLoads, 0.1f, 1, 64);
                } });
            settings.push_back({ streaming, section, "Statistics", "memory vram resident requested", [this, noun, levelStats, megabytes](Project&) {
                const auto& stats = streamingStats.*levelStats;
                ImGui::TextDisabled("%d %s, %d on the GPU, %.0f MB resident, %.0f MB requested", stats.count, noun,
                    stats.residentCount, megabytes(stats.residentBytes), megabytes(stats.requestedBytes));
                ImGui::TextDisabled("Lowered by %d levels for the budget, %d loads pending", stats.budgetBias,
                    stats.pendingLoads);
                } });
            };

        addLevelSettings("Textures", "textures", &Streaming::StreamingSettings::textures,
            &Streaming::StreamingManager::Stats::textures, "");
        settings.push_back({ streaming, "Textures", "Not streamed", "png ktx2 mip level", [this](Project&) {
            auto textures = ResourceManager<Texture::Texture2D>::GetResources();
            auto loaded = std::count_if(textures.begin(), textures.end(), [](const auto& texture) { return texture.IsLoaded(); });
            ImGui::TextDisabled("%d other textures stay in full resolution, only ktx2 files with mip levels are streamed",
                std::max(int32_t(loaded) - streamingStats.textures.count, 0));
            } });
        // The editor bakes and saves impostors and meshes, it keeps them resident whatever the project sets
        const char* editorNote = ". The editor keeps them resident to bake and save them";
        addLevelSettings("Impostors", "impostors", &Streaming::StreamingSettings::impostors,
            &Streaming::StreamingManager::Stats::impostors, editorNote);

        const char* meshes = "Meshes";
        settings.push_back({ streaming, meshes, "Enable", "", [editorNote](Project& project) {
            ImGui::Checkbox("##Enable", &project.settings.streaming.meshes.enable);
            ImGui::SetItemTooltip("Disabled, all streamed meshes get their geometry back%s", editorNote);
            } });
        settings.push_back({ streaming, meshes, "Budget (MB)", "memory vram limit megabytes geometry", [](Project& project) {
            ImGui::DragInt("##Budget (MB)", &project.settings.streaming.meshes.budget, 16.0f, 64, 65536);
            ImGui::SetItemTooltip("Above it meshes far from their impostor distance are shown as impostors only");
            } });
        settings.push_back({ streaming, meshes, "Distance margin", "prefetch load", [](Project& project) {
            ImGui::DragFloat("##Distance margin", &project.settings.streaming.meshes.distanceMargin, 1.0f, 0.0f, 1000.0f);
            ImGui::SetItemTooltip("Meshes are loaded once their closest instance is this close to where they are needed");
            } });
        settings.push_back({ streaming, meshes, "Ray tracing distance", "rt blas geometry", [](Project& project) {
            ImGui::DragFloat("##Ray tracing distance", &project.settings.streaming.meshes.rayTracingDistance, 10.0f, 0.0f, 10000.0f);
            ImGui::SetItemTooltip("While ray tracing, meshes keep their geometry up to this distance at most");
            } });
        settings.push_back({ streaming, meshes, "Unload delay (frames)", "frames hysteresis", [](Project& project) {
            ImGui::DragInt("##Unload delay (frames)", &project.settings.streaming.meshes.reductionDelay, 1.0f, 0, 10000);
            } });
        settings.push_back({ streaming, meshes, "Max pending loads", "jobs parallel", [](Project& project) {
            ImGui::DragInt("##Max pending loads", &project.settings.streaming.meshes.maxPendingLoads, 0.1f, 1, 64);
            } });
        settings.push_back({ streaming, meshes, "Statistics", "memory vram resident requested impostors", [this, megabytes](Project&) {
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

        auto text = toLower(std::string(setting.page) + " " + setting.section + " " + setting.name + " " + setting.keywords);
        return text.find(toLower(search)) != std::string::npos;

    }

}
