#include "SoftBodyComponentPanel.h"
#include "../../../Theme.h"
#include "../../Widgets.h"
#include "../../windows/ContentBrowserWindow.h"

#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

namespace Atlas::Editor::UI {

    bool SoftBodyComponentPanel::Render(const Ref<Scene::Scene>& scene,
        Scene::Entity entity, SoftBodyComponent& softBodyComponent) {

        if (!entity.HasComponent<MeshComponent>() || !entity.HasComponent<TransformComponent>())
            Widgets::Callout("Soft body component needs a mesh and a transform component", Theme::Warning());

        auto& settings = softBodyComponent.settings;
        bool changed = false;

        if (Widgets::BeginProperties("Soft body")) {
            Widgets::PropertyLabel("Simulated vertices", "The resolution of the simulation mesh is set per mesh in the mesh editor");
            ImGui::Text("%zu", softBodyComponent.GetSimulatedVertexCount());

            Widgets::PropertyLabel("Mass", "Spread over all vertices which aren't pinned");
            changed |= ImGui::DragFloat("##Mass", &settings.mass, 0.05f, 0.001f, 10000.0f);

            Widgets::PropertyLabel("Edge compliance", "Inverse stiffness of the edges, 0 is rigid");
            changed |= ImGui::DragFloat("##Edge compliance", &settings.edgeCompliance, 0.0001f, 0.0f, 1.0f, "%.5f");
            Widgets::PropertyLabel("Shear compliance");
            changed |= ImGui::DragFloat("##Shear compliance", &settings.shearCompliance, 0.0001f, 0.0f, 1.0f, "%.5f");
            Widgets::PropertyLabel("Bend compliance");
            changed |= ImGui::DragFloat("##Bend compliance", &settings.bendCompliance, 0.0001f, 0.0f, 1.0f, "%.5f");

            const char* bendTypes[] = { "None", "Distance", "Dihedral" };
            auto bendType = int32_t(settings.bendType);
            Widgets::PropertyLabel("Bend type", "Dihedral is the most expensive, but also bends non-planar triangles");
            if (ImGui::Combo("##Bend type", &bendType, bendTypes, IM_ARRAYSIZE(bendTypes))) {
                settings.bendType = Physics::SoftBodyBendType(bendType);
                changed = true;
            }

            Widgets::PropertyLabel("Pressure", "Inflates closed meshes");
            changed |= ImGui::DragFloat("##Pressure", &settings.pressure, 1.0f, 0.0f, 100000.0f);
            Widgets::PropertyLabel("Iterations");
            changed |= ImGui::DragInt("##Iterations", &settings.iterations, 0.1f, 1, 64);
            Widgets::PropertyLabel("Linear damping");
            changed |= ImGui::DragFloat("##Linear damping", &settings.linearDamping, 0.01f, 0.0f, 10.0f);
            Widgets::PropertyLabel("Friction");
            changed |= ImGui::DragFloat("##Friction", &settings.friction, 0.01f, 0.0f, 1.0f);
            Widgets::PropertyLabel("Restitution");
            changed |= ImGui::DragFloat("##Restitution", &settings.restitution, 0.01f, 0.0f, 1.0f);
            Widgets::PropertyLabel("Gravity factor");
            changed |= ImGui::DragFloat("##Gravity factor", &settings.gravityFactor, 0.01f, -10.0f, 10.0f);
            Widgets::PropertyLabel("Vertex radius", "Keeps the vertices a little away from other bodies");
            changed |= ImGui::DragFloat("##Vertex radius", &settings.vertexRadius, 0.001f, 0.0f, 1.0f);
            Widgets::PropertyLabel("Double sided");
            changed |= ImGui::Checkbox("##Double sided", &settings.doubleSided);
            Widgets::PropertyLabel("Allow sleeping");
            changed |= ImGui::Checkbox("##Allow sleeping", &settings.allowSleeping);
            Widgets::PropertyLabel("Wind factor", "Share of the difference to the scene wind velocity taken on per second");
            ImGui::DragFloat("##Wind factor", &settings.windFactor, 0.01f, 0.0f, 10.0f);
            Widgets::PropertyLabel("Simulation distance", "Simulated within this distance of the camera only, 0 always");
            ImGui::DragFloat("##Simulation distance", &settings.simulationDistance, 1.0f, 0.0f, 10000.0f);

            auto layerName = settings.objectLayer == Physics::Layers::Cloth ? "Cloth" : "Movable";
            Widgets::PropertyLabel("Object layer", "Cloth hangs on the static world, kinematic bodies and cloth colliders, "
                "it passes simulated bodies and other cloth");
            if (ImGui::BeginCombo("##Object layer", layerName)) {
                for (auto layer : { Physics::Layers::Movable, Physics::Layers::Cloth }) {
                    auto isSelected = layer == settings.objectLayer;
                    if (ImGui::Selectable(layer == Physics::Layers::Cloth ? "Cloth" : "Movable", isSelected)) {
                        settings.objectLayer = layer;
                        changed = true;
                    }
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            Widgets::PropertyLabel("Pinned", "Vertices within the pin bounds (mesh space) follow the entity");
            changed |= ImGui::Checkbox("##Pinned", &settings.pinned);

            Widgets::EndProperties();
        }

        if (ECS::Entity(entity) != selectedPinEntity) {
            selectedPin = -1;
            selectedPinEntity = entity;
        }

        if (settings.pinned) {
            Widgets::PropertySection("Pins");
            auto meshComponent = entity.TryGetComponent<MeshComponent>();
            auto simulationMesh = meshComponent && meshComponent->mesh.IsLoaded() ?
                meshComponent->mesh->GetSoftBodySimulationMesh() : nullptr;
            auto& pins = settings.pinBounds;
            for (int32_t i = 0; i < int32_t(pins.size()); i++) {
                ImGui::PushID(i);
                auto vertexCount = simulationMesh ? simulationMesh->GetVertexCount(pins[i]) : size_t(0);
                auto label = "Pin bounds " + std::to_string(i) + " (" + std::to_string(vertexCount) + " vertices)";
                if (ImGui::Selectable(label.c_str(), selectedPin == i))
                    selectedPin = selectedPin == i ? -1 : i;
                ImGui::SetItemTooltip("The gizmo in the viewport moves and resizes the selected bounds");
                if (simulationMesh && vertexCount == 0)
                    ImGui::TextColored(Theme::Warning(), "No simulated vertex inside, the closest one is pinned");
                if (selectedPin == i && Widgets::BeginProperties("Pin bounds")) {
                    Widgets::PropertyLabel("Min");
                    changed |= Widgets::DragVec3("##Min", pins[i].min, 0.005f);
                    Widgets::PropertyLabel("Max");
                    changed |= Widgets::DragVec3("##Max", pins[i].max, 0.005f);
                    Widgets::EndProperties();
                }
                ImGui::PopID();
            }

            // New bounds cover the top of the mesh, where banners and curtains hang
            if (ImGui::Button("Add pin bounds")) {
                auto aabb = meshComponent && meshComponent->mesh.IsLoaded() ? meshComponent->mesh->data.aabb :
                    Volume::AABB(vec3(-0.5f), vec3(0.5f));
                auto margin = 0.01f * (aabb.max - aabb.min) + vec3(0.001f);
                pins.emplace_back(vec3(aabb.min.x, aabb.max.y - 0.05f * (aabb.max.y - aabb.min.y), aabb.min.z) - margin,
                    aabb.max + margin);
                selectedPin = int32_t(pins.size()) - 1;
                changed = true;
            }
            if (selectedPin >= 0 && selectedPin < int32_t(pins.size())) {
                ImGui::SameLine();
                if (Widgets::DangerButton("Remove pin bounds")) {
                    pins.erase(pins.begin() + selectedPin);
                    selectedPin = -1;
                    changed = true;
                }
            }
        }

        ImGui::Checkbox("Show simulation mesh", &showSimulationMesh);
        ImGui::SetItemTooltip("The simulated vertices and edges, pinned vertices and the pin bounds in the viewport");

        // The body is built again from the rest pose with the new settings, once a dragged value is released
        pendingReset |= changed;
        auto reset = ImGui::Button("Reset simulation");
        auto meshComponent = entity.TryGetComponent<MeshComponent>();
        if (meshComponent && meshComponent->mesh.IsValid()) {
            ImGui::SameLine();
            if (ImGui::Button("Edit simulation mesh"))
                ContentBrowserWindow::contentToEditPath = meshComponent->mesh.GetResource()->path;
            ImGui::SetItemTooltip("Opens the mesh editor, the resolution of the simulation mesh is set per mesh");
        }
        if (reset || (pendingReset && !ImGui::IsAnyItemActive() && !ImGuizmo::IsUsing())) {
            softBodyComponent.Reset();
            pendingReset = false;
        }

        return false;

    }

    void SoftBodyComponentPanel::RenderOverlay(const Ref<Scene::Scene>& scene, Scene::Entity entity,
        PrimitiveBatchWrapper& wrapper) {

        auto softBodyComponent = entity.TryGetComponent<SoftBodyComponent>();
        auto pinSelected = softBodyComponent && ECS::Entity(entity) == selectedPinEntity && selectedPin >= 0;
        auto meshComponent = entity.TryGetComponent<MeshComponent>();
        if ((!showSimulationMesh && !pinSelected) || !meshComponent || !meshComponent->mesh.IsLoaded())
            return;

        const auto& mesh = meshComponent->mesh;
        auto simulationMesh = mesh->GetSoftBodySimulationMesh();
        if (simulationMesh != drawnSimulationMesh) {
            simulationMeshEdges.clear();
            const auto& indices = simulationMesh->indices;
            for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                for (size_t j = 0; j < 3; j++) {
                    auto a = indices[i + j], b = indices[i + (j + 1) % 3];
                    simulationMeshEdges.emplace_back(std::min(a, b), std::max(a, b));
                }
            }
            std::ranges::sort(simulationMeshEdges);
            auto duplicates = std::ranges::unique(simulationMeshEdges);
            simulationMeshEdges.erase(duplicates.begin(), duplicates.end());
            drawnSimulationMesh = simulationMesh;
        }

        // A simulating body shows its current vertices
        const auto* positions = &simulationMesh->positions;
        if (softBodyComponent && softBodyComponent->IsValid() && !softBodyComponent->IsOutdated(*meshComponent) &&
            softBodyComponent->GetSimulatedPositions().size() == positions->size())
            positions = &softBodyComponent->GetSimulatedPositions();

        auto globalMatrix = scene->GetGlobalMatrix(entity);
        auto toWorld = [&](vec3 position) { return vec3(globalMatrix * vec4(position, 1.0f)); };
        auto& batch = wrapper.GetBatch(false);

        const auto edgeColor = vec3(0.2f, 0.8f, 1.0f);
        for (const auto& [a, b] : simulationMeshEdges)
            batch->AddLine(toWorld((*positions)[a]), toWorld((*positions)[b]), edgeColor, edgeColor);

        if (!softBodyComponent || !softBodyComponent->settings.pinned)
            return;

        const auto& pins = softBodyComponent->settings.pinBounds;
        auto extent = glm::length(mesh->data.aabb.max - mesh->data.aabb.min);
        for (auto vertex : simulationMesh->GetPinnedVertices(pins))
            batch->AddLineCross(toWorld((*positions)[vertex]), 0.01f * extent, vec3(1.0f, 0.6f, 0.1f));

        for (int32_t i = 0; i < int32_t(pins.size()); i++) {
            auto color = pinSelected && i == selectedPin ? vec3(1.0f, 0.6f, 0.1f) : vec3(1.0f, 1.0f, 0.3f);
            batch->AddLineAABB(pins[i].Transform(globalMatrix), color);
        }

    }

    bool SoftBodyComponentPanel::RenderPinGizmo(const Ref<Scene::Scene>& scene, Scene::Entity entity,
        const Ref<Viewport>& viewport, const CameraComponent& camera) {

        auto softBodyComponent = entity.TryGetComponent<SoftBodyComponent>();
        if (!softBodyComponent || ECS::Entity(entity) != selectedPinEntity || !softBodyComponent->settings.pinned)
            return false;

        auto& pins = softBodyComponent->settings.pinBounds;
        if (selectedPin < 0 || selectedPin >= int32_t(pins.size()))
            return false;

        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(float(viewport->x), float(viewport->y), float(viewport->width), float(viewport->height));

        // ImGuizmo expects an OpenGL clip space
        const mat4 clip = mat4(1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f, 0.0f,
            0.0f, 0.0f, 0.5f, 1.0f);
        auto viewMatrix = camera.viewMatrix;
        auto projectionMatrix = glm::inverse(clip) * camera.unjitterdProjection;

        // A unit cube scaled to the bounds, the gizmo moves it and its handles resize it
        auto& bounds = pins[selectedPin];
        auto globalMatrix = scene->GetGlobalMatrix(entity);
        auto center = 0.5f * (bounds.min + bounds.max);
        auto size = glm::max(bounds.max - bounds.min, vec3(0.0001f));
        auto matrix = globalMatrix * glm::translate(mat4(1.0f), center) * glm::scale(mat4(1.0f), size);
        const float localBounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };

        if (ImGuizmo::Manipulate(glm::value_ptr(viewMatrix), glm::value_ptr(projectionMatrix),
            ImGuizmo::TRANSLATE | ImGuizmo::BOUNDS, ImGuizmo::LOCAL, glm::value_ptr(matrix), nullptr, nullptr, localBounds)) {
            auto local = glm::inverse(globalMatrix) * matrix;
            center = vec3(local[3]);
            size = vec3(glm::length(vec3(local[0])), glm::length(vec3(local[1])), glm::length(vec3(local[2])));
            bounds = Volume::AABB(center - 0.5f * size, center + 0.5f * size);
            pendingReset = true;
        }

        return true;

    }

}
