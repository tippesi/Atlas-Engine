#pragma once

#include "Panel.h"
#include "scene/Scene.h"

#include "components/NameComponentPanel.h"
#include "components/TransformComponentPanel.h"
#include "components/MeshComponentPanel.h"
#include "components/LightComponentPanel.h"
#include "components/AudioVolumeComponentPanel.h"
#include "components/RigidBodyComponentPanel.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    class EntityPropertiesPanel : public Panel {

    public:
        EntityPropertiesPanel() : Panel("Entity properties") {}

        void Render(Ref<Scene::Scene>& scene, Scene::Entity entity);

    private:
        NameComponentPanel nameComponentPanel;
        TransformComponentPanel transformComponentPanel;
        MeshComponentPanel meshComponentPanel;
        LightComponentPanel lightComponentPanel;
        AudioVolumeComponentPanel audioVolumeComponentPanel;
        RigidBodyComponentPanel rigidBodyComponentPanel;

        template<class S, class T>
        bool RenderComponentPanel(const std::string& name, Ref<Scene::Scene>& scene,
            Scene::Entity entity, S& panel, T& component) {
            bool resourceChanged = false;

            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen |
                ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

            if (ImGui::TreeNodeEx(name.c_str(), nodeFlags)) {
                resourceChanged = panel.Render(scene, entity, component);

                ImGui::TreePop();
            }

            return resourceChanged;
        }

    };

}