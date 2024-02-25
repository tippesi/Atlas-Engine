#pragma once

#include "Scene.h"

namespace Atlas {

    namespace Scene {

        class SceneSerializer {

        public:
            static void SerializeScene(Ref<Scene> scene, const std::string& filename);

            static Ref<Scene> DeserializeScene(const std::string& filename);

            static void SerializePrefab(Ref<Scene> scene, Entity entity, const std::string& filename);

            static Entity DeserializePrefab(Ref<Scene> scene, const std::string& filename);

        };

    };


}