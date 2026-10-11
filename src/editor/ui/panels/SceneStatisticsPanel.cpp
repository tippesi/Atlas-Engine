#include "SceneStatisticsPanel.h"

namespace Atlas::Editor::UI {

    void SceneStatisticsPanel::Render(Ref<Scene::Scene> scene) {

        vec3 min = glm::vec3(std::numeric_limits<float>::max());
        vec3 max = glm::vec3(-std::numeric_limits<float>::max());

        Volume::AABB aabb(min, max);
        auto meshSubset = scene->GetSubset<MeshComponent>();
        for (auto entity : meshSubset) {
            const auto& meshComponent = meshSubset.Get(entity);

            aabb.Grow(meshComponent.aabb);
        }

        min = aabb.min;
        max = aabb.max;
        auto size = aabb.GetSize();

        auto meshes = scene->GetMeshes();
        auto materials = scene->GetMaterials();

        auto lightCount = scene->GetComponentCount<LightComponent>();
        auto meshInstanceCount = scene->GetComponentCount<MeshComponent>();

        auto tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;

        auto row = [](const char* name, const std::string& value) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", name);
            ImGui::TableNextColumn();
            // Right align the values
            auto width = ImGui::CalcTextSize(value.c_str()).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - width);
            ImGui::TextUnformatted(value.c_str());
            };

        auto vecToString = [](vec3 v) {
            char buffer[96];
            std::snprintf(buffer, sizeof(buffer), "%.2f, %.2f, %.2f", v.x, v.y, v.z);
            return std::string(buffer);
            };

        ImGui::SeparatorText("Dimensions");
        if (ImGui::BeginTable("Dimensions table", 2, tableFlags)) {
            row("Min", vecToString(min));
            row("Max", vecToString(max));
            row("Size", vecToString(size));
            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::SeparatorText("Content");
        if (ImGui::BeginTable("Content table", 2, tableFlags)) {
            row("Entities", std::to_string(scene->GetEntityCount()));
            row("Meshes", std::to_string(meshes.size()));
            row("Materials", std::to_string(materials.size()));
            row("Lights", std::to_string(lightCount));
            row("Mesh instances", std::to_string(meshInstanceCount));
            row("Physics bodies", std::to_string(scene->physicsWorld->GetBodyCount()));
            ImGui::EndTable();
        }

    }

}