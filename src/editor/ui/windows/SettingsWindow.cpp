#include "SettingsWindow.h"
#include "../Widgets.h"

#include "resource/ResourceManager.h"
#include "texture/Texture2D.h"

#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>

namespace Atlas::Editor::UI {

    SettingsWindow::SettingsWindow(bool show) : Window("Engine settings", show) {

        AddStreamingSettings();

        for (const auto& setting : settings) {
            if (std::find(categories.begin(), categories.end(), setting.category) == categories.end())
                categories.push_back(setting.category);
        }

    }

    void SettingsWindow::Render() {

        if (!Begin())
            return;

        streamingStats = Streaming::StreamingManager::GetStats();

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##Settings search", "Search settings...", &search);

        for (auto category : categories) {
            auto hasMatch = std::any_of(settings.begin(), settings.end(), [&](const Setting& setting) {
                return setting.category == category && Matches(setting);
                });
            if (!hasMatch)
                continue;

            // While searching all categories with matches are shown
            if (!search.empty())
                ImGui::SetNextItemOpen(true);
            if (!Widgets::SectionHeader(category, ImGuiTreeNodeFlags_DefaultOpen))
                continue;

            // The name of a setting is its label, the widgets of the settings don't show their own
            ImGui::PushID(category);
            if (Widgets::BeginProperties("Settings")) {
                for (const auto& setting : settings) {
                    if (setting.category != category || !Matches(setting))
                        continue;
                    Widgets::PropertyLabel(setting.name);
                    ImGui::PushID(setting.name);
                    ImGui::PushTextWrapPos(0.0f);
                    setting.render();
                    ImGui::PopTextWrapPos();
                    ImGui::PopID();
                }
                Widgets::EndProperties();
            }
            ImGui::PopID();
        }

        End();

    }

