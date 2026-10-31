#include "ConstraintComponentPanel.h"
#include "../../../Theme.h"
#include "../../Widgets.h"

namespace Atlas::Editor::UI {

    using namespace Physics;

    bool ConstraintComponentPanel::Render(Ref<Scene::Scene>& scene, Scene::Entity entity,
        ConstraintComponent& constraintComponent) {

        ImGui::PushID(GetNameID());

        if (!entity.HasComponent<RigidBodyComponent>())
            Widgets::Callout("Constraint component needs a rigid body component to work properly", Theme::Warning());

        if (constraintComponent.broken)
            Widgets::Callout("The constraint broke, changing it creates it again", Theme::Warning());

        ImGui::SeparatorText("Connected entity");

        bool changed = RenderConnectedEntity(entity, constraintComponent);

        ImGui::SeparatorText("Constraint");

        changed |= RenderConstraintSettings(constraintComponent.creationSettings);

        // Constraints are created while simulating, changes create the constraint again
        if (changed)
            constraintComponent.Reset();

        ImGui::PopID();

        return false;

    }

    bool ConstraintComponentPanel::RenderConnectedEntity(Scene::Entity entity, ConstraintComponent& constraintComponent) {

        auto connectedEntity = constraintComponent.GetConnectedEntity();

        std::string buttonName = "Drop entity with rigid body component here (world)";
        if (connectedEntity.IsValid()) {
            buttonName = "Entity " + std::to_string(connectedEntity.GetID());
            if (connectedEntity.HasComponent<NameComponent>())
                buttonName = connectedEntity.GetComponent<NameComponent>().name;
        }

        Widgets::DropSlot(buttonName.c_str(), !connectedEntity.IsValid());

        bool changed = false;
        if (ImGui::BeginDragDropTarget()) {
            auto dropPayload = ImGui::GetDragDropPayload();
            if (dropPayload->IsDataType(typeid(Scene::Entity).name())) {
                Scene::Entity dropEntity;
                std::memcpy(&dropEntity, dropPayload->Data, dropPayload->DataSize);
                bool validEntity = dropEntity != entity && dropEntity.HasComponent<RigidBodyComponent>();

                if (validEntity && ImGui::AcceptDragDropPayload(typeid(Scene::Entity).name())) {
                    constraintComponent.SetConnectedEntity(dropEntity);
                    changed = true;
                }
            }

            ImGui::EndDragDropTarget();
        }

        if (connectedEntity.IsValid() && ImGui::Button("Connect to world", { -FLT_MIN, 0 })) {
            constraintComponent.SetConnectedEntity(Scene::Entity());
            changed = true;
        }

        return changed;

    }

    bool ConstraintComponentPanel::RenderConstraintSettings(Physics::ConstraintCreationSettings& settings) {

        bool changed = false;

        const char* typeItems[] = { "Fixed", "Point", "Hinge", "Slider", "Distance", "Cone" };
        int currentItem = static_cast<int>(settings.type);
        changed |= ImGui::Combo("Type", &currentItem, typeItems, IM_ARRAYSIZE(typeItems));
        settings.type = static_cast<ConstraintType>(currentItem);

        auto type = settings.type;

        // Fixed and slider constraints keep the bodies where they are when the constraint is created
        if (type != ConstraintType::Fixed && type != ConstraintType::Slider)
            changed |= Widgets::DragVec3("Point", settings.point, 0.01f, 0.0f, -10000.0f, 10000.0f);
        if (type == ConstraintType::Distance)
            changed |= Widgets::DragVec3("Other point", settings.otherPoint, 0.01f, 0.0f, -10000.0f, 10000.0f);
        if (type == ConstraintType::Hinge || type == ConstraintType::Slider || type == ConstraintType::Cone)
            changed |= Widgets::DragVec3("Axis", settings.axis, 0.01f, 0.0f, -1.0f, 1.0f);

        if (type == ConstraintType::Hinge || type == ConstraintType::Slider || type == ConstraintType::Distance) {
            changed |= ImGui::Checkbox("Enable limits", &settings.enableLimits);
            if (settings.enableLimits) {
                if (type == ConstraintType::Hinge) {
                    changed |= ImGui::SliderAngle("Limits min", &settings.limitsMin, -180.0f, 0.0f);
                    changed |= ImGui::SliderAngle("Limits max", &settings.limitsMax, 0.0f, 180.0f);
                }
                else {
                    changed |= ImGui::DragFloat("Limits min", &settings.limitsMin, 0.01f, -10000.0f, 10000.0f);
                    changed |= ImGui::DragFloat("Limits max", &settings.limitsMax, 0.01f, -10000.0f, 10000.0f);
                }
            }
        }

        if (type == ConstraintType::Cone)
            changed |= ImGui::SliderAngle("Half cone angle", &settings.halfConeAngle, 0.0f, 180.0f);

        changed |= ImGui::DragFloat("Break force", &settings.breakForce, 10.0f, 0.0f, 1000000.0f);
        ImGui::SetItemTooltip("Zero never breaks");

        return changed;

    }

}
