#include "TerrainPanel.h"
#include <tools/TerrainTool.h>

namespace Atlas::Editor::UI {

    void TerrainPanel::Render(ResourceHandle<Terrain::Terrain> terrain, Ref<Scene::Scene> scene) {

        scene->terrain = terrainSelectionPanel.Render(scene->terrain);

        if (ImGui::CollapsingHeader("General")) {
            RenderGeneralSettings(terrain);
        }
        if (ImGui::CollapsingHeader("Materials")) {
            RenderMaterialSettings(terrain);
        }
        if (ImGui::CollapsingHeader("Editing")) {
            RenderEditingSettings(terrain);
        }
        if (ImGui::CollapsingHeader("Generator")) {
            if (scene->terrain.IsLoaded())
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Regenerating the terrain overrides any manual changes.");

            terrainGenerator.Render();

            auto generatedTerrain = terrainGenerator.GetTerrain();
            if (generatedTerrain != nullptr) {
                scene->terrain = generatedTerrain;

                auto resource = scene->terrain.GetResource();
                ResourceManager<Terrain::Terrain>::AddResource(resource->path, resource);
            }
        }

    }

    void TerrainPanel::RenderGeneralSettings(ResourceHandle<Terrain::Terrain>& terrain) {

        if (!terrain.IsLoaded()) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No terrain generated or selected.");
            return;
        }

        ImGui::Checkbox("Wireframe", &terrain->wireframe);
        ImGui::SliderFloat("Height", &terrain->heightScale, 1.0f, 1000.0f, "%.3f", 2.0f);

        ImGui::Separator();
        ImGui::Text("Tessellation");

        ImGui::SliderFloat("Factor", &terrain->tessellationFactor, 0.0f,
            16000.0f, "%.3f", 4.0f);

        ImGui::SliderFloat("Slope", &terrain->tessellationSlope, 0.0f, 10.0f);
        ImGui::SliderFloat("Shift", &terrain->tessellationShift, 0.0f, 10.0f);

        ImGui::SliderFloat("Subdivisions", &terrain->maxTessellationLevel, 1.0f,
            64.0f);
        ImGui::SliderFloat("Distance", &terrain->displacementDistance, 0.0f, 100.0f,
            "%.3f", ImGuiSliderFlags_Logarithmic);

        ImGui::Separator();
        ImGui::Text("Lod distances");

        for (size_t i = 0; i < terrain->LoDCount; i++) {
            auto distance = terrain->GetLoDDistance((int32_t)i);
            ImGui::SliderFloat(("LoD" + std::to_string(i)).c_str(),
                &distance, 1.0f, 8192.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            terrain->SetLoDDistance((int32_t)i, distance);
        }

    }

    void TerrainPanel::RenderMaterialSettings(ResourceHandle<Terrain::Terrain>& terrain) {

        if (!terrain.IsLoaded()) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No terrain generated or selected.");
            return;
        }

        materialsPanel.Render(Singletons::imguiWrapper, terrain->storage->materials,
            [&](ResourceHandle<Material> material) {
                return materialSelectionPanel.Render(material);
            },
            [&](ResourceHandle<Texture::Texture2D> texture) {
                return textureSelectionPanel.Render(texture);
            });

    }

    void TerrainPanel::RenderEditingSettings(ResourceHandle<Terrain::Terrain>& terrain) {

        if (!terrain.IsLoaded()) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No terrain generated or selected.");
            return;
        }

        auto region = ImGui::GetContentRegionAvail();

        if (ImGui::Button("Bake terrain", ImVec2(region.x, 0.0f))) {
            Tools::TerrainTool::BakeTerrain(terrain.Get());
        }

        /*
        ImGui::Checkbox("Wireframe", &terrain->wireframe);
        ImGui::SliderFloat("Height", &terrain->heightScale, 1.0f, 1000.0f, "%.3f", 2.0f);

        ImGui::Separator();
        ImGui::Text("Tessellation");

        ImGui::SliderFloat("Factor", &terrain->tessellationFactor, 0.0f,
            16000.0f, "%.3f", 4.0f);

        ImGui::SliderFloat("Slope", &terrain->tessellationSlope, 0.0f, 10.0f);
        ImGui::SliderFloat("Shift", &terrain->tessellationShift, 0.0f, 10.0f);

        ImGui::SliderFloat("Subdivisions", &terrain->maxTessellationLevel, 1.0f,
            64.0f);
        ImGui::SliderFloat("Distance", &terrain->displacementDistance, 0.0f, 100.0f,
            "%.3f", ImGuiSliderFlags_Logarithmic);

        ImGui::Separator();
        ImGui::Text("Lod distances");

        for (size_t i = 0; i < terrain->LoDCount; i++) {
            auto distance = terrain->GetLoDDistance((int32_t)i);
            ImGui::SliderFloat(("LoD" + std::to_string(i)).c_str(),
                &distance, 1.0f, 8192.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            terrain->SetLoDDistance((int32_t)i, distance);
        }
        */

    }

}