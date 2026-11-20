#include "TerrainPanel.h"
#include "../../Theme.h"
#include "../Widgets.h"

#include <tools/TerrainTool.h>
#include <ImguiExtension/UiElements.h>

namespace Atlas::Editor::UI {

    using namespace ImguiExtension;

    void TerrainPanel::Render(ResourceHandle<Terrain::Terrain> terrain, Ref<Scene::Scene> scene) {

        terrainSelectionPanel.Reset();
        materialSelectionPanel.Reset();
        textureSelectionPanel.Reset();

        bool terrainChanged = false;
        terrain = terrainSelectionPanel.Render(scene->terrain, terrainChanged);

        if (Widgets::SectionHeader("General")) {
            RenderGeneralSettings(terrain);
        }
        if (Widgets::SectionHeader("Materials")) {
            RenderMaterialSettings(terrain);
        }
        if (Widgets::SectionHeader("Editing")) {
            RenderEditingSettings(terrain, scene);
        }
        if (Widgets::SectionHeader("Generator")) {
            if (scene->terrain.IsLoaded() && editingMode) {
                Widgets::Callout("Exit terrain editing mode to access the generator", Theme::Warning());
            }
            else {
                if (scene->terrain.IsLoaded()) {
                    Widgets::Callout("Regenerating the terrain overrides any manual changes.", Theme::Warning());
                }

                terrainGenerator.Render(scene->terrain);

                terrain = terrainGenerator.GetTerrain();
                if (terrain.IsValid()) {
                    terrainChanged = true;
                }
            }
        }
        if(Widgets::SectionHeader("Vegetation")) {
            vegetationGeneratorPanel.Render(scene, terrainGenerator);
        }

        if (terrainChanged) {
            AddTerrainToScene(terrain, scene);
        }

        if (editingMode) {
            for (int32_t i = 0; i < terrain->LoDCount; i++) {
                if (terrain->GetLoDDistance(i) < 512.0f)
                    terrain->SetLoDDistance(i, 512.0f);
            }
        }

    }

