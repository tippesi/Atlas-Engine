#pragma once

#include "Panel.h"
#include "scene/Scene.h"

#include "Singletons.h"
#include "EntityPropertiesPanel.h"
#include "SceneStatisticsPanel.h"
#include "ResourceSelectionPanel.h"
#include "../Widgets.h"
#include "TerrainPanel.h"
#include "NavigationPanel.h"
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

            // Labels are on the right side of the widgets, they get a fixed room to not be cut off in narrow panels
            ImGui::PushItemWidth(-std::min(150.0f, ImGui::GetContentRegionAvail().x * 0.5f));

            if constexpr (std::is_same_v<T, EntityProperties>) {
                if (t.entity.IsValid()) {
                    RenderHeading("Entity");
                    entityPropertiesPanel.Render(scene, t);
                }
            }
            else if constexpr (std::is_same_v<T, ResourceHandle<Terrain::Terrain>>) {
                RenderHeading("Terrain");
                terrainPanel.Render(t, scene);
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
                RenderHeading("Ray traced global illumination");
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
            else if constexpr (std::is_same_v<T, Ref<Ocean::Ocean>>) {
                RenderHeading("Ocean");
                if (t) {
                    oceanPanel.Render(t,
                        [&](ResourceHandle<Texture::Texture2D> handle) {
                            return textureSelectionPanel.Render(handle);
                        });
                    ImGui::Separator();
                    if (Widgets::DangerButton("Remove ocean"))
                        t = nullptr;
                }
                else if (ImGui::Button("Create ocean", ImVec2(-FLT_MIN, 0.0f))) {
                    t = CreateRef<Ocean::Ocean>(9, 4096.0f);
                }
            }
            else if constexpr (std::is_same_v<T, Ref<Navigation::Navigation>>) {
                RenderHeading("Navigation");
                navigationPanel.Render(t);
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
                postProcessingPanel.isUpscalerSupported = [](PostProcessing::Upscaler upscaler) {
                    return Singletons::mainRenderer->IsUpscalerSupported(upscaler);
                };
                postProcessingPanel.Render(t,
                    [&](ResourceHandle<Texture::Texture2D> handle) {
                        return textureSelectionPanel.Render(handle);
                    });
            }
            else if constexpr (std::is_same_v<T, Ref<Scene::Scene>>) {
                // One heading for the scene, the statistics bring their own sections
                RenderHeading("Scene");
                if (Widgets::BeginProperties("Scene rendering")) {
                    Widgets::PropertyLabel("Occlusion culling", "Skips meshes hidden behind others");
                    ImGui::Checkbox("##Occlusion culling", &t->occlusionCulling);
                    Widgets::EndProperties();
                }
                sceneStatisticsPanel.Render(t);
            }

            cubemapSelectionPanel.Reset();
            textureSelectionPanel.Reset();

            ImGui::PopItemWidth();

            ImGui::End();

        }
       
        EntityPropertiesPanel entityPropertiesPanel;
        SceneStatisticsPanel sceneStatisticsPanel;

        ResourceSelectionPanel<Texture::Cubemap> cubemapSelectionPanel;
        ResourceSelectionPanel<Texture::Texture2D> textureSelectionPanel;

        TerrainPanel terrainPanel;
        NavigationPanel navigationPanel;
        ImguiExtension::FogPanel fogPanel;
        ImguiExtension::VolumetricCloudsPanel volumetricCloudsPanel;
        ImguiExtension::IrradianceVolumePanel irradianceVolumePanel;
        ImguiExtension::ReflectionPanel reflectionPanel;
        ImguiExtension::SSGIPanel ssgiPanel;
        ImguiExtension::RTGIPanel rtgiPanel;
        ImguiExtension::SSSPanel sssPanel;
        ImguiExtension::OceanPanel oceanPanel;
        ImguiExtension::WindPanel windPanel;
        ImguiExtension::SkyPanel skyPanel;
        ImguiExtension::PostProcessingPanel postProcessingPanel;

    private:
        void RenderHeading(const std::string& heading) {

            Widgets::Heading(heading.c_str());
            ImGui::Separator();

        }

    };

}