#include "SceneTools.h"
#include "Image.h"
#include "TerrainTools.h"

#include "scene/SceneSerializer.h"
#include "scene/components/ComponentSerializer.h"
#include "scene/components/LuaScriptComponent.h"
#include "lighting/LightingSerializer.h"
#include "postprocessing/PostProcessingSerializer.h"
#include "mesh/MeshSerializer.h"
#include "resource/ResourceManager.h"
#include "common/MathHelper.h"

#include <cfloat>
#include <optional>
#include <set>
#include <stdexcept>

namespace Atlas::Capture {

    using namespace Scene::Components;

    static json HitToJson(const RaycastHit& hit) {

        if (!hit.valid)
            return json{ { "hit", false } };

        return json{
            { "material", GetHitMaterial(hit) },
            { "hit", true },
            { "distance", Round(hit.distance) },
            { "position", FromVec3(hit.position) },
            { "normal", FromVec3(hit.normal) },
            { "kind", hit.kind },
            { "entity", hit.entity.IsValid() ? GetEntityLabel(hit.entity) : "" }
        };

    }

    static json BoundsToJson(const Volume::AABB& aabb) {

        return json{ { "min", FromVec3(aabb.min) }, { "max", FromVec3(aabb.max) }, { "size", FromVec3(aabb.GetSize()) } };

    }

    static EntitySet GetIgnoredEntities(Scene::Scene& scene, const json& query) {

        return CollectHierarchies(scene, query.value("ignore", std::vector<std::string>()));

    }

    static std::vector<std::string> GetComponentNames(Scene::Entity entity) {

        std::vector<std::string> names;
        if (entity.HasComponent<NameComponent>())
            names.push_back("Name");
        if (entity.HasComponent<TransformComponent>())
            names.push_back("Transform");
        if (entity.HasComponent<HierarchyComponent>())
            names.push_back("Hierarchy");
        if (entity.HasComponent<MeshComponent>())
            names.push_back("Mesh");
        if (entity.HasComponent<CameraComponent>())
            names.push_back("Camera");
        if (entity.HasComponent<LightComponent>())
            names.push_back("Light");
        if (entity.HasComponent<RigidBodyComponent>())
            names.push_back("RigidBody");
        if (entity.HasComponent<PlayerComponent>())
            names.push_back("Player");
        if (entity.HasComponent<LuaScriptComponent>())
            names.push_back("LuaScript");
        if (entity.HasComponent<AudioComponent>())
            names.push_back("Audio");
        if (entity.HasComponent<AudioVolumeComponent>())
            names.push_back("AudioVolume");
        if (entity.HasComponent<TextComponent>())
            names.push_back("Text");
        if (entity.HasComponent<SplineComponent>())
            names.push_back("Spline");
        return names;

    }

    static json ComponentDetails(Scene::Entity entity) {

        json j;
        if (auto comp = entity.TryGetComponent<TransformComponent>())
            j["transform"] = *comp;
        if (auto comp = entity.TryGetComponent<MeshComponent>())
            j["mesh"] = *comp;
        if (auto comp = entity.TryGetComponent<CameraComponent>())
            j["camera"] = *comp;
        if (auto comp = entity.TryGetComponent<LightComponent>())
            j["light"] = *comp;
        if (auto comp = entity.TryGetComponent<RigidBodyComponent>())
            j["rigidBody"] = *comp;
        if (auto comp = entity.TryGetComponent<PlayerComponent>())
            j["player"] = *comp;
        if (auto comp = entity.TryGetComponent<LuaScriptComponent>())
            j["luaScript"] = *comp;
        if (auto comp = entity.TryGetComponent<AudioComponent>())
            j["audio"] = *comp;
        if (auto comp = entity.TryGetComponent<AudioVolumeComponent>())
            j["audioVolume"] = *comp;
        if (auto comp = entity.TryGetComponent<TextComponent>())
            j["text"] = *comp;
        if (auto comp = entity.TryGetComponent<SplineComponent>())
            j["spline"] = *comp;
        return j;

    }

