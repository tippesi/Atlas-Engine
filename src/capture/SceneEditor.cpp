#include "SceneEditor.h"
#include "Checks.h"

#include "common/MatrixDecomposition.h"
#include "common/Path.h"
#include "loader/MeshLoader.h"
#include "loader/ModelImporter.h"
#include "physics/ShapesManager.h"
#include "resource/ResourceManager.h"
#include "scene/components/LuaScriptComponent.h"
#include "scripting/Script.h"

#include <fstream>
#include <stdexcept>

namespace Atlas::Capture {

    using namespace Scene::Components;

    static std::vector<Scene::Entity> SelectEntities(Scene::Scene& scene, const json& operation) {

        auto pattern = operation.at("entity").get<std::string>();
        auto entities = RemoveDescendants(scene, FindEntities(scene, pattern));
        auto all = operation.value("all", false);

        if (entities.empty())
            throw std::runtime_error("No entity matches '" + pattern + "' in " + operation.dump());
        if (!all && entities.size() > 1) {
            std::string names;
            for (size_t i = 0; i < entities.size() && i < 10; i++)
                names += "\n  " + GetEntityLabel(entities[i]);
            throw std::runtime_error("'" + pattern + "' matches " + std::to_string(entities.size()) +
                " entities, set \"all\": true or use a more specific pattern / #id:" + names);
        }

        return entities;

    }

    static TransformComponent& GetTransform(Scene::Entity entity) {

        auto transform = entity.TryGetComponent<TransformComponent>();
        if (!transform)
            throw std::runtime_error(GetEntityLabel(entity) + " has no transform component");

        return *transform;

    }

    // World space matrix of the parent, derived from the current global and local matrices
    static mat4 GetParentMatrix(const TransformComponent& transform) {

        return transform.globalMatrix * glm::inverse(transform.matrix);

    }

    // Update the global matrix immediately for subsequent edits in the same batch
    static void SetGlobal(TransformComponent& transform, const mat4& parentMatrix, const mat4& global) {

        transform.Set(glm::inverse(parentMatrix) * global);
        transform.globalMatrix = global;

    }

    static void TranslateWorld(Scene::Entity entity, vec3 delta) {

        auto& transform = GetTransform(entity);
        SetGlobal(transform, GetParentMatrix(transform), glm::translate(delta) * transform.globalMatrix);

    }

    static Scene::Entity FindRoot(Scene::Scene& scene) {

        auto hierarchySubset = scene.GetSubset<HierarchyComponent>();
        for (auto entity : hierarchySubset)
            if (hierarchySubset.Get(entity).root)
                return { entity, &scene.entityManager };

        throw std::runtime_error("The scene has no root entity");

    }

    // Model files (glTF, FBX, ...) are imported next to the source into meshes/ and materials/ first,
    // scenes can only reference .aemesh files
    static ResourceHandle<Mesh::Mesh> LoadMeshForScene(const std::string& path) {

        auto meshPath = ToAssetPath(path);
        if (Common::Path::GetFileType(meshPath) != "aemesh") {
            auto imported = Loader::ModelImporter::ImportMesh(meshPath, true);
            if (!imported)
                throw std::runtime_error("Couldn't import " + meshPath);
            auto directory = Common::Path::GetDirectory(meshPath);
            meshPath = (directory.empty() ? "" : directory + "/") + "meshes/" + imported->name + ".aemesh";
        }

        auto mesh = ResourceManager<Mesh::Mesh>::GetOrLoadResourceWithLoader(
            meshPath, ResourceOrigin::User, Loader::MeshLoader::LoadMesh, true);
        if (!mesh.IsLoaded())
            throw std::runtime_error("Couldn't load mesh " + meshPath);

        return mesh;

    }

