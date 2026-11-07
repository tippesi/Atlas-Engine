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
#include "loader/ImageLoader.h"
#include "loader/KtxLoader.h"
#include "loader/ImpostorLoader.h"
#include "jobsystem/JobSystem.h"
#include "loader/ModelImporter.h"
#include "mesh/MeshSerializer.h"
#include "physics/ShapesManager.h"
#include "resource/ResourceManager.h"
#include "scene/SceneSerializer.h"
#include "scene/components/ComponentSerializer.h"
#include "scene/components/LuaScriptComponent.h"
#include "scripting/Script.h"
#include "materialgraph/Baker.h"
#include "tools/ImpostorTool.h"
#include "Serializer.h"

#include <fstream>
#include <functional>
#include <map>
#include <stdexcept>

namespace Atlas::Capture {

    using namespace Scene::Components;

    // Set by the edit for the models it imports (EditState::textureFormat)
    static Loader::ImportTextureFormat importTextureFormat = Loader::ImportTextureFormat::Original;

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

    // A pattern or a list of patterns of asset paths
    static std::vector<std::string> ToAssetPatterns(const json& j) {

        std::vector<std::string> patterns;
        for (const auto& pattern : j.is_array() ? j : json::array({ j }))
            patterns.push_back(ToAssetPath(pattern.get<std::string>()));
        return patterns;

    }

    // The parent given by an operation or the root
    static Scene::Entity GetParent(Scene::Scene& scene, const json& operation) {

        return operation.contains("parent") ? SelectEntities(scene, json { { "entity", operation["parent"] } }).front() : scene.GetRootEntity();

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
        entity.SetParent(parent);

    }

    void AddMeshBody(Scene::Entity entity, const ResourceHandle<Mesh::Mesh>& mesh, vec3 scale) {

        Physics::MeshShapeSettings settings = { .mesh = mesh, .scale = scale };
        entity.AddComponent<RigidBodyComponent>(Physics::BodyCreationSettings {
            .objectLayer = Physics::Layers::Static, .shape = Physics::ShapesManager::CreateShape(settings) });

    }

    // A simulated body around a mesh instance: its bounding box or convex hull at the scale of the instance
    static void AddMovableBody(Scene::Entity entity, const json& operation, bool inOcean) {

        auto& mesh = entity.GetComponent<MeshComponent>().mesh;
        auto& transform = GetTransform(entity);
        auto scale = transform.DecomposeGlobal().scale;
        auto density = operation.value("density", 500.0f);

        Ref<Physics::Shape> shape;
        auto type = operation.value("shape", std::string("box"));
        if (type == "box") {
            Physics::BoundingBoxShapeSettings settings = { .aabb = mesh->data.aabb, .density = density, .scale = scale };
            shape = Physics::ShapesManager::CreateShape(settings);
        }
        else if (type == "convex") {
            Physics::ConvexHullShapeSettings settings = { .mesh = mesh, .density = density, .scale = scale };
            shape = Physics::ShapesManager::CreateShape(settings);
        }
        else if (type == "boxes") {
            // Boxes in mesh space [minX, minY, minZ, maxX, maxY, maxZ], e.g. a hull up to its deck and the rails
            Physics::CompoundShapeSettings settings = { .scale = scale };
            for (const auto& box : operation.at("boxes")) {
                Physics::BoundingBoxShapeSettings boxSettings = { .aabb = Volume::AABB(vec3(box[0], box[1], box[2]),
                    vec3(box[3], box[4], box[5])), .density = density };
                settings.children.push_back({ .shape = Physics::ShapesManager::CreateShape(boxSettings) });
            }
            shape = Physics::ShapesManager::CreateShape(settings);
        }
        else {
            throw std::runtime_error("meshBodies: shape has to be box, convex or boxes");
        }

        entity.AddComponent<RigidBodyComponent>(Physics::BodyCreationSettings {
            .objectLayer = Physics::Layers::Movable,
            .friction = operation.value("friction", 0.8f),
            .restitution = operation.value("restitution", 0.1f),
            .buoyancy = inOcean ? operation.value("buoyancy", 0.0f) : 0.0f,
            .fluidLinearDrag = operation.value("fluidLinearDrag", 0.5f),
            .fluidAngularDrag = operation.value("fluidAngularDrag", 0.01f),
            .startAsleep = operation.value("asleep", true),
            .shape = shape });
        transform.isStatic = false;

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
        auto shadowResolution = spec.value("shadowResolution", 1024);
        if (type == "point") {
            light.properties.point.radius = radius;
            if (shadow)
                light.AddPointShadow(0.25f, shadowResolution);
        }
        else {
            // The direction is in entity space
            light.properties.spot.radius = radius;
            light.properties.spot.direction =
                glm::normalize(ToVec3(spec.value("direction", json()), vec3(0.0f, -1.0f, 0.0f)));
            light.properties.spot.innerConeAngle = glm::radians(spec.value("innerAngle", 25.0f));
            light.properties.spot.outerConeAngle = glm::radians(spec.value("outerAngle", 40.0f));
            if (shadow)
                light.AddSpotShadow(2.0f, shadowResolution);
        }

    }