    std::string GetHitMaterial(const RaycastHit& hit) {

        if (hit.triangle < 0 || !hit.entity.IsValid())
            return "";

        auto meshComponent = hit.entity.TryGetComponent<MeshComponent>();
        if (!meshComponent || !meshComponent->mesh.IsLoaded())
            return "";

        auto index = uint32_t(hit.triangle * 3);
        for (const auto& subData : meshComponent->mesh->data.subData) {
            if (index < subData.indicesOffset || index >= subData.indicesOffset + subData.indicesCount)
                continue;
            if (!subData.material.IsValid())
                return subData.name;
            return subData.material.GetResource()->path;
        }

        return "";

    }

    std::string GetHitLabel(const RaycastHit& hit) {

        return hit.entity.IsValid() ? GetEntityLabel(hit.entity) : hit.kind;

    }

    std::string GetEntityName(Scene::Entity entity) {

        if (!entity.IsValid())
            return "";

        auto nameComponent = entity.TryGetComponent<NameComponent>();
        return nameComponent ? nameComponent->name : "";

    }

    std::string GetEntityLabel(Scene::Entity entity) {

        auto name = GetEntityName(entity);
        return (name.empty() ? "<unnamed>" : name) + " (#" + std::to_string(entity.GetID()) + ")";

    }

    std::vector<Scene::Entity> FindEntities(Scene::Scene& scene, const std::vector<std::string>& patterns) {

        std::vector<Scene::Entity> entities;
        if (patterns.empty())
            return entities;

        auto nameSubset = scene.GetSubset<NameComponent>();
        for (auto entity : nameSubset) {
            const auto& nameComponent = nameSubset.Get(entity);
            if (MatchAnyPattern(patterns, nameComponent.name))
                entities.push_back({ entity, &scene.entityManager });
        }

        // Allow to address entities by id as well, e.g. "#123"
        for (const auto& pattern : patterns) {
            if (pattern.size() > 1 && pattern[0] == '#') {
                auto id = ECS::Entity(std::stoull(pattern.substr(1)));
                Scene::Entity entity = { id, &scene.entityManager };
                if (entity.IsValid())
                    entities.push_back(entity);
            }
        }

        return entities;

    }

    std::vector<Scene::Entity> FindEntities(Scene::Scene& scene, const std::string& pattern) {

        return FindEntities(scene, std::vector<std::string> { pattern });

    }

    std::vector<Scene::Entity> FindTopmostEntities(Scene::Scene& scene, const std::vector<std::string>& patterns) {

        auto entities = FindEntities(scene, patterns);
        EntitySet contained(entities.begin(), entities.end());

        std::vector<Scene::Entity> result;
        for (auto entity : entities) {
            bool ancestorContained = false;
            auto parent = scene.GetParentEntity(entity);
            while (parent.IsValid() && !ancestorContained) {
                ancestorContained = contained.contains(parent);
                parent = scene.GetParentEntity(parent);
            }

            if (!ancestorContained)
                result.push_back(entity);
        }

        return result;

    }

    std::vector<Scene::Entity> FindTopmostEntities(Scene::Scene& scene, const std::string& pattern) {

        return FindTopmostEntities(scene, std::vector<std::string> { pattern });

    }

    void CollectHierarchy(Scene::Entity entity, EntitySet& set) {

        if (!entity.IsValid() || set.contains(entity))
            return;

        set.insert(entity);

        auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
        if (!hierarchyComponent)
            return;

        for (auto child : hierarchyComponent->GetChildren())
            CollectHierarchy(child, set);

    }

    EntitySet CollectHierarchies(Scene::Scene& scene, const std::vector<std::string>& patterns) {

        EntitySet set;
        for (auto entity : FindEntities(scene, patterns))
            CollectHierarchy(entity, set);

        return set;

    }

    Volume::AABB GetEntityBounds(Scene::Entity entity, bool* hasMesh) {

        EntitySet hierarchy;
        CollectHierarchy(entity, hierarchy);

        auto scene = entity.GetScene();

        bool foundMesh = false;
        Volume::AABB bounds(vec3(std::numeric_limits<float>::max()), vec3(-std::numeric_limits<float>::max()));
        for (auto ecsEntity : hierarchy) {
            Scene::Entity member = { ecsEntity, &scene->entityManager };
            auto meshComponent = member.TryGetComponent<MeshComponent>();
            if (!meshComponent || !meshComponent->mesh.IsLoaded())
                continue;

            bounds.Grow(meshComponent->aabb);
            foundMesh = true;
        }

        if (!foundMesh) {
            auto position = GetEntityPosition(entity);
            bounds = Volume::AABB(position - vec3(0.05f), position + vec3(0.05f));
        }

        if (hasMesh)
            *hasMesh = foundMesh;

        return bounds;

    }

