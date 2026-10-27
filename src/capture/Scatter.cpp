#include "Scatter.h"
#include "SceneEditor.h"
#include "TerrainTools.h"

#include "common/MathHelper.h"
#include "common/MatrixDecomposition.h"
#include "physics/ShapesManager.h"
#include "scene/prefabs/MeshInstance.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/noise.hpp>
#include <glm/gtx/quaternion.hpp>

#include <cfloat>
#include <cstdio>

#include <map>
#include <random>
#include <stdexcept>
#include <unordered_map>

namespace Atlas::Capture {

    using namespace Scene::Components;

    // Circle (optionally a ring), rectangle, polygon or a corridor along a polyline
    struct Area {

        enum class Type { Circle, Rect, Polygon, Corridor } type = Type::Circle;
        vec2 center = vec2(0.0f);
        float radius = 0.0f;
        float innerRadius = 0.0f;
        vec2 min = vec2(0.0f), max = vec2(0.0f);
        std::vector<vec2> points;
        float halfWidth = 0.0f;

        bool Contains(vec2 point) const {

            switch (type) {
            case Type::Circle: {
                auto distance = glm::distance(point, center);
                return distance <= radius && distance >= innerRadius;
            }
            case Type::Rect:
                return glm::all(glm::greaterThanEqual(point, min)) && glm::all(glm::lessThanEqual(point, max));
            case Type::Polygon:
                return Helper::Math::IsInsidePolygon(point, points);
            case Type::Corridor:
                for (size_t i = 1; i < points.size(); i++)
                    if (Helper::Math::DistanceToSegment(point, points[i - 1], points[i]) <= halfWidth)
                        return true;
                return false;
            }
            return false;

        }

        void GetBounds(vec2& boundsMin, vec2& boundsMax) const {

            switch (type) {
            case Type::Circle:
                boundsMin = center - vec2(radius);
                boundsMax = center + vec2(radius);
                break;
            case Type::Rect:
                boundsMin = min;
                boundsMax = max;
                break;
            default:
                boundsMin = vec2(FLT_MAX);
                boundsMax = vec2(-FLT_MAX);
                for (auto point : points) {
                    boundsMin = glm::min(boundsMin, point - vec2(halfWidth));
                    boundsMax = glm::max(boundsMax, point + vec2(halfWidth));
                }
            }

        }

    };

    static Area ParseArea(const json& j) {

        Area area;
        if (j.contains("radius")) {
            area.type = Area::Type::Circle;
            area.center = ToVec2(j.at("center"));
            area.radius = j["radius"].get<float>();
            area.innerRadius = j.value("innerRadius", 0.0f);
        }
        else if (j.contains("min")) {
            area.type = Area::Type::Rect;
            area.min = ToVec2(j["min"]);
            area.max = ToVec2(j.at("max"));
        }
        else if (j.contains("polygon")) {
            area.type = Area::Type::Polygon;
            for (const auto& point : j["polygon"])
                area.points.push_back(ToVec2(point));
            if (area.points.size() < 3)
                throw std::runtime_error("scatter: a polygon needs at least 3 points");
        }
        else if (j.contains("path")) {
            area.type = Area::Type::Corridor;
            for (const auto& point : j["path"])
                area.points.push_back(ToVec2(point));
            area.halfWidth = j.at("width").get<float>() * 0.5f;
            if (area.points.size() < 2)
                throw std::runtime_error("scatter: a path needs at least 2 points");
        }
        else {
            throw std::runtime_error("scatter: an area needs center/radius, min/max, polygon or path/width: " + j.dump());
        }
        return area;

    }

    std::function<bool(vec2)> MakeAreaTest(const json& area) {

        auto parsed = ParseArea(area);
        return [parsed](vec2 point) { return parsed.Contains(point); };

    }

    struct Item {

