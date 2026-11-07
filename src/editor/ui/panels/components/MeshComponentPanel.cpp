#include "MeshComponentPanel.h"

#include "../../../Singletons.h"
#include "../../../tools/ResourcePayloadHelper.h"

#include <imgui.h>
#include <tools/ImpostorTool.h>

namespace Atlas::Editor::UI {

    bool MeshComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, MeshComponent &meshComponent) {

        bool resourceChanged = false;
        auto handle = meshSelectionPanel.Render(meshComponent.mesh, resourceChanged);
        if (resourceChanged)
            meshComponent.ChangeResource(handle);

        auto visible = meshComponent.visible;
        if (ImGui::Checkbox("Visible", &visible))
            meshComponent.SetVisible(visible);
        auto dontCull = meshComponent.dontCull;
        if (ImGui::Checkbox("Don't cull", &dontCull))
            meshComponent.SetDontCull(dontCull);

        if (meshComponent.mesh.IsLoaded()) {
            auto& mesh = meshComponent.mesh;

            {
                ImGui::SeparatorText("Mesh settings");

                const char* mobilityItems[] = { "Stationary", "Movable" };
                int mobilityItem = static_cast<int>(mesh->mobility);
                ImGui::Combo("Mobility", &mobilityItem, mobilityItems, IM_ARRAYSIZE(mobilityItems));
                mesh->mobility = static_cast<Mesh::MeshMobility>(mobilityItem);

                ImGui::Checkbox("Invert UVs", &mesh->invertUVs);
                ImGui::Checkbox("Cull backfaces", &mesh->cullBackFaces);
                ImGui::Checkbox("Ray trace", &mesh->rayTrace);
                ImGui::Checkbox("Is vegetation", &mesh->vegetation);

                ImGui::Checkbox("Cast shadow", &mesh->castShadow);
                ImGui::SliderInt("Shadow cascades", &mesh->allowedShadowCascades, 1, 6);

                auto region = ImGui::GetContentRegionAvail();
                if (ImGui::Button("Invert normals", { region.x, 0.0 }))
                    mesh->InvertNormals();
            }

            {
                ImGui::SeparatorText("Culling settings");
                ImGui::DragFloat("Distance culling", &mesh->distanceCulling, 1.0f);
                ImGui::DragFloat("Shadow distance culling", &mesh->shadowDistanceCulling, 1.0f);
                ImGui::DragFloat("Ray trace distance culling", &mesh->rayTraceDistanceCulling, 1.0f);
                ImGui::DragFloat("Minimum screen size (px)", &mesh->minScreenSize, 0.1f, 0.0f, 64.0f);
            }

            if (mesh->data.IsSkinned()) {
                ImGui::SeparatorText("Animation settings");
                ImGui::DragFloat("Level of detail distance", &mesh->animationLodDistance, 1.0f, 0.0f, 500.0f);
                ImGui::SliderInt("Maximum frames between updates", &mesh->animationMaxInterval, 1, 16);
            }

            {
                ImGui::SeparatorText("Impostor settings");

                auto impostorFilename = mesh.GetResource()->path + ".aeimpostor";
                bool impostorEnabled = mesh->impostor.IsLoaded();
                ImGui::Checkbox("Enable", &impostorEnabled);
                if (impostorEnabled && !mesh->impostor.IsLoaded()) {
                    
                    auto impostor = CreateRef<Mesh::Impostor>();
                    mesh->impostor = ResourceManager<Mesh::Impostor>::AddResource(impostorFilename, impostor);
                }
                else if (!impostorEnabled && mesh->impostor.IsLoaded()) {
                    mesh->impostor = ResourceHandle<Mesh::Impostor>();
                }

                if (mesh->impostor.IsLoaded()) {
                    auto& impostor = mesh->impostor;

                    ImGui::SliderInt("Resolution", &impostor->resolution, 64, 1024);
                    ImGui::SliderInt("Views", &impostor->views, 2, 32);
                    ImGui::DragFloat("Distance", &mesh->impostorDistance, 1.0f, 1.0f, 2000.0f);
                    ImGui::DragFloat("Shadow distance", &mesh->impostorShadowDistance, 1.0f, 1.0f, 2000.0f);
                    ImGui::DragFloat("Mip bias", &impostor->mipBias, 0.01f, 0.0f, 1.0f);
                    ImGui::SliderFloat("Cutoff", &impostor->cutoff, 0.0, 1.0f);
                    ImGui::Checkbox("Interpolation", &impostor->interpolation);
                    ImGui::Checkbox("Pixel-depth offset", &impostor->pixelDepthOffset);
                    ImGui::Checkbox("Compress textures", &impostor->compressTextures);

                    if (ImGui::Button("Generate", ImVec2(-FLT_MIN, 0.0f))) {
                        auto newImpostor = Atlas::Tools::ImpostorTool::GenerateImpostor(
                            mesh, impostor->views, impostor->resolution
                        );
                        newImpostor->cutoff = impostor->cutoff;
                        newImpostor->mipBias = impostor->mipBias;
                        newImpostor->interpolation = impostor->interpolation;
                        newImpostor->pixelDepthOffset = impostor->pixelDepthOffset;
                        newImpostor->compressTextures = impostor->compressTextures;
                        impostor.GetResource()->Swap(newImpostor);                        
                    }
                }
            }
            
            {
                ImGui::SeparatorText("Wind settings");
                ImGui::DragFloat("Noise lod", &mesh->windNoiseTextureLod, 1.0f, 0.0f, 6.0f);
                ImGui::DragFloat("Bend scale", &mesh->windBendScale, 0.05f, 0.0f, 5.0f);
                ImGui::DragFloat("Wiggle scale", &mesh->windWiggleScale, 0.05f, 0.0f, 5.0f);
            }

            ImGui::SeparatorText("Materials");
            materialsPanel.Render(Singletons::imguiWrapper, mesh->data.materials, 
                [&](ResourceHandle<Material> material) {
                    auto selected = materialSelectionPanel.Render(material);
                    if (selected.IsLoaded() && selected->HasGraph()) {
                        ImGui::SameLine();
                        ImGui::TextColored(Theme::TextMuted(), "Uses the graph %s", selected->graph.GetResource()->GetFileName().c_str());
                    }
                    return selected;
                },
                [&](ResourceHandle<Texture::Texture2D> texture) {
                    return textureSelectionPanel.Render(texture);
                });

            // Just update materials regardless of any change
            mesh->UpdatePipelines();
        }

        meshSelectionPanel.Reset();
        materialSelectionPanel.Reset();
        textureSelectionPanel.Reset();

        return resourceChanged;

    }

}