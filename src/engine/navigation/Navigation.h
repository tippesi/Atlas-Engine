#pragma once

#include "NavigationMesh.h"

namespace Atlas {

    namespace Navigation {

        /**
         * The navigation of a scene: a navigation mesh which the scene update builds around the main camera.
         * The mesh is created on the first update and again when the settings change.
         */
        class Navigation {

        public:
            void Update(Scene::Scene* scene, vec3 center);

            bool enable = true;
            // Tiles are built within this distance around the main camera
            float radius = 100.0f;
            NavigationSettings settings;

            // Per update and at the same time, see NavigationMesh. They don't rebuild the mesh
            int32_t maxTileStartsPerUpdate = 2;
            int32_t maxConcurrentBuilds = 8;
            JobPriority buildPriority = JobPriority::Medium;

            Ref<NavigationMesh> mesh = nullptr;

        };

    }

}
