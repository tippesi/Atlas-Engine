#include "AssetPlacer.h"
#include "../FileImporter.h"
#include "../Notifications.h"

#include "Serializer.h"
#include "common/Path.h"
#include "scene/components/LuaScriptComponent.h"

#include <glm/gtx/quaternion.hpp>

#include <algorithm>

namespace Atlas::Editor {

    bool AssetPlacer::CanPlace(const std::string& path) {

        switch (GetType(path)) {
        case ContentType::Mesh:
        case ContentType::MeshSource:
        case ContentType::Prefab:
        case ContentType::Audio:
        case ContentType::Script:
        case ContentType::Font: return true;
        default: return false;
        }

    }

    AssetPlacer::Surface AssetPlacer::FindSurface(Scene::Scene& scene, Volume::Ray ray, bool alignToNormal) {

        const float maxGroundDistance = 500.0f;
        const float defaultDistance = 10.0f;

        auto result = scene.CastRay(ray);
        if (result.valid) {
            Surface surface { .position = ray.Get(result.hitDistance), .viewDirection = ray.direction };
            if (alignToNormal && result.IsNormalValid())
                surface.normal = result.normal;
            return surface;
        }

        auto groundDistance = ray.direction.y != 0.0f ? -ray.origin.y / ray.direction.y : -1.0f;
        auto hitsGround = groundDistance > 0.0f && groundDistance <= maxGroundDistance;
        return { .position = ray.Get(hitsGround ? groundDistance : defaultDistance), .viewDirection = ray.direction };

    }

    void AssetPlacer::Place(const std::string& path, const Surface& surface, Scene::Entity parent) {

        Asset asset { .path = path, .type = GetType(path), .surface = surface, .parent = parent };
        if (asset.type == ContentType::Mesh || asset.type == ContentType::MeshSource)
            asset.mesh = FileImporter::ImportFile<Mesh::Mesh>(path);
        assets.push_back(asset);

        if (asset.mesh.IsValid() && !asset.mesh.IsLoaded() && asset.type == ContentType::MeshSource)
            Notifications::Push({ .message = "Importing " + Common::Path::GetFileName(path) + ", it's placed afterwards" });

    }

    void AssetPlacer::RenderPreview(Renderer::PrimitiveBatch& batch, const std::string& path, const Surface& surface) {

        const vec3 color(1.0f, 0.8f, 0.2f);
        batch.AddLineCross(surface.position, 0.25f, color);

        if (previewPath != path) {
            previewPath = path;
            // Models are only imported once they are dropped
            previewMesh = GetType(path) == ContentType::Mesh ? FileImporter::ImportFile<Mesh::Mesh>(path) : ResourceHandle<Mesh::Mesh>();
        }
        if (!previewMesh.IsLoaded())
            return;

        const auto& aabb = previewMesh->data.aabb;
        auto matrix = GetMeshMatrix(aabb, surface);
        auto getCorner = [&](int32_t i) {
            return vec3(matrix * vec4(i & 1 ? aabb.max.x : aabb.min.x, i & 2 ? aabb.max.y : aabb.min.y,
                i & 4 ? aabb.max.z : aabb.min.z, 1.0f));
        };
        for (int32_t i = 0; i < 8; i++) {
            for (auto axis : { 1, 2, 4 })
                if (!(i & axis))
                    batch.AddLine(getCorner(i), getCorner(i | axis), color, color);
        }

    }

    void AssetPlacer::EndPreview() {

        previewPath.clear();
        previewMesh = ResourceHandle<Mesh::Mesh>();

    }

    std::optional<std::string> AssetPlacer::GetNextName() {

        std::erase_if(assets, [](const Asset& asset) {
            if (!asset.mesh.IsValid() || !asset.mesh.GetResource()->errorOnLoad)
                return false;
            Notifications::Push({ .message = "Couldn't load " + asset.path + ": " + asset.mesh.GetResource()->errorMessageOnLoad,
                .type = NotificationType::Error });
            return true;
            });

        // Loaded ones don't wait for models which are still imported
        auto next = std::find_if(assets.begin(), assets.end(), [](const Asset& asset) {
            return !asset.mesh.IsValid() || asset.mesh.IsLoaded(); });
        if (next == assets.end())
            return {};

        std::rotate(assets.begin(), next, next + 1);
        return Common::Path::GetFileNameWithoutExtension(assets.front().path);

    }

