#include "MeshComponentPanel.h"
#include "../../Widgets.h"

#include "../../../Singletons.h"
#include "../../../tools/ResourcePayloadHelper.h"
#include "../../../Notifications.h"

#include <imgui.h>
#include <tools/ImpostorTool.h>

namespace Atlas::Editor::UI {

    bool MeshComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, MeshComponent &meshComponent) {

        bool resourceChanged = false;
        auto handle = meshSelectionPanel.Render(meshComponent.mesh, resourceChanged);
        if (resourceChanged)
            meshComponent.ChangeResource(handle);

        if (Widgets::BeginProperties("MeshComponentVisibility")) {
            auto visible = meshComponent.visible;
            Widgets::PropertyLabel("Visible");
            if (ImGui::Checkbox("##Visible", &visible))
                meshComponent.SetVisible(visible);
            auto dontCull = meshComponent.dontCull;
            Widgets::PropertyLabel("Don't cull");
            if (ImGui::Checkbox("##Don't cull", &dontCull))
                meshComponent.SetDontCull(dontCull);
            Widgets::EndProperties();
        }

        if (meshComponent.mesh.IsLoaded()) {
            auto& mesh = meshComponent.mesh;

            {
                ImGui::SeparatorText("Rendering");

                if (Widgets::BeginProperties("MeshRendering")) {
                    const char* mobilityItems[] = { "Stationary", "Movable" };
                    int mobilityItem = static_cast<int>(mesh->mobility);
                    Widgets::PropertyLabel("Mobility");
                    ImGui::Combo("##Mobility", &mobilityItem, mobilityItems, IM_ARRAYSIZE(mobilityItems));
                    mesh->mobility = static_cast<Mesh::MeshMobility>(mobilityItem);

                    Widgets::PropertyLabel("Invert UVs");
                    ImGui::Checkbox("##Invert UVs", &mesh->invertUVs);
                    Widgets::PropertyLabel("Cull backfaces");
                    ImGui::Checkbox("##Cull backfaces", &mesh->cullBackFaces);
                    Widgets::PropertyLabel("Ray trace");
                    ImGui::Checkbox("##Ray trace", &mesh->rayTrace);
                    Widgets::PropertyLabel("Is vegetation");
                    ImGui::Checkbox("##Is vegetation", &mesh->vegetation);

                    Widgets::PropertyLabel("Cast shadow");
                    ImGui::Checkbox("##Cast shadow", &mesh->castShadow);
                    Widgets::PropertyLabel("Shadow cascades");
                    ImGui::SliderInt("##Shadow cascades", &mesh->allowedShadowCascades, 1, 6);
                    Widgets::EndProperties();
                }

                if (ImGui::Button("Invert normals", ImVec2(-FLT_MIN, 0.0f)))
                    mesh->InvertNormals();
            }

            {
                ImGui::SeparatorText("Culling");
                if (Widgets::BeginProperties("MeshCulling")) {
                    Widgets::PropertyLabel("Distance culling");
                    ImGui::DragFloat("##Distance culling", &mesh->distanceCulling, 1.0f);
                    Widgets::PropertyLabel("Shadow distance culling");
                    ImGui::DragFloat("##Shadow distance culling", &mesh->shadowDistanceCulling, 1.0f);
                    Widgets::PropertyLabel("Ray trace distance culling");
                    ImGui::DragFloat("##Ray trace distance culling", &mesh->rayTraceDistanceCulling, 1.0f);
                    Widgets::PropertyLabel("Minimum screen size (px)");
                    ImGui::DragFloat("##Minimum screen size (px)", &mesh->minScreenSize, 0.1f, 0.0f, 64.0f);
                    Widgets::EndProperties();
                }
            }

            if (mesh->data.IsSkinned()) {
                ImGui::SeparatorText("Animation updates");
                if (Widgets::BeginProperties("MeshAnimation")) {
                    Widgets::PropertyLabel("Level of detail distance", "Beyond this distance the skeleton is updated less often");
                    ImGui::DragFloat("##Level of detail distance", &mesh->animationLodDistance, 1.0f, 0.0f, 500.0f);
                    Widgets::PropertyLabel("Maximum frames between updates", "How many frames distant meshes skip at most");
                    ImGui::SliderInt("##Maximum frames between updates", &mesh->animationMaxInterval, 1, 16);
                    Widgets::EndProperties();
                }
            }

            {
                ImGui::SeparatorText("Impostor");

                if (Widgets::BeginProperties("MeshImpostor")) {
                    auto impostorFilename = mesh.GetResource()->path + ".aeimpostor";
                    bool impostorEnabled = mesh->impostor.IsLoaded();
                    Widgets::PropertyLabel("Enable");
                    ImGui::Checkbox("##Enable", &impostorEnabled);
                    if (impostorEnabled && !mesh->impostor.IsLoaded()) {

                        auto impostor = CreateRef<Mesh::Impostor>();
                        mesh->impostor = ResourceManager<Mesh::Impostor>::AddResource(impostorFilename, impostor);
                    }
                    else if (!impostorEnabled && mesh->impostor.IsLoaded()) {
                        mesh->impostor = ResourceHandle<Mesh::Impostor>();
                    }

                    if (mesh->impostor.IsLoaded()) {
                        auto& impostor = mesh->impostor;

                        Widgets::PropertyLabel("Resolution");
                        ImGui::SliderInt("##Resolution", &impostor->resolution, 64, 1024);
                        Widgets::PropertyLabel("Views");
                        ImGui::SliderInt("##Views", &impostor->views, 2, 32);
                        Widgets::PropertyLabel("Distance");
                        ImGui::DragFloat("##Distance", &mesh->impostorDistance, 1.0f, 1.0f, 2000.0f);
                        Widgets::PropertyLabel("Shadow distance");
                        ImGui::DragFloat("##Shadow distance", &mesh->impostorShadowDistance, 1.0f, 1.0f, 2000.0f);
                        Widgets::PropertyLabel("Mip bias");
                        ImGui::DragFloat("##Mip bias", &impostor->mipBias, 0.01f, 0.0f, 1.0f);
                        Widgets::PropertyLabel("Cutoff");
                        ImGui::SliderFloat("##Cutoff", &impostor->cutoff, 0.0, 1.0f);
                        Widgets::PropertyLabel("Interpolation");
                        ImGui::Checkbox("##Interpolation", &impostor->interpolation);
                        Widgets::PropertyLabel("Pixel-depth offset");
                        ImGui::Checkbox("##Pixel-depth offset", &impostor->pixelDepthOffset);
                        Widgets::PropertyLabel("Compress textures");
                        ImGui::Checkbox("##Compress textures", &impostor->compressTextures);
                    }
                    Widgets::EndProperties();
                }

                if (mesh->impostor.IsLoaded()) {
                    auto& impostor = mesh->impostor;

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
                        Notifications::Push({ .message = "Generated the impostor of " + mesh.GetResource()->GetFileName() });
                    }
                }
            }

            {
                ImGui::SeparatorText("Wind");
                if (Widgets::BeginProperties("MeshWind")) {
                    Widgets::PropertyLabel("Noise lod");
                    ImGui::DragFloat("##Noise lod", &mesh->windNoiseTextureLod, 1.0f, 0.0f, 6.0f);
                    Widgets::PropertyLabel("Bend scale");
                    ImGui::DragFloat("##Bend scale", &mesh->windBendScale, 0.05f, 0.0f, 5.0f);
                    Widgets::PropertyLabel("Wiggle scale");
                    ImGui::DragFloat("##Wiggle scale", &mesh->windWiggleScale, 0.05f, 0.0f, 5.0f);
                    Widgets::EndProperties();
                }
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