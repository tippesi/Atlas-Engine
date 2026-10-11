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

    }

}