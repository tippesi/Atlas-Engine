#include "SceneSerializer.h"

#include "lighting/LightingSerializer.h"
#include "audio/AudioSerializer.h"
#include "postprocessing/PostProcessingSerializer.h"
#include "physics/PhysicsSerializer.h"
#include "ocean/OceanSerializer.h"
#include "navigation/NavigationSerializer.h"

#include "loader/TerrainLoader.h"
#include "common/SerializationContext.h"
#include "Serializer.h"
#include "Log.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <tuple>

namespace Atlas::Scene {

    template<class T>
    struct ComponentSection {
        using Type = T;
        const char* key;
        const char* name;
    };

    // All serialized components with their key, entities and sections read them in this order
    static constexpr auto componentSections = std::make_tuple(
        ComponentSection<NameComponent>{ "name", "Name" },
        ComponentSection<TransformComponent>{ "transform", "Transform" },
        ComponentSection<MeshComponent>{ "mesh", "Mesh" },
        ComponentSection<LightComponent>{ "light", "Light" },
        ComponentSection<CameraComponent>{ "camera", "Camera" },
        ComponentSection<AudioComponent>{ "audio", "Audio" },
        ComponentSection<AudioVolumeComponent>{ "audioVolume", "Audio volume" },
        ComponentSection<FogVolumeComponent>{ "fogVolume", "Fog volume" },
        ComponentSection<RigidBodyComponent>{ "rigidBody", "Rigid body" },
        ComponentSection<SoftBodyComponent>{ "softBody", "Soft body" },
        ComponentSection<PlayerComponent>{ "player", "Player" },
        ComponentSection<ConstraintComponent>{ "constraint", "Constraint" },
        ComponentSection<TextComponent>{ "text", "Text" },
        ComponentSection<LuaScriptComponent>{ "script", "Script" },
        ComponentSection<SplineComponent>{ "spline", "Spline" },
        ComponentSection<AnimationComponent>{ "animation", "Animation" }
    );

    template<class Func>
    static void ForEachComponentSection(Func&& func) {

        std::apply([&](const auto&... sections) { (func(sections), ...); }, componentSections);

    }

    const std::vector<ComponentType>& GetComponentTypes() {

        static const std::vector<ComponentType> types = []() {
            std::vector<ComponentType> types;
            ForEachComponentSection([&](const auto& section) { types.push_back({ section.key, section.name }); });
            return types;
        }();
        return types;

    }

    uint32_t GetComponentMask(const Entity& p) {

        uint32_t mask = 0;
        uint32_t bit = 0;
        ForEachComponentSection([&](const auto& section) {
            using T = typename std::decay_t<decltype(section)>::Type;
            if (p.HasComponent<T>())
                mask |= 1u << bit;
            bit++;
            });
        return mask;

    }

    std::vector<ECS::Storage> CopyComponentStorages(Scene* scene) {

        std::vector<ECS::Storage> storages;
        ForEachComponentSection([&](const auto& section) {
            using T = typename std::decay_t<decltype(section)>::Type;
            storages.push_back(scene->entityManager.GetPool<T>());
            });
        return storages;

    }

    void RemoveComponent(Entity& p, const std::string& key) {

        ForEachComponentSection([&](const auto& section) {
            using T = typename std::decay_t<decltype(section)>::Type;
            if (key == section.key && p.HasComponent<T>())
                p.RemoveComponent<T>();
            });

    }

    template<class T>
    static T ComponentValue(const Entity& p, Scene* scene) {

        if constexpr (std::is_same_v<T, TransformComponent>) {
            // Create a copy and update local matrix (physics always just update global matrices)
            auto transformComponent = p.GetComponent<TransformComponent>();
            // Assigned directly, Set() would mark the entity of the copy dirty
            transformComponent.matrix = transformComponent.GetReconstructedLocalMatrix(scene->GetParentEntity(p));
            return transformComponent;
        }
        else {
            return p.GetComponent<T>();
        }

    }

    template<class T>
    static json ComponentToJson(const Entity& p, Scene* scene) {

        if constexpr (std::is_same_v<T, TransformComponent>)
            return ComponentValue<T>(p, scene);
        else
            return p.GetComponent<T>();

    }

