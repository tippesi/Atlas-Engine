#pragma once

#include "scene/Scene.h"
#include "loader/ModelImporter.h"

namespace Atlas::Editor {

    class DataCreator {

    public:
        static Ref<Scene::Scene> CreateScene(const std::string& name, vec3 min, vec3 max, int32_t depth);

        static Ref<Scene::Scene> CreateSceneFromMesh(const std::string& filename,vec3 min, vec3 max, int32_t depth,
            bool invertUVs, bool addRigidBodies, bool makeMeshesStatic, Loader::ImportTextureFormat textureFormat);

        // A unit sphere with smooth normals, e.g. to look at a material. Enough rings and segments show the displacement of a graph
        static Ref<Mesh::Mesh> CreateSphere(const ResourceHandle<Material>& material, int32_t rings, int32_t segments);

    };

}