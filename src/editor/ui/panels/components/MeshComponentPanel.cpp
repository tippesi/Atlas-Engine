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
        ImGui::Button(buttonName.c_str(), {-FLT_MIN, 0});

        auto handle = ResourcePayloadHelper::AcceptDropResource<Mesh::Mesh>();
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
            ImGui::Checkbox("Invert UVs", &mesh->invertUVs);
            ImGui::Checkbox("Cull backfaces", &mesh->cullBackFaces);
            ImGui::Separator();
            ImGui::Text("Materials");
            materialsPanel.Render(Singletons::imguiWrapper, mesh->data.materials);
        }        

        return resourceChanged;

    }

}