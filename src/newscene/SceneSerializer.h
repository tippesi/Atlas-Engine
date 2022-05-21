#ifndef AE_SCENESERIALIZER_H
#define AE_SCENESERIALIZER_H

#include "Scene.h"

#include <YAML/include/yaml-cpp/yaml.h>

namespace Atlas {

    namespace NewScene {

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

            void DeserializeEntity(const YAML::Node& node);

            Ref<Scene> scene;

        };

    };


}

#endif