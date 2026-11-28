#pragma once

#include "scene/Scene.h"
#include "loader/ModelImporter.h"

namespace Atlas::Editor {

    class DataCreator {

    public:
        static Ref<Scene::Scene> CreateScene(const std::string& name, vec3 min, vec3 max, int32_t depth);

        // Lights a single asset in an editor, e.g. a mesh or material. The probes only cover the bounds of the asset
        static Ref<Scene::Scene> CreatePreviewScene(const std::string& name, const Volume::AABB& bounds);

        static Ref<Scene::Scene> CreateSceneFromMesh(const std::string& filename,vec3 min, vec3 max, int32_t depth,
            bool invertUVs, bool addRigidBodies, bool makeMeshesStatic, Loader::ImportTextureFormat textureFormat);

        // A unit sphere with smooth normals, e.g. to look at a material. Enough rings and segments show the displacement of a graph
        static Ref<Mesh::Mesh> CreateSphere(const ResourceHandle<Material>& material, int32_t rings, int32_t segments);

    private:
        static void AddDirectionalLight(Ref<Scene::Scene>& scene, Scene::Components::HierarchyComponent& parent, vec3 direction);

    };

}