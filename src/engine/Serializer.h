#pragma once

#include "scene/Scene.h"

namespace Atlas {

    class Serializer {

    public:
        static void SerializeScene(Ref<Scene::Scene> scene, const std::string& filename, bool saveDependencies, 
            bool binaryJson = false, bool formatJson = false);

        static Ref<Scene::Scene> DeserializeScene(const std::string& filename, bool binaryJson = false);

        static void SerializePrefab(Ref<Scene::Scene> scene, Scene::Entity entity, const std::string& filename, bool formatJson = false);

        static Scene::Entity DeserializePrefab(Ref<Scene::Scene> scene, const std::string& filename);

    private:
        static void SaveDependencies(Ref<Scene::Scene> scene, bool multithreaded = true);

    };

}