    Volume::AABB GetBounds(const std::vector<Scene::Entity>& entities) {

        Volume::AABB bounds(vec3(std::numeric_limits<float>::max()), vec3(-std::numeric_limits<float>::max()));
        for (auto entity : entities)
            bounds.Grow(GetEntityBounds(entity));

        return bounds;

    }

    Volume::AABB GetSceneBounds(Scene::Scene& scene) {

        Volume::AABB bounds(vec3(std::numeric_limits<float>::max()), vec3(-std::numeric_limits<float>::max()));
        bool found = false;

        auto meshSubset = scene.GetSubset<MeshComponent>();
        for (auto entity : meshSubset) {
            const auto& meshComponent = meshSubset.Get(entity);
            if (!meshComponent.mesh.IsLoaded())
                continue;

            bounds.Grow(meshComponent.aabb);
            found = true;
        }

        if (scene.terrain.IsLoaded()) {
            auto& terrain = scene.terrain;
            bounds.Grow(terrain->translation);
            bounds.Grow(terrain->translation + vec3(terrain->sideLength, terrain->heightScale, terrain->sideLength));
            found = true;
        }

        if (!found)
            bounds = Volume::AABB(vec3(-10.0f), vec3(10.0f));

        return bounds;

    }

    vec3 GetEntityPosition(Scene::Entity entity) {

        auto transformComponent = entity.TryGetComponent<TransformComponent>();
        if (!transformComponent)
            return vec3(0.0f);

        return vec3(transformComponent->globalMatrix[3]);

    }

    float GetEntityYaw(Scene::Entity entity) {

        auto transformComponent = entity.TryGetComponent<TransformComponent>();
        if (!transformComponent)
            return 0.0f;

        auto forward = vec3(transformComponent->globalMatrix * vec4(0.0f, 0.0f, 1.0f, 0.0f));
        return std::atan2(forward.x, forward.z);

    }

    RaycastHit Raycast(Scene::Scene& scene, vec3 origin, vec3 direction, float maxDistance, const EntitySet& ignored,
        Scene::SceneQueryComponents components) {

        RaycastHit hit;
        direction = Helper::Math::NormalizeOrZero(direction);
        if (direction == vec3(0.0f))
            return hit;

        // Skip invisible meshes and players for visual and ground queries
        auto ray = Volume::Ray(origin, direction, 0.0f, maxDistance);
        auto result = scene.CastRay(ray, components, [&](const Scene::Entity& entity) {

            if (ignored.contains(entity) || entity.HasComponent<PlayerComponent>())
                return true;
            auto meshComponent = entity.TryGetComponent<MeshComponent>();
            return meshComponent && !meshComponent->visible;

        });

        if (!result.valid)
            return hit;

        hit.valid = true;
        hit.distance = result.hitDistance;
        hit.position = ray.Get(result.hitDistance);
        hit.normal = result.normal;
        hit.entity = result.data;
        hit.triangle = result.triangleIndex;
        if (result.triangleIndex >= 0)
            hit.kind = "mesh";
        else if (!result.data.IsValid())
            hit.kind = "terrain";
        else
            hit.kind = result.data.HasComponent<TextComponent>() ? "text" : "body";

        return hit;

    }

    RaycastHit GroundBelow(Scene::Scene& scene, vec3 position, const EntitySet& ignored) {

        return Raycast(scene, position, vec3(0.0f, -1.0f, 0.0f), 4096.0f, ignored, CollisionComponents);

    }

    EntityGround GroundBelowEntity(Scene::Scene& scene, Scene::Entity entity, float searchAbove) {

        EntityGround result;
        result.bounds = GetEntityBounds(entity, &result.hasMesh);

        EntitySet ignored;
        CollectHierarchy(entity, ignored);
        auto center = result.bounds.GetCenter();
        result.ground = GroundBelow(scene, vec3(center.x, result.bounds.max.y + searchAbove, center.z), ignored);

        return result;

    }

