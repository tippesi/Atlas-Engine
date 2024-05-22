#include "MeshComponentPanel.h"

#include "../../../Singletons.h"
#include "../../../tools/ResourcePayloadHelper.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    bool MeshComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, MeshComponent &meshComponent) {

        bool resourceChanged = false;

        auto buttonName = meshComponent.mesh.IsValid() ? meshComponent.mesh.GetResource()->GetFileName() :
            "Drop mesh resource here";
        if (ImGui::Button(buttonName.c_str(), {-FLT_MIN, 0}))
            meshSelectionPopup.Open();

        // Such that drag and drop will work from the content browser
        if (ImGui::IsDragDropActive() && ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly)) {
            ImGui::SetWindowFocus();
            ImGui::SetItemDefaultFocus();
        }

        auto handle = ResourcePayloadHelper::AcceptDropResource<Mesh::Mesh>();
        // Need to change here already
        if (handle.IsValid()) {
            meshComponent.mesh = handle;
            resourceChanged = true;
        }

        auto meshResources = ResourceManager<Mesh::Mesh>::GetResources();
        handle = meshSelectionPopup.Render(meshResources);

        if (handle.IsValid()) {
            meshComponent.mesh = handle;
            resourceChanged = true;
        }

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
            ImGui::Text("Wind settings");
            ImGui::DragFloat("Noise lod", &mesh->windNoiseTextureLod, 1.0f, 0.0f, 5.0f);
            ImGui::DragFloat("Bend scale", &mesh->windBendScale, 0.05f, 0.0f, 5.0f);
            ImGui::DragFloat("Wiggle scale", &mesh->windWiggleScale, 0.05f, 0.0f, 5.0f);

            ImGui::Separator();
            ImGui::Text("Materials");
            materialsPanel.Render(Singletons::imguiWrapper, mesh->data.materials);

            // Just update materials regardless of any change
            mesh->UpdateMaterials();
        }        

        return resourceChanged;

    }

}