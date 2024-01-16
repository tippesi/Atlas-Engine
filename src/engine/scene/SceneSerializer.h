#pragma once

#include "Scene.h"

namespace Atlas {

    namespace Scene {

        class SceneSerializer {

        public:
            SceneSerializer() = default;
            SceneSerializer(const SceneSerializer& that) = default;
            explicit SceneSerializer(Ref<Scene> scene) : scene(scene) {}

            void SerializeScene(const std::string& filename);

            void DeserializeScene(const std::string& filename);

            Ref<Scene> GetScene() const;

        private:
            void SerializeEntity(Entity entity);

            void DeserializeEntity();

            Ref<Scene> scene;

        };

    };


}