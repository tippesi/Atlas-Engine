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

        ImGui::SeparatorText("Dimensions");
        ImGui::Text("Min: %.2f, %.2f, %.2f", min.x, min.y, min.z);
        ImGui::Text("Max: %.2f, %.2f, %.2f", max.x, max.y, max.z);
        ImGui::Text("Size: %.2f, %.2f, %.2f", size.x, size.y, size.z);

        ImGui::SeparatorText("Content");
        ImGui::Text("Entity count: %d", int32_t(scene->GetEntityCount()));
        ImGui::Text("Mesh count: %d", int32_t(meshes.size()));
        ImGui::Text("Material count: %d", int32_t(materials.size()));

    }

}