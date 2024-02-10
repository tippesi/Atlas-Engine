#pragma once

#include "Panel.h"
#include "scene/Scene.h"

#include "EntityPropertiesPanel.h"
#include "ImguiExtension/Panels.h"

#include <type_traits>

namespace Atlas::Editor::UI {

    class ScenePropertiesPanel : public Panel {

    public:
        ScenePropertiesPanel() : Panel("Scene properties") {}

        template<class T>
        void Render(T& t, Ref<Scene::Scene> scene = nullptr) {

            ImGui::Begin(GetNameID());

            isFocused = ImGui::IsWindowFocused();

            if constexpr (std::is_same_v<T, Scene::Entity>) {
                if (t.IsValid()) {
                    RenderHeading("Entity");
                    entityPropertiesPanel.Render(scene, t);
                }
            }
            else if constexpr (std::is_same_v<T, Ref<Lighting::Fog>>) {
                RenderHeading("Fog");
                fogPanel.Render(t);
            }
            else if constexpr (std::is_same_v<T, Ref<Lighting::VolumetricClouds>>) {
                RenderHeading("Clouds");
                volumetricCloudsPanel.Render(t);
            }
            else if constexpr (std::is_same_v<T, Ref<Lighting::IrradianceVolume>>) {
                RenderHeading("Irradiance volume");
                irradianceVolumePanel.Render(t);
            }
            else if constexpr (std::is_same_v<T, Ref<Lighting::Reflection>>) {
                RenderHeading("Reflection");
                reflectionPanel.Render(t);
            }
            else if constexpr (std::is_same_v<T, PostProcessing::PostProcessing>) {
                RenderHeading("Post processing");
                postProcessingPanel.Render(t);
            }

            ImGui::End();

        }

    private:
        void RenderHeading(const std::string& heading) {

            ImGui::Text("%s", heading.c_str());

            ImGui::Separator();

        }

        EntityPropertiesPanel entityPropertiesPanel;

        ImguiExtension::FogPanel fogPanel;
        ImguiExtension::VolumetricCloudsPanel volumetricCloudsPanel;
        ImguiExtension::IrradianceVolumePanel irradianceVolumePanel;
        ImguiExtension::ReflectionPanel reflectionPanel;
        ImguiExtension::PostProcessingPanel postProcessingPanel;

    };

}