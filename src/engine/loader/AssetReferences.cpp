#include "AssetReferences.h"
#include "AssetLoader.h"

#include <filesystem>
#include <unordered_map>
#include <vector>

namespace Atlas {

    namespace Loader {

        const std::string AssetReferences::packageFilename = "package.aepackage";

        // Json pointers are parsed once, a scene of a million entities would otherwise parse them for every entity
        static std::vector<json::json_pointer> MakePointers(std::initializer_list<const char*> pointers) {

            std::vector<json::json_pointer> jsonPointers;
            for (auto pointer : pointers)
                jsonPointers.emplace_back(pointer);
            return jsonPointers;

        }

        // Json pointers to all resource paths of an asset type. New resource fields need to be added here.
        static const auto sceneReferences = MakePointers({
            "/terrain",
            "/sky/probe/cubemapPath",
            "/postProcessing/bloom/dirtMap",
            "/ocean/rippleTexture",
            "/ocean/foamTexture",
            // Resource table of binary scenes, their entities only contain indices into it
            "/resources"
        });

        // Relative to the component, by the key of the component in an entity or the scene sections
        static const std::unordered_map<std::string, std::vector<json::json_pointer>> componentReferences = {
            { "mesh", MakePointers({ "/resourcePath" }) },
            { "animation", MakePointers({ "/resourcePath" }) },
            { "text", MakePointers({ "/resourcePath" }) },
            { "script", MakePointers({ "/resourcePath" }) },
            { "audio", MakePointers({ "/stream/resourcePath" }) },
            { "audioVolume", MakePointers({ "/stream/resourcePath" }) }
        };

        // Shapes of bodies, relative to the creation settings. Compound shapes contain further shapes
        static const auto shapeReferences = MakePointers({
            "/meshSettings/resourcePath",
            "/convexHullSettings/resourcePath"
        });

        static const auto meshReferences = MakePointers({
            "/impostor",
            "/data/materials",
            "/data/animations"
        });

        static const auto materialReferences = MakePointers({
            "/baseColorMapPath",
            "/opacityMapPath",
            "/normalMapPath",
            "/roughnessMapPath",
            "/metalnessMapPath",
            "/aoMapPath",
            "/displacementMapPath",
            "/emissiveMapPath",
            "/graphPath",
            "/graphBake/meshPath",
            "/graphBake/unbaked/maps/baseColor",
            "/graphBake/unbaked/maps/normal",
            "/graphBake/unbaked/maps/roughness",
            "/graphBake/unbaked/maps/metalness",
            "/graphBake/unbaked/maps/ao",
            "/graphBake/unbaked/maps/emissive",
            "/graphBake/unbaked/maps/opacity"
        });

        static const auto creationShapeReference = json::json_pointer("/creationSettings/shape");
        static const auto compoundChildrenReference = json::json_pointer("/compoundSettings/children");
        static const auto layerReferences = MakePointers({ "/resourcePath" });
        static const auto graphNodeReferences = MakePointers({ "/texturePath" });

        // Calls func with every value that references a resource
        template<class Func>
        static void TransformPaths(json& j, const std::vector<json::json_pointer>& pointers, Func& func) {

            for (const auto& pointer : pointers) {
                if (!j.contains(pointer))
                    continue;

                // Arrays are used for lists of resources, e.g. the materials of a mesh
                auto& value = j[pointer];
                if (value.is_array()) {
                    for (auto& element : value)
                        func(element);
                }
                else {
                    func(value);
                }
            }

        }

        template<class Func>
        static void TransformShape(json& j, Func& func) {

            TransformPaths(j, shapeReferences, func);

            if (j.contains(compoundChildrenReference)) {
                for (auto& child : j[compoundChildrenReference])
                    if (child.contains("shape"))
                        TransformShape(child["shape"], func);
            }

        }

        static const std::vector<json::json_pointer>* GetComponentReferences(const std::string& key) {

            auto references = componentReferences.find(key);
            return references != componentReferences.end() ? &references->second : nullptr;

        }

        template<class Func>
        static void TransformComponent(const std::string& key, const std::vector<json::json_pointer>* references,
            json& j, Func& func) {

            if (references)
                TransformPaths(j, *references, func);

            if ((key == "rigidBody" || key == "player") && j.contains(creationShapeReference))
                TransformShape(j[creationShapeReference], func);

            // The layers of an animation component are a list of objects with a path each
            if (key == "animation" && j.contains("layers")) {
                for (auto& layer : j["layers"])
                    TransformPaths(layer, layerReferences, func);
            }

        }

        template<class Func>
        static void TransformEntity(json& j, Func& func) {

            for (auto& [key, component] : j.items())
                TransformComponent(key, GetComponentReferences(key), component, func);

            // Prefabs store the body creation settings of the root entity separately
            if (j.contains("body") && j["body"].contains("shape"))
                TransformShape(j["body"]["shape"], func);

            if (j.contains("entities")) {
                for (auto& child : j["entities"])
                    TransformEntity(child, func);
            }

        }