        ResourceHandle<Mesh::Mesh> mesh;
        std::string name;
        float weight = 1.0f;
        vec2 scale = vec2(1.0f);
        // Factor on the height, e.g. for wide flat patches of grass
        float scaleY = 1.0f;
        // Largest height difference between the ground below the footprint and the plane of the instance, 0 for any
        float flatness = 0.0f;
        vec2 yaw = vec2(0.0f, 360.0f);
        float tilt = 0.0f;
        // Fixed rotation around the local x axis after the yaw, e.g. for fallen objects
        float pitch = 0.0f;
        float align = 0.0f;
        float sink = 0.0f;
        // Height of the lowest point of the pitched mesh relative to its pivot, at scale 1
        float bottom = 0.0f;
        // Footprint radius at scale 1
        float radius = 0.5f;
        float height = 1.0f;
        // Empty, "mesh" or "capsule"
        std::string body;
        float bodyRadius = 0.3f;
        float bodyHeight = 2.0f;
        // Instance name instead of "<group> <mesh> <index>", the index is appended if several are placed
        std::string instanceName;
        json light;
        int32_t placed = 0;

    };

    // Uniform grid over x/z for quick neighborhood tests
    template<class T>
    struct SpatialGrid {

        float cellSize = 16.0f;
        std::unordered_map<int64_t, std::vector<T>> cells;

        static int64_t Key(int32_t x, int32_t y) {

            return (int64_t(x) << 32) ^ int64_t(uint32_t(y));

        }

        void Insert(vec2 min, vec2 max, const T& value) {

            auto cellMin = ivec2(glm::floor(min / cellSize));
            auto cellMax = ivec2(glm::floor(max / cellSize));
            for (auto y = cellMin.y; y <= cellMax.y; y++)
                for (auto x = cellMin.x; x <= cellMax.x; x++)
                    cells[Key(x, y)].push_back(value);

        }

        template<class Func>
        bool Any(vec2 min, vec2 max, Func func) const {

            auto cellMin = ivec2(glm::floor(min / cellSize));
            auto cellMax = ivec2(glm::floor(max / cellSize));
            for (auto y = cellMin.y; y <= cellMax.y; y++) {
                for (auto x = cellMin.x; x <= cellMax.x; x++) {
                    auto it = cells.find(Key(x, y));
                    if (it == cells.end())
                        continue;
                    for (const auto& value : it->second)
                        if (func(value))
                            return true;
                }
            }
            return false;

        }

    };

    struct Blocker {

        Volume::AABB aabb;
        ECS::Entity entity;

    };

    struct Placed {

        vec2 position;
        float radius;

    };