    Scene::Entity AssetPlacer::CreateNext(const Ref<Scene::Scene>& scene) {

        auto asset = assets.front();
        assets.erase(assets.begin());

        auto parent = asset.parent.IsValid() ? asset.parent : scene->GetRootEntity();
        auto matrix = asset.mesh.IsValid() ? GetMeshMatrix(asset.mesh->data.aabb, asset.surface) : GetSurfaceMatrix(asset.surface);
        // Texts face the camera
        if (asset.type == ContentType::Font)
            matrix *= glm::mat4_cast(glm::angleAxis(std::atan2(-asset.surface.viewDirection.x, -asset.surface.viewDirection.z),
                vec3(0.0f, 1.0f, 0.0f)));

        if (asset.type == ContentType::Prefab) {
            Scene::Entity entity;
            try {
                entity = Serializer::DeserializePrefab(scene, asset.path);
            }
            catch (const std::exception& exception) {
                Notifications::Push({ .message = "Couldn't load " + asset.path + ": " + exception.what(),
                    .type = NotificationType::Error });
                return {};
            }
            entity.SetParent(parent);
            // It was the root of its own hierarchy
            if (auto hierarchy = entity.TryGetComponent<HierarchyComponent>())
                hierarchy->root = false;
            // The prefab keeps its rotation and scale
            if (auto transform = entity.TryGetComponent<TransformComponent>()) {
                auto prefabMatrix = transform->matrix;
                prefabMatrix[3] = vec4(0.0f, 0.0f, 0.0f, 1.0f);
                transform->SetGlobal(matrix * prefabMatrix, parent);
            }
            return entity;
        }

        auto entity = scene->CreateEntity();
        entity.AddComponent<NameComponent>(Common::Path::GetFileNameWithoutExtension(asset.path));
        entity.SetParent(parent);
        auto& transform = entity.AddComponent<TransformComponent>(matrix, false);
        transform.SetGlobal(matrix, parent);

        switch (asset.type) {
        case ContentType::Audio:
            entity.AddComponent<AudioComponent>(FileImporter::ImportFile<Audio::AudioData>(asset.path));
            break;
        case ContentType::Script:
            entity.AddComponent<LuaScriptComponent>(FileImporter::ImportFile<Scripting::Script>(asset.path));
            break;
        case ContentType::Font: {
            auto& text = entity.AddComponent<TextComponent>(FileImporter::ImportFile<Font>(asset.path), "Text");
            // The box of the text stands on the surface
            text.halfSize = vec2(2.0f, 1.0f);
            text.position.y = text.halfSize.y;
            break;
        }
        default:
            entity.AddComponent<MeshComponent>(asset.mesh);
            break;
        }
        return entity;

    }

    ContentType AssetPlacer::GetType(const std::string& path) {

        auto mapping = Content::contentTypeMapping.find(Common::Path::ToLower(Common::Path::GetFileType(path)));
        return mapping != Content::contentTypeMapping.end() ? mapping->second : ContentType::None;

    }

    mat4 AssetPlacer::GetSurfaceMatrix(const Surface& surface) {

        auto rotation = glm::rotation(vec3(0.0f, 1.0f, 0.0f), surface.normal);
        return glm::translate(surface.position) * glm::mat4_cast(rotation);

    }

    mat4 AssetPlacer::GetMeshMatrix(const Volume::AABB& aabb, const Surface& surface) {

        auto foot = glm::clamp(vec3(0.0f), aabb.min, aabb.max);
        auto isNearFoot = aabb.min.y <= 0.0f && -aabb.min.y <= 0.25f * (aabb.max.y - aabb.min.y);
        foot.y = isNearFoot ? 0.0f : aabb.min.y;
        return GetSurfaceMatrix(surface) * glm::translate(-foot);

    }

}