    void TerrainPanel::RenderGeneralSettings(ResourceHandle<Terrain::Terrain>& terrain) {

        if (!terrain.IsLoaded()) {
            Widgets::Callout("No terrain generated or selected.", Theme::Warning());
            return;
        }

        if (Widgets::BeginProperties("General")) {
            Widgets::PropertyLabel("Wireframe");
            ImGui::Checkbox("##Wireframe", &terrain->wireframe);
            Widgets::PropertyLabel("Translation");
            Widgets::DragVec3("##Translation", terrain->translation, 1.0f, 0.0f, -10000.0f, 10000.0f);
            Widgets::PropertyLabel("Height");
            ImGui::SliderFloat("##Height", &terrain->heightScale, 1.0f, 2000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            Widgets::EndProperties();
        }

        ImGui::SeparatorText("Tessellation");

        if (Widgets::BeginProperties("Tessellation")) {
            Widgets::PropertyLabel("Factor");
            ImGui::SliderFloat("##Factor", &terrain->tessellationFactor, 0.0f,
                16000.0f, "%.3f", 4.0f);

            Widgets::PropertyLabel("Slope");
            ImGui::SliderFloat("##Slope", &terrain->tessellationSlope, 0.0f, 10.0f);
            Widgets::PropertyLabel("Shift");
            ImGui::SliderFloat("##Shift", &terrain->tessellationShift, 0.0f, 10.0f);

            Widgets::PropertyLabel("Subdivisions");
            ImGui::SliderFloat("##Subdivisions", &terrain->maxTessellationLevel, 1.0f,
                64.0f);
            Widgets::PropertyLabel("Distance");
            ImGui::SliderFloat("##Distance", &terrain->displacementDistance, 0.0f, 100.0f,
                "%.3f", ImGuiSliderFlags_Logarithmic);
            Widgets::EndProperties();
        }

        ImGui::SeparatorText("Lod distances");

        if (editingMode)
            Widgets::Callout("Lods can't be changed in editing mode", Theme::Warning());

        if (Widgets::BeginProperties("Lod distances")) {
            for (size_t i = 0; i < terrain->LoDCount; i++) {
                auto distance = terrain->GetLoDDistance((int32_t)i);
                auto label = "LoD" + std::to_string(i);
                Widgets::PropertyLabel(label.c_str());
                ImGui::SliderFloat(("##" + label).c_str(),
                    &distance, 1.0f, 8192.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
                terrain->SetLoDDistance((int32_t)i, distance);
            }
            Widgets::EndProperties();
        }

    }

    void TerrainPanel::RenderMaterialSettings(ResourceHandle<Terrain::Terrain>& terrain) {

        if (!terrain.IsLoaded()) {
            Widgets::Callout("No terrain generated or selected.", Theme::Warning());
            return;
        }

        materialsPanel.Render(Singletons::imguiWrapper, terrain->storage->materials,
            [&](ResourceHandle<Material> material) {
                bool materialChanged = false;
                return materialSelectionPanel.Render(material, materialChanged);
            },
            [&](ResourceHandle<Texture::Texture2D> texture) {
                return textureSelectionPanel.Render(texture);
            });

    }

    void TerrainPanel::RenderEditingSettings(ResourceHandle<Terrain::Terrain>& terrain, Ref<Scene::Scene> scene) {

        if (!terrain.IsLoaded()) {
            Widgets::Callout("No terrain generated or selected.", Theme::Warning());
            return;
        }

        auto region = ImGui::GetContentRegionAvail();

        if (Widgets::BeginProperties("Editing")) {
            Widgets::PropertyLabel("Editing mode", "Pushes LoD distances to 512 to enable editing without baking");
            ImGui::Checkbox("##Editing mode", &editingMode);
            Widgets::EndProperties();
        }

        if (editingMode && !LoDDistances.size()) {
            // Can't really reset other than when the scene is finally saved (together with the new terrain)
            terrain->storage->inEditing = true;
            for (int32_t i = 0; i < terrain->LoDCount; i++) {
                LoDDistances.push_back(terrain->GetLoDDistance(i));
            }
        }
        else if (!editingMode && LoDDistances.size()) {
            // If we are here it means we have exited editing mode
            for (int32_t i = 0; i < terrain->LoDCount; i++) {
                terrain->SetLoDDistance(i, LoDDistances[i]);
            }
        }

        if (!editingMode)
            Widgets::Callout("Enable editing mode, then hold the right mouse button in the viewport to paint.",
                Theme::Accent());

        // Only one brush can be active, show the settings of the selected one
        ImGui::SeparatorText("Brush");

        if (Widgets::BeginProperties("Brush")) {
            const char* brushTypeItems[] = { "Height", "Material", "Hole" };
            int32_t brushTypeSelection = static_cast<int32_t>(brushType);
            Widgets::PropertyLabel("Type");
            Widgets::SegmentedControl("##Type", brushTypeSelection, brushTypeItems, IM_ARRAYSIZE(brushTypeItems));
            brushType = static_cast<TerrainBrushType>(brushTypeSelection);

            auto maxSize = 8.0f * terrain->patchSizeFactor - 1.0f;
            Widgets::PropertyLabel("Brush size");
            ImGui::DragFloat("##Brush size", &brushSize, 0.25f, 1.0f, maxSize);

            if (brushType == TerrainBrushType::Height) {
                ImGui::PushID("Height");

                const char* functionItems[] = { "Gauss", "Box", "Smooth", "Flatten" };
                int32_t heightBrushSelection = static_cast<int32_t>(brushFunction);
                Widgets::PropertyLabel("Function");
                Widgets::SegmentedControl("##Function", heightBrushSelection, functionItems, IM_ARRAYSIZE(functionItems));
                brushFunction = static_cast<TerrainBrushFunction>(heightBrushSelection);

                auto brushStrengthMin = -2000.0f;
                auto brushStrengthMax = 2000.0f;

                if (brushFunction == TerrainBrushFunction::Smooth ||
                    brushFunction == TerrainBrushFunction::Flatten) {
                    brushStrengthMin = 0.0f;
                    brushStrengthMax = 1.0f;
                    brushStrength = glm::min(1.0f, brushStrength);
                }
                Widgets::PropertyLabel("Brush strength");
                ImGui::SliderFloat("##Brush strength", &brushStrength, brushStrengthMin,
                    brushStrengthMax, "%.3f", ImGuiSliderFlags_Logarithmic);

                // Need some extra setting here
                if (brushFunction == TerrainBrushFunction::Flatten) {
                    Widgets::PropertyLabel("Query regular geometry");
                    ImGui::Checkbox("##Query regular geometry", &brushFlattenQueryRegularGeometry);
                }

                ImGui::PopID();
            }
            else if (brushType == TerrainBrushType::Material) {
                ImGui::PushID("Material");

                std::vector<std::string> counter;
                std::vector<const char*> pointer;

                for (size_t count = 0; count < terrain->storage->materials.size(); count++) {
                    auto& material = terrain->storage->materials[count];
                    auto name = material.IsLoaded() ? material->name : std::string("Empty");
                    counter.push_back(std::to_string(count) + ": " + name);
                }
                for (const auto& name : counter) {
                    pointer.push_back(name.c_str());
                }

                Widgets::PropertyLabel("Slot");
                ImGui::Combo("##Slot", &materialBrushSelection,
                    pointer.data(), pointer.size());

                ImGui::PopID();
            }

            Widgets::EndProperties();
        }

        if (brushType == TerrainBrushType::Hole) {
            ImGui::TextDisabled("Paints holes into the terrain");
        }

        ImGui::SeparatorText("Actions");
        ImGui::PushID("Actions");

        if (ImGui::Button("Bake terrain", ImVec2(region.x, 0.0f))) {
            Tools::TerrainTool::LoadMissingCells(terrain.Get(), terrain.GetResource()->path);
            Tools::TerrainTool::BakeTerrain(terrain.Get());
        }
        ImGui::SetItemTooltip("Bakes the edited cells, such that the terrain can be rendered with the regular LoD distances");

        if (ImGui::Button("Recalculate physics shape", ImVec2(region.x, 0.0f))) {
            Tools::TerrainTool::LoadMissingCells(terrain.Get(), terrain.GetResource()->path);
            AddTerrainToScene(terrain, scene);
        }

        ImGui::PopID();
    }

    void TerrainPanel::AddTerrainToScene(ResourceHandle<Terrain::Terrain>& terrain, Ref<Scene::Scene>& scene) {

        scene->terrain = terrain;

        if (!terrain.IsValid())
            return;

        terrain.WaitForLoad();

        RebuildTerrainBody(scene);

    }

    void TerrainPanel::RebuildTerrainBody(Ref<Scene::Scene>& scene) {

        auto& terrain = scene->terrain;
        auto heightImage = terrain->GetHeightField(terrain->LoDCount - 1);

        Atlas::Physics::HeightFieldShapeSettings terrainShapeSettings{
            .heightData = heightImage.GetData(),
            .translation = terrain->translation,
            .scale = vec3(terrain->resolution, terrain->heightScale, terrain->resolution)
        };
        auto terrainShape = Atlas::Physics::ShapesManager::CreateShape(terrainShapeSettings);

        Scene::Entity terrainPhysicsEntity;
        auto rigidBodySubset = scene->GetSubset<RigidBodyComponent>();
        for (auto entity : rigidBodySubset) {
            auto& rigidBodyComponent = rigidBodySubset.Get(entity);
            if (!rigidBodyComponent.IsValid())
                continue;

            auto shape = rigidBodyComponent.GetShape();

            if (shape->type == Physics::ShapeType::HeightField) {
                terrainPhysicsEntity = entity; 
                break;
            }
        }

        if (!terrainPhysicsEntity.IsValid()) {
            terrainPhysicsEntity = scene->CreateEntity();
            terrainPhysicsEntity.AddComponent<NameComponent>(terrain->filename);
            
            terrainPhysicsEntity.SetParent(scene->GetRootEntity());
        }
        else {
            terrainPhysicsEntity.RemoveComponent<RigidBodyComponent>();
            if (terrainPhysicsEntity.HasComponent<TransformComponent>())
                terrainPhysicsEntity.RemoveComponent<TransformComponent>();
        }

        auto bodySettings = Atlas::Physics::BodyCreationSettings{ .shape = terrainShape };
        terrainPhysicsEntity.AddComponent<RigidBodyComponent>(bodySettings);
        terrainPhysicsEntity.AddComponent<TransformComponent>(mat4(1.0f));

    }

}
