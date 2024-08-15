#include "MeshComponentPanel.h"

#include "../../../Singletons.h"
#include "../../../tools/ResourcePayloadHelper.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    bool MeshComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, MeshComponent &meshComponent) {

        bool resourceChanged = false;
        meshComponent.mesh = meshSelectionPanel.Render(meshComponent.mesh, resourceChanged);

        ImGui::Checkbox("Visible", &meshComponent.visible);
        ImGui::Checkbox("Don't cull", &meshComponent.dontCull);

        if (meshComponent.mesh.IsLoaded()) {
            auto& mesh = meshComponent.mesh;
            ImGui::Separator();
            ImGui::Text("Mesh settings");

            const char* mobilityItems[] = { "Stationary", "Movable" };
            int mobilityItem = static_cast<int>(mesh->mobility);
            ImGui::Combo("Mobility", &mobilityItem, mobilityItems, IM_ARRAYSIZE(mobilityItems));
            mesh->mobility = static_cast<Mesh::MeshMobility>(mobilityItem);


            ImGui::Checkbox("Invert UVs", &mesh->invertUVs);
            ImGui::Checkbox("Cull backfaces", &mesh->cullBackFaces);
            ImGui::Checkbox("Is vegetation", &mesh->vegetation);

            ImGui::Checkbox("Cast shadow", &mesh->castShadow);
            ImGui::SliderInt("Shadow cascades", &mesh->allowedShadowCascades, 1, 6);

            ImGui::Separator();
            ImGui::Text("Culling settings");            
            ImGui::DragFloat("Distance culling", &mesh->distanceCulling, 1.0f);
            ImGui::DragFloat("Shadow distance culling", &mesh->shadowDistanceCulling, 1.0f);
            
            ImGui::Separator();
            ImGui::Text("Wind settings");
            ImGui::DragFloat("Noise lod", &mesh->windNoiseTextureLod, 1.0f, 0.0f, 6.0f);
            ImGui::DragFloat("Bend scale", &mesh->windBendScale, 0.05f, 0.0f, 5.0f);
            ImGui::DragFloat("Wiggle scale", &mesh->windWiggleScale, 0.05f, 0.0f, 5.0f);

            ImGui::Separator();
            ImGui::Text("Materials");
            materialsPanel.Render(Singletons::imguiWrapper, mesh->data.materials, 
                [&](ResourceHandle<Material> material) {
                    return materialSelectionPanel.Render(material);
                },
                [&](ResourceHandle<Texture::Texture2D> texture) {
                    return textureSelectionPanel.Render(texture);
                });

            // Just update materials regardless of any change
            mesh->UpdatePipelines();
        }        

        return resourceChanged;

    }

}