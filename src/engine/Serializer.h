#pragma once

#include "scene/Scene.h"
#include "common/SerializationHelper.h"

namespace Atlas {

    class Serializer {

    public:
        // False if the scene file couldn't be written, the previous one is unchanged then
        static bool SerializeScene(Ref<Scene::Scene> scene, const std::string& filename, bool saveDependencies,
            bool binaryJson = false, bool formatJson = false);

        static Ref<Scene::Scene> DeserializeScene(const std::string& filename, bool binaryJson = false);

        static bool SerializePrefab(Ref<Scene::Scene> scene, Scene::Entity entity, const std::string& filename, bool formatJson = false);

        static Scene::Entity DeserializePrefab(Ref<Scene::Scene> scene, const std::string& filename);

        /**
         * Converts the entity table and component sections of a scene json into nested entities like in prefabs,
         * e.g. for tools that copy entities out of a scene. Scenes with nested entities stay unchanged.
         */
        static void NestSceneEntities(json& j);

    private:
        static void SaveDependencies(Ref<Scene::Scene> scene, bool multithreaded = true);

    };

}