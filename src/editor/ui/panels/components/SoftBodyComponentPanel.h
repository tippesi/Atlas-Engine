#pragma once

#include "../Panel.h"
#include "../../../tools/PrimitiveBatchWrapper.h"

#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/components/SoftBodyComponent.h"
#include "Viewport.h"

namespace Atlas::Editor::UI {

    class SoftBodyComponentPanel : public Panel {

    public:
        SoftBodyComponentPanel() : Panel("Soft body component") {}

        bool Render(const Ref<Scene::Scene>& scene, Scene::Entity entity, SoftBodyComponent& softBodyComponent);

        // Edges and pinned vertices of the simulation mesh of the entity with the pin bounds of its soft body
        void RenderOverlay(const Ref<Scene::Scene>& scene, Scene::Entity entity, PrimitiveBatchWrapper& wrapper);

        // Inside of the viewport, moves and resizes the selected pin bounds. Returns false without a selected pin
        bool RenderPinGizmo(const Ref<Scene::Scene>& scene, Scene::Entity entity, const Ref<Viewport>& viewport,
            const CameraComponent& camera);

        bool showSimulationMesh = false;

    private:
        // Of the entity the panel was rendered for last
        int32_t selectedPin = -1;
        ECS::Entity selectedPinEntity = ECS::EntityConfig::InvalidEntity;

        bool pendingReset = false;

        Ref<Mesh::SoftBodySimulationMesh> drawnSimulationMesh;
        std::vector<std::pair<uint32_t, uint32_t>> simulationMeshEdges;

    };

}