    json InspectScene(Scene::Scene& scene, const std::vector<std::string>& filter,
        const std::vector<std::string>& detail, const EntitySet& excluded) {

        json j;
        j["name"] = scene.name;
        j["entityCount"] = scene.GetEntityCount() - excluded.size();
        j["bounds"] = BoundsToJson(GetSceneBounds(scene));
        j["hasMainCamera"] = scene.HasMainCamera();
        j["hasTerrain"] = scene.terrain.IsValid();
        j["hasOcean"] = scene.ocean != nullptr;
        j["settings"] = ExportSettings(scene);

        json entities = json::array();
        for (auto entity : scene) {
            if (excluded.contains(entity))
                continue;

            auto name = GetEntityName(entity);
            if (!filter.empty() && !MatchAnyPattern(filter, name))
                continue;

            json e;
            e["id"] = entity.GetID();
            e["name"] = name;
            e["components"] = GetComponentNames(entity);

            auto parent = scene.GetParentEntity(entity);
            if (parent.IsValid())
                e["parent"] = GetEntityLabel(parent);

            if (auto hierarchy = entity.TryGetComponent<HierarchyComponent>())
                e["childCount"] = hierarchy->GetChildren().size();

            if (auto transform = entity.TryGetComponent<TransformComponent>()) {
                auto local = transform->Decompose();
                e["position"] = FromVec3(vec3(transform->globalMatrix[3]));
                e["localPosition"] = FromVec3(local.translation);
                e["localRotationDegrees"] = FromVec3(glm::degrees(local.rotation));
                e["localScale"] = FromVec3(local.scale);
                e["worldYawDegrees"] = Round(glm::degrees(GetEntityYaw(entity)), 2);
                e["static"] = transform->IsStatic();
            }

            bool hasMesh = false;
            auto bounds = GetEntityBounds(entity, &hasMesh);
            if (hasMesh)
                e["bounds"] = BoundsToJson(bounds);

            if (auto mesh = entity.TryGetComponent<MeshComponent>()) {
                e["mesh"] = mesh->mesh.IsValid() ? mesh->mesh.GetResource()->path : "";
                e["visible"] = mesh->visible;
            }
            if (auto camera = entity.TryGetComponent<CameraComponent>())
                e["camera"] = { { "isMain", camera->isMain }, { "fov", camera->fieldOfView },
                    { "location", FromVec3(camera->GetLocation()) } };
            if (auto light = entity.TryGetComponent<LightComponent>())
                e["light"] = { { "type", int32_t(light->type) }, { "isMain", light->isMain } };
            if (auto script = entity.TryGetComponent<LuaScriptComponent>())
                e["script"] = script->script.IsValid() ? script->script.GetResource()->path : "";

            if (!detail.empty() && MatchAnyPattern(detail, name))
                e["detail"] = ComponentDetails(entity);

            entities.push_back(e);
        }
        j["entities"] = entities;

        return j;

    }

    json ExportSettings(Scene::Scene& scene) {

        json j;
        Scene::SceneSettingsToJson(j, &scene);

        // Variants restore these settings, so the sun has to be part of them
        if (scene.HasMainLight()) {
            const auto& light = scene.GetMainLight();
            j["sun"] = { { "direction", FromVec3(light.properties.directional.direction) },
                { "color", FromVec3(light.color) }, { "intensity", light.intensity } };
        }
        return j;

    }