    json Scatter(Scene::Scene& scene, const json& spec) {

        // Scenes without a terrain are scattered on the first visible surface below castFrom (default: above the scene)
        Ref<Terrain::Terrain> terrain;
        if (scene.terrain.IsLoaded())
            terrain = GetCompleteTerrain(scene);
        auto sceneBounds = terrain ? Volume::AABB() : GetSceneBounds(scene);
        auto castFrom = spec.value("castFrom", sceneBounds.max.y + 1.0f);

        // With append, instances are added to an existing group of the same name
        auto groupName = spec.at("group").get<std::string>();
        auto existingGroup = scene.GetEntityByName(groupName);
        auto append = spec.value("append", false);
        if (existingGroup.IsValid() && !append)
            throw std::runtime_error("scatter: an entity named '" + groupName + "' already exists, set \"append\": true");

        auto area = ParseArea(spec.at("area"));
        std::vector<Area> excluded;
        for (const auto& exclude : spec.value("exclude", json::array()))
            excluded.push_back(ParseArea(exclude));

        std::vector<Item> items;
        float weightSum = 0.0f;
        for (const auto& itemSpec : spec.at("items")) {
            Item item;
            item.mesh = LoadMeshForScene(itemSpec.at("mesh").get<std::string>());
            item.name = item.mesh->name;
            item.weight = itemSpec.value("weight", 1.0f);
            if (itemSpec.contains("scale")) {
                const auto& scale = itemSpec["scale"];
                item.scale = scale.is_array() ? vec2(scale.at(0).get<float>(), scale.at(1).get<float>()) :
                    vec2(scale.get<float>());
            }
            item.scaleY = itemSpec.value("scaleY", 1.0f);
            item.flatness = itemSpec.value("flatness", 0.0f);
            if (itemSpec.contains("yaw"))
                item.yaw = vec2(itemSpec["yaw"].at(0).get<float>(), itemSpec["yaw"].at(1).get<float>());
            item.tilt = itemSpec.value("tilt", 0.0f);
            item.pitch = itemSpec.value("pitch", 0.0f);
            item.align = itemSpec.value("align", 0.0f);
            item.sink = itemSpec.value("sink", 0.0f);

            // The lowest point of the mesh is put on the ground unless the pivot is (e.g. meshes modelled on their base)
            if (!itemSpec.value("pivotOnGround", false)) {
                const auto& aabb = item.mesh->data.aabb;
                auto pitch = glm::rotate(glm::radians(item.pitch), vec3(1.0f, 0.0f, 0.0f));
                item.bottom = FLT_MAX;
                for (int32_t corner = 0; corner < 8; corner++) {
                    auto point = vec3(corner & 1 ? aabb.max.x : aabb.min.x, corner & 2 ? aabb.max.y : aabb.min.y,
                        corner & 4 ? aabb.max.z : aabb.min.z);
                    item.bottom = std::min(item.bottom, vec3(pitch * vec4(point, 1.0f)).y);
                }
            }

            auto size = item.mesh->data.aabb.max - item.mesh->data.aabb.min;
            item.radius = itemSpec.value("radius", 0.35f * std::max(size.x, size.z));
            item.height = size.y;

            auto body = itemSpec.value("body", json(false));
            item.body = body.is_boolean() ? (body.get<bool>() ? "mesh" : "") : body.get<std::string>();
            if (!item.body.empty() && item.body != "mesh" && item.body != "capsule")
                throw std::runtime_error("scatter: body must be false, true/\"mesh\" or \"capsule\"");
            item.bodyRadius = itemSpec.value("bodyRadius", 0.15f * std::max(size.x, size.z));
            item.bodyHeight = itemSpec.value("bodyHeight", size.y * 0.6f);

            item.instanceName = itemSpec.value("name", std::string());
            item.light = itemSpec.value("light", json());

            weightSum += item.weight;
            items.push_back(item);
        }
        if (items.empty() || weightSum <= 0.0f)
            throw std::runtime_error("scatter: no items");

        auto count = spec.at("count").get<int32_t>();
        auto attempts = spec.value("attempts", count * 40);
        auto seed = spec.value("seed", 1u);
        auto spacing = spec.value("spacing", 0.0f);
        auto clearance = spec.value("clearance", 0.5f);
        auto onMeshes = spec.value("onMeshes", false);
        auto slopeRange = spec.contains("slope") ? vec2(spec["slope"].at(0).get<float>(), spec["slope"].at(1).get<float>()) :
            vec2(0.0f, 35.0f);
        auto heightRange = spec.contains("height") ? vec2(spec["height"].at(0).get<float>(), spec["height"].at(1).get<float>()) :
            vec2(-FLT_MAX, FLT_MAX);
        std::vector<int32_t> materials;
        if (spec.contains("materials"))
            materials = spec["materials"].get<std::vector<int32_t>>();
        if (!terrain && !materials.empty())
            throw std::runtime_error("scatter: materials are terrain material slots, the scene has no terrain");
        // Without a terrain every ground is a mesh
        if (!terrain)
            onMeshes = true;

        // Simplex noise mask for clusters and clearings: kept where the noise is above the threshold,
        // with a linear probability ramp of the given softness
        bool useNoise = spec.contains("noise");
        auto noiseScale = useNoise ? spec["noise"].value("scale", 60.0f) : 1.0f;
        auto noiseThreshold = useNoise ? spec["noise"].value("threshold", 0.0f) : 0.0f;
        auto noiseSoftness = useNoise ? spec["noise"].value("softness", 0.2f) : 0.0f;
        auto noiseOffset = vec2(float(seed % 1000) * 17.31f, float(seed % 1000) * 9.73f);

        // Existing meshes block placement, except ignored entities (e.g. grass) and huge meshes like merged
        // terrain pieces, which the ground raycast handles
        auto ignorePatterns = spec.value("ignore", std::vector<std::string>());
        auto maxBlockerSize = spec.value("maxBlockerSize", 120.0f);

        // The ignored hierarchies (e.g. all grass) are only collected for the blockers, the casts check their few hits
        auto isIgnored = [&](const Scene::Entity& entity) { return IsInHierarchies(scene, entity, ignorePatterns); };

        SpatialGrid<Blocker> blockers;
        if (clearance >= 0.0f) {
            auto ignored = CollectHierarchies(scene, ignorePatterns);
            auto meshSubset = scene.GetSubset<MeshComponent>();
            for (auto entity : meshSubset) {
                const auto& mesh = meshSubset.Get(entity);
                if (ignored.contains(entity) || !mesh.visible || !mesh.mesh.IsLoaded())
                    continue;
                auto size = mesh.aabb.max - mesh.aabb.min;
                if (size.x > maxBlockerSize || size.z > maxBlockerSize)
                    continue;
                blockers.Insert(vec2(mesh.aabb.min.x, mesh.aabb.min.z), vec2(mesh.aabb.max.x, mesh.aabb.max.z), { mesh.aabb, entity });
            }
        }

        SpatialGrid<Placed> placedGrid;
        placedGrid.cellSize = 8.0f;

        // Instances of earlier scatters this one keeps its spacing to as well, e.g. grass patches of other groups
        for (auto entity : FindEntities(scene, spec.value("spacingWith", std::vector<std::string>()))) {
            auto position = GetEntityPosition(entity);
            auto point = vec2(position.x, position.z);
            placedGrid.Insert(point, point, { point, 0.0f });
        }

        std::mt19937 random(seed);
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);

