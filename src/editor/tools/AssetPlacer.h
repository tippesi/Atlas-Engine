#pragma once

#include "../Content.h"

#include "audio/AudioData.h"
#include "mesh/Mesh.h"
#include "renderer/PrimitiveBatch.h"
#include "resource/Resource.h"
#include "scene/Scene.h"
#include "volume/Ray.h"

#include <optional>
#include <string>
#include <vector>

namespace Atlas::Editor {

    // Makes entities of assets which are dragged into a scene: meshes and models, prefabs, audio, scripts and fonts.
    // Meshes are placed once they are loaded, since they stand on their bounds
    class AssetPlacer {

    public:
        struct Surface {
            vec3 position = vec3(0.0f);
            vec3 normal = vec3(0.0f, 1.0f, 0.0f);
            vec3 viewDirection = vec3(0.0f, 0.0f, -1.0f);
        };

        static bool CanPlace(const std::string& path);

        // What the ray hits, the ground plane or a point in front of the camera
        static Surface FindSurface(Scene::Scene& scene, Volume::Ray ray, bool alignToNormal);

        void Place(const std::string& path, const Surface& surface, Scene::Entity parent);

        // Where the asset would be, with the bounds of a mesh once it is loaded
        void RenderPreview(Renderer::PrimitiveBatch& batch, const std::string& path, const Surface& surface);

        void EndPreview();

        // The name of the next asset which is loaded. Failed ones are dropped with a notification
        std::optional<std::string> GetNextName();

        // Creates the entity of the next asset, an invalid one if a prefab can't be loaded
        Scene::Entity CreateNext(const Ref<Scene::Scene>& scene);

    private:
        struct Asset {
            std::string path;
            ContentType type;
            Surface surface;
            Scene::Entity parent;
            ResourceHandle<Mesh::Mesh> mesh;
        };

        static ContentType GetType(const std::string& path);

        // Rotates the up axis to the normal of the surface
        static mat4 GetSurfaceMatrix(const Surface& surface);

        // The foot of the bounds stands on the surface. Meshes with an origin near their foot, e.g. trees with roots
        // below it, stand on the origin
        static mat4 GetMeshMatrix(const Volume::AABB& aabb, const Surface& surface);

        std::vector<Asset> assets;

        // Kept while it is dragged, failed ones would be loaded again and again
        std::string previewPath;
        ResourceHandle<Mesh::Mesh> previewMesh;

    };

}
