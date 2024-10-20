#pragma once

#include "Panel.h"
#include "scene/Scene.h"

#include "Singletons.h"
#include "EntityPropertiesPanel.h"
#include "SceneStatisticsPanel.h"
#include "ResourceSelectionPanel.h"
#include "ImguiExtension/Panels.h"

#include <type_traits>

namespace Atlas::Editor::UI {

    class ScenePropertiesPanel : public Panel {

    public:
        ScenePropertiesPanel() : Panel("Scene properties") {}

        template<class T>
        void Render(T& t, Ref<Scene::Scene> scene = nullptr, Ref<Renderer::RenderTarget> target = nullptr) {

            ImGui::Begin(GetNameID());

            if (ImGui::IsDragDropActive() && ImGui::IsWindowHovered() && !ImGui::IsWindowFocused())
                ImGui::SetWindowFocus();

            isFocused = ImGui::IsWindowFocused();

            if constexpr (std::is_same_v<T, EntityProperties>) {
                if (t.entity.IsValid()) {
                    RenderHeading("Entity");
                    entityPropertiesPanel.Render(scene, t);
                }
            }
            else if constexpr (std::is_same_v<T, Ref<Lighting::Fog>>) {
                RenderHeading("Fog");
                fogPanel.Render(t, target);
            }
            else if constexpr (std::is_same_v<T, Ref<Lighting::VolumetricClouds>>) {
                RenderHeading("Clouds");
                volumetricCloudsPanel.Render(t, target);
            }
            else if constexpr (std::is_same_v<T, Ref<Lighting::IrradianceVolume>>) {
                RenderHeading("Irradiance volume");
                irradianceVolumePanel.Render(t, scene);
            }
            else if constexpr (std::is_same_v<T, Ref<Lighting::RTGI>>) {
                RenderHeading("Raytraced global illumination");
                rtgiPanel.Render(t, target);
            }
            else if constexpr (std::is_same_v<T, Ref<Lighting::SSGI>>) {
                RenderHeading("Screen-space global illumination");
                ssgiPanel.Render(t, target);
            }
            else if constexpr (std::is_same_v<T, Ref<Lighting::Reflection>>) {
                RenderHeading("Reflection");
                reflectionPanel.Render(t, target);
            }
            else if constexpr (std::is_same_v<T, Ref<Lighting::SSS>>) {
                RenderHeading("Screen-space shadows");
                sssPanel.Render(t);
            }
            else if constexpr (std::is_same_v<T, Scene::Wind>) {
                RenderHeading("Wind");
                windPanel.Render(Singletons::imguiWrapper, t);
            }
            else if constexpr (std::is_same_v<T, Lighting::Sky>) {
                RenderHeading("Sky");
                skyPanel.Render(Singletons::imguiWrapper, t, 
                    [&](ResourceHandle<Texture::Cubemap> handle) {
                        return cubemapSelectionPanel.Render(handle);
                    });
            }
            else if constexpr (std::is_same_v<T, PostProcessing::PostProcessing>) {
                RenderHeading("Post processing");
                postProcessingPanel.Render(t);
            }
            else if constexpr (std::is_same_v<T, Ref<Scene::Scene>>) {
                RenderHeading("Scene statistics");
                sceneStatisticsPanel.Render(t);
            }

            ImGui::End();

        }

    private:
        void RenderHeading(const std::string& heading) {

            ImGui::SetWindowFontScale(1.5f);

            ImGui::Text("%s", heading.c_str());

            ImGui::SetWindowFontScale(1.0f);

            ImGui::Separator();

        }

        EntityPropertiesPanel entityPropertiesPanel;
        SceneStatisticsPanel sceneStatisticsPanel;

        ResourceSelectionPanel<Texture::Cubemap> cubemapSelectionPanel;

        ImguiExtension::FogPanel fogPanel;
        ImguiExtension::VolumetricCloudsPanel volumetricCloudsPanel;
        ImguiExtension::IrradianceVolumePanel irradianceVolumePanel;
        ImguiExtension::ReflectionPanel reflectionPanel;
        ImguiExtension::SSGIPanel ssgiPanel;
        ImguiExtension::RTGIPanel rtgiPanel;
        ImguiExtension::SSSPanel sssPanel;
        ImguiExtension::WindPanel windPanel;
        ImguiExtension::SkyPanel skyPanel;
        ImguiExtension::PostProcessingPanel postProcessingPanel;

    };

}