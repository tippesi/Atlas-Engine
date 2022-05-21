#ifndef AE_SCENESERIALIZER_H
#define AE_SCENESERIALIZER_H

#include "Scene.h"
#include "../common/Serializer.h"

namespace Atlas {

    namespace NewScene {

        class SceneSerializer {

        public:
            SceneSerializer() = default;

            SceneSerializer(Ref<Scene> scene) : scene(scene) {}

            SceneSerializer(const SceneSerializer& that) = default;

            void SerializeScene();

            void DeserializeScene();

            Ref<Scene> GetScene() const;

        private:
            void SerializeEntity(Entity entity);

            void DeserializeEntity();

            Ref<Scene> scene;

        };

    };


}

#endif