    // Index lists are arrays in readable scenes and raw values in binary ones
    template<class T>
    static std::vector<T> ReadIndices(const json& j) {

        std::vector<T> values;
        if (j.is_binary()) {
            const auto& data = j.get_binary();
            values.resize(data.size() / sizeof(T));
            std::memcpy(values.data(), data.data(), values.size() * sizeof(T));
        }
        else {
            j.get_to(values);
        }
        return values;

    }

    void ComponentsToJson(json& j, const Entity& p, Scene* scene) {

        ForEachComponentSection([&]<class T>(const ComponentSection<T>& section) {
            if (p.HasComponent<T>())
                j[section.key] = ComponentToJson<T>(p, scene);
        });

    }

    void EntityToJson(json& j, const Entity& p, Scene* scene,
        std::set<ECS::Entity>& insertedEntities) {

        if (!p.IsValid())
            return;

        AE_ASSERT(!insertedEntities.contains(p) && "Entity should only be present once in the hierarchy");

        if (!insertedEntities.contains(p))
            insertedEntities.insert(p);

        j["id"] = size_t(p);

        ComponentsToJson(j, p, scene);

        if (p.HasComponent<HierarchyComponent>()) {
            // Need to check how to get this to work
            auto& hierarchyComponent = p.GetComponent<HierarchyComponent>();
            j["entities"] = json::array();
            auto& entities = j["entities"];

            for (auto entity : hierarchyComponent.GetChildren()) {
                entities.emplace_back();
                EntityToJson(entities.back(), entity, scene, insertedEntities);
            }
            j["root"] = hierarchyComponent.root;
        }
    }

    static void EntityFromJson(const json& j, Entity& p, Scene* scene, bool containsEntityManager,
        std::unordered_map<ECS::Entity, ECS::Entity>& fileToSceneIds);

    void EntityFromJson(const json& j, Entity& p, Scene* scene, bool containsEntityManager) {

        std::unordered_map<ECS::Entity, ECS::Entity> fileToSceneIds;
        EntityFromJson(j, p, scene, containsEntityManager, fileToSceneIds);
        if (containsEntityManager)
            return;

        // Fresh entities got new ids, constraints between them follow, others keep the id they had
        for (auto [fileId, sceneId] : fileToSceneIds) {
            auto constraint = scene->entityManager.TryGet<ConstraintComponent>(sceneId);
            if (!constraint)
                continue;
            auto connectedId = ECS::Entity(constraint->GetConnectedEntity());
            if (fileToSceneIds.contains(connectedId))
                constraint->SetConnectedEntity(Entity(fileToSceneIds[connectedId], &scene->entityManager));
        }

    }

    static void EntityFromJson(const json& j, Entity& p, Scene* scene, bool containsEntityManager,
        std::unordered_map<ECS::Entity, ECS::Entity>& fileToSceneIds) {

        if (auto id = j.find("id"); id != j.end())
            fileToSceneIds[id->get<ECS::Entity>()] = p;

        ForEachComponentSection([&]<class T>(const ComponentSection<T>& section) {
            auto it = j.find(section.key);
            if (it == j.end())
                return;
            T comp = *it;
            p.AddComponent<T>(std::move(comp));
        });

        if (auto children = j.find("entities"); children != j.end()) {
            // We need to first push back to a temporary vector to not invalidate
            // component references (i.e. when directly getting the hierarchy component).
            // That way all children will have components created before the parent creates its own
            std::vector<Entity> entities;
            entities.reserve(children->size());
            for (const auto& jEntity : *children) {
                Entity entity;
                if (containsEntityManager)
                    entity = Entity(jEntity["id"], &scene->entityManager);
                else
                    entity = scene->CreateEntity();
                EntityFromJson(jEntity, entity, scene, containsEntityManager, fileToSceneIds);
                entities.push_back(entity);
            }

            auto& comp = p.AddComponent<HierarchyComponent>();
            comp.root = j["root"];
            for (auto entity : entities) {
                comp.AddChild(entity);
            }
        }
    }

