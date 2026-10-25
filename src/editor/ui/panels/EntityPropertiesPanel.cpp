#include "EntityPropertiesPanel.h"

#include "Notifications.h"
#include "Theme.h"

#include <imgui_stdlib.h>
#include <algorithm>

namespace Atlas::Editor::UI {

    void EntityPropertiesPanel::Render(Ref<Scene::Scene>& scene, EntityProperties entityProperties) {

        auto entity = entityProperties.entity;
        if (!entity.IsValid())
            return;

        auto nameComponent = entity.TryGetComponent<NameComponent>();
        if (nameComponent) {
            nameComponentPanel.Render(scene, entity, *nameComponent);
        }

        auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
        if (hierarchyComponent) {
            hierarchyComponentPanel.Render(scene, entity, *hierarchyComponent);
        }

        std::string nodeName = nameComponent ? nameComponent->name : "Entity " + std::to_string(entity);
        bool rootNode = nodeName == "Root";

        // General components
        {
            if (entity.HasComponent<TransformComponent>()) {
                RenderComponentPanel("Transform", scene,
                    entity, transformComponentPanel, entity.GetComponent<TransformComponent>());
            }

            if (entity.HasComponent<MeshComponent>()) {
                // Create a copy here to be able to change the resource
                auto comp = entity.GetComponent<MeshComponent>();
                RenderComponentPanel("Mesh", scene,
                    entity, meshComponentPanel, comp);
                entity.RemoveComponent<MeshComponent>();
                entity.AddComponent<MeshComponent>(comp);
            }

            if (entity.HasComponent<AnimationComponent>()) {
                RenderComponentPanel("Animation", scene, entity,
                    animationComponentPanel, entity.GetComponent<AnimationComponent>());
            }

            if (entity.HasComponent<LightComponent>()) {
                RenderComponentPanel("Light", scene, entity,
                    lightComponentPanel, entity.GetComponent<LightComponent>());
            }

            if (entity.HasComponent<AudioComponent>()) {
                auto& comp = entity.GetComponent<AudioComponent>();
                RenderComponentPanel("Audio", scene,
                    entity, audioComponentPanel, comp);
            }

            if (entity.HasComponent<AudioVolumeComponent>()) {
                auto& comp = entity.GetComponent<AudioVolumeComponent>();
                RenderComponentPanel("Audio volume", scene,
                    entity, audioVolumeComponentPanel, comp);
            }

            if (entity.HasComponent<CameraComponent>()) {
                auto& comp = entity.GetComponent<CameraComponent>();
                RenderComponentPanel("Camera", scene,
                    entity, cameraComponentPanel, comp);
            }

            if (entity.HasComponent<TextComponent>()) {
                auto& comp = entity.GetComponent<TextComponent>();
                RenderComponentPanel("Text", scene,
                    entity, textComponentPanel, comp);
            }

            if (entity.HasComponent<LuaScriptComponent>()) {
                RenderComponentPanel("Lua script", scene,
                    entity, luaScriptComponentPanel, entity.GetComponent<LuaScriptComponent>());
            }

            if (entity.HasComponent<RigidBodyComponent>()) {
                auto comp = entity.GetComponent<RigidBodyComponent>();
                RenderComponentPanel("Rigid body", scene,
                    entity, rigidBodyComponentPanel, comp);
                // Only change if we have new settings
                if (comp.creationSettings) {
                    entity.RemoveComponent<RigidBodyComponent>();
                    entity.AddComponent<RigidBodyComponent>(*comp.creationSettings);
                }
            }

            if (entity.HasComponent<PlayerComponent>()) {
                auto comp = entity.GetComponent<PlayerComponent>();
                RenderComponentPanel("Player", scene,
                    entity, playerComponentPanel, comp);
                // Only change if we have new settings
                entity.RemoveComponent<PlayerComponent>();
                auto& newComp = entity.AddComponent<PlayerComponent>(*comp.creationSettings);
                newComp.slowVelocity = comp.slowVelocity;
                newComp.fastVelocity = comp.fastVelocity;
                newComp.jumpVelocity = comp.jumpVelocity;
            }

            if (entity.HasComponent<SplineComponent>()) {
                auto& comp = entity.GetComponent<SplineComponent>();
                RenderComponentPanel("Spline", scene,
                    entity, splineComponentPanel, comp);
            }
        }

        // Add components
        {
            ImGui::Spacing();

            auto spacing = ImGui::GetStyle().ItemSpacing.x;
            auto buttonWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;

            Theme::PushPrimaryButton();
            if (ImGui::Button("Add component", { buttonWidth, 0 }))
                ImGui::OpenPopup("AddComponent");
            Theme::PopPrimaryButton();

            if (ImGui::BeginPopup("AddComponent")) {
                // Type to filter the list of components
                if (ImGui::IsWindowAppearing()) {
                    componentSearch.clear();
                    ImGui::SetKeyboardFocusHere();
                }
                ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
                ImGui::InputTextWithHint("##Component search", "Search components...", &componentSearch);
                ImGui::Separator();

                auto matches = [&](const char* name) {
                    std::string lowerName = name, lowerSearch = componentSearch;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                    std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);
                    return lowerName.find(lowerSearch) != std::string::npos;
                    };
                if (!entity.HasComponent<NameComponent>() && !rootNode && matches("Name") && ImGui::MenuItem("Name"))
                    entity.AddComponent<NameComponent>("Entity " + std::to_string(entity));
                if (!entity.HasComponent<TransformComponent>() && matches("Transform") && ImGui::MenuItem("Transform"))
                    entity.AddComponent<TransformComponent>(mat4(1.0f), false);
                if (!entity.HasComponent<MeshComponent>() && matches("Mesh") && ImGui::MenuItem("Mesh"))
                    entity.AddComponent<MeshComponent>();
                if (!entity.HasComponent<AnimationComponent>() && matches("Animation") && ImGui::MenuItem("Animation"))
                    entity.AddComponent<AnimationComponent>();
                if (!entity.HasComponent<LightComponent>() && matches("Light") && ImGui::MenuItem("Light"))
                    entity.AddComponent<LightComponent>(LightType::PointLight);
                if (!entity.HasComponent<AudioComponent>() && matches("Audio") && ImGui::MenuItem("Audio"))
                    entity.AddComponent<AudioComponent>();
                if (!entity.HasComponent<AudioVolumeComponent>() && matches("Audio volume") && ImGui::MenuItem("Audio volume"))
                    entity.AddComponent<AudioVolumeComponent>();
                if (!entity.HasComponent<CameraComponent>() && matches("Camera") && ImGui::MenuItem("Camera"))
                    entity.AddComponent<CameraComponent>();
                if (!entity.HasComponent<TextComponent>() && matches("Text") && ImGui::MenuItem("Text"))
                    entity.AddComponent<TextComponent>();
				if (!entity.HasComponent<LuaScriptComponent>() && matches("Lua script") && ImGui::MenuItem("Lua script"))
                    entity.AddComponent<LuaScriptComponent>();
                if (!entity.HasComponent<SplineComponent>() && matches("Spline") && ImGui::MenuItem("Spline"))
                    entity.AddComponent<SplineComponent>();

                // Just make the player component addable if there is a transform component
                if (entity.HasComponent<TransformComponent>() &&
                    !entity.HasComponent<PlayerComponent>() && matches("Player") && ImGui::MenuItem("Player")) {
                    // Created standardized shapes/rigid body component as a default (we need to get the scale from
                    // the transform component
                    vec3 scale = vec3(1.0f);
                    scale = entity.GetComponent<TransformComponent>().Decompose().scale;

                    auto shape = Physics::ShapesManager::CreateShape(Physics::CapsuleShapeSettings{ .scale = scale });
                    auto bodySettings = Physics::PlayerCreationSettings { .shape = shape };
                    entity.AddComponent<PlayerComponent>(bodySettings);
                }

                // Just make the rigid body component addable if there is a transform component
                if (entity.HasComponent<TransformComponent>() &&
                    !entity.HasComponent<RigidBodyComponent>() && matches("Rigid body") && ImGui::MenuItem("Rigid body")) {
                    // Created standardized shapes/rigid body component as a default (we need to get the scale from
                    // the transform component
                    vec3 scale = vec3(1.0f);
                    auto& globalMatrix = entity.GetComponent<TransformComponent>().globalMatrix;
                    // Need scale from global matrix decomposition
                    scale = Common::MatrixDecomposition(globalMatrix).scale;

                    auto shape = Physics::ShapesManager::CreateShape(Physics::BoundingBoxShapeSettings { .scale = scale });
                    auto bodySettings = Physics::BodyCreationSettings { .objectLayer = Physics::Layers::Movable, .shape = shape };
                    entity.AddComponent<RigidBodyComponent>(bodySettings);
                }

                ImGui::EndPopup();
            }

            // Remove components
            ImGui::SameLine(0.0f, spacing);
            if (ImGui::Button("Remove component", { -FLT_MIN, 0 }))
                ImGui::OpenPopup("RemoveComponent");

            if (ImGui::BeginPopup("RemoveComponent")) {
                if (entity.HasComponent<NameComponent>()  && !rootNode && ImGui::MenuItem("Name"))
                    entity.RemoveComponent<NameComponent>();
                if (entity.HasComponent<TransformComponent>() && ImGui::MenuItem("Transform"))
                    entity.RemoveComponent<TransformComponent>();
                if (entity.HasComponent<MeshComponent>() && ImGui::MenuItem("Mesh"))
                    entity.RemoveComponent<MeshComponent>();
                if (entity.HasComponent<AnimationComponent>() && ImGui::MenuItem("Animation"))
                    entity.RemoveComponent<AnimationComponent>();
                if (entity.HasComponent<LightComponent>() && ImGui::MenuItem("Light"))
                    entity.RemoveComponent<LightComponent>();
                if (entity.HasComponent<AudioComponent>() && ImGui::MenuItem("Audio"))
                    entity.RemoveComponent<AudioComponent>();
                if (entity.HasComponent<AudioVolumeComponent>() && ImGui::MenuItem("Audio volume"))
                    entity.RemoveComponent<AudioVolumeComponent>();
                if (entity.HasComponent<CameraComponent>() && ImGui::MenuItem("Camera"))
                    entity.RemoveComponent<CameraComponent>();
                if (entity.HasComponent<TextComponent>() && ImGui::MenuItem("Text"))
                    entity.RemoveComponent<TextComponent>();
                if (entity.HasComponent<LuaScriptComponent>() && ImGui::MenuItem("Lua script"))
                    entity.RemoveComponent<LuaScriptComponent>();
                if (entity.HasComponent<SplineComponent>() && ImGui::MenuItem("Spline"))
                    entity.RemoveComponent<SplineComponent>();
                if (entity.HasComponent<PlayerComponent>() && ImGui::MenuItem("Player"))
                    entity.RemoveComponent<PlayerComponent>();
                if (entity.HasComponent<RigidBodyComponent>() && ImGui::MenuItem("Rigid body"))
                    entity.RemoveComponent<RigidBodyComponent>();

                ImGui::EndPopup();
            }
        }