    json ApplyEditOperations(Scene::Scene& scene, const json& operations) {

        json log = json::array();

        for (const auto& operation : operations) {
            auto type = operation.at("op").get<std::string>();
            json entry;
            entry["operation"] = operation;
            json affected = json::array();

            if (type == "setTransform") {
                for (auto entity : SelectEntities(scene, operation)) {
                    auto& transform = GetTransform(entity);
                    // Needs to be derived before the local matrix changes
                    auto parentMatrix = GetParentMatrix(transform);
                    auto decomposition = transform.Decompose();

                    if (operation.contains("rotation"))
                        decomposition.rotation = glm::radians(ToVec3(operation["rotation"]));
                    if (operation.contains("scale"))
                        decomposition.scale = ToVec3(operation["scale"], vec3(1.0f));
                    if (operation.contains("localPosition"))
                        decomposition.translation = ToVec3(operation["localPosition"]);

                    auto global = parentMatrix * decomposition.Compose();
                    // World positions are converted with the parent transform
                    if (operation.contains("position"))
                        global[3] = vec4(ToVec3(operation["position"]), 1.0f);
                    SetGlobal(transform, parentMatrix, global);

                    affected.push_back(GetEntityLabel(entity));
                }
            }
            else if (type == "translate") {
                auto delta = ToVec3(operation.at("delta"));
                auto space = operation.value("space", std::string("world"));
                for (auto entity : SelectEntities(scene, operation)) {
                    if (space == "world") {
                        TranslateWorld(entity, delta);
                    }
                    else if (space == "local") {
                        auto& transform = GetTransform(entity);
                        transform.Set(transform.matrix * glm::translate(delta));
                    }
                    else {
                        throw std::runtime_error("translate.space must be world or local");
                    }
                    affected.push_back(GetEntityLabel(entity));
                }
            }
            else if (type == "rotate") {
                // Rotate around the world Y axis through the entity origin
                auto yaw = glm::radians(operation.at("yaw").get<float>());
                for (auto entity : SelectEntities(scene, operation)) {
                    auto& transform = GetTransform(entity);
                    auto origin = vec3(transform.globalMatrix[3]);
                    auto rotation =
                        glm::translate(origin) * glm::rotate(yaw, vec3(0.0f, 1.0f, 0.0f)) * glm::translate(-origin);
                    SetGlobal(transform, GetParentMatrix(transform), rotation * transform.globalMatrix);
                    affected.push_back(GetEntityLabel(entity));
                }
            }
            else if (type == "snapToGround") {
                auto offset = operation.value("offset", 0.0f);
                for (auto entity : SelectEntities(scene, operation)) {
                    EntitySet ignored;
                    CollectHierarchy(entity, ignored);
                    auto bounds = GetEntityBounds(entity);
                    auto center = bounds.GetCenter();
                    auto ground = GroundBelow(
                        scene, vec3(center.x, bounds.max.y + operation.value("searchAbove", 0.5f), center.z), ignored);
                    if (!ground.valid)
                        throw std::runtime_error("No ground below " + GetEntityLabel(entity));

                    auto delta = ground.position.y + offset - bounds.min.y;
                    TranslateWorld(entity, vec3(0.0f, delta, 0.0f));
                    affected.push_back({ { "entity", GetEntityLabel(entity) }, { "deltaY", delta },
                        { "ground", ground.entity.IsValid() ? GetEntityLabel(ground.entity) : ground.kind } });
                }
            }
            else if (type == "rename") {
                for (auto entity : SelectEntities(scene, operation)) {
                    auto nameComponent = entity.TryGetComponent<NameComponent>();
                    if (!nameComponent)
                        throw std::runtime_error(GetEntityLabel(entity) + " has no name component");
                    affected.push_back(GetEntityLabel(entity));
                    nameComponent->name = operation.at("name").get<std::string>();
                }
            }
            else if (type == "setVisible") {
                for (auto entity : SelectEntities(scene, operation)) {
                    EntitySet hierarchy;
                    CollectHierarchy(entity, hierarchy);
                    for (auto member : hierarchy) {
                        Scene::Entity memberEntity = { member, &scene.entityManager };
                        if (auto mesh = memberEntity.TryGetComponent<MeshComponent>())
                            mesh->visible = operation.at("visible").get<bool>();
                    }
                    affected.push_back(GetEntityLabel(entity));
                }
            }
            else if (type == "duplicate") {
                for (auto entity : SelectEntities(scene, operation)) {
                    auto copy = scene.DuplicateEntity(entity);

                    // Keep the duplicate in the same place of the hierarchy, the local matrix refers to the parent
                    auto parent = scene.GetParentEntity(entity);
                    if (parent.IsValid())
                        parent.GetComponent<HierarchyComponent>().AddChild(copy);

                    if (operation.contains("name")) {
                        if (auto nameComponent = copy.TryGetComponent<NameComponent>())
                            nameComponent->name = operation["name"].get<std::string>();
                        else
                            copy.AddComponent<NameComponent>(operation["name"].get<std::string>());
                    }

                    if (operation.contains("position")) {
                        auto& original = GetTransform(entity);
                        auto& transform = GetTransform(copy);
                        // The copy has no updated global matrix yet, it shares the parent with the original
                        auto global = original.globalMatrix;
                        global[3] = vec4(ToVec3(operation["position"]), 1.0f);
                        SetGlobal(transform, GetParentMatrix(original), global);
                    }

                    affected.push_back(GetEntityLabel(copy));
                }
            }
            else if (type == "material") {
                // Keep unknown JSON fields when patching the material file
                auto materialPath = ToAssetPath(operation.at("material").get<std::string>());
                auto file = GetAssetDirectory() / materialPath;
                std::ifstream input(file);
                if (!input.is_open())
                    throw std::runtime_error("Couldn't open material " + file.string());
                auto material = json::parse(input);
                input.close();

                auto changes = operation.at("set");
                auto before = material;
                material.merge_patch(changes);

                auto backup = file;
                backup += ".backup";
                if (!fs::exists(backup))
                    fs::copy_file(file, backup);

                std::ofstream output(file);
                output << material.dump();
                output.close();

                ApplyMaterialPatches(json{ { materialPath, changes } });

                json changed = json::object();
                for (auto& [key, value] : changes.items())
                    changed[key] = { { "before", before.value(key, json()) }, { "after", value } };
                affected.push_back(
                    { { "material", materialPath }, { "changes", changed }, { "backup", backup.string() } });
            }
            else if (type == "delete") {
                for (auto entity : SelectEntities(scene, operation)) {
                    affected.push_back(GetEntityLabel(entity));
                    scene.DestroyEntity(entity);
                }
            }
            else if (type == "add") {
                auto name = operation.at("name").get<std::string>();
                if (scene.GetEntityByName(name).IsValid())
                    throw std::runtime_error("An entity named '" + name + "' already exists");

                auto parent = operation.contains("parent") ?
                    SelectEntities(scene, json{ { "entity", operation["parent"] } }).front() :
                    FindRoot(scene);
                auto parentHierarchy = parent.TryGetComponent<HierarchyComponent>();
                if (!parentHierarchy)
                    parentHierarchy = &parent.AddComponent<HierarchyComponent>();
                auto parentTransform = parent.TryGetComponent<TransformComponent>();
                auto parentMatrix = parentTransform ? parentTransform->globalMatrix : mat4(1.0f);

                Common::MatrixDecomposition decomposition;
                decomposition.translation = ToVec3(operation.value("position", json()));
                decomposition.rotation = glm::radians(ToVec3(operation.value("rotation", json())));
                decomposition.scale = ToVec3(operation.value("scale", json()), vec3(1.0f));
                auto global = decomposition.Compose();

                ResourceHandle<Mesh::Mesh> mesh;
                if (operation.contains("mesh"))
                    mesh = LoadMeshForScene(operation["mesh"].get<std::string>());

                auto hasScript = operation.contains("script");
                auto local = glm::inverse(parentMatrix) * global;
                Scene::Entity entity;
                if (mesh.IsValid()) {
                    entity = scene.CreatePrefab<Scene::Prefabs::MeshInstance>(mesh, local, !hasScript);
                }
                else {
                    entity = scene.CreateEntity();
                    entity.AddComponent<TransformComponent>(local, !hasScript);
                }
                entity.AddComponent<NameComponent>(name);
                auto& transform = entity.GetComponent<TransformComponent>();
                transform.globalMatrix = global;
                parentHierarchy->AddChild(entity);

                json added = { { "entity", GetEntityLabel(entity) } };
                if (mesh.IsValid()) {
                    added["mesh"] = mesh.GetResource()->path;

                    // Use the world scale for the collision mesh
                    if (operation.value("body", false)) {
                        Physics::MeshShapeSettings settings = { .mesh = mesh,
                            .scale = transform.DecomposeGlobal().scale };
                        auto bodySettings = Physics::BodyCreationSettings {
                            .objectLayer = Physics::Layers::Static,
                            .shape = Physics::ShapesManager::CreateShape(settings),
                        };
                        entity.AddComponent<RigidBodyComponent>(bodySettings);
                    }
                }
                else if (operation.contains("body")) {
                    throw std::runtime_error("add: a body requires a mesh");
                }

                // Main camera looking along +Z, the editor needs one to start playing
                if (operation.contains("camera")) {
                    const auto& spec = operation["camera"];
                    auto& camera = entity.AddComponent<CameraComponent>(
                        spec.value("fov", 70.0f), 16.0f / 9.0f, spec.value("near", 0.1f), spec.value("far", 2000.0f));
                    camera.isMain = spec.value("main", true);
                    added["camera"] = camera.isMain ? "main" : "secondary";
                }

                // Point and spot lights at the entity origin, the spot direction is in entity space
                if (operation.contains("light")) {
                    const auto& spec = operation["light"];
                    auto lightType = spec.value("type", std::string("point"));
                    if (lightType != "point" && lightType != "spot")
                        throw std::runtime_error("add: light.type must be point or spot");

                    auto& light = entity.AddComponent<LightComponent>(
                        lightType == "point" ? LightType::PointLight : LightType::SpotLight,
                        LightMobility::StationaryLight);
                    light.color = ToVec3(spec.value("color", json()), vec3(1.0f));
                    light.intensity = spec.value("intensity", 1.0f);
                    light.volumetric = spec.value("volumetric", true);
                    auto radius = spec.value("radius", 20.0f);
                    auto shadow = spec.value("shadow", false);
                    if (lightType == "point") {
                        light.properties.point.radius = radius;
                        if (shadow)
                            light.AddPointShadow(0.25f, 1024);
                    }
                    else {
                        light.properties.spot.radius = radius;
                        light.properties.spot.direction =
                            glm::normalize(ToVec3(spec.value("direction", json()), vec3(0.0f, -1.0f, 0.0f)));
                        light.properties.spot.innerConeAngle = glm::radians(spec.value("innerAngle", 25.0f));
                        light.properties.spot.outerConeAngle = glm::radians(spec.value("outerAngle", 40.0f));
                        if (shadow)
                            light.AddSpotShadow(2.0f, 1024);
                    }
                    added["light"] = lightType;
                }

                if (hasScript) {
                    auto scriptPath = ToAssetPath(operation["script"].get<std::string>());
                    auto script =
                        ResourceManager<Scripting::Script>::GetOrLoadResource(scriptPath, ResourceOrigin::User);
                    if (!script.IsLoaded())
                        throw std::runtime_error("Couldn't load script " + scriptPath);
                    entity.AddComponent<LuaScriptComponent>(script);
                    added["script"] = scriptPath;
                }

                affected.push_back(added);
            }
            else {
                throw std::runtime_error("Unknown edit operation '" + type +
                    "' (setTransform, translate, rotate, " "snapToGround, rename, setVisible, duplicate, delete, material, add)");
            }

            entry["affected"] = affected;
            log.push_back(entry);
        }

        return log;

    }

