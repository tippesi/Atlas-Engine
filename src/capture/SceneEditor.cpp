#include "SceneEditor.h"
#include "Checks.h"
#include "Scatter.h"
#include "TerrainTools.h"

#include "common/MatrixDecomposition.h"
#include "common/Path.h"
#include "loader/AssetReferences.h"
#include "loader/MaterialLoader.h"
#include "loader/MeshLoader.h"
#include "loader/AnimationLoader.h"
#include "loader/ModelImporter.h"
#include "mesh/MeshSerializer.h"
#include "physics/ShapesManager.h"
#include "resource/ResourceManager.h"
#include "scene/components/ComponentSerializer.h"
#include "scene/components/LuaScriptComponent.h"
#include "scripting/Script.h"
#include "Serializer.h"

#include <fstream>
#include <functional>
#include <stdexcept>

namespace Atlas::Capture {

    using namespace Scene::Components;

    static std::vector<Scene::Entity> SelectEntities(Scene::Scene& scene, const json& operation) {

        auto pattern = operation.at("entity").get<std::string>();
        auto entities = FindTopmostEntities(scene, pattern);
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

    static void TranslateWorld(Scene::Entity entity, vec3 delta) {

        auto& transform = GetTransform(entity);
        transform.SetGlobal(glm::translate(delta) * transform.globalMatrix, entity.GetScene()->GetParentEntity(entity));

    }

    Scene::Entity FindRoot(Scene::Scene& scene) {

        auto hierarchySubset = scene.GetSubset<HierarchyComponent>();
        for (auto entity : hierarchySubset)
            if (hierarchySubset.Get(entity).root)
                return { entity, &scene.entityManager };

        return { ECS::EntityConfig::InvalidEntity, &scene.entityManager };

    }

    // The parent given by an operation or the root
    static Scene::Entity GetParent(Scene::Scene& scene, const json& operation) {

        return operation.contains("parent") ? SelectEntities(scene, json { { "entity", operation["parent"] } }).front() : FindRoot(scene);

    }

    // Added entities need unique names, such that later operations can select them
    static void CheckNewName(Scene::Scene& scene, const std::string& name) {

        if (scene.GetEntityByName(name).IsValid())
            throw std::runtime_error("An entity named '" + name + "' already exists");

    }

    static void SetName(Scene::Entity entity, const std::string& name) {

        if (auto nameComponent = entity.TryGetComponent<NameComponent>())
            nameComponent->name = name;
        else
            entity.AddComponent<NameComponent>(name);

    }

    // position is in world space, localPosition (with rotation and scale) in the space of the parent
    static mat4 GetPlacement(Scene::Scene& scene, const json& operation, Scene::Entity parent) {

        Common::MatrixDecomposition decomposition;
        decomposition.translation = ToVec3(operation.value("position", json()));
        decomposition.rotation = glm::radians(ToVec3(operation.value("rotation", json())));
        decomposition.scale = ToVec3(operation.value("scale", json()), vec3(1.0f));
        if (!operation.contains("localPosition"))
            return decomposition.Compose();

        decomposition.translation = ToVec3(operation["localPosition"]);
        auto parentMatrix = scene.GetGlobalMatrix(parent);
        auto animationComponent = parent.IsValid() ? parent.TryGetComponent<AnimationComponent>() : nullptr;
        if (animationComponent && operation.contains("parentJoint"))
            parentMatrix = parentMatrix * animationComponent->GetJointMatrix(operation["parentJoint"].get<int32_t>());
        return parentMatrix * decomposition.Compose();

    }

    static void Place(Scene::Entity entity, Scene::Entity parent, const mat4& global) {

        GetTransform(entity).SetGlobal(global, parent);
        AddChild(parent, entity);

    }

    void AddChild(Scene::Entity parent, Scene::Entity entity) {

        if (!parent.IsValid())
            return;
        if (!parent.HasComponent<HierarchyComponent>())
            parent.AddComponent<HierarchyComponent>();
        parent.GetComponent<HierarchyComponent>().AddChild(entity);

    }

    void AddMeshBody(Scene::Entity entity, const ResourceHandle<Mesh::Mesh>& mesh, vec3 scale) {

        Physics::MeshShapeSettings settings = { .mesh = mesh, .scale = scale };
        entity.AddComponent<RigidBodyComponent>(Physics::BodyCreationSettings {
            .objectLayer = Physics::Layers::Static, .shape = Physics::ShapesManager::CreateShape(settings) });

    }

    void AddLight(Scene::Entity entity, const json& spec) {

        auto type = spec.value("type", std::string("point"));
        if (type != "point" && type != "spot")
            throw std::runtime_error("light.type must be point or spot");

        auto& light = entity.AddComponent<LightComponent>(
            type == "point" ? LightType::PointLight : LightType::SpotLight, LightMobility::StationaryLight);
        light.color = ToVec3(spec.value("color", json()), vec3(1.0f));
        light.intensity = spec.value("intensity", 1.0f);
        light.volumetric = spec.value("volumetric", false);
        auto radius = spec.value("radius", 20.0f);
        auto shadow = spec.value("shadow", false);
        if (type == "point") {
            light.properties.point.radius = radius;
            if (shadow)
                light.AddPointShadow(0.25f, 1024);
        }
        else {
            // The direction is in entity space
            light.properties.spot.radius = radius;
            light.properties.spot.direction =
                glm::normalize(ToVec3(spec.value("direction", json()), vec3(0.0f, -1.0f, 0.0f)));
            light.properties.spot.innerConeAngle = glm::radians(spec.value("innerAngle", 25.0f));
            light.properties.spot.outerConeAngle = glm::radians(spec.value("outerAngle", 40.0f));
            if (shadow)
                light.AddSpotShadow(2.0f, 1024);
        }

    }

    ResourceHandle<Mesh::Mesh> LoadMeshForScene(const std::string& path) {

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

    ResourceHandle<Animation::Animation> LoadAnimationForScene(const std::string& animation, const std::string& modelPath) {

        auto animationPath = ToAssetPath(animation);
        if (Common::Path::GetFileType(animationPath) != "aeanimation") {
            auto path = ToAssetPath(modelPath);
            auto imported = Loader::ModelImporter::ImportAnimations(path, true);

            auto match = std::find_if(imported.begin(), imported.end(),
                [&](const auto& item) { return item->name == animation; });
            if (match == imported.end()) {
                std::string names;
                for (const auto& item : imported)
                    names += (names.empty() ? "" : ", ") + item->name;
                throw std::runtime_error("No animation " + animation + " in " + path + " (" + names + ")");
            }

            animationPath = Loader::ModelImporter::GetAnimationFilename(path, (*match)->name);
        }

        auto handle = ResourceManager<Animation::Animation>::GetOrLoadResourceWithLoader(
            animationPath, ResourceOrigin::User, Loader::AnimationLoader::LoadAnimation, true);
        if (!handle.IsLoaded())
            throw std::runtime_error("Couldn't load animation " + animationPath);

        return handle;

    }

    // Settings of the meshCopy and meshSettings operations
    static void ApplyMeshSettings(Mesh::Mesh& mesh, const json& settings) {

        mesh.invertUVs = settings.value("invertUVs", mesh.invertUVs);
        mesh.cullBackFaces = settings.value("cullBackFaces", mesh.cullBackFaces);
        mesh.vegetation = settings.value("vegetation", mesh.vegetation);
        mesh.castShadow = settings.value("castShadow", mesh.castShadow);
        mesh.UpdatePipelines();

    }

    // Turns the root of a scene file into a prefab: excluded entities (name patterns), directional lights,
    // players, cameras and optionally scripts are left out, such that the prefab can be placed into another scene
    static json MakePrefabFromScene(const json& operation) {

        auto scenePath = ToAssetPath(operation.at("scene").get<std::string>());
        auto output = ToAssetPath(operation.at("output").get<std::string>());
        auto outputFile = GetAssetDirectory() / output;
        if (fs::exists(outputFile) && !operation.value("overwrite", false))
            throw std::runtime_error("makePrefab: " + output + " already exists, set \"overwrite\": true");

        std::ifstream input(GetAssetDirectory() / scenePath, std::ios::binary);
        if (!input.is_open())
            throw std::runtime_error("makePrefab: couldn't open " + scenePath);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(input)), {});
        // Scenes are stored as MessagePack or as plain JSON
        auto j = !data.empty() && data[0] == '{' ? json::parse(data) : json::from_msgpack(data);
        Loader::AssetReferences::ResolveReferences(j, Loader::AssetReferences::Type::Scene, scenePath);

        json* root = nullptr;
        for (auto& entity : j.at("entities"))
            if (entity.value("root", false))
                root = &entity;
        if (!root)
            throw std::runtime_error("makePrefab: " + scenePath + " has no root entity");

        auto excluded = operation.value("exclude", std::vector<std::string>());
        auto keepScripts = operation.value("scripts", true);
        int32_t removed = 0, kept = 0;

        std::function<void(json&)> filter = [&](json& entity) {

            if (!keepScripts)
                entity.erase("script");
            entity.erase("camera");
            if (!entity.contains("entities"))
                return;

            json children = json::array();
            for (auto& child : entity["entities"]) {
                auto name = child.contains("name") ? child["name"].value("name", std::string()) : std::string();
                auto directional = child.contains("light") &&
                    child["light"].value("type", -1) == int32_t(LightType::DirectionalLight);
                if (child.contains("player") || directional || MatchAnyPattern(excluded, name)) {
                    removed++;
                    continue;
                }
                filter(child);
                children.push_back(child);
                kept++;
            }
            entity["entities"] = children;

        };
        filter(*root);

        json prefab = *root;

        // With an area only the mesh and light entities around its center are kept, flattened and relative to the center
        if (operation.contains("area")) {
            auto center = ToVec3(operation["area"].at("center"));
            auto radius = operation["area"].at("radius").get<float>();
            auto excludedMeshes = operation.value("excludeMeshes", std::vector<std::string>());
            auto includedMeshes = operation.value("includeMeshes", std::vector<std::string>{ "*" });

            json children = json::array();
            std::function<void(const json&, const mat4&)> collect = [&](const json& entity, const mat4& parentMatrix) {

                auto global = parentMatrix;
                if (entity.contains("transform"))
                    global = parentMatrix * entity["transform"].at("matrix").get<mat4>();

                auto meshPath = entity.contains("mesh") ? entity["mesh"].value("resourcePath", std::string()) : std::string();
                auto inside = glm::distance(vec3(global[3]), center) <= radius;
                if (inside && (entity.contains("light") || (!meshPath.empty() && MatchAnyPattern(includedMeshes, meshPath) &&
                    !MatchAnyPattern(excludedMeshes, meshPath)))) {
                    json child = entity;
                    child.erase("entities");
                    child["transform"]["matrix"] = glm::translate(-center) * global;
                    children.push_back(child);
                }
                for (const auto& child : entity.value("entities", json::array()))
                    collect(child, global);

            };
            collect(prefab, mat4(1.0f));

            kept = int32_t(children.size());
            if (children.empty())
                throw std::runtime_error("makePrefab: no meshes or lights in the area of " + scenePath);
            prefab["entities"] = children;
        }

        prefab["root"] = false;
        prefab["name"] = NameComponent(operation.value("name", fs::path(output).stem().string()));
        prefab["transform"] = TransformComponent();

        Loader::AssetReferences::MakeReferences(prefab, Loader::AssetReferences::Type::Entity, output);
        fs::create_directories(outputFile.parent_path());
        std::ofstream file(outputFile);
        file << prefab.dump();
        file.close();

        return { { "prefab", output }, { "entities", kept }, { "removed", removed } };

    }

