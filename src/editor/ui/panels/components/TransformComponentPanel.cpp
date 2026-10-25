#include "TransformComponentPanel.h"
#include "../../Widgets.h"

#include "scene/components/MeshComponent.h"
#include "scene/components/AnimationComponent.h"

#include <imgui.h>
#include <ImGuizmo.h>

namespace Atlas::Editor::UI {

    bool TransformComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, TransformComponent &transform) {

        ImGui::PushID(GetNameID());

        // Make sure the local matrix is updated from the global one
        // This needs to happen when external system like physics just updated the globalMatrix
        auto parentEntity = scene->GetParentEntity(entity);
        transform.ReconstructLocalMatrix(parentEntity);

        // The matrix decomposition/composition code is a bit unstable and
        // we work with fixed information that is recomposed when changed,
        // but only decomposed when the entity changes. Note, that all
        // component panels are unique per scene window
        if (lastEntity != entity || ImGuizmo::IsUsing()) {
            auto decomposition = transform.Decompose();
            position = decomposition.translation;
            rotation = decomposition.rotation;
            scale = decomposition.scale;
            lastEntity = entity;
        }

        vec3 localPosition = position, localRotation = rotation, localScale = scale;

        Widgets::DragVec3("Position", position, 0.01f);

        // Rotation is stored in radians, but degrees are easier to work with
        auto rotationDegrees = glm::degrees(rotation);
        if (Widgets::DragVec3("Rotation", rotationDegrees, 0.5f, 0.0f, 0.0f, 0.0f, "%.2f\xc2\xb0"))
            rotation = glm::radians(rotationDegrees);

        Widgets::DragVec3("Scale", scale, 0.01f, 1.0f, -100.0f, 100.0f);

        // Entities below an animated mesh can follow one of its joints, they keep their position when it changes
        auto parentAnimationComponent = parentEntity.IsValid() ? parentEntity.TryGetComponent<AnimationComponent>() : nullptr;
        auto parentMeshComponent = parentEntity.IsValid() ? parentEntity.TryGetComponent<MeshComponent>() : nullptr;
        if (parentAnimationComponent && parentMeshComponent && parentMeshComponent->mesh.IsLoaded() &&
            parentMeshComponent->mesh->data.IsSkinned()) {
            const auto& joints = parentMeshComponent->mesh->data.skeleton.joints;
            auto jointName = transform.parentJoint >= 0 && transform.parentJoint < int32_t(joints.size()) ?
                joints[transform.parentJoint].name : std::string("None");

            if (ImGui::BeginCombo("Parent joint", jointName.c_str())) {
                if (ImGui::Selectable("None", transform.parentJoint < 0)) {
                    transform.parentJoint = -1;
                    transform.ReconstructLocalMatrix(parentEntity);
                }
                for (int32_t i = 0; i < int32_t(joints.size()); i++) {
                    ImGui::PushID(i);
                    if (ImGui::Selectable(joints[i].name.c_str(), transform.parentJoint == i)) {
                        transform.parentJoint = i;
                        transform.ReconstructLocalMatrix(parentEntity);
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
        }

        // Only recompose when a local change happened
        if (localPosition != position || localScale != scale ||
            localRotation != rotation) {
            Common::MatrixDecomposition composition;
            composition.translation = position;
            composition.rotation = rotation;
            composition.scale = scale;
            transform.Set(composition.Compose());
            lastTransform = transform.matrix;
        }

        ImGui::PopID();

        return false;

    }

}