        vec2 boundsMin, boundsMax;
        area.GetBounds(boundsMin, boundsMax);

        std::map<std::string, int32_t> rejected;
        std::vector<Scene::Entity> created;

        auto tooClose = [&](vec2 point, float radius) {

            auto searchRadius = std::max(spacing, radius * 2.0f) + 1.0f;
            return placedGrid.Any(point - vec2(searchRadius), point + vec2(searchRadius), [&](const Placed& other) {

                return glm::distance(other.position, point) < (spacing > 0.0f ? spacing : radius + other.radius);

            });

        };

        // What the instance stands on doesn't block it, e.g. a building that is a single mesh
        auto blocked = [&](vec2 point, float reach, const RaycastHit& ground, float top) {

            return blockers.Any(point - vec2(reach), point + vec2(reach), [&](const Blocker& blocker) {

                const auto& aabb = blocker.aabb;
                if (blocker.entity == ECS::Entity(ground.entity) || aabb.max.y < ground.position.y - 0.25f || aabb.min.y > top)
                    return false;
                auto closest = glm::clamp(point, vec2(aabb.min.x, aabb.min.z), vec2(aabb.max.x, aabb.max.z));
                return glm::distance(closest, point) < reach;

            });

        };

        // Large scatters take minutes while the window doesn't update
        auto progressStep = std::max(attempts / 10, 1);
        for (int32_t attempt = 0; attempt < attempts && int32_t(created.size()) < count; attempt++) {
            if (attempts >= 1000000 && attempt > 0 && attempt % progressStep == 0) {
                std::printf("[capture] scatter %s: %d%% of the attempts, %zu placed\n", groupName.c_str(),
                    attempt / progressStep * 10, created.size());
                std::fflush(stdout);
            }

            auto point = glm::mix(boundsMin, boundsMax, vec2(unit(random), unit(random)));
            // Draw all random numbers of an attempt up front, such that rejections don't shift the sequence
            auto itemChoice = unit(random) * weightSum;
            auto scaleChoice = unit(random), yawChoice = unit(random), tiltChoice = unit(random),
                tiltDirection = unit(random), noiseChoice = unit(random);

            if (!area.Contains(point)) {
                rejected["outside"]++;
                continue;
            }
            if (std::any_of(excluded.begin(), excluded.end(), [&](const Area& exclude) { return exclude.Contains(point); })) {
                rejected["excluded"]++;
                continue;
            }
            if (useNoise) {
                auto noise = glm::simplex((point + noiseOffset) / noiseScale);
                auto probability = noiseSoftness > 0.0f ?
                    glm::clamp((noise - noiseThreshold) / noiseSoftness, 0.0f, 1.0f) : (noise >= noiseThreshold ? 1.0f : 0.0f);
                if (noiseChoice >= probability) {
                    rejected["noise"]++;
                    continue;
                }
            }

            TerrainSample sample;
            if (terrain) {
                sample = SampleTerrain(terrain, point);
            }
            else {
                auto surface = Raycast(scene, vec3(point.x, castFrom, point.y), vec3(0.0f, -1.0f, 0.0f),
                    castFrom - sceneBounds.min.y + 1.0f, isIgnored);
                sample.valid = surface.valid;
                sample.height = surface.position.y;
                sample.normal = surface.normal;
                sample.slopeDegrees = glm::degrees(std::acos(Helper::Math::Saturate(surface.normal.y)));
            }
            if (!sample.valid || sample.hole) {
                rejected[terrain ? "noTerrain" : "noGround"]++;
                continue;
            }
            if (sample.slopeDegrees < slopeRange.x || sample.slopeDegrees > slopeRange.y) {
                rejected["slope"]++;
                continue;
            }
            if (sample.height < heightRange.x || sample.height > heightRange.y) {
                rejected["height"]++;
                continue;
            }
            if (!materials.empty() && std::find(materials.begin(), materials.end(), sample.material) == materials.end()) {
                rejected["material"]++;
                continue;
            }

            size_t itemIndex = 0;
            for (float weight = items[0].weight; weight < itemChoice && itemIndex + 1 < items.size();)
                weight += items[++itemIndex].weight;
            auto& item = items[itemIndex];

            auto scale = glm::mix(item.scale.x, item.scale.y, scaleChoice);
            auto radius = item.radius * scale;

            if (tooClose(point, radius)) {
                rejected["spacing"]++;
                continue;
            }

            // Wide flat items lift off convex ground and dig into hollows, up to the edge of the footprint
            if (terrain && item.flatness > 0.0f) {
                auto uneven = false;
                for (int32_t k = 0; k < 16 && !uneven; k++) {
                    auto angle = float(k) * glm::quarter_pi<float>();
                    auto offset = vec2(std::cos(angle), std::sin(angle)) * radius * (k < 8 ? 0.7f : 1.4f);
                    auto edge = SampleTerrain(terrain, point + offset);
                    auto plane = sample.height - (sample.normal.x * offset.x + sample.normal.z * offset.y) / sample.normal.y;
                    uneven = !edge.valid || edge.hole || std::abs(edge.height - plane) > item.flatness;
                }
                if (uneven) {
                    rejected["uneven"]++;
                    continue;
                }
            }

            // On a terrain the ground has to be the terrain itself, something lying on it (a road, a house) is a mesh hit
            auto ground = Raycast(scene, vec3(point.x, sample.height + 3.0f, point.y), vec3(0.0f, -1.0f, 0.0f), 8.0f, isIgnored);
            if (!ground.valid) {
                rejected["noGround"]++;
                continue;
            }
            if (ground.kind != "terrain" && !onMeshes) {
                rejected["onMesh"]++;
                continue;
            }

            if (clearance >= 0.0f && blocked(point, radius + clearance, ground, ground.position.y + item.height * scale * item.scaleY)) {
                rejected["blocked"]++;
                continue;
            }

            // Align the up axis partially to the ground normal, then tilt randomly
            auto normal = ground.kind == "terrain" ? sample.normal : ground.normal;
            auto up = glm::normalize(glm::mix(vec3(0.0f, 1.0f, 0.0f), normal, item.align));
            auto tiltAngle = glm::radians(item.tilt * tiltChoice);
            auto tiltAxis = vec3(std::cos(tiltDirection * glm::two_pi<float>()), 0.0f, std::sin(tiltDirection * glm::two_pi<float>()));
            up = glm::normalize(glm::rotate(glm::angleAxis(tiltAngle, tiltAxis), up));
            auto alignment = glm::rotation(vec3(0.0f, 1.0f, 0.0f), up);
            auto yaw = glm::radians(glm::mix(item.yaw.x, item.yaw.y, yawChoice));

            auto position = ground.position - up * ((item.sink + item.bottom) * scale * item.scaleY);
            auto matrix = glm::translate(position) * glm::mat4_cast(alignment) *
                glm::rotate(yaw, vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(glm::radians(item.pitch), vec3(1.0f, 0.0f, 0.0f)) *
                glm::scale(vec3(scale, scale * item.scaleY, scale));

            auto name = item.instanceName.empty() ? groupName + " " + item.name + " " + std::to_string(created.size()) :
                (count == 1 ? item.instanceName : item.instanceName + " " + std::to_string(item.placed + 1));
            auto entity = scene.CreatePrefab<Scene::Prefabs::MeshInstance>(item.mesh, matrix, true);
            entity.AddComponent<NameComponent>(name);
            entity.GetComponent<TransformComponent>().globalMatrix = matrix;
            // The scene computes the bounds in its next update, later operations of the edit already need them
            entity.GetComponent<MeshComponent>().aabb = item.mesh->data.aabb.Transform(matrix);

            // A light on a child entity, offset in the space of the instance (e.g. the flame of a lantern)
            if (!item.light.is_null()) {
                auto offset = ToVec3(item.light.value("offset", json()));
                auto lightEntity = scene.CreateEntity();
                lightEntity.AddComponent<NameComponent>(name + " Light");
                auto& lightTransform = lightEntity.AddComponent<TransformComponent>(glm::translate(offset), true);
                lightTransform.globalMatrix = matrix * glm::translate(offset);
                AddLight(lightEntity, item.light);
                lightEntity.SetParent(entity);
            }

            if (item.body == "mesh") {
                AddMeshBody(entity, item.mesh, vec3(scale));
            }
            else if (item.body == "capsule") {
                // Trunks and posts: a feet aligned capsule, the crown stays passable
                Physics::CapsuleShapeSettings settings;
                settings.radius = item.bodyRadius * scale;
                settings.height = std::max(item.bodyHeight * scale - 2.0f * settings.radius, 0.01f);
                entity.AddComponent<RigidBodyComponent>(Physics::BodyCreationSettings {
                    .objectLayer = Physics::Layers::Static, .shape = Physics::ShapesManager::CreateShape(settings) });
            }

            placedGrid.Insert(point - vec2(radius), point + vec2(radius), { point, radius });
            created.push_back(entity);
            item.placed++;
        }

        // Children are added once all instances exist, creating components can invalidate component references.
        // The group is created below the parent (or the root, scenes without one get a top level group)
        Scene::Entity parent;
        if (spec.contains("parent")) {
            auto parents = FindEntities(scene, spec["parent"].get<std::string>());
            if (parents.empty())
                throw std::runtime_error("scatter: no parent matches " + spec["parent"].dump());
            parent = parents.front();
        }
        else {
            parent = scene.GetRootEntity();
        }

        auto group = existingGroup;
        if (!group.IsValid()) {
            group = scene.CreateEntity();
            group.AddComponent<NameComponent>(groupName);
            // Without a transform the group is in the space of its parent, the scene only updates it in the next frame
            group.AddComponent<HierarchyComponent>().globalMatrix = scene.GetGlobalMatrix(parent);
            group.SetParent(parent);
        }
        for (auto entity : created) {
            entity.SetParent(group);
            // The instances were placed in world space, groups (or their parents) may have a transform
            auto& transform = entity.GetComponent<TransformComponent>();
            transform.SetGlobal(transform.globalMatrix, group);
        }

        json placedItems = json::object();
        for (const auto& item : items)
            placedItems[item.name] = item.placed;

        json result = {
            { "group", GetEntityLabel(group) },
            { "placed", created.size() },
            { "requested", count },
            { "items", placedItems },
            { "rejected", rejected }
        };
        if (created.empty() && spec.value("required", false))
            throw std::runtime_error("scatter: nothing of '" + groupName + "' (" + items.front().name + ") could be placed, rejections: " +
                json(rejected).dump());
        if (int32_t(created.size()) < count)
            result["warning"] = "Only " + std::to_string(created.size()) + " of " + std::to_string(count) +
                " instances placed, the area is too small or too restricted";
        return result;

    }

}
