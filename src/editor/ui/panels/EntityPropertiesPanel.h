#pragma once

#include "Panel.h"
#include "scene/Scene.h"

#include "components/NameComponentPanel.h"
#include "components/TransformComponentPanel.h"
#include "components/MeshComponentPanel.h"
#include "components/AnimationComponentPanel.h"
#include "components/LightComponentPanel.h"
#include "components/AudioComponentPanel.h"
#include "components/AudioVolumeComponentPanel.h"
#include "components/LuaScriptComponentPanel.h"
#include "components/RigidBodyComponentPanel.h"
#include "components/PlayerComponentPanel.h"
#include "components/CameraComponentPanel.h"
#include "components/TextComponentPanel.h"
#include "components/SplineComponentPanel.h"
#include "components/HierarchyComponentPanel.h"

#include "tools/CopyPasteHelper.h"
#include "Notifications.h"
#include "Singletons.h"
#include "ui/Widgets.h"

#include <imgui.h>
#include <optional>
#include <functional>

namespace Atlas::Editor::UI {

    struct EntityProperties {
        Scene::Entity entity;
        Scene::Entity editorCameraEntity;
    };

    class EntityPropertiesPanel : public Panel {

    public:
        EntityPropertiesPanel() : Panel("Entity properties") {}

        void Render(Ref<Scene::Scene>& scene, EntityProperties entityProperties);

        NameComponentPanel nameComponentPanel;
        TransformComponentPanel transformComponentPanel;
        MeshComponentPanel meshComponentPanel;
        AnimationComponentPanel animationComponentPanel;
        LightComponentPanel lightComponentPanel;
        AudioComponentPanel audioComponentPanel;
        AudioVolumeComponentPanel audioVolumeComponentPanel;
        LuaScriptComponentPanel luaScriptComponentPanel;
        RigidBodyComponentPanel rigidBodyComponentPanel;
        PlayerComponentPanel playerComponentPanel;
        CameraComponentPanel cameraComponentPanel;
        TextComponentPanel textComponentPanel;
        SplineComponentPanel splineComponentPanel;
        HierarchyComponentPanel hierarchyComponentPanel;

    private:
        std::function<void()> pendingRemoval;
        std::string componentSearch;

        template<class T>
        struct ComponentCopy {
            Scene::Scene* scene;
            T component;
        };

        template<class S, class T>
        bool RenderComponentPanel(const std::string& name, Ref<Scene::Scene>& scene,
            Scene::Entity entity, S& panel, T& component) {
            bool resourceChanged = false;

            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen |
                ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Framed |
                ImGuiTreeNodeFlags_AllowOverlap;

            // Component headers are neutral, the accent is reserved for selection
            ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_Button));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            bool open = ImGui::TreeNodeEx(name.c_str(), nodeFlags);
            ImGui::PopStyleColor(3);

            bool removeComponent = false;
            if (ImGui::BeginPopupContextItem(name.c_str())) {
                if (ImGui::MenuItem("Copy")) {
                    if constexpr (std::is_same_v<T, RigidBodyComponent>) {
                        // Restore body creation settings, after the copy the current body id is not valid anymore due
                        // to body recreation
                        component.creationSettings = CreateRef(component.GetBodyCreationSettings());
                    }
                    ComponentCopy<T> copy { scene.get(), component };
                    CopyPasteHelper::Copy(copy);
                }
                if (ImGui::MenuItem("Remove"))
                    removeComponent = true;

                ImGui::EndPopup();
            }

            // Remove button on the right side of the header
            {
                auto headerMin = ImGui::GetItemRectMin();
                auto headerMax = ImGui::GetItemRectMax();
                auto iconSize = ImGui::GetTextLineHeight();
                auto buttonExtent = iconSize + 2.0f;
                auto cursorPos = ImGui::GetCursorPos();

                ImGui::SameLine();
                ImGui::SetCursorScreenPos(ImVec2(headerMax.x - buttonExtent - ImGui::GetStyle().FramePadding.x,
                    headerMin.y + (headerMax.y - headerMin.y - buttonExtent) * 0.5f));

                auto alpha = ImGui::IsMouseHoveringRect(headerMin, headerMax) ? 1.0f : 0.45f;
                ImGui::PushID(name.c_str());
                if (Widgets::IconButton("Remove component", Singletons::icons->Get(IconType::Trash),
                    iconSize, "Remove component", alpha))
                    removeComponent = true;
                ImGui::PopID();

                ImGui::SetCursorPos(cursorPos);
            }

            // Defer removal until all panels are rendered. Some components are copied and
            // re-added after their panel was rendered, which would otherwise revive them
            if (removeComponent)
                pendingRemoval = [entity]() mutable { entity.RemoveComponent<T>(); };

            if (open) {
                resourceChanged = panel.Render(scene, entity, component);

                ImGui::TreePop();
            }

            ImGui::Spacing();

            return resourceChanged;
        }

        template<class T>
        bool HandleComponentPaste(Ref<Scene::Scene>& scene, Scene::Entity entity, std::optional<std::function<void(T&)>> func = std::nullopt) {

            if (CopyPasteHelper::AcceptPaste<ComponentCopy<T>>()) {
                ComponentCopy<T> copy;
                CopyPasteHelper::Paste(copy);

                if (copy.scene != scene.get()) {
                    Notifications::Push({ "Cannot copy component from one scene to another", vec3(1.0f, 0.0f, 0.0f) });
                    return true;
                }

                if (entity.HasComponent<T>())
                    entity.RemoveComponent<T>();

                auto& comp = entity.AddComponent<T>(copy.component);
                if (func.has_value())
                    func.value()(comp);

                return true;
            }

            return false;

        }

    };

}