#pragma once

#include "Panel.h"
#include "scene/Scene.h"

#include <optional>

namespace Atlas::Editor::UI {

    class SceneStatisticsPanel : public Panel {

    public:
        SceneStatisticsPanel() : Panel("Scene statistics") {}

        void Render(Ref<Scene::Scene> scene);

    private:
        void Collect(const Ref<Scene::Scene>& scene);

        void GrowBounds(const Ref<Scene::Scene>& scene);

        // Collecting everything takes milliseconds in large scenes. Each frame collects one of the values and a part
        // of the bounds, such that the cost is the same in every frame
        const size_t boundsComponentsPerFrame = 32768;

        // Doesn't keep a closed scene alive, a new scene at the same address is still a new one
        std::weak_ptr<Scene::Scene> collectedScene;
        int32_t collectionStep = 0;

        Volume::AABB bounds;
        Volume::AABB growingBounds;
        size_t boundsIndex = 0;
        size_t entityCount = 0;
        size_t meshCount = 0;
        size_t materialCount = 0;
        size_t lightCount = 0;
        size_t meshInstanceCount = 0;
        std::optional<Physics::WorldStatistics> physicsStatistics;

    };

}