    void SettingsWindow::AddStreamingSettings() {

        auto megabytes = [](size_t bytes) { return float(double(bytes) / (1024.0 * 1024.0)); };

        // Textures and impostors are streamed by their mip levels, each with its own budget
        auto addLevelSettings = [&](const char* category, const char* noun, Streaming::StreamingSettings::Levels& levels,
            Streaming::ResidencyStats Streaming::StreamingManager::Stats::* levelStats) {
            settings.push_back({ category, "Enable", "", [&levels]() {
                ImGui::Checkbox("##Enable", &levels.enable);
                ImGui::SetItemTooltip("Disabled, all of them get their full resolution");
                } });
            settings.push_back({ category, "Budget (MB)", "memory vram limit megabytes", [&levels]() {
                ImGui::DragInt("##Budget (MB)", &levels.budget, 16.0f, 16, 65536);
                ImGui::SetItemTooltip("Above it all resolutions are lowered evenly");
                } });
            settings.push_back({ category, "Minimum resolution", "mip level quality", [&levels]() {
                ImGui::DragInt("##Minimum resolution", &levels.minResolution, 1.0f, 1, 4096);
                ImGui::SetItemTooltip("They don't go below this resolution and load with it first");
                } });
            settings.push_back({ category, "Resolution scale", "quality mip bias", [&levels]() {
                ImGui::SliderFloat("##Resolution scale", &levels.resolutionScale, 0.125f, 2.0f);
                ImGui::SetItemTooltip("Scales all requests, e.g. 0.5 for half the resolution");
                } });
            settings.push_back({ category, "Reduction delay (frames)", "frames hysteresis", [&levels]() {
                ImGui::DragInt("##Reduction delay (frames)", &levels.reductionDelay, 1.0f, 0, 10000);
                } });
            settings.push_back({ category, "Max pending loads", "jobs parallel", [&levels]() {
                ImGui::DragInt("##Max pending loads", &levels.maxPendingLoads, 0.1f, 1, 64);
                } });
            settings.push_back({ category, "Statistics", "memory vram resident requested", [this, noun, levelStats, megabytes, &levels]() {
                const auto& stats = streamingStats.*levelStats;
                ImGui::TextDisabled("%d %s, %d on the GPU, %.0f MB resident, %.0f MB requested", stats.count, noun,
                    stats.residentCount, megabytes(stats.residentBytes), megabytes(stats.requestedBytes));
                ImGui::TextDisabled("Lowered by %d levels for the budget, %d loads pending", stats.budgetBias,
                    stats.pendingLoads);
                if (!levels.enable)
                    ImGui::TextDisabled("Disabled, they keep their full resolution");
                } });
            };

        addLevelSettings("Texture streaming", "textures", Streaming::StreamingManager::settings.textures,
            &Streaming::StreamingManager::Stats::textures);
        settings.push_back({ "Texture streaming", "Not streamed", "png ktx2 mip level", [this]() {
            auto textures = ResourceManager<Texture::Texture2D>::GetResources();
            auto loaded = std::count_if(textures.begin(), textures.end(), [](const auto& texture) { return texture.IsLoaded(); });
            ImGui::TextDisabled("%d other textures stay in full resolution, only ktx2 files with mip levels are streamed",
                std::max(int32_t(loaded) - streamingStats.textures.count, 0));
            } });
        addLevelSettings("Impostor streaming", "impostors", Streaming::StreamingManager::settings.impostors,
            &Streaming::StreamingManager::Stats::impostors);

        auto& meshes = Streaming::StreamingManager::settings.meshes;
        const char* meshCategory = "Mesh streaming";

        settings.push_back({ meshCategory, "Enable", "", [&meshes]() {
            ImGui::Checkbox("##Enable", &meshes.enable);
            ImGui::SetItemTooltip("Disabled, all streamed meshes get their geometry back");
            } });
        settings.push_back({ meshCategory, "Budget (MB)", "memory vram limit megabytes geometry", [&meshes]() {
            ImGui::DragInt("##Budget (MB)", &meshes.budget, 16.0f, 64, 65536);
            ImGui::SetItemTooltip("Above it meshes far from their impostor distance are shown as impostors only");
            } });
        settings.push_back({ meshCategory, "Distance margin", "prefetch load", [&meshes]() {
            ImGui::DragFloat("##Distance margin", &meshes.distanceMargin, 1.0f, 0.0f, 1000.0f);
            ImGui::SetItemTooltip("Meshes are loaded once their closest instance is this close to where they are needed");
            } });
        settings.push_back({ meshCategory, "Ray tracing distance", "rt blas geometry", [&meshes]() {
            ImGui::DragFloat("##Ray tracing distance", &meshes.rayTracingDistance, 10.0f, 0.0f, 10000.0f);
            ImGui::SetItemTooltip("While ray tracing, meshes keep their geometry up to this distance at most");
            } });
        settings.push_back({ meshCategory, "Unload delay (frames)", "frames hysteresis", [&meshes]() {
            ImGui::DragInt("##Unload delay (frames)", &meshes.reductionDelay, 1.0f, 0, 10000);
            } });
        settings.push_back({ meshCategory, "Max pending loads", "jobs parallel", [&meshes]() {
            ImGui::DragInt("##Max pending loads", &meshes.maxPendingLoads, 0.1f, 1, 64);
            } });
        settings.push_back({ meshCategory, "Statistics", "memory vram resident requested impostors", [this, megabytes]() {
            const auto& stats = streamingStats.meshes;
            ImGui::TextDisabled("%d meshes, %d with geometry, %d loads pending", stats.count, stats.residentCount,
                stats.pendingLoads);
            ImGui::TextDisabled("%.0f MB geometry resident, %.0f MB requested", megabytes(stats.residentBytes),
                megabytes(stats.requestedBytes));
            if (!Streaming::StreamingManager::settings.meshes.enable)
                ImGui::TextDisabled("Disabled, all meshes keep their geometry");
            } });

    }

    bool SettingsWindow::Matches(const Setting& setting) const {

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