    void SetWorldPosition(Scene::Entity entity, vec3 position) {

        auto& transform = GetTransform(entity);
        TranslateWorld(entity, position - vec3(transform.globalMatrix[3]));

    }

    json DiffInspections(const json& before, const json& after) {

        std::map<uint64_t, const json*> beforeEntities, afterEntities;
        for (const auto& entity : before["entities"])
            beforeEntities[entity["id"].get<uint64_t>()] = &entity;
        for (const auto& entity : after["entities"])
            afterEntities[entity["id"].get<uint64_t>()] = &entity;

        static const std::vector<std::string> fields = { "name", "parent", "components", "position", "localPosition",
            "localRotationDegrees", "localScale", "mesh", "visible", "script" };

        json diff;
        json changed = json::array(), added = json::array(), removed = json::array();

        for (const auto& [id, entity] : beforeEntities) {
            auto it = afterEntities.find(id);
            if (it == afterEntities.end()) {
                removed.push_back({ { "id", id }, { "name", (*entity)["name"] } });
                continue;
            }

            json changes = json::object();
            for (const auto& field : fields) {
                auto beforeValue = entity->value(field, json());
                auto afterValue = it->second->value(field, json());
                if (beforeValue != afterValue)
                    changes[field] = { { "before", beforeValue }, { "after", afterValue } };
            }

            if (!changes.empty())
                changed.push_back({ { "id", id }, { "name", (*it->second)["name"] }, { "changes", changes } });
        }

        for (const auto& [id, entity] : afterEntities) {
            if (!beforeEntities.contains(id))
                added.push_back(
                    { { "id", id }, { "name", (*entity)["name"] }, { "position", entity->value("position", json()) } });
        }

        diff["changed"] = changed;
        diff["added"] = added;
        diff["removed"] = removed;
        diff["summary"] = std::to_string(changed.size()) + " changed, " + std::to_string(added.size()) + " added, " +
            std::to_string(removed.size()) + " removed";

        return diff;

    }

}