    ResourceHandle<Mesh::Mesh> LoadMeshForScene(const std::string& path) {

        auto meshPath = ToAssetPath(path);
        if (Common::Path::GetFileType(meshPath) != "aemesh") {
            auto imported = Loader::ModelImporter::ImportMesh(meshPath, true, 4096, importTextureFormat);
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

    // The animation files matching a pattern or a list of patterns, e.g. "models/skinned/animations/Horse_*.aeanimation"
    static std::vector<std::string> FindAnimationFiles(const json& j) {

        std::vector<std::string> paths;
        for (const auto& pattern : ToAssetPatterns(j)) {
            auto directory = fs::path(pattern).parent_path();
            if (!fs::is_directory(GetAssetDirectory() / directory))
                throw std::runtime_error("animations: no directory " + directory.generic_string());
            for (const auto& file : fs::directory_iterator(GetAssetDirectory() / directory)) {
                auto path = (directory / file.path().filename()).generic_string();
                if (file.is_regular_file() && file.path().extension() == ".aeanimation" && Helper::String::MatchWildcard(pattern, path))
                    paths.push_back(path);
            }
        }
        std::sort(paths.begin(), paths.end());
        return paths;

    }

    static std::vector<std::string> GetAnimationFiles(const Mesh::Mesh& mesh) {

        std::vector<std::string> paths;
        for (const auto& animation : mesh.data.animations)
            paths.push_back(animation.GetResource()->path);
        return paths;

    }

    // Settings of the meshCopy and meshSettings operations
    static void ApplyMeshSettings(Mesh::Mesh& mesh, const json& settings) {

        mesh.invertUVs = settings.value("invertUVs", mesh.invertUVs);
        mesh.cullBackFaces = settings.value("cullBackFaces", mesh.cullBackFaces);
        mesh.vegetation = settings.value("vegetation", mesh.vegetation);
        mesh.castShadow = settings.value("castShadow", mesh.castShadow);
        mesh.rayTrace = settings.value("rayTrace", mesh.rayTrace);
        mesh.minScreenSize = settings.value("minScreenSize", mesh.minScreenSize);
        mesh.animationLodDistance = settings.value("animationLodDistance", mesh.animationLodDistance);
        mesh.animationMaxInterval = settings.value("animationMaxInterval", mesh.animationMaxInterval);
        if (settings.contains("animations")) {
            mesh.data.animations.clear();
            for (const auto& path : FindAnimationFiles(settings["animations"])) {
                mesh.data.animations.push_back(ResourceManager<Animation::Animation>::GetOrLoadResourceWithLoader(
                    path, ResourceOrigin::User, Loader::AnimationLoader::LoadAnimation, true));
            }
        }
        // Instances moved by the physics need the motion of the last frame
        if (settings.contains("movable"))
            mesh.mobility = settings["movable"].get<bool>() ? Mesh::MeshMobility::Movable : Mesh::MeshMobility::Stationary;
        mesh.UpdatePipelines();

    }

    // A distant impostor of views x views renderings, saved next to the mesh by SaveMesh
    static void GenerateMeshImpostor(ResourceHandle<Mesh::Mesh>& mesh, const json& spec) {

        // A pending load must finish before replacing the same resource
        mesh->impostor.WaitForLoad();

        // Copies of a mesh (meshCopy) still reference the impostor of the original
        auto impostorPath = mesh.GetResource()->path + ".aeimpostor";
        if (!mesh->impostor.IsLoaded() || mesh->impostor.GetResource()->path != impostorPath) {
            // A new impostor is compressed like the textures the edit imports
            auto impostor = CreateRef<Mesh::Impostor>();
            impostor->compressTextures = importTextureFormat == Loader::ImportTextureFormat::KTX2;
            mesh->impostor = ResourceManager<Mesh::Impostor>::AddResource(impostorPath, impostor);
        }

        // Settings the spec leaves out stay those of the impostor that is replaced, like in the editor
        auto interpolation = spec.value("interpolation", mesh->impostor->interpolation);
        auto pixelDepthOffset = spec.value("pixelDepthOffset", mesh->impostor->pixelDepthOffset);
        auto textureFormat = spec.value("textureFormat", std::string(mesh->impostor->compressTextures ? "ktx2" : "original"));
        if (textureFormat != "original" && textureFormat != "ktx2")
            throw std::runtime_error("impostor.textureFormat has to be \"original\" or \"ktx2\"");

        auto impostor = Tools::ImpostorTool::GenerateImpostor(mesh, spec.value("views", 12), spec.value("resolution", 256));
        mesh->impostor.GetResource()->Swap(impostor);
        mesh->impostor->interpolation = interpolation;
        mesh->impostor->pixelDepthOffset = pixelDepthOffset;
        mesh->impostor->compressTextures = textureFormat == "ktx2";
        mesh->impostorDistance = spec.value("distance", mesh->impostorDistance);
        mesh->impostorShadowDistance = spec.value("shadowDistance", mesh->impostorShadowDistance);

    }

    // Whether the saved impostor of a mesh was baked with these settings after the last change of the mesh, its
    // materials and their textures, then baking it again would give the same result
    static bool IsImpostorCurrent(ResourceHandle<Mesh::Mesh>& mesh, const fs::path& meshFile, const json& spec) {

        mesh->impostor.WaitForLoad();
        auto impostorFile = fs::path(meshFile).concat(".aeimpostor");
        if (!mesh->impostor.IsLoaded() || mesh->impostor.GetResource()->path != mesh.GetResource()->path + ".aeimpostor" ||
            !fs::exists(impostorFile))
            return false;

        const auto& impostor = mesh->impostor;
        auto textureFormat = spec.value("textureFormat", std::string(impostor->compressTextures ? "ktx2" : "original"));
        if (impostor->views != spec.value("views", 12) || impostor->resolution != spec.value("resolution", 256) ||
            impostor->interpolation != spec.value("interpolation", impostor->interpolation) ||
            impostor->pixelDepthOffset != spec.value("pixelDepthOffset", impostor->pixelDepthOffset) ||
            impostor->compressTextures != (textureFormat == "ktx2") ||
            mesh->impostorDistance != spec.value("distance", mesh->impostorDistance) ||
            mesh->impostorShadowDistance != spec.value("shadowDistance", mesh->impostorShadowDistance))
            return false;

        auto baked = fs::last_write_time(impostorFile);
        auto newer = [&](const std::string& path) {
            auto file = fs::path(Loader::AssetLoader::GetFullPath(path));
            return fs::exists(file) && fs::last_write_time(file) > baked;
        };
        if (fs::last_write_time(meshFile) > baked)
            return false;
        auto newerMap = [&](const ResourceHandle<Texture::Texture2D>& map) {
            return map.IsValid() && newer(map.GetResource()->path);
        };
        for (const auto& material : mesh->data.materials) {
            if (!material.IsLoaded())
                continue;
            if (newer(material.GetResource()->path) || newerMap(material->baseColorMap) || newerMap(material->opacityMap) ||
                newerMap(material->normalMap) || newerMap(material->roughnessMap) || newerMap(material->metalnessMap) ||
                newerMap(material->aoMap) || newerMap(material->displacementMap) || newerMap(material->emissiveMap))
                return false;
        }
        return true;

    }

    // Turns the root of a scene file into a prefab: excluded entities (name patterns), directional lights,
    // players, cameras and optionally scripts are left out, such that the prefab can be placed into another scene
    static json MakePrefabFromScene(const json& operation, std::map<std::string, json>& sceneSources) {

        auto scenePath = ToAssetPath(operation.at("scene").get<std::string>());
        auto output = ToAssetPath(operation.at("output").get<std::string>());
        auto outputFile = GetAssetDirectory() / output;
        if (fs::exists(outputFile) && !operation.value("overwrite", false))
            throw std::runtime_error("makePrefab: " + output + " already exists, set \"overwrite\": true");

        json imported;
        const json* source = nullptr;
        if (Common::Path::GetFileType(scenePath) != "aescene") {
            // Model scenes (glTF, FBX, ...) are imported like in the editor, meshes and materials are saved next to the source
            auto importedScene = Loader::ModelImporter::ImportScene(scenePath, vec3(-2048.0f), vec3(2048.0f), 6, true, false,
                operation.value("invertUVs", false), operation.value("maxTextureResolution", 4096), importTextureFormat);
            if (!importedScene)
                throw std::runtime_error("makePrefab: couldn't import " + scenePath);
            Scene::SceneToJson(imported, importedScene.get());
            source = &imported;
        }
        else {
            // Many prefabs are cut from the same scene, it is only read once per edit
            auto& cached = sceneSources[scenePath];
            if (cached.is_null()) {
                std::ifstream input(GetAssetDirectory() / scenePath, std::ios::binary);
                if (!input.is_open())
                    throw std::runtime_error("makePrefab: couldn't open " + scenePath);
                std::vector<uint8_t> data((std::istreambuf_iterator<char>(input)), {});
                // Scenes are stored as MessagePack or as plain JSON
                auto j = !data.empty() && data[0] == '{' ? json::parse(data) : json::from_msgpack(data);
                Loader::AssetReferences::ResolveReferences(j, Loader::AssetReferences::Type::Scene, scenePath);
                cached = std::move(j);
            }
            source = &cached;
        }

        const json* root = nullptr;
        for (const auto& entity : source->at("entities"))
            if (entity.value("root", false))
                root = &entity;
        if (!root)
            throw std::runtime_error("makePrefab: " + scenePath + " has no root entity");

        auto excluded = operation.value("exclude", std::vector<std::string>());
        auto keepScripts = operation.value("scripts", true);
        // Plants and other small dressing shouldn't block the player
        auto withoutBodies = operation.value("withoutBodies", std::vector<std::string>());
        auto stripBody = [&](json& entity) {
            if (entity.contains("rigidBody") && entity.contains("mesh") &&
                MatchAnyPattern(withoutBodies, entity["mesh"].value("resourcePath", std::string())))
                entity.erase("rigidBody");
        };
        int32_t removed = 0, kept = 0;

        auto getName = [](const json& entity) {
            return entity.contains("name") ? entity["name"].value("name", std::string()) : std::string();
        };
        auto isLeftOut = [&](const json& entity) {
            auto directional = entity.contains("light") &&
                entity["light"].value("type", -1) == int32_t(LightType::DirectionalLight);
            return entity.contains("player") || directional || MatchAnyPattern(excluded, getName(entity));
        };

        std::function<void(json&)> filter = [&](json& entity) {

            if (!keepScripts)
                entity.erase("script");
            entity.erase("camera");
            stripBody(entity);
            if (!entity.contains("entities"))
                return;

            json children = json::array();
            for (auto& child : entity["entities"]) {
                if (isLeftOut(child)) {
                    removed++;
                    continue;
                }
                filter(child);
                children.push_back(std::move(child));
                kept++;
            }
            entity["entities"] = std::move(children);

        };

        // With an entity only its subtree becomes the prefab, e.g. one of several places built in a scene
        const json* selected = root;
        mat4 parentMatrix(1.0f);
        if (operation.contains("entity")) {
            auto pattern = operation["entity"].get<std::string>();
            const json* found = nullptr;
            std::function<void(const json&, const mat4&)> find = [&](const json& entity, const mat4& matrix) {
                if (!entity.contains("entities"))
                    return;
                for (const auto& child : entity.at("entities")) {
                    if (found)
                        return;
                    if (isLeftOut(child))
                        continue;
                    if (MatchAnyPattern({ pattern }, getName(child))) {
                        found = &child;
                        parentMatrix = matrix;
                        return;
                    }
                    find(child, child.contains("transform") ? matrix * child["transform"].at("matrix").get<mat4>() : matrix);
                }
            };
            find(*root, mat4(1.0f));
            if (!found)
                throw std::runtime_error("makePrefab: no entity " + pattern + " in " + scenePath);
            selected = found;
        }

        json prefab = *selected;
        filter(prefab);

        if (operation.contains("entity")) {
            // The children keep their hierarchy, relative to the origin
            auto local = prefab.contains("transform") ? prefab["transform"].at("matrix").get<mat4>() : mat4(1.0f);
            auto origin = glm::translate(-ToVec3(operation.value("origin", json::array({ 0.0f, 0.0f, 0.0f }))));
            for (auto& child : prefab["entities"]) {
                auto childMatrix = child.contains("transform") ? child["transform"].at("matrix").get<mat4>() : mat4(1.0f);
                // Groups without a transform are in the space of their parent, they get one
                if (!child.contains("transform"))
                    child["transform"]["isStatic"] = false;
                child["transform"]["matrix"] = origin * parentMatrix * local * childMatrix;
            }
            parentMatrix = glm::translate(ToVec3(operation.value("origin", json::array({ 0.0f, 0.0f, 0.0f }))));
        }

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
                    stripBody(child);
                    child["transform"]["matrix"] = glm::translate(-center) * global;
                    children.push_back(child);
                }
                for (const auto& child : entity.value("entities", json::array()))
                    collect(child, global);

            };
            collect(prefab, parentMatrix);

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

    struct EditContext {

        const Ref<Scene::Scene>& sceneRef;
        Scene::Scene& scene;
        EditState& state;
        // Many prefabs are cut from the same scene, it is only read once per edit
        std::map<std::string, json> prefabSources;
        // Impostors encoded and written while meshSettings renders the next ones
        JobGroup impostorWrites{ "Impostor writes" };

        ~EditContext() {

            JobSystem::Wait(impostorWrites);

        }

    };

    static json EditSetTransform(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        json affected = json::array();

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

        return affected;

    }

    static json EditTranslate(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        json affected = json::array();

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

        return affected;

    }

    static json EditRotate(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        json affected = json::array();

        // Rotate around the world Y axis through the entity origin
        auto yaw = glm::radians(operation.at("yaw").get<float>());
        for (auto entity : SelectEntities(scene, operation)) {
            auto& transform = GetTransform(entity);
            auto origin = vec3(transform.globalMatrix[3]);
            auto rotation = glm::translate(origin) * glm::rotate(yaw, vec3(0.0f, 1.0f, 0.0f)) * glm::translate(-origin);
            transform.SetGlobal(rotation * transform.globalMatrix, scene.GetParentEntity(entity));
            affected.push_back(GetEntityLabel(entity));
        }

        return affected;

    }

    static json EditSnapToGround(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        json affected = json::array();

        auto offset = operation.value("offset", 0.0f);
        for (auto entity : SelectEntities(scene, operation)) {
            auto [bounds, hasMesh, ground] = GroundBelowEntity(scene, entity, operation.value("searchAbove", 0.5f));
            if (!ground.valid)
                throw std::runtime_error("No ground below " + GetEntityLabel(entity));

            auto delta = ground.position.y + offset - bounds.min.y;
            TranslateWorld(entity, vec3(0.0f, delta, 0.0f));
            affected.push_back({ { "entity", GetEntityLabel(entity) }, { "deltaY", delta }, { "ground", GetHitLabel(ground) } });
        }

        return affected;

    }

    static json EditRename(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        json affected = json::array();

        for (auto entity : SelectEntities(scene, operation)) {
            auto nameComponent = entity.TryGetComponent<NameComponent>();
            if (!nameComponent)
                throw std::runtime_error(GetEntityLabel(entity) + " has no name component");
            affected.push_back(GetEntityLabel(entity));
            nameComponent->name = operation.at("name").get<std::string>();
        }

        return affected;

    }

    static json EditSetVisible(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        json affected = json::array();

        for (auto entity : SelectEntities(scene, operation)) {
            EntitySet hierarchy;
            CollectHierarchy(entity, hierarchy);
            for (auto member : hierarchy) {
                Scene::Entity memberEntity = { member, &scene.entityManager };
                if (auto mesh = memberEntity.TryGetComponent<MeshComponent>())
                    mesh->SetVisible(operation.at("visible").get<bool>());
            }
            affected.push_back(GetEntityLabel(entity));
        }

        return affected;

    }

    static json EditDuplicate(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        json affected = json::array();

        for (auto entity : SelectEntities(scene, operation)) {
            auto copy = scene.DuplicateEntity(entity);
            auto parent = scene.GetParentEntity(entity);

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

        return affected;

    }

    static json EditMaterial(EditContext& context, const json& operation) {

        auto& state = context.state;
        json affected = json::array();

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

        return affected;

    }

    static json EditDelete(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        json affected = json::array();

        for (auto entity : SelectEntities(scene, operation)) {
            affected.push_back(GetEntityLabel(entity));
            scene.DestroyEntity(entity);
        }

        return affected;

    }

    static json EditAdd(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        json affected = json::array();

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
            entity.GetComponent<MeshComponent>().SetVisible(operation.value("visible", true));

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

        // The scale of the entity is the half size of the volume
        if (operation.contains("fogVolume")) {
            FogVolumeComponent fogVolume = operation["fogVolume"];
            entity.AddComponent<FogVolumeComponent>(fogVolume);
            added["fogVolume"] = true;
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

        return affected;

    }

    static json EditTerrainCopy(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        auto& state = context.state;
        json affected = json::array();

        CopySceneTerrain(scene, operation.at("path").get<std::string>(), operation.value("overwrite", false));
        state.terrainChanged = true;
        affected.push_back({ { "terrain", scene.terrain.GetResource()->path } });

        return affected;

    }

    static json EditTerrainCreate(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        auto& state = context.state;
        json affected = json::array();

        affected.push_back(CreateSceneTerrain(scene, operation));
        state.terrainChanged = true;

        return affected;

    }

    static json EditTerrain(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        auto& state = context.state;

        // Terrain files can be shared by several scenes, edit a copy unless overwriting is explicitly allowed
        if (scene.terrain.IsValid() && scene.terrain.GetResource()->path == state.loadedTerrain &&
            !operation.value("overwriteTerrain", false))
            throw std::runtime_error("terrain: edit a copy (terrainCopy) or set \"overwriteTerrain\": true to change " +
                state.loadedTerrain);
        auto affected = ApplyTerrainBrushes(scene, operation.at("brushes"));
        state.terrainChanged = true;

        return affected;

    }

    static json EditShoreLine(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        auto& state = context.state;
        json affected = json::array();

        affected.push_back(BakeShoreLine(scene, operation));
        state.terrainChanged = true;

        return affected;

    }

    static json EditScatter(EditContext& context, const json& operation) {

        auto& scene = context.scene;

        return json::array({ Scatter(scene, operation) });

    }

    static json EditDetachTerrain(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        auto& state = context.state;
        json affected = json::array();

        // E.g. to look at underground geometry from above
        if (!state.dryRun)
            throw std::runtime_error("detachTerrain is only allowed in a dry run");
        scene.terrain = ResourceHandle<Terrain::Terrain>();
        affected.push_back("terrain detached");

        return affected;

    }

    static json EditClearArea(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        json affected = json::array();

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

        return affected;

    }

    static json EditMeshSettings(EditContext& context, const json& operation) {

        json affected = json::array();

        // Changes mesh files matching a pattern, e.g. "dungeon/meshes/*.aemesh", after backing them up once
        auto pattern = ToAssetPath(operation.at("meshes").get<std::string>());
        const auto& settings = operation.at("set");
        auto directory = GetAssetDirectory() / fs::path(pattern).parent_path();
        if (!fs::is_directory(directory))
            throw std::runtime_error("meshSettings: no directory " + directory.string());

        json changed = json::array();
        for (const auto& file : fs::directory_iterator(directory)) {
            auto path = (fs::path(pattern).parent_path() / file.path().filename()).generic_string();
            if (!file.is_regular_file() || file.path().extension() != ".aemesh" || !Helper::String::MatchWildcard(pattern, path))
                continue;

            auto backup = file.path();
            backup += ".backup";
            if (!fs::exists(backup))
                fs::copy_file(file.path(), backup);

            auto mesh = LoadMeshForScene(path);
            // Meshes that already have these settings and a current impostor are left as they are
            auto unchanged = settings.value("invertUVs", mesh->invertUVs) == mesh->invertUVs &&
                settings.value("cullBackFaces", mesh->cullBackFaces) == mesh->cullBackFaces &&
                settings.value("vegetation", mesh->vegetation) == mesh->vegetation &&
                settings.value("castShadow", mesh->castShadow) == mesh->castShadow &&
                settings.value("rayTrace", mesh->rayTrace) == mesh->rayTrace &&
                settings.value("minScreenSize", mesh->minScreenSize) == mesh->minScreenSize &&
                settings.value("animationLodDistance", mesh->animationLodDistance) == mesh->animationLodDistance &&
                settings.value("animationMaxInterval", mesh->animationMaxInterval) == mesh->animationMaxInterval &&
                (!settings.contains("animations") || FindAnimationFiles(settings["animations"]) == GetAnimationFiles(*mesh.Get())) &&
                settings.value("movable", mesh->mobility == Mesh::MeshMobility::Movable) == (mesh->mobility == Mesh::MeshMobility::Movable);
            auto bake = settings.contains("impostor") && (!unchanged || !IsImpostorCurrent(mesh, file.path(), settings["impostor"]));
            if (unchanged && !bake)
                continue;

            ApplyMeshSettings(*mesh.Get(), settings);
            changed.push_back(path);
            if (!bake) {
                Loader::MeshLoader::SaveMesh(mesh.Get(), path, true);
                continue;
            }

            // Impostors are rendered one at a time, jobs encode and write them meanwhile. Rendering waits while
            // the jobs are behind, every pending impostor keeps its renderings in memory
            GenerateMeshImpostor(mesh, settings["impostor"]);
            Loader::MeshLoader::SaveMesh(mesh.Get(), path, true, false, false);
            auto data = Loader::ImpostorLoader::ReadImpostor(mesh->impostor.Get(), mesh->impostor.GetResource()->path);
            auto& writes = context.impostorWrites;
            auto maxPending = std::max(2, 2 * JobSystem::GetWorkerCount(writes.priority));
            if (writes.counter.load() >= maxPending)
                JobSystem::Wait(writes);
            JobSystem::Execute(writes, [data](JobData&) { Loader::ImpostorLoader::WriteImpostor(data); });

            // The renderings of an impostor are only released at the end of a frame, which doesn't come during an
            // edit: without this many impostors in one edit run out of GPU memory
            Graphics::GraphicsDevice::DefaultDevice->WaitForIdle();
            Graphics::GraphicsDevice::DefaultDevice->ForceMemoryCleanup();
        }
        affected.push_back({ { "meshes", changed.size() }, { "set", settings } });

        return affected;

    }

    // The mesh instances an operation is for: all instances of "meshes" or the entities named like "entity" (with all)
    static std::vector<Scene::Entity> SelectMeshInstances(Scene::Scene& scene, const json& operation) {

        if (!operation.contains("entity"))
            return FindMeshInstances(scene, ToAssetPatterns(operation.at("meshes")));

        std::vector<Scene::Entity> instances;
        for (auto entity : SelectEntities(scene, operation))
            if (entity.HasComponent<MeshComponent>())
                instances.push_back(entity);
        return instances;

    }

    static json EditMeshBodies(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        json affected = json::array();

        // Static mesh shapes for every instance of the matching meshes, e.g. trees of prefabs and scatter groups.
        // With remove the instances lose their bodies instead, e.g. grass the player walks through. "motion":
        // "dynamic" makes them simulated bodies (shape box or convex, density, friction, restitution, asleep, buoyancy),
        // with "supported": true only those resting on a body, the terrain or the ocean, the others stay as they are
        auto replace = operation.value("replace", false);
        auto remove = operation.value("remove", false);
        auto motion = operation.value("motion", std::string("static"));
        if (motion != "static" && motion != "dynamic")
            throw std::runtime_error("meshBodies: motion has to be static or dynamic");

        std::vector<Scene::Entity> matches;
        for (auto entity : SelectMeshInstances(scene, operation))
            if (remove ? entity.HasComponent<RigidBodyComponent>() : replace || !entity.HasComponent<RigidBodyComponent>())
                matches.push_back(entity);

        // Something with a body or the terrain right below the bottom of an instance: a pot on a table without
        // collision would fall through it once touched. Floating instances are carried by the ocean they reach
        size_t unsupported = 0;
        auto floats = operation.value("buoyancy", 0.0f) > 0.0f && scene.ocean && scene.ocean->enable && scene.ocean->physics;
        // Only instances in the ocean float, sunken ones, e.g. the cargo of a wreck on the sea floor, stay down there
        auto isInOcean = [&](Scene::Entity entity) {
            auto aabb = entity.GetComponent<MeshComponent>().mesh->data.aabb.Transform(GetTransform(entity).globalMatrix);
            return floats && aabb.min.y < scene.ocean->translation.y && aabb.max.y > scene.ocean->translation.y - 2.0f;
        };
        auto isSupported = [&](Scene::Entity entity) {
            if (isInOcean(entity))
                return true;
            auto aabb = entity.GetComponent<MeshComponent>().mesh->data.aabb.Transform(GetTransform(entity).globalMatrix);
            Volume::Ray ray(vec3(aabb.GetCenter().x, aabb.min.y + 0.15f, aabb.GetCenter().z), vec3(0.0f, -1.0f, 0.0f), 0.0f, 0.4f);
            auto hit = scene.CastRay(ray, Scene::RigidBodyComponentBit | Scene::TerrainComponentBit,
                [&](const Scene::Entity& other) { return other == entity; });
            return hit.valid;
        };
        // Only the instances that float in the ocean, e.g. the boats on the water and not those pulled up on a beach
        if (operation.value("inOcean", false)) {
            if (!floats)
                throw std::runtime_error("meshBodies: inOcean needs buoyancy and an ocean with physics");
            std::erase_if(matches, [&](Scene::Entity entity) { return !isInOcean(entity); });
        }
        if (motion == "dynamic" && operation.value("supported", false)) {
            std::erase_if(matches, [&](Scene::Entity entity) {
                auto supported = isSupported(entity);
                unsupported += supported ? 0 : 1;
                return !supported;
            });
        }

        // Components are changed once all matches are known, adding them invalidates the subset
        for (auto entity : matches) {
            if (entity.HasComponent<RigidBodyComponent>())
                entity.RemoveComponent<RigidBodyComponent>();
            if (remove)
                continue;
            if (motion == "dynamic")
                AddMovableBody(entity, operation, isInOcean(entity));
            else
                AddMeshBody(entity, entity.GetComponent<MeshComponent>().mesh, GetTransform(entity).DecomposeGlobal().scale);
        }
        affected.push_back({ { remove ? "removed" : "bodies", matches.size() } });
        if (unsupported)
            affected.push_back({ { "unsupported", unsupported } });

        return affected;

    }

    // A zone that reports the hero and simulated bodies entering and leaving it (contact events with isTrigger), e.g.
    // a place the hero discovers: a sphere (radius), a box (halfSize) or an upright cylinder (radius, height)
    static json EditTrigger(EditContext& context, const json& operation) {

        auto& scene = context.scene;

        auto name = operation.at("name").get<std::string>();
        CheckNewName(scene, name);
        auto parent = GetParent(scene, operation);
        auto global = GetPlacement(scene, operation, parent);

        Ref<Physics::Shape> shape;
        auto type = operation.value("shape", std::string("sphere"));
        if (type == "sphere") {
            Physics::SphereShapeSettings settings = { .radius = operation.at("radius").get<float>() };
            shape = Physics::ShapesManager::CreateShape(settings);
        }
        else if (type == "box") {
            auto halfSize = ToVec3(operation.at("halfSize"));
            Physics::BoundingBoxShapeSettings settings = { .aabb = Volume::AABB(-halfSize, halfSize) };
            shape = Physics::ShapesManager::CreateShape(settings);
        }
        else if (type == "cylinder") {
            Physics::CylinderShapeSettings settings = { .height = operation.at("height").get<float>(),
                .radius = operation.at("radius").get<float>() };
            shape = Physics::ShapesManager::CreateShape(settings);
        }
        else {
            throw std::runtime_error("trigger: shape has to be sphere, box or cylinder");
        }

        auto entity = scene.CreateEntity();
        entity.AddComponent<TransformComponent>(global, true);
        entity.AddComponent<NameComponent>(name);
        Place(entity, parent, global);
        entity.AddComponent<RigidBodyComponent>(Physics::BodyCreationSettings {
            .objectLayer = Physics::Layers::Static, .isTrigger = true, .shape = shape });

        return json::array({ { { "entity", GetEntityLabel(entity) }, { "shape", type } } });

    }

    // Hangs the simulated instances of matching meshes on the world or connects them to an entity: a point, hinge,
    // cone, distance, slider or fixed constraint at an anchor in the mesh ("top", "bottom" or a position in mesh
    // space), to the anchor where the instance is or to a fixed worldPoint
    static json EditConstraint(EditContext& context, const json& operation) {

        auto& scene = context.scene;

        static const std::unordered_map<std::string, Physics::ConstraintType> types = {
            { "fixed", Physics::ConstraintType::Fixed }, { "point", Physics::ConstraintType::Point },
            { "hinge", Physics::ConstraintType::Hinge }, { "slider", Physics::ConstraintType::Slider },
            { "distance", Physics::ConstraintType::Distance }, { "cone", Physics::ConstraintType::Cone } };
        auto type = types.find(operation.value("type", std::string("point")));
        if (type == types.end())
            throw std::runtime_error("constraint: unknown type");

        Scene::Entity connected;
        if (operation.contains("connect")) {
            connected = scene.GetEntityByName(operation["connect"].get<std::string>());
            if (!connected.IsValid() || !connected.HasComponent<RigidBodyComponent>())
                throw std::runtime_error("constraint: connect needs an entity with a body");
        }

        // Instances that already hang on a constraint keep it, e.g. the boats tied to a bollard among those anchored
        std::vector<Scene::Entity> matches;
        for (auto entity : SelectMeshInstances(scene, operation))
            if (auto body = entity.TryGetComponent<RigidBodyComponent>(); body && body->layer == Physics::Layers::Movable &&
                entity != connected && !entity.HasComponent<ConstraintComponent>())
                matches.push_back(entity);

        for (auto entity : matches) {
            const auto& aabb = entity.GetComponent<MeshComponent>().mesh->data.aabb;
            auto center = aabb.GetCenter();
            vec3 anchor;
            auto spec = operation.value("anchor", json("top"));
            if (spec == "top")
                anchor = vec3(center.x, aabb.max.y, center.z);
            else if (spec == "bottom")
                anchor = vec3(center.x, aabb.min.y, center.z);
            else
                anchor = ToVec3(spec);

            // The body has the position and rotation of the instance, its shape carries the scale
            auto& transform = GetTransform(entity);
            auto decomposition = transform.DecomposeGlobal();
            Physics::ConstraintCreationSettings settings = {
                .type = type->second,
                .point = anchor * decomposition.scale,
                .otherPoint = vec3(transform.globalMatrix * vec4(anchor, 1.0f)),
                .axis = ToVec3(operation.value("axis", json()), vec3(0.0f, 1.0f, 0.0f)),
                .enableLimits = operation.contains("limits"),
                .limitsMin = operation.contains("limits") ? operation["limits"][0].get<float>() : 0.0f,
                .limitsMax = operation.contains("limits") ? operation["limits"][1].get<float>() : 0.0f,
                .halfConeAngle = glm::radians(operation.value("halfConeAngle", 30.0f)),
                .breakForce = operation.value("breakForce", 0.0f) };
            // The other end at a fixed point of the world, e.g. the bollard a boat is tied to, or at an offset in the
            // world from where the anchor is now, e.g. the stone a boat is anchored to on the sea floor
            if (operation.contains("worldPoint"))
                settings.otherPoint = ToVec3(operation["worldPoint"]);
            if (operation.contains("worldOffset"))
                settings.otherPoint += ToVec3(operation["worldOffset"]);
            if (connected.IsValid()) {
                auto other = GetTransform(connected).DecomposeGlobal();
                other.scale = vec3(1.0f);
                auto body = other.Compose();
                settings.otherPoint = vec3(glm::inverse(body) * vec4(settings.otherPoint, 1.0f));
            }
            entity.AddComponent<ConstraintComponent>(settings, connected);
        }

        return json::array({ { { "constraints", matches.size() } } });

    }

    static json EditMaterialCopy(EditContext& context, const json& operation) {

        json affected = json::array();

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

        return affected;

    }

    static json EditCompressTextures(EditContext& context, const json& operation) {

        auto& state = context.state;
        json affected = json::array();

        // Converts the textures of all materials and material graphs below the directories into compressed KTX2 files
        // next to the sources and points the files to them, a .backup of each changed file is kept
        std::map<std::string, std::string> conversions;
        std::vector<std::pair<fs::path, json>> materials;
        for (const auto& directory : operation.at("directories")) {
            auto root = GetAssetDirectory() / ToAssetPath(directory.get<std::string>());
            if (!fs::is_directory(root))
                throw std::runtime_error("compressTextures: " + directory.get<std::string>() + " isn't a directory");
            for (const auto& file : fs::recursive_directory_iterator(root, fs::directory_options::follow_directory_symlink)) {
                auto extension = file.path().extension();
                if (!file.is_regular_file() || (extension != ".aematerial" && extension != ".aematgraph"))
                    continue;
                std::ifstream input(file.path());
                auto material = json::parse(input);
                auto materialDirectory = fs::relative(file.path().parent_path(), GetAssetDirectory());
                bool changed = false;
                auto convert = [&](json& value) {
                    if (!value.is_string() || Loader::KtxLoader::IsKtxFile(value.get<std::string>()))
                        return;
                    // References inside packages are relative to the file
                    auto reference = value.get<std::string>();
                    auto source = reference.starts_with("./") || reference.starts_with("../") ?
                        (materialDirectory / reference).lexically_normal().generic_string() : reference;
                    auto target = fs::path(source).replace_extension(".ktx2").generic_string();
                    conversions[source] = target;
                    value = fs::path(reference).replace_extension(".ktx2").generic_string();
                    changed = true;
                    };
                for (auto& [key, value] : material.items()) {
                    if (key.ends_with("MapPath"))
                        convert(value);
                }
                if (material.contains("graphTextures")) {
                    for (auto& [_, value] : material["graphTextures"].items())
                        convert(value);
                }
                if (material.contains("nodes")) {
                    for (auto& node : material["nodes"]) {
                        if (node.contains("texturePath"))
                            convert(node["texturePath"]);
                    }
                }
                if (changed)
                    materials.emplace_back(file.path(), material);
            }
        }

        std::vector<std::pair<std::string, std::string>> pending;
        for (const auto& conversion : conversions)
            if (!fs::exists(GetAssetDirectory() / conversion.second))
                pending.push_back(conversion);

        // A dry run only reports what would be converted
        if (state.dryRun) {
            affected.push_back({ { "materials", materials.size() }, { "textures", conversions.size() },
                { "converted", 0 }, { "pending", pending.size() } });
            return affected;
        }

        // The encoder runs single threaded per image
        std::vector<std::string> failed(pending.size());
        JobGroup group{ "Compress textures", JobPriority::High };
        JobSystem::ParallelFor(group, int32_t(pending.size()), int32_t(pending.size()), [&](JobData&, int32_t idx) {
            const auto& [source, target] = pending[idx];
            if (!fs::exists(GetAssetDirectory() / source)) {
                failed[idx] = source + " doesn't exist";
                return;
            }
            auto image = Loader::ImageLoader::LoadImage<uint8_t>(source);
            auto fileData = Loader::KtxLoader::EncodeImage({ image });
            if (fileData.empty()) {
                failed[idx] = "couldn't encode " + source;
                return;
            }
            auto fileStream = Loader::AssetLoader::WriteFile(target, std::ios::out | std::ios::binary);
            if (!fileStream.is_open()) {
                failed[idx] = "couldn't write " + target;
                return;
            }
            fileStream.write(fileData.data(), std::streamsize(fileData.size()));
            });
        JobSystem::Wait(group);

        json errors = json::array();
        for (const auto& error : failed)
            if (!error.empty())
                errors.push_back(error);
        if (!errors.empty())
            throw std::runtime_error("compressTextures: " + errors.dump());

        uintmax_t sourceBytes = 0, targetBytes = 0;
        for (const auto& [source, target] : conversions) {
            sourceBytes += fs::file_size(GetAssetDirectory() / source);
            targetBytes += fs::file_size(GetAssetDirectory() / target);
        }

        for (const auto& [path, material] : materials) {
            auto backup = path;
            backup += ".backup";
            if (!fs::exists(backup))
                fs::copy_file(path, backup);
            std::ofstream file(path);
            file << material.dump();
        }

        affected.push_back({ { "materials", materials.size() }, { "textures", conversions.size() },
            { "converted", pending.size() }, { "sourceMB", double(sourceBytes) / 1048576.0 },
            { "ktx2MB", double(targetBytes) / 1048576.0 } });

        return affected;

    }

    static json EditBakeMaterial(EditContext& context, const json& operation) {

        auto path = ToAssetPath(operation.at("material").get<std::string>());
        auto material = ResourceManager<Material>::GetOrLoadResourceWithLoader(path, ResourceOrigin::User, Loader::MaterialLoader::LoadMaterial, false);
        if (!material.IsLoaded())
            throw std::runtime_error("bakeMaterial: couldn't load " + path);

        auto meshPath = operation.at("mesh").get<std::string>();
        auto mesh = LoadMeshForScene(meshPath);

        auto& settings = material->graphBake;
        settings.mode = operation.value("mode", std::string("textures")) == "average" ? MaterialGraph::BakeMode::Average : MaterialGraph::BakeMode::Textures;
        settings.resolution = operation.value("resolution", 1024);
        settings.time = operation.value("time", 0.0f);
        settings.meshPath = ToAssetPath(meshPath);

        // Pipelines of the graph don't change, the compiled state decides whether a bake is up to date
        if (material->HasGraph() && !material->graph->GetCompiled())
            material->graph->Compile();

        auto result = MaterialGraph::Baker::Bake(material, mesh);
        if (!result.success)
            throw std::runtime_error("bakeMaterial: " + result.error);

        Loader::MaterialLoader::SaveMaterial(material.Get(), material.GetResource()->path);

        return { { "material", path }, { "maps", result.maps }, { "baseColor", { material->baseColor.x, material->baseColor.y, material->baseColor.z } },
            { "roughness", material->roughness }, { "metalness", material->metalness }, { "emissiveIntensity", material->emissiveIntensity } };

    }

    static json EditMeshMaterials(EditContext& context, const json& operation) {

        json affected = json::array();

        // Replaces materials of mesh files, e.g. of copies made by meshCopy
        auto replace = operation.value("replace", json::object());
        auto replacementForAll = operation.value("all", std::string());
        for (const auto& meshPath : operation.at("meshes")) {
            auto mesh = LoadMeshForScene(meshPath.get<std::string>());
            int32_t replaced = 0;
            for (auto& material : mesh->data.materials) {
                auto path = material.GetResource()->path;
                if (replacementForAll.empty() && !replace.contains(path))
                    continue;
                auto newPath = ToAssetPath(replacementForAll.empty() ? replace[path].get<std::string>() : replacementForAll);
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

        return affected;

    }

    static json EditMeshCopy(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        json affected = json::array();

        // Shared mesh files stay untouched: the copy is changed and assigned to the selected hierarchies
        auto sourcePath = ToAssetPath(operation.at("mesh").get<std::string>());
        auto outputPath = ToAssetPath(operation.at("output").get<std::string>());
        auto outputFile = GetAssetDirectory() / outputPath;
        if (fs::exists(outputFile) && !operation.value("overwrite", false))
            throw std::runtime_error("meshCopy: " + outputPath + " already exists, set \"overwrite\": true");
        fs::create_directories(outputFile.parent_path());

        // Model files (OBJ, glTF, ...) are imported, the import is saved as the copy
        ResourceHandle<Mesh::Mesh> mesh;
        if (Common::Path::GetFileType(sourcePath) != "aemesh") {
            mesh = LoadMeshForScene(sourcePath);
        }
        else {
            // An earlier copy can already be loaded (e.g. by the scene or a meshSettings operation), the
            // changes below have to start from the source again, otherwise normals are inverted twice
            auto cached = ResourceManager<Mesh::Mesh>::GetResource(outputPath);
            fs::copy_file(GetAssetDirectory() / sourcePath, outputFile, fs::copy_options::overwrite_existing);
            mesh = LoadMeshForScene(outputPath);
            if (cached.IsLoaded()) {
                json fresh = *Loader::MeshLoader::LoadMesh(outputPath, true);
                from_json(fresh, *mesh.Get());
                mesh->UpdatePipelines();
            }
        }
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
                    meshComponent->ChangeResource(mesh);
                    assigned++;
                }
            }
        }
        affected.push_back({ { "mesh", outputPath }, { "assigned", assigned } });

        return affected;

    }

    static json EditMakePrefab(EditContext& context, const json& operation) {

        return json::array({ MakePrefabFromScene(operation, context.prefabSources) });

    }

    static json EditPrefab(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        json affected = json::array();

        auto name = operation.at("name").get<std::string>();
        CheckNewName(scene, name);

        auto parent = GetParent(scene, operation);
        auto global = GetPlacement(scene, operation, parent);

        auto prefabPath = ToAssetPath(operation.at("prefab").get<std::string>());
        auto entity = Serializer::DeserializePrefab(context.sceneRef, prefabPath);
        SetName(entity, name);

        if (!entity.HasComponent<TransformComponent>())
            entity.AddComponent<TransformComponent>(mat4(1.0f), true);
        Place(entity, parent, global);

        EntitySet members;
        CollectHierarchy(entity, members);
        affected.push_back({ { "entity", GetEntityLabel(entity) }, { "prefab", prefabPath }, { "entities", members.size() } });

        return affected;

    }

    static json EditReplaceMesh(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        json affected = json::array();

        // Every instance of the matching meshes gets one of the meshes in "with", chosen by the instance so the
        // choices mix evenly. The bottom of the new mesh stays where the old one was, pivots differ between packs
        std::vector<ResourceHandle<Mesh::Mesh>> replacements;
        for (const auto& path : operation.at("with"))
            replacements.push_back(LoadMeshForScene(path.get<std::string>()));
        if (replacements.empty())
            throw std::runtime_error("replaceMesh: no meshes in 'with'");
        auto matchHeight = operation.value("matchHeight", true);
        auto seed = operation.value("seed", 0u);
        auto inCircle = [&](const char* key, Scene::Entity entity) {
            const auto& circle = operation.at(key);
            auto center = circle.at("center").get<std::vector<float>>();
            auto position = vec3(GetTransform(entity).globalMatrix[3]);
            return glm::distance(vec2(position.x, position.z), vec2(center[0], center[1])) < circle.at("radius").get<float>();
        };

        std::vector<Scene::Entity> found;
        for (auto entity : FindMeshInstances(scene, ToAssetPatterns(operation.at("meshes")),
            ToAssetPatterns(operation.value("exclude", json::array())))) {
            if ((operation.contains("within") && !inCircle("within", entity)) || (operation.contains("beyond") && inCircle("beyond", entity)))
                continue;
            found.push_back(entity);
        }

        for (auto entity : found) {
            auto& meshComponent = entity.GetComponent<MeshComponent>();
            auto id = static_cast<uint32_t>(static_cast<ECS::Entity>(entity));
            const auto& replacement = replacements[(id * 2654435761u + seed) % replacements.size()];
            auto& transform = GetTransform(entity);
            const auto& oldAabb = meshComponent.mesh->data.aabb;
            const auto& newAabb = replacement->data.aabb;
            auto factor = 1.0f;
            if (matchHeight && oldAabb.max.y > oldAabb.min.y && newAabb.max.y > newAabb.min.y)
                factor = (oldAabb.max.y - oldAabb.min.y) / (newAabb.max.y - newAabb.min.y);
            // The global matrix too: later operations of the edit (meshBodies) read it before the next update
            auto adjust = glm::translate(vec3(0.0f, oldAabb.min.y - newAabb.min.y * factor, 0.0f)) * glm::scale(vec3(factor));
            transform.Set(transform.matrix * adjust);
            transform.globalMatrix = transform.globalMatrix * adjust;
            meshComponent.ChangeResource(replacement);
            meshComponent.aabb = replacement->data.aabb.Transform(transform.globalMatrix);
            if (entity.HasComponent<RigidBodyComponent>())
                entity.RemoveComponent<RigidBodyComponent>();
        }
        affected.push_back({ { "replaced", found.size() }, { "with", operation.at("with") } });

        return affected;

    }

    static json EditReplaceWithPrefab(EditContext& context, const json& operation) {

        auto& scene = context.scene;
        json affected = json::array();

        // Every instance of the matching meshes becomes an instance of the prefab with the same transform and
        // parent. The bottom of the prefab stays where the bottom of the mesh was
        auto replaced = FindMeshInstances(scene, ToAssetPatterns(operation.at("meshes")),
            ToAssetPatterns(operation.value("exclude", json::array())));
        auto nearby = FindMeshInstances(scene, ToAssetPatterns(operation.value("removeNear", json::array())));

        auto prefabPath = ToAssetPath(operation.at("prefab").get<std::string>());
        auto name = operation.value("name", std::string("Prefab"));
        // The lowest point of the meshes of a prefab instance in its own space
        std::function<float(Scene::Entity, const mat4&)> bottom = [&](Scene::Entity entity, const mat4& parentMatrix) {
            auto matrix = parentMatrix;
            if (auto transform = entity.TryGetComponent<TransformComponent>())
                matrix = parentMatrix * transform->matrix;
            auto lowest = std::numeric_limits<float>::max();
            // Prefab meshes load asynchronously
            if (auto meshComponent = entity.TryGetComponent<MeshComponent>(); meshComponent && meshComponent->mesh.IsValid()) {
                meshComponent->mesh.WaitForLoad();
                if (meshComponent->mesh.IsLoaded())
                    lowest = meshComponent->mesh->data.aabb.Transform(matrix).min.y;
            }
            if (auto hierarchy = entity.TryGetComponent<HierarchyComponent>())
                for (auto child : hierarchy->GetChildren())
                    lowest = std::min(lowest, bottom(child, matrix));
            return lowest;
        };
        EntitySet removed;
        for (size_t index = 0; index < replaced.size(); index++) {
            auto original = replaced[index];
            auto global = GetTransform(original).globalMatrix;
            auto parent = scene.GetParentEntity(original);
            if (!parent.IsValid())
                parent = scene.GetRootEntity();

            auto entity = Serializer::DeserializePrefab(context.sceneRef, prefabPath);
            SetName(entity, name + " " + std::to_string(index + 1));
            if (!entity.HasComponent<TransformComponent>())
                entity.AddComponent<TransformComponent>(mat4(1.0f), true);
            auto prefabBottom = bottom(entity, glm::inverse(GetTransform(entity).matrix));
            auto meshBottom = original.GetComponent<MeshComponent>().mesh->data.aabb.min.y;
            if (prefabBottom == std::numeric_limits<float>::max())
                throw std::runtime_error("replaceWithPrefab: " + prefabPath + " has no loaded mesh to place its bottom");
            global = global * glm::translate(vec3(0.0f, meshBottom - prefabBottom, 0.0f));
            Place(entity, parent, global);

            auto position = vec3(global[3]);
            for (auto other : nearby) {
                if (!removed.contains(other) && glm::distance(vec3(GetTransform(other).globalMatrix[3]), position) < 1.5f)
                    removed.insert(other);
            }
            removed.insert(original);
        }
        for (auto entity : removed)
            scene.DestroyEntity(Scene::Entity(entity, &scene.entityManager));
        affected.push_back({ { "replaced", replaced.size() }, { "removed", removed.size() - replaced.size() }, { "prefab", prefabPath } });

        return affected;

    }

    static const std::map<std::string, json(*)(EditContext&, const json&)> editOperations = {
        { "setTransform", EditSetTransform },
        { "translate", EditTranslate },
        { "rotate", EditRotate },
        { "snapToGround", EditSnapToGround },
        { "rename", EditRename },
        { "setVisible", EditSetVisible },
        { "duplicate", EditDuplicate },
        { "material", EditMaterial },
        { "delete", EditDelete },
        { "add", EditAdd },
        { "terrainCopy", EditTerrainCopy },
        { "terrainCreate", EditTerrainCreate },
        { "terrain", EditTerrain },
        { "shoreLine", EditShoreLine },
        { "scatter", EditScatter },
        { "detachTerrain", EditDetachTerrain },
        { "clearArea", EditClearArea },
        { "meshSettings", EditMeshSettings },
        { "meshBodies", EditMeshBodies },
        { "trigger", EditTrigger },
        { "constraint", EditConstraint },
        { "materialCopy", EditMaterialCopy },
        { "compressTextures", EditCompressTextures },
        { "bakeMaterial", EditBakeMaterial },
        { "meshMaterials", EditMeshMaterials },
        { "meshCopy", EditMeshCopy },
        { "makePrefab", EditMakePrefab },
        { "prefab", EditPrefab },
        { "replaceMesh", EditReplaceMesh },
        { "replaceWithPrefab", EditReplaceWithPrefab }
    };

    json CapList(const json& list, size_t count) {

        if (!list.is_array() || list.size() <= count)
            return list;
        return { { "count", list.size() }, { "first", json(list.begin(), list.begin() + count) } };

    }

    json ApplyEditOperations(const Ref<Scene::Scene>& sceneRef, const json& operations, EditState& state) {

        EditContext context = { sceneRef, *sceneRef, state };
        importTextureFormat = state.textureFormat;

        json log = json::array();
        for (const auto& operation : operations) {
            auto type = operation.at("op").get<std::string>();
            auto handler = editOperations.find(type);
            if (handler == editOperations.end()) {
                std::string known;
                for (const auto& [name, _] : editOperations)
                    known += (known.empty() ? "" : ", ") + name;
                throw std::runtime_error("Unknown edit operation '" + type + "' (" + known + ")");
            }

            // Other operations may read the impostor files
            if (type != "meshSettings")
                JobSystem::Wait(context.impostorWrites);

            auto start = std::chrono::steady_clock::now();
            auto affected = handler->second(context, operation);
            log.push_back({ { "operation", CapList(operation) }, { "affected", CapList(affected) }, { "seconds", SecondsSince(start) } });
        }
        JobSystem::Wait(context.impostorWrites);

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

}
