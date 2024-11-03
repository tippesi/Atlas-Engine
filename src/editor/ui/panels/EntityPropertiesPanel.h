#pragma once

#include "Panel.h"
#include "scene/Scene.h"

#include "components/NameComponentPanel.h"
#include "components/TransformComponentPanel.h"
#include "components/MeshComponentPanel.h"
#include "components/LightComponentPanel.h"
#include "components/AudioComponentPanel.h"
#include "components/AudioVolumeComponentPanel.h"
#include "components/LuaScriptComponentPanel.h"
#include "components/RigidBodyComponentPanel.h"
#include "components/PlayerComponentPanel.h"
#include "components/CameraComponentPanel.h"
#include "components/TextComponentPanel.h"

#include "tools/CopyPasteHelper.h"
#include "Notifications.h"

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

    private:
        template<class T>
        struct ComponentCopy {
            Scene::Scene* scene;
            T component;
        };

        NameComponentPanel nameComponentPanel;
        TransformComponentPanel transformComponentPanel;
        MeshComponentPanel meshComponentPanel;
        LightComponentPanel lightComponentPanel;
        AudioComponentPanel audioComponentPanel;
        AudioVolumeComponentPanel audioVolumeComponentPanel;
		LuaScriptComponentPanel luaScriptComponentPanel;
        RigidBodyComponentPanel rigidBodyComponentPanel;
        PlayerComponentPanel playerComponentPanel;
        CameraComponentPanel cameraComponentPanel;
        TextComponentPanel textComponentPanel;

        template<class S, class T>
        bool RenderComponentPanel(const std::string& name, Ref<Scene::Scene>& scene,
            Scene::Entity entity, S& panel, T& component) {
            bool resourceChanged = false;

            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen |
                ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Framed;

            bool open = ImGui::TreeNodeEx(name.c_str(), nodeFlags);

            if (ImGui::BeginPopupContextItem(name.c_str())) {
                // We shouldn't allow the user to delete the root entity
                if (ImGui::MenuItem("Copy")) {
                    if constexpr (std::is_same_v<T, RigidBodyComponent>) {
                        // Restore body creation settings, after the copy the current body id is not valid anymore due
                        // to body recreation
                        component.creationSettings = CreateRef(component.GetBodyCreationSettings());
                    }
                    ComponentCopy<T> copy { scene.get(), component };
                    CopyPasteHelper::Copy(copy);
                }

                ImGui::EndPopup();
            }

            if (open) {
                resourceChanged = panel.Render(scene, entity, component);

                ImGui::TreePop();
            }

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