        // Paste components
        if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
            if (ImGui::MenuItem("Paste")) {
                if (HandleComponentPaste<TransformComponent>(scene, entity)) {}
                else if (HandleComponentPaste<MeshComponent>(scene, entity)) {}
                else if (HandleComponentPaste<AnimationComponent>(scene, entity)) {}
                else if (HandleComponentPaste<LightComponent>(scene, entity)) {}
                else if (HandleComponentPaste<AudioComponent>(scene, entity)) {}
                else if (HandleComponentPaste<AudioVolumeComponent>(scene, entity)) {}
                else if (HandleComponentPaste<CameraComponent>(scene, entity)) {}
                else if (HandleComponentPaste<TextComponent>(scene, entity)) {}
                else if (HandleComponentPaste<LuaScriptComponent>(scene, entity)) {}
                else if (HandleComponentPaste<SplineComponent>(scene, entity)) {}
                else if (HandleComponentPaste<PlayerComponent>(scene, entity)) {}
                else if (HandleComponentPaste<RigidBodyComponent>(scene, entity)) {}
                else {
                    Notifications::Push({"Invalid type to paste as a component."});
                }
            }

            ImGui::EndPopup();
        }

        if (pendingRemoval) {
            pendingRemoval();
            pendingRemoval = nullptr;
        }

    }

}
