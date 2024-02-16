#include "MeshComponentPanel.h"
#include "resource/ResourceManager.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    bool MeshComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, MeshComponent &meshComponent) {

        bool resourceChanged = false;

        auto buttonName = meshComponent.mesh.IsValid() ? meshComponent.mesh.GetResource()->GetFileName() :
            "Drop mesh resource here";
        ImGui::Button(buttonName.c_str(), {-FLT_MIN, 0});

        if (ImGui::BeginDragDropTarget()) {
            if (auto dropPayload = ImGui::AcceptDragDropPayload(typeid(Mesh::Mesh).name())) {
                Resource<Mesh::Mesh>* resource;
                std::memcpy(&resource, dropPayload->Data, dropPayload->DataSize);
                // We know this mesh is loaded, so we can just request a handle without loading
                meshComponent.mesh = ResourceManager<Mesh::Mesh>::GetResource(resource->path);
                resourceChanged = true;
            }

            ImGui::EndDragDropTarget();
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
            materialsPanel.Render(mesh->data.materials);
        }        

        return resourceChanged;

    }

}