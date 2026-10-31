#include "RigidBodyComponentPanel.h"
#include "../../../Theme.h"
#include "../../Widgets.h"

namespace Atlas::Editor::UI {

    using namespace Physics;

    bool RigidBodyComponentPanel::Render(const Ref<Scene::Scene>& scene,
        Scene::Entity entity, RigidBodyComponent &rigidBodyComponent) {

        if (!entity.HasComponent<TransformComponent>())
            Widgets::Callout("Rigid body component needs a transform component to work properly", Theme::Warning());

        auto creationSettings = rigidBodyComponent.GetBodyCreationSettings();
        if (!creationSettings.shape) {
            Widgets::Callout("Couldn't get body creation settings or shape", Theme::Error());
            return false;
        }

        ImGui::SeparatorText("Shape");

        RenderShapeSettings(entity, creationSettings);

        ImGui::SeparatorText("Body");

        RenderBodySettings(entity, creationSettings);

        // Only change when simulation is paused (otherwise we can't restore the state properly)
        if (scene->physicsWorld->pauseSimulation)
            rigidBodyComponent.creationSettings = CreateRef(creationSettings);

        meshSelectionPanel.Reset();

        return false;

    }

    void RigidBodyComponentPanel::RenderShapeSettings(Scene::Entity entity,
        Physics::BodyCreationSettings& creationSettings) {

        auto& shape = creationSettings.shape;

        if (shape->type == Physics::ShapeType::HeightField) {
            ImGui::TextDisabled("Body uses a height field shape");
            return;
        }

        std::vector<ShapeType> types = { ShapeType::Mesh, ShapeType::ConvexHull, ShapeType::Sphere,
            ShapeType::BoundingBox, ShapeType::Capsule, ShapeType::Cylinder, ShapeType::Compound };

        auto previousType = shape->type;
        if (ImGui::BeginCombo("Shape type", GetShapeTypeName(shape->type))) {
            for (auto type : types) {
                auto isSelected = type == shape->type;
                if (ImGui::Selectable(GetShapeTypeName(type), isSelected))
                    shape->type = type;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        bool shapeTypeChanged = shape->type != previousType;

        auto meshComponent = entity.TryGetComponent<MeshComponent>();
        auto transformComponent = entity.TryGetComponent<TransformComponent>();

        glm::vec3* scale = nullptr;
        float* density = nullptr;

        if (shape->type == ShapeType::Mesh) {
            if (shapeTypeChanged)
                shape->settings = CreateRef<MeshShapeSettings>();

            auto meshSettings = static_cast<MeshShapeSettings*>(shape->settings.get());

            bool resourceChanged = false;
            meshSettings->mesh = meshSelectionPanel.Render(meshSettings->mesh, resourceChanged);

            if (meshComponent)
                if (ImGui::Button("Take resource from mesh component", ImVec2(-FLT_MIN, 0.0f)))
                    meshSettings->mesh = meshComponent->mesh;

            scale = &meshSettings->scale;
        }
        else if (shape->type == ShapeType::Sphere) {
            if (shapeTypeChanged)
                shape->settings = CreateRef<SphereShapeSettings>();

            auto sphereSettings = static_cast<SphereShapeSettings*>(shape->settings.get());

            ImGui::DragFloat("Radius", &sphereSettings->radius, 0.1f, 0.0f);
            if (meshComponent && meshComponent->mesh.IsLoaded())
                if (ImGui::Button("Take radius from mesh component"))
                    sphereSettings->radius = meshComponent->mesh->data.radius;

            density = &sphereSettings->density;
            scale = &sphereSettings->scale;
        }
        else if (shape->type == ShapeType::BoundingBox) {
            if (shapeTypeChanged)
                shape->settings = CreateRef<BoundingBoxShapeSettings>();

            auto boundingBoxSettings = static_cast<BoundingBoxShapeSettings*>(shape->settings.get());

            Widgets::DragVec3("Min", boundingBoxSettings->aabb.min, 0.1f, 0.0f, -10000.0f, 10000.0f);
            Widgets::DragVec3("Max", boundingBoxSettings->aabb.max, 0.1f, 0.0f, -10000.0f, 10000.0f);
            if (meshComponent && meshComponent->mesh.IsLoaded())
                if (ImGui::Button("Take bounding box from mesh component"))
                    boundingBoxSettings->aabb = meshComponent->mesh->data.aabb;

            density = &boundingBoxSettings->density;
            scale = &boundingBoxSettings->scale;
        }
        else if (shape->type == ShapeType::Capsule) {
            if (shapeTypeChanged)
                shape->settings = CreateRef<CapsuleShapeSettings>();

            auto capsuleSettings = static_cast<CapsuleShapeSettings*>(shape->settings.get());

            ImGui::DragFloat("Radius", &capsuleSettings->radius, 0.1f, 0.01f, 100.0f);
            ImGui::DragFloat("Height", &capsuleSettings->height, 0.1f, 0.01f, 100.0f);

            density = &capsuleSettings->density;
            scale = &capsuleSettings->scale;
        }
        else if (shape->type == ShapeType::ConvexHull) {
            if (shapeTypeChanged)
                shape->settings = CreateRef<ConvexHullShapeSettings>();

            auto convexHullSettings = static_cast<ConvexHullShapeSettings*>(shape->settings.get());

            bool resourceChanged = false;
            convexHullSettings->mesh = meshSelectionPanel.Render(convexHullSettings->mesh, resourceChanged);

            if (meshComponent)
                if (ImGui::Button("Take resource from mesh component", ImVec2(-FLT_MIN, 0.0f)))
                    convexHullSettings->mesh = meshComponent->mesh;

            density = &convexHullSettings->density;
            scale = &convexHullSettings->scale;
        }
        else if (shape->type == ShapeType::Cylinder) {
            if (shapeTypeChanged)
                shape->settings = CreateRef<CylinderShapeSettings>();

            auto cylinderSettings = static_cast<CylinderShapeSettings*>(shape->settings.get());

            ImGui::DragFloat("Radius", &cylinderSettings->radius, 0.1f, 0.01f, 100.0f);
            ImGui::DragFloat("Height", &cylinderSettings->height, 0.1f, 0.01f, 100.0f);

            density = &cylinderSettings->density;
            scale = &cylinderSettings->scale;
        }
        else if (shape->type == ShapeType::Compound) {
            if (shapeTypeChanged) {
                auto compoundSettings = CreateRef<CompoundShapeSettings>();
                compoundSettings->children.push_back({ ShapesManager::CreateShape(BoundingBoxShapeSettings {}) });
                shape->settings = compoundSettings;
            }

            auto compoundSettings = static_cast<CompoundShapeSettings*>(shape->settings.get());

            RenderCompoundSettings(*compoundSettings);

            scale = &compoundSettings->scale;
        }

        if (density != nullptr) {
            ImGui::DragFloat("Density", density, 0.01f, 0.001f);
        }

        if (scale != nullptr) {
            if (transformComponent) {
                auto& globalMatrix = transformComponent->globalMatrix;
                // Need scale from global matrix decomposition
                *scale = Common::MatrixDecomposition(globalMatrix).scale;
            }
            Widgets::DragVec3("Scale", *scale, 0.01f, 1.0f, -100.0f, 100.0f);
        }

        if (ImGui::Button("Generate shape", { -FLT_MIN, 0 }) || shapeTypeChanged)
            shape->TryCreate();
        else if (shape->type != Physics::ShapeType::HeightField)
            shape->Scale(*scale);

    }

    void RigidBodyComponentPanel::RenderBodySettings(Scene::Entity entity,
        Physics::BodyCreationSettings &settings) {

        // Mesh shapes can't be simulated, but they can be moved kinematically
        std::vector<Physics::ObjectLayer> layers = { Physics::Layers::Static, Physics::Layers::Kinematic };
        if (settings.shape->type != ShapeType::Mesh)
            layers.insert(layers.begin() + 1, Physics::Layers::Movable);

        auto layerName = [](Physics::ObjectLayer layer) {
            switch (layer) {
                case Physics::Layers::Movable: return "Movable";
                case Physics::Layers::Kinematic: return "Kinematic";
                default: return "Static";
            }
        };

        if (std::find(layers.begin(), layers.end(), settings.objectLayer) == layers.end())
            settings.objectLayer = Physics::Layers::Static;

        if (ImGui::BeginCombo("Object layer", layerName(settings.objectLayer))) {
            for (auto layer : layers) {
                auto isSelected = layer == settings.objectLayer;
                if (ImGui::Selectable(layerName(layer), isSelected))
                    settings.objectLayer = layer;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        const char* motionQualityItems[] = { "Discrete", "Linear cast" };
        int currentItem = static_cast<int>(settings.motionQuality);
        ImGui::Combo("Motion quality", &currentItem, motionQualityItems, IM_ARRAYSIZE(motionQualityItems));
        settings.motionQuality = static_cast<Physics::MotionQuality>(currentItem);

        Widgets::DragVec3("Linear velocity", settings.linearVelocity, 0.1f, 0.0f, -1000.0f, 1000.0f);
        Widgets::DragVec3("Angular velocity", settings.angularVelocity, 0.1f, 0.0f, -1000.0f, 1000.0f);

        ImGui::DragFloat("Friction", &settings.friction, 0.01f, -0.0f, 1.0f);
        ImGui::DragFloat("Restitution", &settings.restitution, 0.01f, -0.0f, 1.0f);

        ImGui::DragFloat("Linear dampening", &settings.linearDampening, 0.01f, -0.0f, 1.0f);
        ImGui::DragFloat("Angular dampening", &settings.angularDampening, 0.01f, -0.0f, 1.0f);

        ImGui::DragFloat("Gravity factor", &settings.gravityFactor, 0.01f, -0.0f, 1.0f);

        ImGui::Checkbox("Trigger", &settings.isTrigger);
        ImGui::SetItemTooltip("Reports contacts to scripts without colliding");

    }

    void RigidBodyComponentPanel::RenderCompoundSettings(Physics::CompoundShapeSettings& compoundSettings) {

        std::vector<ShapeType> types = { ShapeType::Sphere, ShapeType::BoundingBox, ShapeType::Capsule, ShapeType::Cylinder };

        auto& children = compoundSettings.children;
        int32_t removeIdx = -1;

        for (size_t i = 0; i < children.size(); i++) {
            auto& child = children[i];
            if (!child.shape)
                continue;

            ImGui::PushID(int32_t(i));

            if (Widgets::SectionHeader(("Child " + std::to_string(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                auto previousType = child.shape->type;
                if (ImGui::BeginCombo("Shape type", GetShapeTypeName(child.shape->type))) {
                    for (auto type : types) {
                        auto isSelected = type == child.shape->type;
                        if (ImGui::Selectable(GetShapeTypeName(type), isSelected))
                            child.shape->type = type;
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (child.shape->type != previousType) {
                    if (child.shape->type == ShapeType::Sphere)
                        child.shape->settings = CreateRef<SphereShapeSettings>();
                    else if (child.shape->type == ShapeType::BoundingBox)
                        child.shape->settings = CreateRef<BoundingBoxShapeSettings>();
                    else if (child.shape->type == ShapeType::Capsule)
                        child.shape->settings = CreateRef<CapsuleShapeSettings>();
                    else if (child.shape->type == ShapeType::Cylinder)
                        child.shape->settings = CreateRef<CylinderShapeSettings>();
                }

                if (child.shape->type == ShapeType::Sphere) {
                    auto sphereSettings = static_cast<SphereShapeSettings*>(child.shape->settings.get());
                    ImGui::DragFloat("Radius", &sphereSettings->radius, 0.1f, 0.01f, 100.0f);
                    ImGui::DragFloat("Density", &sphereSettings->density, 0.01f, 0.001f);
                }
                else if (child.shape->type == ShapeType::BoundingBox) {
                    auto boundingBoxSettings = static_cast<BoundingBoxShapeSettings*>(child.shape->settings.get());
                    Widgets::DragVec3("Min", boundingBoxSettings->aabb.min, 0.1f, 0.0f, -10000.0f, 10000.0f);
                    Widgets::DragVec3("Max", boundingBoxSettings->aabb.max, 0.1f, 0.0f, -10000.0f, 10000.0f);
                    ImGui::DragFloat("Density", &boundingBoxSettings->density, 0.01f, 0.001f);
                }
                else if (child.shape->type == ShapeType::Capsule) {
                    auto capsuleSettings = static_cast<CapsuleShapeSettings*>(child.shape->settings.get());
                    ImGui::DragFloat("Radius", &capsuleSettings->radius, 0.1f, 0.01f, 100.0f);
                    ImGui::DragFloat("Height", &capsuleSettings->height, 0.1f, 0.01f, 100.0f);
                    ImGui::DragFloat("Density", &capsuleSettings->density, 0.01f, 0.001f);
                }
                else if (child.shape->type == ShapeType::Cylinder) {
                    auto cylinderSettings = static_cast<CylinderShapeSettings*>(child.shape->settings.get());
                    ImGui::DragFloat("Radius", &cylinderSettings->radius, 0.1f, 0.01f, 100.0f);
                    ImGui::DragFloat("Height", &cylinderSettings->height, 0.1f, 0.01f, 100.0f);
                    ImGui::DragFloat("Density", &cylinderSettings->density, 0.01f, 0.001f);
                }

                Widgets::DragVec3("Translation", child.translation, 0.01f, 0.0f, -10000.0f, 10000.0f);

                auto rotation = glm::degrees(glm::eulerAngles(child.rotation));
                if (Widgets::DragVec3("Rotation", rotation, 0.5f, 0.0f, -360.0f, 360.0f))
                    child.rotation = quat(glm::radians(rotation));

                if (ImGui::Button("Remove child", { -FLT_MIN, 0 }))
                    removeIdx = int32_t(i);
            }

            ImGui::PopID();
        }

        if (removeIdx >= 0 && children.size() > 1)
            children.erase(children.begin() + removeIdx);

        if (ImGui::Button("Add child", { -FLT_MIN, 0 }))
            children.push_back({ ShapesManager::CreateShape(BoundingBoxShapeSettings {}) });

    }

    const char* RigidBodyComponentPanel::GetShapeTypeName(Physics::ShapeType type) const {

        switch (type) {
            case ShapeType::Mesh: return "Mesh";
            case ShapeType::Sphere: return "Sphere";
            case ShapeType::BoundingBox: return "Bounding box";
            case ShapeType::Capsule: return "Capsule";
            case ShapeType::HeightField: return "Height field";
            case ShapeType::ConvexHull: return "Convex hull";
            case ShapeType::Cylinder: return "Cylinder";
            case ShapeType::Compound: return "Compound";
            default: return "Unknown";
        }

    }

}