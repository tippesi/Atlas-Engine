#include "Navigation.h"

namespace Atlas {

    namespace Navigation {

        void Navigation::Update(Scene::Scene* scene, vec3 center) {

            if (!mesh || mesh->settings != settings)
                mesh = CreateRef<NavigationMesh>(settings);

            mesh->maxTileStartsPerUpdate = maxTileStartsPerUpdate;
            mesh->maxConcurrentBuilds = maxConcurrentBuilds;
            mesh->buildPriority = buildPriority;

            mesh->Update(scene, center, radius);

        }

    }

}