    // Binary files of scenes with binary columns store the index lists and the components with columns as raw values
    static bool WritesBinaryColumns(const Scene* scene) {

        return SerializationContext::IsWriting() && scene->binaryColumns;

    }

    // Sections are checked once, invalid files would otherwise access entities outside of the table
    static bool IsValidSection(const std::string& key, const std::vector<uint32_t>& indices, size_t valueCount, size_t entityCount) {

        auto valid = indices.size() == valueCount &&
            std::all_of(indices.begin(), indices.end(), [&](uint32_t index) { return index < entityCount; });
        if (!valid)
            Log::Error("Invalid scene component section " + key);
        return valid;

    }

    static bool IsValidEntityTable(const std::vector<uint64_t>& ids, const std::vector<int32_t>& parents) {

        if (ids.size() == parents.size())
            return true;

        Log::Error("Scene entity table has a different number of ids and parents");
        return false;

    }

    static void EntitiesToSections(json& j, Scene* scene) {

        // Parents come before their children, siblings keep their order
        std::vector<Entity> entities;
        std::vector<int32_t> parents;
        // Table index per entity slot, UINT32_MAX for entities that aren't in the table
        std::vector<uint32_t> tableIndices;
        auto tableIndex = [&](const Entity& entity) -> uint32_t& {
            auto slot = ECS::EntityToIdx(entity);
            if (slot >= tableIndices.size())
                tableIndices.resize(slot + 1, UINT32_MAX);
            return tableIndices[slot];
        };

        std::function<void(const Entity&, int32_t)> insert = [&](const Entity& entity, int32_t parent) {
            if (!entity.IsValid())
                return;

            AE_ASSERT(tableIndex(entity) == UINT32_MAX && "Entity should only be present once in the hierarchy");

            auto index = int32_t(entities.size());
            tableIndex(entity) = uint32_t(index);
            entities.push_back(entity);
            parents.push_back(parent);

            if (auto hierarchy = entity.TryGetComponent<HierarchyComponent>())
                for (auto child : hierarchy->GetChildren())
                    insert(child, index);
        };

        auto hierarchySubset = scene->GetSubset<HierarchyComponent>();
        for (auto entity : hierarchySubset) {
            auto hierarchy = hierarchySubset.Get(entity);
            // Serialize from root recursively. Also means free floating hierarchies without a
            // root will be ignored.
            if (!hierarchy.root) continue;
            insert(entity, -1);
        }

        for (auto entity : *scene) {
            if (!entity.IsValid())
                continue;
            // No need here, since it was already inserted through the hierarchy
            if (tableIndex(entity) != UINT32_MAX)
                continue;
            // Children are written by their parent. Reused entity slots can come before the slot of their parent,
            // they would otherwise be written twice and loaded as two entities
            if (scene->GetParentEntity(entity).IsValid())
                continue;

            insert(entity, -1);
        }

        auto binary = WritesBinaryColumns(scene);

        std::vector<uint64_t> ids;
        ids.reserve(entities.size());
        for (const auto& entity : entities)
            ids.push_back(uint64_t(entity));

        auto& table = j["entities"];
        if (binary) {
            set_binary_json(table, "id", ids);
            set_binary_json(table, "parent", parents);
        }
        else {
            table["id"] = ids;
            table["parent"] = parents;
        }

        auto writeSection = [&]<class T>(const char* key, auto&& toJson) {
            // Only the entities with the component are visited, they are usually in table order already
            std::vector<uint32_t> indices;
            for (auto entity : scene->GetSubset<T>()) {
                auto slot = ECS::EntityToIdx(entity);
                if (slot < tableIndices.size() && tableIndices[slot] != UINT32_MAX)
                    indices.push_back(tableIndices[slot]);
            }
            if (indices.empty())
                return;
            if (!std::is_sorted(indices.begin(), indices.end()))
                std::sort(indices.begin(), indices.end());

            auto& section = j["components"][key];
            if (binary)
                set_binary_json(section, "entity", indices);
            else
                section["entity"] = indices;

            if constexpr (ComponentColumns<T>::available) {
                if (binary) {
                    T value;
                    ComponentColumns<T>::Write(section["columns"], indices.size(), [&](size_t i) -> const T& {
                        const auto& entity = entities[indices[i]];
                        if constexpr (std::is_same_v<T, TransformComponent>) {
                            value = ComponentValue<T>(entity, scene);
                            return value;
                        }
                        else {
                            return entity.GetComponent<T>();
                        }
                    });
                    return;
                }
            }

            json values = json::array();
            values.get_ref<json::array_t&>().reserve(indices.size());
            for (auto index : indices)
                values.push_back(toJson(entities[index]));
            section["values"] = std::move(values);
        };

        ForEachComponentSection([&]<class T>(const ComponentSection<T>& section) {
            writeSection.template operator()<T>(section.key, [&](const Entity& entity) { return ComponentToJson<T>(entity, scene); });
        });
        // The children are known from the parents of the entities
        writeSection.template operator()<HierarchyComponent>("hierarchy", [](const Entity& entity) {
            return json(entity.GetComponent<HierarchyComponent>().root);
        });

    }

