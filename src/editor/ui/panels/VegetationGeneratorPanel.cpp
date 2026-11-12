#include "VegetationGeneratorPanel.h"
#include "../../Theme.h"
#include "../Widgets.h"

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
            Widgets::Callout("Terrain must be loaded or generated first", Theme::Warning());
            return;
        }

        ImGui::PushID(GetNameID());

        meshSelectionPanel.Reset();

        auto width = ImGui::GetContentRegionAvail().x;

        if (Widgets::BeginProperties("Vegetation generator")) {
            Widgets::PropertyLabel("Seed");
            ImGui::DragInt("##Seed", &vegetationGenerator.seed, 1.0f, 1, 255);
            Widgets::EndProperties();
        }

        RenderBiomeVegetationTypes(scene, scene->terrain.Get(), terrainGenerator);

        if (ImGui::Button("Generate", ImVec2(width, 0.0f))) {

            Singletons::blockingOperation->Block("Generating vegetation. Please wait...", 
                [&, scene = scene]() mutable {

                Tools::TerrainTool::LoadMissingCells(scene->terrain.Get(), scene->terrain.GetResource()->path);
                vegetationGenerator.GenerateAll(scene, terrainGenerator);
                });

        }

        ImGui::PopID();

    }

    void VegetationGeneratorPanel::RenderBiomeVegetationTypes(Ref<Scene::Scene>& scene,
        Ref<Terrain::Terrain>& terrain, TerrainGenerator& terrainGenerator) {
        
        if (Singletons::blockingOperation->block)
            return;

        int32_t deleteIdx = -1;
        auto& types = vegetationGenerator.types;
        for (int32_t i = 0; i < int32_t(types.size()); i++) {
            auto& type = types[i];

            // The name can be edited below, the id keeps the header open while typing
            ImGui::PushID(int32_t(type.id));
            bool remove = false;
            bool open = Widgets::RemovableSectionHeader((type.name + "###Vegetation type").c_str(), 0,
                remove, "Remove vegetation type");
            if (remove)
                deleteIdx = i;

            if (!open) {
                ImGui::PopID();
                continue;
            }

            if (Widgets::BeginProperties("Vegetation type")) {
                Widgets::PropertyLabel("Name");
                ImGui::InputText("##Name", &type.name);
                Widgets::EndProperties();
            }

            ImGui::SeparatorText("Entity");

            std::string buttonName = "Drop entity with transform component here";

            Scene::Entity entity = Scene::Entity(type.entity, &scene->entityManager);
            if (entity.IsValid() && entity.HasComponent<NameComponent>())
                buttonName = entity.GetComponent<NameComponent>().name;
            ImGui::Button(buttonName.c_str(), { -FLT_MIN, 0 });

            Scene::Entity dropEntity;
            if (ImGui::BeginDragDropTarget()) {
                auto dropPayload = ImGui::GetDragDropPayload();
                if (dropPayload->IsDataType(typeid(Scene::Entity).name())) {
                    std::memcpy(&dropEntity, dropPayload->Data, dropPayload->DataSize);
                    bool validEntity = dropEntity.HasComponent<TransformComponent>();

                    if (validEntity && ImGui::AcceptDragDropPayload(typeid(Scene::Entity).name())) {
                        entity = dropEntity;
                    }
                }

                ImGui::EndDragDropTarget();
            }

            type.entity = entity;

            type.iterations = std::max(0, type.iterations);
            type.growthMaxAge = std::max(0, type.growthMaxAge);
            type.initialDensity = std::max(0.001f, type.initialDensity);

            ImGui::SeparatorText("General");
            if (Widgets::BeginProperties("General")) {
                Widgets::PropertyLabel("Iterations");
                ImGui::DragInt("##Iterations", &type.iterations);
                Widgets::PropertyLabel("Seed");
                ImGui::DragInt("##Seed", &type.seed);
                Widgets::PropertyLabel("Initial density");
                ImGui::DragFloat("##Initial density", &type.initialDensity, 0.01f, 0.0f, 1.0f);
                Widgets::PropertyLabel("Offspring per iteration");
                ImGui::DragFloat("##Offspring per iteration", &type.offspringPerIteration, 0.01f, 0.0f, 1.0f);
                Widgets::PropertyLabel("Offspring spread radius");
                ImGui::DragFloat("##Offspring spread radius", &type.offspringSpreadRadius, 0.1f, 0.1f, 100.0f);
                Widgets::EndProperties();
            }

            ImGui::SeparatorText("Placement");
            if (Widgets::BeginProperties("Placement")) {
                Widgets::PropertyLabel("Min height");
                ImGui::DragFloat("##Min height", &type.heightMin, 0.001f, 0.0f, 1.0f);
                Widgets::PropertyLabel("Max height");
                ImGui::DragFloat("##Max height", &type.heightMax, 0.001f, 0.0f, 1.0f);

                Widgets::PropertyLabel("Min slope");
                ImGui::DragFloat("##Min slope", &type.slopeMin, 0.001f, 0.0f, 1.0f);
                Widgets::PropertyLabel("Max slope");
                ImGui::DragFloat("##Max slope", &type.slopeMax, 0.001f, 0.0f, 1.0f);
                Widgets::EndProperties();
            }

            ImGui::SeparatorText("Transform");
            if (Widgets::BeginProperties("Transform")) {
                Widgets::PropertyLabel("Offset");
                Widgets::DragVec3("##Offset", type.offset, 0.1f, 0.0f);
                Widgets::PropertyLabel("Min scale");
                Widgets::DragVec3("##Min scale", type.scaleMin, 0.1f, 1.0f);
                Widgets::PropertyLabel("Max scale");
                Widgets::DragVec3("##Max scale", type.scaleMax, 0.1f, 1.0f);

                Widgets::PropertyLabel("Align to surface");
                ImGui::Checkbox("##Align to surface", &type.alignToSurface);
                Widgets::PropertyLabel("Max alignment angle");
                ImGui::DragFloat("##Max alignment angle", &type.maxAlignmentAngle, 0.01f, 0.0f, 3.14f / 2.0f);
                Widgets::EndProperties();
            }

            ImGui::SeparatorText("Growth");
            if (Widgets::BeginProperties("Growth")) {
                Widgets::PropertyLabel("Collision radius");
                ImGui::DragFloat("##Collision radius", &type.collisionRadius, 0.01f, 0.0f);
                Widgets::PropertyLabel("Shade radius");
                ImGui::DragFloat("##Shade radius", &type.shadeRadius, 0.01f, 0.0f);

                Widgets::PropertyLabel("Can grow in shade");
                ImGui::Checkbox("##Can grow in shade", &type.canGrowInShade);
                Widgets::PropertyLabel("Max growth age");
                ImGui::DragInt("##Max growth age", &type.growthMaxAge);
                Widgets::PropertyLabel("Min growth scale");
                ImGui::DragFloat("##Min growth scale", &type.growthMinScale, 0.01f);
                Widgets::PropertyLabel("Max growth scale");
                ImGui::DragFloat("##Max growth scale", &type.growthMaxScale, 0.01f);
                Widgets::EndProperties();
            }

            // Create a child window with a scrollbar
            ImGui::SeparatorText("Exclude materials");
            ImGui::BeginChild("ChildWindow", ImVec2(0, 150), true, ImGuiWindowFlags_HorizontalScrollbar);

            for (int32_t j = 0; j < int32_t(terrain->storage->materials.size()); j++) {
                auto& material = terrain->storage->materials[j];
                if (!material.IsLoaded())
                    continue;

                auto fileName = material.GetResource()->GetFileName() + "##" + std::to_string(j);

                bool excluded = type.exludeMaterialIndices.contains(j);
                if (ImGui::RadioButton(fileName.c_str(), excluded)) {
                    if (excluded) {
                        type.exludeMaterialIndices.erase(j);
                    }
                    else {
                        type.exludeMaterialIndices.insert(j);
                    }
                }
            }

            ImGui::EndChild();

            ImGui::PopID();
        }

        if (deleteIdx >= 0) {
            types.erase(types.begin() + size_t(deleteIdx));
        }

        if (ImGui::Button("Add vegetation type", ImVec2(-FLT_MIN, 0.0f))) {
            types.push_back(VegetationGenerator::VegetationType{
                .id = vegetationGenerator.typeCounter,
                .name = "Vegetation type " + std::to_string(vegetationGenerator.typeCounter++)
                });
        }

    }

}
