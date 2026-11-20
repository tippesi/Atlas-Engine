#include "SceneStatisticsPanel.h"
#include "profiler/CPUProfiler.h"

namespace Atlas::Editor::UI {

    void SceneStatisticsPanel::Render(Ref<Scene::Scene> scene) {

        Collect(scene);
        GrowBounds(scene);

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
            row("Min", vecToString(bounds.min));
            row("Max", vecToString(bounds.max));
            row("Size", vecToString(bounds.GetSize()));
            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::SeparatorText("Content");
        if (ImGui::BeginTable("Content table", 2, tableFlags)) {
            row("Entities", std::to_string(entityCount));
            row("Meshes", std::to_string(meshCount));
            row("Materials", std::to_string(materialCount));
            row("Lights", std::to_string(lightCount));
            row("Mesh instances", std::to_string(meshInstanceCount));
            ImGui::EndTable();
        }

        if (!physicsStatistics)
            return;

        auto activeToString = [](uint32_t active, uint32_t count) {
            return std::to_string(active) + " / " + std::to_string(count);
            };

        char updateTime[32];
        std::snprintf(updateTime, sizeof(updateTime), "%.2f ms", physicsStatistics->updateTime);

        ImGui::Spacing();
        ImGui::SeparatorText("Physics");
        if (ImGui::BeginTable("Physics table", 2, tableFlags)) {
            row("Bodies", std::to_string(physicsStatistics->bodyCount));
            row("Static bodies", std::to_string(physicsStatistics->staticBodyCount));
            row("Active dynamic bodies", activeToString(physicsStatistics->activeDynamicBodyCount, physicsStatistics->dynamicBodyCount));
            row("Active kinematic bodies", activeToString(physicsStatistics->activeKinematicBodyCount, physicsStatistics->kinematicBodyCount));
            row("Constraints", std::to_string(physicsStatistics->constraintCount));
            row("Contacts", std::to_string(physicsStatistics->contactCount));
            row("Update time", updateTime);
            ImGui::EndTable();
        }

    }

    void SceneStatisticsPanel::Collect(const Ref<Scene::Scene>& scene) {

        CPUProfiler::Scope profilerScope("Collect scene statistics");

        // A new scene starts from scratch
        if (collectedScene.lock() != scene) {
            collectedScene = scene;
            bounds = Volume::AABB(vec3(0.0f), vec3(0.0f));
            boundsIndex = 0;
            collectionStep = 0;
        }

        switch (collectionStep) {
        case 0:
            entityCount = scene->GetEntityCount();
            lightCount = scene->GetComponentCount<LightComponent>();
            meshInstanceCount = scene->GetComponentCount<MeshComponent>();
            break;
        case 1: meshCount = scene->GetMeshes().size(); break;
        case 2: materialCount = scene->GetMaterials().size(); break;
        default:
            physicsStatistics.reset();
            if (scene->physicsWorld)
                physicsStatistics = scene->physicsWorld->GetStatistics();
            break;
        }
        collectionStep = (collectionStep + 1) % 4;

    }

    void SceneStatisticsPanel::GrowBounds(const Ref<Scene::Scene>& scene) {

        // The pool is dense, iterating it directly avoids a lookup per entity. Components that are added or removed
        // during a pass only show up in the next one
        auto& meshPool = scene->entityManager.GetPool<MeshComponent>();
        if (!boundsIndex)
            growingBounds = Volume::AABB(vec3(std::numeric_limits<float>::max()), vec3(-std::numeric_limits<float>::max()));

        auto end = std::min(boundsIndex + boundsComponentsPerFrame, meshPool.GetCount());
        for (; boundsIndex < end; boundsIndex++)
            growingBounds.Grow(meshPool.GetByIndex(boundsIndex).aabb);

        if (boundsIndex >= meshPool.GetCount()) {
            bounds = boundsIndex ? growingBounds : Volume::AABB(vec3(0.0f), vec3(0.0f));
            boundsIndex = 0;
        }

    }

}