    json ApplyEditOperations(const Ref<Scene::Scene>& sceneRef, const json& operations, EditState& state) {

        auto& scene = *sceneRef;
        json log = json::array();

        for (const auto& operation : operations) {
            auto type = operation.at("op").get<std::string>();
            json entry;
            entry["operation"] = operation;
            json affected = json::array();

            if (type == "setTransform") {
                for (auto entity : SelectEntities(scene, operation)) {
                    auto& transform = GetTransform(entity);
                    auto parent = scene.GetParentEntity(entity);
                    auto parentMatrix = scene.GetGlobalMatrix(parent);
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
                    transform.SetGlobal(global, parent);

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
                    auto rotation = glm::translate(origin) * glm::rotate(yaw, vec3(0.0f, 1.0f, 0.0f)) * glm::translate(-origin);
                    transform.SetGlobal(rotation * transform.globalMatrix, scene.GetParentEntity(entity));
                    affected.push_back(GetEntityLabel(entity));
                }
            }
            else if (type == "snapToGround") {
                auto offset = operation.value("offset", 0.0f);
                for (auto entity : SelectEntities(scene, operation)) {
                    auto [bounds, hasMesh, ground] = GroundBelowEntity(scene, entity, operation.value("searchAbove", 0.5f));
                    if (!ground.valid)
                        throw std::runtime_error("No ground below " + GetEntityLabel(entity));

                    auto delta = ground.position.y + offset - bounds.min.y;
                    TranslateWorld(entity, vec3(0.0f, delta, 0.0f));
                    affected.push_back({ { "entity", GetEntityLabel(entity) }, { "deltaY", delta }, { "ground", GetHitLabel(ground) } });
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
                    AddChild(parent, copy);

                    if (operation.contains("name"))
                        SetName(copy, operation["name"].get<std::string>());

                    if (operation.contains("position")) {
                        // The copy has no updated global matrix yet, so start from the original
                        auto global = GetTransform(entity).globalMatrix;
                        global[3] = vec4(ToVec3(operation["position"]), 1.0f);
                        GetTransform(copy).SetGlobal(global, parent);
                    }

                    affected.push_back(GetEntityLabel(copy));
                }
            }
            else if (type == "material") {
                // The file is written by SaveMaterials once the checks passed
                auto materialPath = ToAssetPath(operation.at("material").get<std::string>());
                auto material = ResourceManager<Material>::GetOrLoadResourceWithLoader(
                    materialPath, ResourceOrigin::User, Loader::MaterialLoader::LoadMaterial, false);
                if (!material.IsLoaded())
                    throw std::runtime_error("Couldn't load material " + materialPath);

                auto changes = operation.at("set");
                json before = *material.Get();
                PatchMaterial(*material.Get(), changes);
                json after = *material.Get();
                state.changedMaterials.insert(materialPath);

                json changed = json::object();
                for (auto& [key, value] : changes.items())
                    changed[key] = { { "before", before.value(key, json()) }, { "after", after.value(key, json()) } };
                affected.push_back({ { "material", materialPath }, { "changes", changed } });
            }
            else if (type == "delete") {
                for (auto entity : SelectEntities(scene, operation)) {
                    affected.push_back(GetEntityLabel(entity));
                    scene.DestroyEntity(entity);
                }
            }
            else if (type == "add") {
                auto name = operation.at("name").get<std::string>();
                CheckNewName(scene, name);

                auto parent = GetParent(scene, operation);
                auto global = GetPlacement(scene, operation, parent);

                ResourceHandle<Mesh::Mesh> mesh;
                if (operation.contains("mesh"))
                    mesh = LoadMeshForScene(operation["mesh"].get<std::string>());

                auto hasScript = operation.contains("script");
                Scene::Entity entity;
                if (mesh.IsValid()) {
                    entity = scene.CreatePrefab<Scene::Prefabs::MeshInstance>(mesh, global, !hasScript);
                }
                else {
                    entity = scene.CreateEntity();
                    entity.AddComponent<TransformComponent>(global, !hasScript);
                }
                entity.AddComponent<NameComponent>(name);
                // Follows a joint of the animated parent, the position is then relative to the joint
                GetTransform(entity).parentJoint = operation.value("parentJoint", -1);
                Place(entity, parent, global);

                json added = { { "entity", GetEntityLabel(entity) } };
                if (mesh.IsValid()) {
                    added["mesh"] = mesh.GetResource()->path;
                    // Hidden meshes can be shown by scripts, e.g. a lit and an unlit variant
                    entity.GetComponent<MeshComponent>().visible = operation.value("visible", true);

                    // Use the world scale for the collision mesh
                    if (operation.value("body", false))
                        AddMeshBody(entity, mesh, GetTransform(entity).DecomposeGlobal().scale);
                }
                else if (operation.contains("body")) {
                    throw std::runtime_error("add: a body requires a mesh");
                }

                if (operation.contains("animation")) {
                    if (!mesh.IsValid())
                        throw std::runtime_error("add: an animation requires a mesh");
                    auto animation = LoadAnimationForScene(operation["animation"].get<std::string>(),
                        operation["mesh"].get<std::string>());
                    auto& animationComponent = entity.AddComponent<AnimationComponent>(animation);
                    animationComponent.speed = operation.value("animationSpeed", 1.0f);
                    animationComponent.time = operation.value("animationTime", 0.0f);
                    added["animation"] = animation.GetResource()->path;
                }

                // Main camera looking along +Z, the editor needs one to start playing
                if (operation.contains("camera")) {
                    const auto& spec = operation["camera"];
                    auto& camera = entity.AddComponent<CameraComponent>(
                        spec.value("fov", 70.0f), 16.0f / 9.0f, spec.value("near", 0.1f), spec.value("far", 2000.0f));
                    camera.isMain = spec.value("main", true);
                    added["camera"] = camera.isMain ? "main" : "secondary";
                }

                if (operation.contains("light")) {
                    AddLight(entity, operation["light"]);
                    added["light"] = operation["light"].value("type", std::string("point"));
                }

                if (hasScript) {
                    auto scriptPath = ToAssetPath(operation["script"].get<std::string>());
                    auto script = ResourceManager<Scripting::Script>::GetOrLoadResource(scriptPath, ResourceOrigin::User);
                    if (!script.IsLoaded())
                        throw std::runtime_error("Couldn't load script " + scriptPath);
                    entity.AddComponent<LuaScriptComponent>(script);
                    added["script"] = scriptPath;
                }

                affected.push_back(added);
            }
            else if (type == "terrainCopy") {
                CopySceneTerrain(scene, operation.at("path").get<std::string>(), operation.value("overwrite", false));
                state.terrainChanged = true;
                affected.push_back({ { "terrain", scene.terrain.GetResource()->path } });
            }
            else if (type == "terrain") {
                // Terrain files can be shared by several scenes, edit a copy unless overwriting is explicitly allowed
                if (scene.terrain.IsValid() && scene.terrain.GetResource()->path == state.loadedTerrain &&
                    !operation.value("overwriteTerrain", false))
                    throw std::runtime_error("terrain: edit a copy (terrainCopy) or set \"overwriteTerrain\": true to change " +
                        state.loadedTerrain);
                affected = ApplyTerrainBrushes(scene, operation.at("brushes"));
                state.terrainChanged = true;
            }
            else if (type == "scatter") {
                affected.push_back(Scatter(scene, operation));
            }
            else if (type == "detachTerrain") {
                // E.g. to look at underground geometry from above
                if (!state.dryRun)
                    throw std::runtime_error("detachTerrain is only allowed in a dry run");
                scene.terrain = ResourceHandle<Terrain::Terrain>();
                affected.push_back("terrain detached");
            }
            else if (type == "clearArea") {
                // Removes the descendants of the selected entities (e.g. a grass group) inside a scatter area
                auto areaTest = MakeAreaTest(operation.at("area"));
                auto inside = [&](vec3 position) { return areaTest(vec2(position.x, position.z)); };

                std::vector<Scene::Entity> removed;
                for (auto group : FindEntities(scene, operation.at("entities").get<std::vector<std::string>>())) {
                    EntitySet members;
                    CollectHierarchy(group, members);
                    for (auto member : members) {
                        Scene::Entity entity = { member, &scene.entityManager };
                        if (entity != group && entity.HasComponent<MeshComponent>() && inside(GetEntityPosition(entity)))
                            removed.push_back(entity);
                    }
                }
                for (auto entity : removed)
                    scene.DestroyEntity(entity);
                affected.push_back({ { "removed", removed.size() } });
            }
            else if (type == "meshSettings") {
                // Changes mesh files matching a pattern, e.g. "dungeon/meshes/*.aemesh", after backing them up once
                auto pattern = ToAssetPath(operation.at("meshes").get<std::string>());
                const auto& settings = operation.at("set");
                auto directory = GetAssetDirectory() / fs::path(pattern).parent_path();
                if (!fs::is_directory(directory))
                    throw std::runtime_error("meshSettings: no directory " + directory.string());

                json changed = json::array();
                for (const auto& file : fs::directory_iterator(directory)) {
                    auto path = (fs::path(pattern).parent_path() / file.path().filename()).generic_string();
                    if (!file.is_regular_file() || file.path().extension() != ".aemesh" || !MatchPattern(pattern, path))
                        continue;

                    auto backup = file.path();
                    backup += ".backup";
                    if (!fs::exists(backup))
                        fs::copy_file(file.path(), backup);

                    auto mesh = LoadMeshForScene(path);
                    ApplyMeshSettings(*mesh.Get(), settings);
                    Loader::MeshLoader::SaveMesh(mesh.Get(), path, true);
                    changed.push_back(path);
                }
                affected.push_back({ { "meshes", changed.size() }, { "set", settings } });
            }
            else if (type == "materialCopy") {
                // Copies a material file and changes its properties, e.g. to fix imported foliage materials
                auto sourcePath = ToAssetPath(operation.at("material").get<std::string>());
                auto outputPath = ToAssetPath(operation.at("output").get<std::string>());
                auto outputFile = GetAssetDirectory() / outputPath;
                if (fs::exists(outputFile) && !operation.value("overwrite", false))
                    throw std::runtime_error("materialCopy: " + outputPath + " already exists, set \"overwrite\": true");

                std::ifstream input(GetAssetDirectory() / sourcePath);
                if (!input.is_open())
                    throw std::runtime_error("materialCopy: couldn't open " + sourcePath);
                auto material = json::parse(input);
                input.close();
                for (const auto& [key, value] : operation.value("set", json::object()).items()) {
                    // Colors are given as [r, g, b]
                    if (value.is_array() && value.size() == 3)
                        material[key] = { { "x", value[0] }, { "y", value[1] }, { "z", value[2] } };
                    else
                        material[key] = value;
                }
                for (const auto& key : operation.value("remove", std::vector<std::string>()))
                    material.erase(key);
                material["name"] = fs::path(outputPath).stem().string();

                fs::create_directories(outputFile.parent_path());
                std::ofstream file(outputFile);
                file << material.dump();
                file.close();
                affected.push_back(outputPath);
            }
            else if (type == "meshMaterials") {
                // Replaces materials of mesh files, e.g. of copies made by meshCopy
                const auto& replace = operation.at("replace");
                for (const auto& meshPath : operation.at("meshes")) {
                    auto mesh = LoadMeshForScene(meshPath.get<std::string>());
                    int32_t replaced = 0;
                    for (auto& material : mesh->data.materials) {
                        auto path = material.GetResource()->path;
                        if (!replace.contains(path))
                            continue;
                        auto newPath = ToAssetPath(replace[path].get<std::string>());
                        auto newMaterial = ResourceManager<Material>::GetOrLoadResourceWithLoader(newPath,
                            ResourceOrigin::User, Loader::MaterialLoader::LoadMaterial, false);
                        if (!newMaterial.IsLoaded())
                            throw std::runtime_error("meshMaterials: couldn't load " + newPath);
                        for (auto& subData : mesh->data.subData)
                            if (subData.material.GetResource() == material.GetResource())
                                subData.material = newMaterial;
                        material = newMaterial;
                        replaced++;
                    }
                    mesh->UpdatePipelines();
                    Loader::MeshLoader::SaveMesh(mesh.Get(), mesh.GetResource()->path, true);
                    affected.push_back({ { "mesh", mesh.GetResource()->path }, { "replaced", replaced } });
                }
            }
            else if (type == "meshCopy") {
                // Shared mesh files stay untouched: the copy is changed and assigned to the selected hierarchies
                auto sourcePath = ToAssetPath(operation.at("mesh").get<std::string>());
                auto outputPath = ToAssetPath(operation.at("output").get<std::string>());
                auto outputFile = GetAssetDirectory() / outputPath;
                if (fs::exists(outputFile) && !operation.value("overwrite", false))
                    throw std::runtime_error("meshCopy: " + outputPath + " already exists, set \"overwrite\": true");
                fs::create_directories(outputFile.parent_path());
                fs::copy_file(GetAssetDirectory() / sourcePath, outputFile, fs::copy_options::overwrite_existing);

                auto mesh = LoadMeshForScene(outputPath);
                if (operation.value("invertNormals", false)) {
                    for (auto& normal : mesh->data.normals.Get())
                        normal = vec4(-vec3(normal), normal.w);
                    mesh->UpdateData();
                }
                // Mesh settings, e.g. to configure foliage like the other meshes of its asset pack
                if (operation.contains("set"))
                    ApplyMeshSettings(*mesh.Get(), operation["set"]);
                Loader::MeshLoader::SaveMesh(mesh.Get(), outputPath, true);

                int32_t assigned = 0;
                if (operation.contains("assign")) {
                    for (auto member : CollectHierarchies(scene, operation["assign"].get<std::vector<std::string>>())) {
                        Scene::Entity entity = { member, &scene.entityManager };
                        auto meshComponent = entity.TryGetComponent<MeshComponent>();
                        if (meshComponent && meshComponent->mesh.IsValid() &&
                            meshComponent->mesh.GetResource()->path == sourcePath) {
                            // Replaced through the component, such that the scene updates its resource references
                            // and space partitioning (deserialized components can't change their resource)
                            auto visible = meshComponent->visible;
                            auto dontCull = meshComponent->dontCull;
                            entity.RemoveComponent<MeshComponent>();
                            auto& replaced = entity.AddComponent<MeshComponent>(mesh);
                            replaced.visible = visible;
                            replaced.dontCull = dontCull;
                            assigned++;
                        }
                    }
                }
                affected.push_back({ { "mesh", outputPath }, { "assigned", assigned } });
            }
            else if (type == "makePrefab") {
                affected.push_back(MakePrefabFromScene(operation));
            }
            else if (type == "prefab") {
                auto name = operation.at("name").get<std::string>();
                CheckNewName(scene, name);

                auto parent = GetParent(scene, operation);
                auto global = GetPlacement(scene, operation, parent);

                auto prefabPath = ToAssetPath(operation.at("prefab").get<std::string>());
                auto entity = Serializer::DeserializePrefab(sceneRef, prefabPath);
                SetName(entity, name);

                if (!entity.HasComponent<TransformComponent>())
                    entity.AddComponent<TransformComponent>(mat4(1.0f), true);
                Place(entity, parent, global);

                EntitySet members;
                CollectHierarchy(entity, members);
                affected.push_back({ { "entity", GetEntityLabel(entity) }, { "prefab", prefabPath }, { "entities", members.size() } });
            }
            else {
                throw std::runtime_error("Unknown edit operation '" + type + "' (setTransform, translate, rotate, snapToGround, " +
                    "rename, setVisible, duplicate, delete, material, add, terrainCopy, terrain, detachTerrain, scatter, clearArea, " +
                    "meshSettings, meshCopy, materialCopy, meshMaterials, makePrefab, prefab)");
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

    json SaveMaterials(const std::set<std::string>& paths) {

        json saved = json::array();
        for (const auto& path : paths) {
            auto backup = GetAssetDirectory() / path;
            backup += ".backup";
            if (!fs::exists(backup))
                fs::copy_file(GetAssetDirectory() / path, backup);

            // The loader converts the resource paths into references from the material file
            auto material = ResourceManager<Material>::GetResource(path);
            Loader::MaterialLoader::SaveMaterial(material.Get(), path);
            saved.push_back({ { "material", path }, { "backup", backup.string() } });
        }

        return saved;

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

        for (const auto& [id, entity] : afterEntities)
            if (!beforeEntities.contains(id))
                added.push_back({ { "id", id }, { "name", (*entity)["name"] }, { "position", entity->value("position", json()) } });

        diff["changed"] = changed;
        diff["added"] = added;
        diff["removed"] = removed;
        diff["summary"] = std::to_string(changed.size()) + " changed, " + std::to_string(added.size()) + " added, " +
            std::to_string(removed.size()) + " removed";

        return diff;

    }

}