    static void EntitiesFromSections(const json& j, Scene* scene, bool containsEntityManager) {

        const auto& table = j.at("entities");
        auto ids = ReadIndices<uint64_t>(table.at("id"));
        auto parents = ReadIndices<int32_t>(table.at("parent"));
        if (!IsValidEntityTable(ids, parents))
            return;

        std::vector<Entity> entities;
        entities.reserve(ids.size());
        for (auto id : ids)
            entities.push_back(containsEntityManager ? Entity(ECS::Entity(id), &scene->entityManager) : scene->CreateEntity());

        auto components = j.find("components");
        if (components == j.end())
            return;

        ForEachComponentSection([&]<class T>(const ComponentSection<T>& section) {
            auto it = components->find(section.key);
            if (it == components->end())
                return;

            auto indices = ReadIndices<uint32_t>(it->at("entity"));
            if constexpr (ComponentColumns<T>::available) {
                if (auto columns = it->find("columns"); columns != it->end()) {
                    std::vector<T> values;
                    if (!ComponentColumns<T>::Read(*columns, values) ||
                        !IsValidSection(section.key, indices, values.size(), entities.size()))
                        return;
                    for (size_t i = 0; i < indices.size(); i++)
                        entities[indices[i]].AddComponent<T>(std::move(values[i]));
                    return;
                }
            }

            const auto& values = it->at("values");
            if (!IsValidSection(section.key, indices, values.size(), entities.size()))
                return;
            for (size_t i = 0; i < indices.size(); i++) {
                T comp = values[i];
                entities[indices[i]].AddComponent<T>(std::move(comp));
            }
        });

        // All hierarchies exist before children are added, adding components could move them in memory
        auto hierarchies = components->find("hierarchy");
        if (hierarchies == components->end())
            return;

        auto indices = ReadIndices<uint32_t>(hierarchies->at("entity"));
        const auto& values = hierarchies->at("values");
        if (!IsValidSection("hierarchy", indices, values.size(), entities.size()))
            return;
        for (size_t i = 0; i < indices.size(); i++)
            entities[indices[i]].AddComponent<HierarchyComponent>().root = values[i].get<bool>();

        for (size_t i = 0; i < entities.size(); i++) {
            auto parent = parents[i];
            if (parent < 0)
                continue;

            // Parents come before their children in the table
            auto hierarchy = parent < int32_t(i) ? entities[parent].TryGetComponent<HierarchyComponent>() : nullptr;
            if (!hierarchy) {
                Log::Error("Scene entity " + std::to_string(i) + " has an invalid parent");
                continue;
            }
            hierarchy->AddChild(entities[i]);
        }

    }