    void ApplySettings(Scene::Scene& scene, const json& settings) {

        json features = json::object();

        for (auto& [key, patch] : settings.items()) {
            if (key == "materials") {
                ApplyMaterialPatches(patch);
            }
            else if (key == "sun") {
                if (!scene.HasMainLight())
                    throw std::runtime_error("The scene has no main directional light for the sun settings");

                auto& light = scene.GetMainLight();
                if (patch.contains("direction"))
                    light.properties.directional.direction = glm::normalize(ToVec3(patch["direction"]));
                // Light direction points from the sun towards the scene
                if (patch.contains("elevation") || patch.contains("azimuth"))
                    light.properties.directional.direction =
                        -GetDirection(patch.value("azimuth", 0.0f), patch.value("elevation", 45.0f));
                if (patch.contains("color"))
                    light.color = ToVec3(patch["color"]);
                if (patch.contains("intensity"))
                    light.intensity = patch["intensity"].get<float>();
            }
            else {
                // A default constructed ocean has no valid simulation to patch, so it starts with the editor defaults
                if (key == "ocean" && !scene.ocean)
                    scene.ocean = CreateRef<Ocean::Ocean>(9, 4096.0f);

                features[key] = patch;
            }
        }

        Scene::PatchSceneSettings(features, &scene);

        json applied;
        Scene::SceneSettingsToJson(applied, &scene);
        for (auto& [key, patch] : features.items()) {
            if (!applied.contains(key))
                throw std::runtime_error("Unknown settings key '" + key + "'. Known keys: the scene settings "
                    "(sky, postProcessing, wind, fog, ...), materials and sun");
        }

    }

    void PatchMaterial(Material& material, const json& patch) {

        // Round trip through the engine serializer, so the keys are the ones of the material files
        json j = material;
        j.merge_patch(patch);

        // from_json only sets maps which are present, removed maps (null in the patch) are reset here
        auto resetRemoved = [&](const char* key, ResourceHandle<Texture::Texture2D>& map) {

            if (patch.contains(key) && patch[key].is_null())
                map = ResourceHandle<Texture::Texture2D>();

        };
        resetRemoved("baseColorMapPath", material.baseColorMap);
        resetRemoved("opacityMapPath", material.opacityMap);
        resetRemoved("normalMapPath", material.normalMap);
        resetRemoved("roughnessMapPath", material.roughnessMap);
        resetRemoved("metalnessMapPath", material.metalnessMap);
        resetRemoved("aoMapPath", material.aoMap);
        resetRemoved("displacementMapPath", material.displacementMap);
        resetRemoved("emissiveMapPath", material.emissiveMap);

        from_json(j, material);

    }

    int32_t ApplyMaterialPatches(const json& patches) {

        int32_t count = 0;
        for (auto& material : ResourceManager<Material>::GetResources()) {
            if (!material.IsLoaded())
                continue;

            const auto& path = material.GetResource()->path;
            for (auto& [pattern, patch] : patches.items()) {
                if (!MatchPattern(pattern, path))
                    continue;

                PatchMaterial(*material.Get(), patch);
                count++;
            }
        }

        if (count == 0)
            throw std::runtime_error("No loaded material matches the material patches " + patches.dump());

        return count;

    }