        // Scenes store their entities as a table and each component type in a section, older scenes nest them
        template<class Func>
        static void TransformSceneEntities(json& j, Func& func) {

            if (auto components = j.find("components"); components != j.end()) {
                for (auto& [key, section] : components->items()) {
                    // Sections with binary columns only contain indices into the resource table
                    auto values = section.find("values");
                    if (values == section.end())
                        continue;

                    auto references = GetComponentReferences(key);
                    for (auto& component : *values)
                        TransformComponent(key, references, component, func);
                }
            }

            if (j.contains("entities") && j["entities"].is_array()) {
                for (auto& entity : j["entities"])
                    TransformEntity(entity, func);
            }

        }

        template<class Func>
        static void Transform(json& j, AssetReferences::Type type, Func& func) {

            switch (type) {
            case AssetReferences::Type::Scene:
                TransformPaths(j, sceneReferences, func);
                // The components of binary scenes only contain indices into the resource table
                if (!j.contains("resources"))
                    TransformSceneEntities(j, func);
                break;
            case AssetReferences::Type::Entity: TransformEntity(j, func); break;
            case AssetReferences::Type::Mesh: TransformPaths(j, meshReferences, func); break;
            case AssetReferences::Type::Material:
                TransformPaths(j, materialReferences, func);
                // The textures of graph parameters are stored by the name of their parameter
                if (j.contains("graphTextures")) {
                    for (auto& [_, texture] : j["graphTextures"].items())
                        func(texture);
                }
                break;
            case AssetReferences::Type::MaterialGraph:
                if (j.contains("nodes")) {
                    for (auto& node : j["nodes"])
                        TransformPaths(node, graphNodeReferences, func);
                }
                break;
            }

        }

        static std::filesystem::path GetNormalizedFullPath(const std::string& path) {

            return std::filesystem::path(AssetLoader::GetFullPath(path)).lexically_normal();

        }

        static std::filesystem::path FindPackageRootPath(const std::string& file) {

            auto directory = GetNormalizedFullPath(file).parent_path();

            std::error_code errorCode;
            while (!directory.empty()) {
                if (std::filesystem::exists(directory / AssetReferences::packageFilename, errorCode))
                    return directory;

                auto parent = directory.parent_path();
                if (parent == directory)
                    break;
                directory = parent;
            }

            return {};

        }

        static std::string MakeReferenceInPackage(const std::filesystem::path& ownerDirectory,
            const std::filesystem::path& packageRoot, const std::string& resourcePath) {

            if (packageRoot.empty() || resourcePath.empty())
                return resourcePath;

            // Only resources inside the package of the owner are referenced relatively
            auto resource = GetNormalizedFullPath(resourcePath);
            auto packageRelative = resource.lexically_relative(packageRoot);
            if (packageRelative.empty() || *packageRelative.begin() == "..")
                return resourcePath;

            auto reference = resource.lexically_relative(ownerDirectory).generic_string();
            return reference.starts_with("../") ? reference : "./" + reference;

        }

        void AssetReferences::MakeReferences(json& j, Type type, const std::string& ownerFile) {

            // Search for the package once per file instead of once per reference
            auto packageRoot = FindPackageRootPath(ownerFile);
            if (packageRoot.empty())
                return;

            auto ownerDirectory = GetNormalizedFullPath(ownerFile).parent_path();
            auto func = [&](json& value) {
                if (value.is_string())
                    value = MakeReferenceInPackage(ownerDirectory, packageRoot, value.get<std::string>());
            };
            Transform(j, type, func);

        }

        void AssetReferences::ResolveReferences(json& j, Type type, const std::string& ownerFile) {

            auto func = [&](json& value) {
                if (value.is_string())
                    value = ResolveReference(ownerFile, value.get<std::string>());
            };
            Transform(j, type, func);

        }

        void AssetReferences::InlineResourceTable(json& j) {

            if (!j.contains("resources"))
                return;

            if (auto components = j.find("components"); components != j.end()) {
                for (const auto& section : *components)
                    AE_ASSERT(!section.contains("columns") && "Columns keep their indices, nest the scene entities first");
            }

            auto paths = j["resources"].get<std::vector<std::string>>();
            auto func = [&](json& value) {
                if (value.is_number_integer())
                    value = paths.at(value.get<size_t>());
            };
            TransformSceneEntities(j, func);
            j.erase("resources");

        }

        std::string AssetReferences::MakeReference(const std::string& ownerFile, const std::string& resourcePath) {

            auto packageRoot = FindPackageRootPath(ownerFile);
            auto ownerDirectory = GetNormalizedFullPath(ownerFile).parent_path();
            return MakeReferenceInPackage(ownerDirectory, packageRoot, resourcePath);

        }

        std::string AssetReferences::ResolveReference(const std::string& ownerFile, const std::string& reference) {

            bool isRelative = reference.starts_with("./") || reference.starts_with("../") ||
                reference.starts_with(".\\") || reference.starts_with("..\\");
            if (!isRelative)
                return reference;

            // Resolving doesn't touch the file system, such that it works for packed assets as well
            return (std::filesystem::path(ownerFile).parent_path() / reference).lexically_normal().generic_string();

        }

        std::string AssetReferences::FindPackageRoot(const std::string& file) {

            return FindPackageRootPath(file).generic_string();

        }

    }

}