    void SceneToJson(json& j, Scene* scene) {

        EntitiesToSections(j, scene);

        EntityManagerToJson(j["entityManager"], scene->entityManager);

        // Parse all mandatory members
        j["name"] = scene->name;
        j["aabb"] = scene->aabb;
        j["depth"] = scene->depth;

        SceneSettingsToJson(j, scene);

        // Parse all optional members
        if (scene->terrain.IsValid() && !scene->terrain.IsGenerated())
            j["terrain"] = scene->terrain.GetResource()->path;

        if (scene->physicsWorld)
            Physics::SerializePhysicsWorld(j["physicsWorld"], scene->physicsWorld);


    }

    void SceneFromJson(const json& j, Ref<Scene>& scene) {

        std::optional<SerializationContext> context;
        if (j.contains("resources"))
            context.emplace(j["resources"]);

        Volume::AABB aabb = j["aabb"];
        scene = CreateRef<Scene>(j["name"], aabb.min, aabb.max, j["depth"]);

         // Create physics world in any case (and before entities, such that they can push bodies immediately)
        scene->physicsWorld = CreateRef<Physics::PhysicsWorld>();
        scene->physicsWorld->pauseSimulation = true;

        bool containsEntityManager = j.contains("entityManager");
        if (containsEntityManager) {
            EntityManagerFromJson(j["entityManager"], scene->entityManager);
        }

        if (j.contains("physicsWorld")) {
            std::unordered_map<uint32_t, Physics::BodyCreationSettings> bodyCreationMap;
            Physics::DeserializePhysicsWorld(j["physicsWorld"], bodyCreationMap);
        }
        if (j.contains("terrain")) {
            scene->terrain = ResourceManager<Terrain::Terrain>::GetOrLoadResourceWithLoaderAsync(
                j["terrain"], Loader::TerrainLoader::LoadTerrain, false);
        }

        // Older scenes nest the entities like prefabs
        if (j["entities"].is_object()) {
            EntitiesFromSections(j, scene.get(), containsEntityManager);
        }
        else {
            for (const auto& jEntity : j["entities"]) {
                Entity entity;
                if (containsEntityManager)
                    entity = Entity(jEntity["id"], &scene->entityManager);
                else
                    entity = scene->CreateEntity();

                EntityFromJson(jEntity, entity, scene.get(), containsEntityManager);
            }
        }

        SceneSettingsFromJson(j, scene.get());

        scene->rayTracingWorld = CreateRef<RayTracing::RayTracingWorld>();

        scene->physicsWorld->OptimizeBroadphase();

    }

    void SceneSettingsToJson(json& j, const Scene* scene) {

        j["sky"] = scene->sky;
        j["postProcessing"] = scene->postProcessing;
        j["wind"] = scene->wind;
        j["occlusionCulling"] = scene->occlusionCulling;
        j["meshLodErrorScale"] = scene->meshLodErrorScale;
        j["binaryColumns"] = scene->binaryColumns;

        if (scene->fog)
            j["fog"] = *scene->fog;
        if (scene->irradianceVolume)
            j["irradianceVolume"] = *scene->irradianceVolume;
        if (scene->reflection)
            j["reflection"] = *scene->reflection;
        if (scene->rtgi)
            j["rtgi"] = *scene->rtgi;
        if (scene->sss)
            j["sss"] = *scene->sss;
        if (scene->ssgi)
            j["ssgi"] = *scene->ssgi;
        if (scene->ocean)
            j["ocean"] = *scene->ocean;
        if (scene->navigation)
            j["navigation"] = *scene->navigation;

    }

    static void ReadSceneSettings(const json& j, Scene* scene, bool mergePatch) {

        auto readSettings = [&]<class T>(const char* key, T& settings) {
            if (!j.contains(key))
                return;

            // Read into the existing settings, such that e.g. the clouds keep their textures
            if (!mergePatch) {
                j[key].get_to(settings);
                return;
            }

            json merged = settings;
            merged.merge_patch(j[key]);
            merged.get_to(settings);
        };

        auto readFeature = [&]<class T>(const char* key, Ref<T>& feature) {
            if (!j.contains(key))
                return;

            // Null removes an optional feature
            if (j[key].is_null()) {
                feature = nullptr;
                return;
            }

            if (!feature)
                feature = CreateRef<T>();
            readSettings(key, *feature);
        };

        readSettings("sky", scene->sky);
        readSettings("postProcessing", scene->postProcessing);
        readSettings("wind", scene->wind);
        readSettings("occlusionCulling", scene->occlusionCulling);
        readSettings("meshLodErrorScale", scene->meshLodErrorScale);
        readSettings("binaryColumns", scene->binaryColumns);

        readFeature("fog", scene->fog);
        readFeature("irradianceVolume", scene->irradianceVolume);
        readFeature("reflection", scene->reflection);
        readFeature("rtgi", scene->rtgi);
        readFeature("sss", scene->sss);
        readFeature("ssgi", scene->ssgi);
        readFeature("ocean", scene->ocean);
        readFeature("navigation", scene->navigation);

    }