    json RunQueries(Scene::Scene& scene, const json& queries, const PickRayFunction& pickRay, const fs::path& outputDirectory) {

        json results = json::array();

        for (const auto& query : queries) {
            json result;
            result["query"] = query;

            try {
                auto type = query.at("type").get<std::string>();

                if (type == "raycast") {
                    auto ignored = GetIgnoredEntities(scene, query);

                    // Physics bodies can be included explicitly, by default only visible geometry is hit
                    Scene::SceneQueryComponents components = VisualComponents;
                    if (query.value("bodies", false))
                        components |= Scene::RigidBodyComponentBit;

                    auto hit = Raycast(scene, ToVec3(query.at("origin")), ToVec3(query.at("direction")),
                        query.value("maxDistance", 4096.0f), ignored, components);
                    result["result"] = HitToJson(hit);
                }
                else if (type == "ground") {
                    // Ground height at x/z, cast from above the scene unless a start height is given
                    auto bounds = GetSceneBounds(scene);
                    auto from = query.value("from", bounds.max.y + 10.0f);
                    auto hit = GroundBelow(scene, vec3(query.at("x").get<float>(), from, query.at("z").get<float>()));
                    result["result"] = HitToJson(hit);
                }
                else if (type == "entity") {
                    json matches = json::array();
                    for (auto entity : FindEntities(scene, query.at("entity").get<std::string>())) {
                        auto [bounds, hasMesh, ground] = GroundBelowEntity(scene, entity);

                        json match;
                        match["entity"] = GetEntityLabel(entity);
                        match["position"] = FromVec3(GetEntityPosition(entity));
                        match["yawDegrees"] = glm::degrees(GetEntityYaw(entity));
                        match["bounds"] = BoundsToJson(bounds);
                        match["hasMesh"] = hasMesh;
                        match["groundBelow"] = HitToJson(ground);
                        if (ground.valid)
                            match["gapToGround"] = Round(bounds.min.y - ground.position.y);
                        matches.push_back(match);
                    }
                    result["result"] = matches;
                }
                else if (type == "around") {
                    auto position = ToVec3(query.at("position"));
                    auto radius = query.at("radius").get<float>();
                    auto aabb = Volume::AABB(position - vec3(radius), position + vec3(radius));

                    json matches = json::array();
                    for (auto entity : scene.QueryAABB(aabb)) {
                        auto meshComponent = entity.TryGetComponent<MeshComponent>();
                        if (!meshComponent)
                            continue;
                        matches.push_back({ { "entity", GetEntityLabel(entity) },
                            { "distance", meshComponent->aabb.GetDistance(position) },
                            { "bounds", BoundsToJson(meshComponent->aabb) } });
                    }
                    std::sort(matches.begin(), matches.end(), [](const json& a, const json& b) {

                        return a["distance"].get<float>() < b["distance"].get<float>();

                    });
                    result["result"] = matches;
                }
                else if (type == "clearance") {
                    // Sample clearance with a ray grid on the bounds face
                    auto entities = FindEntities(scene, query.at("entity").get<std::string>());
                    if (entities.empty())
                        throw std::runtime_error("No entity matches " + query["entity"].get<std::string>());

                    auto direction = glm::normalize(ToVec3(query.at("direction")));
                    auto distance = query.value("distance", 100.0f);
                    auto samples = std::max(query.value("samples", 5), 2);

                    auto ignored = GetIgnoredEntities(scene, query);
                    for (auto entity : entities)
                        CollectHierarchy(entity, ignored);

                    auto bounds = GetBounds(entities);
                    // Shrink slightly so rays don't start inside touching neighbors
                    auto inset = bounds.GetSize() * 0.02f;
                    auto min = bounds.min + inset, max = bounds.max - inset;

                    float minClearance = distance;
                    json blocker;
                    for (int32_t a = 0; a < samples; a++) {
                        for (int32_t b = 0; b < samples; b++) {
                            auto fa = float(a) / float(samples - 1), fb = float(b) / float(samples - 1);
                            vec3 start;
                            // Pick the face of the bounds in the movement direction
                            auto axis = glm::abs(direction);
                            if (axis.y >= axis.x && axis.y >= axis.z)
                                start = vec3(glm::mix(min.x, max.x, fa), direction.y > 0.0f ? max.y : min.y,
                                    glm::mix(min.z, max.z, fb));
                            else if (axis.x >= axis.z)
                                start = vec3(direction.x > 0.0f ? max.x : min.x, glm::mix(min.y, max.y, fa),
                                    glm::mix(min.z, max.z, fb));
                            else
                                start = vec3(glm::mix(min.x, max.x, fa), glm::mix(min.y, max.y, fb),
                                    direction.z > 0.0f ? max.z : min.z);

                            auto hit = Raycast(scene, start, direction, distance, ignored, CollisionComponents);
                            if (hit.valid && hit.distance < minClearance) {
                                minClearance = hit.distance;
                                blocker = HitToJson(hit);
                            }
                        }
                    }

                    result["result"] = { { "clearance", Round(minClearance) },
                        { "blocked", !blocker.is_null() }, { "blocker", blocker }, { "bounds", BoundsToJson(bounds) } };
                }
                else if (type == "pick") {
                    // Use the shot camera convention for pixel picking
                    if (!pickRay)
                        throw std::runtime_error("pick queries are not available here");

                    json hits = json::array();
                    for (const auto& pixel : query.at("pixels")) {
                        auto [origin, direction] = pickRay(query.at("camera"), vec2(pixel.at(0).get<float>(), pixel.at(1).get<float>()));
                        auto hit = Raycast(scene, origin, direction, 4096.0f);
                        auto entry = HitToJson(hit);
                        entry["pixel"] = pixel;
                        entry["origin"] = FromVec3(origin);
                        hits.push_back(entry);
                    }
                    result["result"] = hits;
                }
                else if (type == "path") {
                    // Sample ground height and obstacles along the polyline
                    auto spacing = std::max(query.value("spacing", 0.5f), 0.05f);
                    auto maxStep = query.value("maxStep", 0.3f);
                    auto radius = query.value("radius", 0.4f);
                    // Points given as x/z start at the terrain height with "snap": true
                    auto snap = query.value("snap", false);
                    std::vector<vec3> points;
                    for (const auto& point : query.at("points"))
                        points.push_back(point.size() == 2 ? vec3(point[0].get<float>(), 0.0f, point[1].get<float>()) : ToVec3(point));
                    if (points.size() < 2)
                        throw std::runtime_error("A path needs at least two points");

                    auto ignored = GetIgnoredEntities(scene, query);

                    json issues = json::array();
                    json samples = json::array();
                    std::optional<vec3> last;
                    float length = 0.0f;

                    for (size_t i = 0; i + 1 < points.size(); i++) {
                        auto segment = points[i + 1] - points[i];
                        auto count = std::max(int32_t(std::ceil(glm::length(vec2(segment.x, segment.z)) / spacing)), 1);
                        for (int32_t k = (i == 0 ? 0 : 1); k <= count; k++) {
                            auto guess = points[i] + segment * (float(k) / float(count));
                            if (snap) {
                                auto sample = SampleTerrain(GetCompleteTerrain(scene), vec2(guess.x, guess.z));
                                if (sample.valid && !sample.hole)
                                    guess.y = sample.height;
                            }
                            auto ground = Raycast(scene, guess + vec3(0.0f, 1.5f, 0.0f), vec3(0.0f, -1.0f, 0.0f), 4.0f, ignored, CollisionComponents);
                            if (!ground.valid) {
                                issues.push_back({ { "at", FromVec3(guess) },
                                    { "issue", "no ground within 1.5 m above and 2.5 m below" } });
                                continue;
                            }

                            auto position = ground.position;
                            samples.push_back({ { "position", FromVec3(position) }, { "on", GetHitLabel(ground) } });

                            if (last) {
                                auto step = position.y - last->y;
                                if (std::abs(step) > maxStep)
                                    issues.push_back({ { "at", FromVec3(position) },
                                        { "issue", "step of " + FormatFloat(step, 2) + " m onto " + GetHitLabel(ground) } });

                                // Obstacles between the previous and this sample, at knee and chest height and at the sides
                                auto direction = position - *last;
                                direction.y = 0.0f;
                                auto distance = glm::length(direction);
                                if (distance > 1e-4f) {
                                    direction /= distance;
                                    auto side = vec3(-direction.z, 0.0f, direction.x) * radius;
                                    for (auto height : { 0.5f, 1.3f }) {
                                        for (auto offset : { vec3(0.0f), side, -side }) {
                                            auto origin = *last + vec3(0.0f, height, 0.0f) + offset;
                                            auto hit = Raycast(scene, origin, direction, distance, ignored, CollisionComponents);
                                            if (hit.valid)
                                                issues.push_back({ { "at", FromVec3(hit.position) },
                                                    { "issue", "blocked by " + GetHitLabel(hit) + " (" + hit.kind + ")" } });
                                        }
                                    }
                                }
                                length += glm::distance(position, *last);
                            }
                            last = position;
                        }
                    }

                    // Neighboring samples often report the same obstacle, keep the first occurrence per description
                    json uniqueIssues = json::array();
                    std::set<std::string> seen;
                    for (const auto& issue : issues)
                        if (seen.insert(issue["issue"].get<std::string>()).second)
                            uniqueIssues.push_back(issue);

                    result["result"] = { { "walkable", uniqueIssues.empty() }, { "length", length },
                        { "issues", uniqueIssues }, { "samples", samples } };
                }
                else if (type == "terrain") {
                    result["result"] = DescribeTerrain(GetCompleteTerrain(scene));
                }
                else if (type == "terrainSample") {
                    // Points or a grid ("min", "max", "step") of x/z positions
                    const auto& terrain = GetCompleteTerrain(scene);
                    std::vector<vec2> points;
                    if (query.contains("points")) {
                        for (const auto& point : query["points"])
                            points.emplace_back(point.at(0).get<float>(), point.at(point.size() == 3 ? 2 : 1).get<float>());
                    }
                    else {
                        auto min = vec2(query.at("min").at(0).get<float>(), query.at("min").at(1).get<float>());
                        auto max = vec2(query.at("max").at(0).get<float>(), query.at("max").at(1).get<float>());
                        auto step = query.at("step").get<float>();
                        if (step <= 0.0f)
                            throw std::runtime_error("terrainSample: the step has to be positive");
                        for (auto z = min.y; z <= max.y + 1e-3f; z += step)
                            for (auto x = min.x; x <= max.x + 1e-3f; x += step)
                                points.emplace_back(x, z);
                    }

                    json samples = json::array();
                    for (auto point : points) {
                        auto sample = SampleTerrain(terrain, point);
                        json entry = { { "x", point.x }, { "z", point.y }, { "valid", sample.valid } };
                        if (sample.valid) {
                            entry["hole"] = sample.hole;
                            entry["material"] = sample.material;
                            if (!sample.hole) {
                                entry["height"] = Round(sample.height);
                                entry["slope"] = Round(sample.slopeDegrees, 1);
                            }
                        }
                        samples.push_back(entry);
                    }
                    result["result"] = samples;
                }
                else if (type == "terrainMap") {
                    if (outputDirectory.empty())
                        throw std::runtime_error("terrainMap needs an output directory");
                    auto file = query.value("file", std::string("terrain_map.png"));
                    auto image = RenderTerrainMap(scene, query);
                    WritePng(image, outputDirectory / file);
                    result["result"] = { { "file", file }, { "width", image->width }, { "height", image->height } };
                }
                else if (type == "route") {
                    result["result"] = FindRoute(scene, query, outputDirectory);
                }
                else if (type == "buried") {
                    // Meshes of a hierarchy which reach above the terrain (or lie below a hole), e.g. to check
                    // that a level placed under the terrain only shows at its entrance
                    const auto& terrain = GetCompleteTerrain(scene);
                    auto margin = query.value("margin", 0.0f);
                    auto members = CollectHierarchies(scene, { query.at("entity").get<std::string>() });

                    json exposed = json::array();
                    int32_t meshCount = 0;
                    for (auto member : members) {
                        Scene::Entity entity = { member, &scene.entityManager };
                        auto mesh = entity.TryGetComponent<MeshComponent>();
                        if (!mesh || !mesh->visible || !mesh->mesh.IsLoaded())
                            continue;
                        meshCount++;

                        const auto& aabb = mesh->aabb;
                        float lowest = FLT_MAX, worst = -FLT_MAX;
                        bool underHole = false;
                        for (auto fraction : { vec2(0.5f), vec2(0.1f), vec2(0.9f), vec2(0.1f, 0.9f), vec2(0.9f, 0.1f) }) {
                            auto point = glm::mix(vec2(aabb.min.x, aabb.min.z), vec2(aabb.max.x, aabb.max.z), fraction);
                            auto sample = SampleTerrain(terrain, point);
                            if (!sample.valid)
                                continue;
                            if (sample.hole) {
                                underHole = true;
                                continue;
                            }
                            lowest = std::min(lowest, sample.height);
                            worst = std::max(worst, aabb.max.y - sample.height);
                        }
                        if (underHole || worst > -margin)
                            exposed.push_back({ { "entity", GetEntityLabel(entity) }, { "above", worst }, { "underHole", underHole },
                                { "top", aabb.max.y }, { "terrain", lowest }, { "center", FromVec3(aabb.GetCenter()) } });
                    }

                    std::sort(exposed.begin(), exposed.end(), [](const json& a, const json& b) {

                        return a["above"].get<float>() > b["above"].get<float>();

                    });
                    auto exposedCount = exposed.size();
                    if (exposed.size() > 200)
                        exposed.erase(exposed.begin() + 200, exposed.end());
                    result["result"] = { { "meshes", meshCount }, { "exposedCount", exposedCount }, { "exposed", exposed } };
                }
                else {
                    throw std::runtime_error("Unknown query type '" + type +
                        "' (raycast, ground, entity, around, clearance, path, pick, terrain, terrainSample, terrainMap, route, buried)");
                }
            }
            catch (const std::exception& e) {
                result["error"] = e.what();
            }

            results.push_back(result);
        }

        return results;

    }

}
