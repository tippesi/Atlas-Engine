#include "VegetationGeneratorPanel.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <ImguiExtension/UiElements.h>
#include <tools/TerrainTool.h>
#include <common/NoiseGenerator.h>
#include <Notifications.h>
#include <Singletons.h>
#include <algorithm>

namespace Atlas::Editor::UI {

    using namespace ImguiExtension;

    void VegetationGeneratorPanel::Render(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator) {

        if (!scene->terrain.IsLoaded()) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Terrain must be loaded or generated first");
            return;
        }

        ImGui::PushID(GetNameID());

        meshSelectionPanel.Reset();

        auto width = ImGui::GetContentRegionAvail().x;

        int32_t eleBiomeCount = 0;
        for (auto& eleBiome : terrainGenerator.elevationBiomes) {
            ImGui::PushID(eleBiomeCount);

            int32_t moiBiomeCount = 0;
            int32_t sloBiomeCount = 0;

            auto eleRegion = ImGui::GetContentRegionAvail();

            bool eleOpen = ImGui::TreeNode(("Elevation biome " + std::to_string(eleBiomeCount++)).c_str());
            if (eleOpen) {
                RenderBiomeVegetationTypes(GetVegetationTypes(eleBiome.id));
                ImGui::Separator();
                if (ImGui::TreeNode("Moisture biomes")) {
                    for (auto& moiBiome : eleBiome.moistureBiomes) {
                        if (ImGui::TreeNode(("Moisture biome " + std::to_string(moiBiomeCount++)).c_str())) {
                            RenderBiomeVegetationTypes(GetVegetationTypes(moiBiome.id));
                        }
                    }
                    ImGui::TreePop();
                }
                ImGui::Separator();
                if (ImGui::TreeNode("Slope biomes")) {
                    for (auto& sloBiome : eleBiome.slopeBiomes) {
                        if (ImGui::TreeNode(("Slope biome " + std::to_string(sloBiomeCount++)).c_str())) {
                            RenderBiomeVegetationTypes(GetVegetationTypes(sloBiome.id));
                        }
                    }
                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }


        if (ImGui::Button("Generate", ImVec2(width, 0.0f))) {

            Singletons::blockingOperation->Block("Generating vegetation. Please wait...", 
                [&, scene = scene]() mutable {

                Tools::TerrainTool::LoadMissingCells(scene->terrain.Get(), scene->terrain.GetResource()->path);
                vegetationGenerator.types.clear();

                for (auto& [id, biomeVegTypes] : biomeToVegetationType) {
                    for (auto& type : biomeVegTypes) {
                        type.biomeId = id;
                        vegetationGenerator.types.push_back(type);
                    }
                } 

                vegetationGenerator.GenerateAll(scene, terrainGenerator);
                
                // Copy resulting entities into our biome map
                for (auto& type : vegetationGenerator.types) {

                    auto& biomeVegTypes = biomeToVegetationType[type.biomeId];
                    for (auto& biomeVegType : biomeVegTypes) {
                        if (biomeVegType.biomeId == type.biomeId) {
                            biomeVegType.entities = std::move(type.entities);
                            biomeVegType.parentEntity = type.parentEntity;
                        }
                    }

                }
                });

        }

        ImGui::PopID();

    }

    void VegetationGeneratorPanel::RenderBiomeVegetationTypes(std::vector<VegetationGenerator::VegetationType>& types) {

        const ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_SpanAvailWidth;

        for (auto& type : types) {
            bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(type.id), nodeFlags, "%s", type.name.c_str());

            // Here we can use the copy pase helper to copy over some settings to other biomes

            if (!open)
                continue;

            ImGui::InputText("Name", &type.name);

            ImGui::SeparatorText("Mesh");

            bool meshChanged = false;
            type.mesh = meshSelectionPanel.Render(type.mesh, meshChanged);

            type.iterations = std::max(0, type.iterations);
            type.growthMaxAge = std::max(0, type.growthMaxAge);
            type.initialDensity = std::max(0.001f, type.initialDensity);

            ImGui::SeparatorText("General");

            ImGui::DragInt("Iterations", &type.iterations);
            ImGui::DragInt("Seed", &type.seed);
            ImGui::DragFloat("Initial density", &type.initialDensity, 0.01f, 0.0f);
            ImGui::DragFloat("Offspring per iteration", &type.offspringPerIteration, 0.01f, 0.0f);
            ImGui::DragFloat("Offspring spread radius", &type.offspringSpreadRadius, 0.1f, 0.1f, 100.0f);

            ImGui::Checkbox("Align to surface", &type.alignToSurface);

            ImGui::SeparatorText("Transform");
            ImGui::DragFloat3("Offset", glm::value_ptr(type.offset), 0.1f);
            ImGui::DragFloat3("Min scale", glm::value_ptr(type.scaleMin), 0.1f);
            ImGui::DragFloat3("Max scale", glm::value_ptr(type.scaleMax), 0.1f);

            ImGui::SeparatorText("Growth");
            ImGui::DragFloat("Collision radius", &type.collisionRadius, 0.01f, 0.0f);
            ImGui::DragFloat("Shade radius", &type.shadeRadius, 0.01f, 0.0f);

            ImGui::DragInt("Max growth age", &type.growthMaxAge);
            ImGui::DragFloat("Min growth scale", &type.growthMinScale, 0.01f);
            ImGui::DragFloat("Max growth scale", &type.growthMaxScale, 0.01f);

            if (open)
                ImGui::TreePop();
        }

        if (ImGui::Button("Add vegetation type", ImVec2(-FLT_MIN, 0.0f))) {
            types.push_back(VegetationGenerator::VegetationType{
                .id = vegetationGenerator.typeCounter,
                .name = "Vegetation type " + std::to_string(vegetationGenerator.typeCounter++)
                });
        }

    }

    std::vector<VegetationGenerator::VegetationType>& VegetationGeneratorPanel::GetVegetationTypes(size_t id) {

        if (!biomeToVegetationType.contains(id)) {
            biomeToVegetationType[id] = {};
        }

        return biomeToVegetationType[id];

    }

}