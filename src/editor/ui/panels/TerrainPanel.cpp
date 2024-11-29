#include "TerrainPanel.h"

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
            if (scene->terrain.IsLoaded() && editingMode) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Exit terrain editing mode to access the generator");
            }
            else {
                if (scene->terrain.IsLoaded()) {
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Regenerating might terrain overrides any manual changes.");
                }

                terrainGenerator.Render(scene->terrain);

                terrain = terrainGenerator.GetTerrain();
                if (terrain.IsValid()) {
                    terrainChanged = true;
                }
            }
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
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No terrain generated or selected.");
            return;
        }

        ImGui::Checkbox("Wireframe", &terrain->wireframe);
        ImGui::DragFloat3("Translation", glm::value_ptr(terrain->translation), 1.0f, -10000.0f, 10000.0f);
        ImGui::SliderFloat("Height", &terrain->heightScale, 1.0f, 2000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);

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

        if (editingMode)
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Lods can't be changed in editing mode");

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

        ImGui::Checkbox("Editing mode", &editingMode);

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

        ImGui::SameLine();

        UIElements::Tooltip("Pushes LoD distances to 512 to enable editing without baking");

        auto maxSize = 8.0f * terrain->patchSizeFactor - 1.0f;
        ImGui::DragFloat("Brush size", &brushSize, 0.25f, 1.0f, maxSize);

        auto brushStrengthMin = -2000.0f;
        auto brushStrengthMax = 2000.0f;

        if (brushFunction == TerrainBrushFunction::Smooth ||
            brushFunction == TerrainBrushFunction::Flatten) {
            brushStrengthMin = 0.0f;
            brushStrengthMax = 1.0f;
            brushStrength = glm::min(1.0f, brushStrength);
        }
        ImGui::SliderFloat("Brush strength", &brushStrength, brushStrengthMin,
            brushStrengthMax, "%.3f", ImGuiSliderFlags_Logarithmic);

        ImGui::Separator();
        ImGui::Text("Height Brush");
        ImGui::PushID("Height");

        bool heightBrushActive = brushType == TerrainBrushType::Height;
        ImGui::Checkbox("Active", &heightBrushActive);
        brushType = heightBrushActive ? TerrainBrushType::Height : brushType;

        const char* comboItems[] = { "Gauss", "Box", "Smooth", "Flatten" };
        int32_t heightBrushSelection = static_cast<int32_t>(brushFunction);
        ImGui::Combo("Function", &heightBrushSelection, comboItems, IM_ARRAYSIZE(comboItems));
        brushFunction = static_cast<TerrainBrushFunction>(heightBrushSelection);
        // Need some extra setting here
        if (brushFunction == TerrainBrushFunction::Flatten) {
            ImGui::Checkbox("Query regular geometry", &brushFlattenQueryRegularGeometry);
        }

        ImGui::Separator();

        ImGui::PopID();
        ImGui::Text("Material Brush");
        ImGui::PushID("Material");

        bool materialBrushActive = brushType == TerrainBrushType::Material;
        ImGui::Checkbox("Active", &materialBrushActive);
        brushType = materialBrushActive ? TerrainBrushType::Material : brushType;

        std::vector<std::string> counter;
        std::vector<const char*> pointer;

        for (size_t count = 0; count < terrain->storage->materials.size(); count++) {
            counter.push_back(std::to_string(count));
        }
        for (auto& name : counter) {
            pointer.push_back(name.c_str());
        }

        ImGui::Combo("Slot", &materialBrushSelection,
            pointer.data(), pointer.size());

        if (ImGui::Button("Bake terrain", ImVec2(region.x, 0.0f))) {
            Tools::TerrainTool::LoadMissingCells(terrain.Get(), terrain.GetResource()->path);
            Tools::TerrainTool::BakeTerrain(terrain.Get());
        }

        ImGui::PopID();
    }

    void TerrainPanel::AddTerrainToScene(ResourceHandle<Terrain::Terrain>& terrain, Ref<Scene::Scene>& scene) {

        scene->terrain = terrain;

        if (!terrain.IsValid())
            return;

        terrain.WaitForLoad();

        auto heightImage = scene->terrain->GetHeightField(terrain->LoDCount - 1);

        Atlas::Physics::HeightFieldShapeSettings terrainShapeSettings{
            .heightData = heightImage.GetData(),
            .translation = terrain->translation,
            .scale = glm::vec3(terrain->resolution, terrain->heightScale, terrain->resolution)
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
            
            auto root = scene->GetEntityByName("Root");
            if (root.IsValid() && root.HasComponent<HierarchyComponent>())
                root.GetComponent<HierarchyComponent>().AddChild(terrainPhysicsEntity);
        }
        else {
            terrainPhysicsEntity.RemoveComponent<RigidBodyComponent>();
            if (terrainPhysicsEntity.HasComponent<TransformComponent>())
                terrainPhysicsEntity.RemoveComponent<TransformComponent>();
        }

        auto bodySettings = Atlas::Physics::BodyCreationSettings{ .shape = terrainShape };
        terrainPhysicsEntity.AddComponent<RigidBodyComponent>(bodySettings);
        terrainPhysicsEntity.AddComponent<TransformComponent>(glm::mat4(1.0f));

    }

}