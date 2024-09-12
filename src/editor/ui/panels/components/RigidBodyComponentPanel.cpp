#include "RigidBodyComponentPanel.h"

namespace Atlas::Editor::UI {

    using namespace Physics;

    bool RigidBodyComponentPanel::Render(const Ref<Scene::Scene>& scene,
        Scene::Entity entity, RigidBodyComponent &rigidBodyComponent) {

        auto creationSettings = rigidBodyComponent.GetBodyCreationSettings();

        ImGui::Text("Shape");

        RenderShapeSettings(entity, creationSettings);

        ImGui::Separator();

        ImGui::Text("Body");

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

        const char* typeItems[] = { "Mesh", "Sphere", "Bounding box" };
        int currentItem = static_cast<int>(shape->type);
        int previousItem = currentItem;
        ImGui::Combo("Shape type", &currentItem, typeItems, IM_ARRAYSIZE(typeItems));
        shape->type = static_cast<ShapeType>(currentItem);

        bool shapeTypeChanged = currentItem != previousItem;

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
                if (ImGui::Button("Take resource from mesh component"))
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

            ImGui::DragFloat3("Min", glm::value_ptr(boundingBoxSettings->aabb.min), 0.1f, -10000.0f, 10000.0f);
            ImGui::DragFloat3("Max", glm::value_ptr(boundingBoxSettings->aabb.max), 0.1f, -10000.0f, 10000.0f);
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

        if (density != nullptr) {
            ImGui::DragFloat("Density", density, 0.01f, 0.001f);
        }

        if (scale != nullptr) {
            if (transformComponent) {
                auto& globalMatrix = transformComponent->globalMatrix;
                // Need scale from global matrix decomposition
                *scale = Common::MatrixDecomposition(globalMatrix).scale;
            }
            ImGui::DragFloat3("Scale", glm::value_ptr(*scale), 0.01f, -100.0f, 100.0f);
        }

        if (ImGui::Button("Generate shape", { -FLT_MIN, 0 }) || shapeTypeChanged)
            shape->TryCreate();
        else
            shape->Scale(*scale);

    }

    void RigidBodyComponentPanel::RenderBodySettings(Scene::Entity entity,
        Physics::BodyCreationSettings &settings) {

        const char* layerItems[] = { "Static", "Movable" };
        int currentItem = settings.objectLayer;
        // Mesh can only be static
        auto layoutItemCount = settings.shape->type != ShapeType::Mesh ? IM_ARRAYSIZE(layerItems) : 1;
        ImGui::Combo("Object layer", &currentItem, layerItems, layoutItemCount);
        settings.objectLayer = std::min(currentItem, layoutItemCount - 1);

        const char* motionQualityItems[] = { "Discrete", "Linear cast" };
        currentItem = static_cast<int>(settings.motionQuality);
        ImGui::Combo("Motion quality", &currentItem, motionQualityItems, IM_ARRAYSIZE(motionQualityItems));
        settings.motionQuality = static_cast<Physics::MotionQuality>(currentItem);

        ImGui::DragFloat3("Linear velocity", glm::value_ptr(settings.linearVelocity), 0.1f, -1000.0f, 1000.0f);
        ImGui::DragFloat3("Angular velocity", glm::value_ptr(settings.angularVelocity), 0.1f, -1000.0f, 1000.0f);

        ImGui::DragFloat("Friction", &settings.friction, 0.01f, -0.0f, 1.0f);
        ImGui::DragFloat("Restitution", &settings.restitution, 0.01f, -0.0f, 1.0f);

        ImGui::DragFloat("Linear dampening", &settings.linearDampening, 0.01f, -0.0f, 1.0f);
        ImGui::DragFloat("Angular dampening", &settings.angularDampening, 0.01f, -0.0f, 1.0f);

        ImGui::DragFloat("Gravity factor", &settings.gravityFactor, 0.01f, -0.0f, 1.0f);

    }

}