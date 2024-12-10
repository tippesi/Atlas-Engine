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

        ImGui::DragInt("Seed", &vegetationGenerator.seed, 1.0f, 1, 255);

        RenderBiomeVegetationTypes(scene->terrain.Get(), terrainGenerator);

        if (ImGui::Button("Generate", ImVec2(width, 0.0f))) {

            Singletons::blockingOperation->Block("Generating vegetation. Please wait...", 
                [&, scene = scene]() mutable {

                Tools::TerrainTool::LoadMissingCells(scene->terrain.Get(), scene->terrain.GetResource()->path);
                vegetationGenerator.GenerateAll(scene, terrainGenerator);
                });

        }

        ImGui::PopID();

    }

    void VegetationGeneratorPanel::RenderBiomeVegetationTypes(Ref<Terrain::Terrain>& terrain, TerrainGenerator& terrainGenerator) {

        const ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_SpanAvailWidth;

        auto& types = vegetationGenerator.types;
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
            ImGui::DragFloat("Initial density", &type.initialDensity, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Offspring per iteration", &type.offspringPerIteration, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Offspring spread radius", &type.offspringSpreadRadius, 0.1f, 0.1f, 100.0f);

            ImGui::SeparatorText("Placement");
            ImGui::DragFloat("Min height", &type.heightMin, 0.001f, 0.0f, 1.0f);
            ImGui::DragFloat("Max height", &type.heightMax, 0.001f, 0.0f, 1.0f);

            ImGui::DragFloat("Min slope", &type.slopeMin, 0.001f, 0.0f, 1.0f);
            ImGui::DragFloat("Max slope", &type.slopeMax, 0.001f, 0.0f, 1.0f);

            ImGui::SeparatorText("Transform");
            ImGui::DragFloat3("Offset", glm::value_ptr(type.offset), 0.1f);
            ImGui::DragFloat3("Min scale", glm::value_ptr(type.scaleMin), 0.1f);
            ImGui::DragFloat3("Max scale", glm::value_ptr(type.scaleMax), 0.1f);

            ImGui::Checkbox("Align to surface", &type.alignToSurface);
            ImGui::DragFloat("Max alignment angle", &type.maxAlignmentAngle, 0.01f, 0.0f, 3.14f / 2.0f);

            ImGui::SeparatorText("Growth");
            ImGui::DragFloat("Collision radius", &type.collisionRadius, 0.01f, 0.0f);
            ImGui::DragFloat("Shade radius", &type.shadeRadius, 0.01f, 0.0f);

            ImGui::Checkbox("Can grow in shade", &type.canGrowInShade);
            ImGui::DragInt("Max growth age", &type.growthMaxAge);
            ImGui::DragFloat("Min growth scale", &type.growthMinScale, 0.01f);
            ImGui::DragFloat("Max growth scale", &type.growthMaxScale, 0.01f);

            // Create a child window with a scrollbar
            ImGui::SeparatorText("Exclude materials");
            ImGui::BeginChild("ChildWindow", ImVec2(0, 150), true, ImGuiWindowFlags_HorizontalScrollbar);

            for (int32_t i = 0; i < int32_t(terrain->storage->materials.size()); i++) {
                auto& material = terrain->storage->materials[i];
                if (!material.IsLoaded())
                    continue;

                auto fileName = material.GetResource()->GetFileName() + "##" + std::to_string(i);

                bool excluded = type.exludeMaterialIndices.contains(i);
                if (ImGui::RadioButton(fileName.c_str(), excluded)) {
                    if (excluded) {
                        type.exludeMaterialIndices.erase(i);
                    }
                    else {
                        type.exludeMaterialIndices.insert(i);
                    }
                }
            }

            ImGui::EndChild();

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

}