    void SceneSettingsFromJson(const json& j, Scene* scene) {

        ReadSceneSettings(j, scene, false);

    }

    void PatchSceneSettings(const json& patch, Scene* scene) {

        ReadSceneSettings(patch, scene, true);

    }

    void EntityManagerToJson(json& j, const ECS::EntityManager& p) {

        j = json{
            {"entities", p.entities},
            {"destroyed", p.destroyed}
        };

    }

    void EntityManagerFromJson(const json& j, ECS::EntityManager& p) {

        try_get_json(j, "entities", p.entities);
        try_get_json(j, "destroyed", p.destroyed);

    }

    void to_json(json& j, const Wind& p) {

        j = json{
            {"direction", p.direction},
            {"speed", p.speed}
        };

    }

    void from_json(const json& j, Wind& p) {

        try_get_json(j, "direction", p.direction);
        try_get_json(j, "speed", p.speed);

    }

}

namespace Atlas {

    void Serializer::NestSceneEntities(json& j) {

        auto table = j.find("entities");
        if (table == j.end() || !table->is_object())
            return;

        auto ids = Scene::ReadIndices<uint64_t>(table->at("id"));
        auto parents = Scene::ReadIndices<int32_t>(table->at("parent"));
        if (!Scene::IsValidEntityTable(ids, parents))
            return;

        std::vector<json> entities(ids.size());
        for (size_t i = 0; i < entities.size(); i++)
            entities[i]["id"] = ids[i];

        if (auto components = j.find("components"); components != j.end()) {
            for (auto& [key, section] : components->items()) {
                // Columns become the json objects of a binary scene without columns
                if (auto columns = section.find("columns"); columns != section.end()) {
                    Scene::ForEachComponentSection([&]<class T>(const Scene::ComponentSection<T>& componentSection) {
                        if constexpr (Scene::Components::ComponentColumns<T>::available) {
                            if (key == componentSection.key)
                                section["values"] = Scene::Components::ComponentColumns<T>::ToValues(*columns);
                        }
                    });
                }

                auto values = section.find("values");
                auto indices = Scene::ReadIndices<uint32_t>(section.at("entity"));
                if (values == section.end() || !Scene::IsValidSection(key, indices, values->size(), entities.size()))
                    continue;

                for (size_t i = 0; i < indices.size(); i++) {
                    auto& entity = entities[indices[i]];
                    if (key == "hierarchy") {
                        entity["root"] = (*values)[i];
                        entity["entities"] = json::array();
                    }
                    else {
                        entity[key] = std::move((*values)[i]);
                    }
                }
            }
            j.erase(components);
        }

        // Children come after their parents: going backwards, every entity is complete before it moves into its parent
        json roots = json::array();
        for (size_t i = entities.size(); i-- > 0;) {
            auto& entity = entities[i];
            if (auto children = entity.find("entities"); children != entity.end())
                std::reverse(children->begin(), children->end());

            auto parent = parents[i];
            auto hasParent = parent >= 0 && parent < int32_t(i) && entities[parent].contains("entities");
            if (parent >= 0 && !hasParent)
                Log::Error("Scene entity " + std::to_string(i) + " has an invalid parent");

            auto& siblings = hasParent ? entities[parent]["entities"] : roots;
            siblings.push_back(std::move(entity));
        }
        std::reverse(roots.begin(), roots.end());
        j["entities"] = std::move(roots);

    }

}
