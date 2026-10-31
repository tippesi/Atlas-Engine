#include "AssetReferences.h"
#include "AssetLoader.h"

#include <filesystem>
#include <vector>

namespace Atlas {

    namespace Loader {

        const std::string AssetReferences::packageFilename = "package.aepackage";

        // Json pointers to all resource paths of an asset type. New resource fields need to be added here.
        static const std::vector<std::string> sceneReferences = {
            "/terrain",
            "/sky/probe/cubemapPath",
            "/postProcessing/bloom/dirtMap"
        };

        static const std::vector<std::string> entityReferences = {
            "/mesh/resourcePath",
            "/animation/resourcePath",
            "/text/resourcePath",
            "/script/resourcePath",
            "/audio/stream/resourcePath",
            "/audioVolume/stream/resourcePath",
            "/rigidBody/creationSettings/shape/meshSettings/resourcePath",
            // Prefabs store the body creation settings of the root entity separately
            "/body/shape/meshSettings/resourcePath"
        };

        static const std::vector<std::string> meshReferences = {
            "/impostor",
            "/data/materials"
        };

        static const std::vector<std::string> materialReferences = {
            "/baseColorMapPath",
            "/opacityMapPath",
            "/normalMapPath",
            "/roughnessMapPath",
            "/metalnessMapPath",
            "/aoMapPath",
            "/displacementMapPath",
            "/emissiveMapPath"
        };

        template<class Func>
        static void TransformPaths(json& j, const std::vector<std::string>& pointers, Func& func) {

            for (const auto& pointer : pointers) {
                auto jsonPointer = json::json_pointer(pointer);
                if (!j.contains(jsonPointer))
                    continue;

                // Arrays are used for lists of resources, e.g. the materials of a mesh
                auto& value = j[jsonPointer];
                if (value.is_string()) {
                    value = func(value.get<std::string>());
                }
                else if (value.is_array()) {
                    for (auto& element : value)
                        if (element.is_string())
                            element = func(element.get<std::string>());
                }
            }

        }

        template<class Func>
        static void TransformEntity(json& j, Func& func) {

            TransformPaths(j, entityReferences, func);

            if (j.contains("entities")) {
                for (auto& child : j["entities"])
                    TransformEntity(child, func);
            }

        }

        template<class Func>
        static void Transform(json& j, AssetReferences::Type type, Func& func) {

            switch (type) {
            case AssetReferences::Type::Scene:
                TransformPaths(j, sceneReferences, func);
                if (j.contains("entities")) {
                    for (auto& entity : j["entities"])
                        TransformEntity(entity, func);
                }
                break;
            case AssetReferences::Type::Entity: TransformEntity(j, func); break;
            case AssetReferences::Type::Mesh: TransformPaths(j, meshReferences, func); break;
            case AssetReferences::Type::Material: TransformPaths(j, materialReferences, func); break;
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
            auto func = [&](const std::string& resourcePath) {
                return MakeReferenceInPackage(ownerDirectory, packageRoot, resourcePath);
            };
            Transform(j, type, func);

        }

        void AssetReferences::ResolveReferences(json& j, Type type, const std::string& ownerFile) {

            auto func = [&](const std::string& reference) {
                return ResolveReference(ownerFile, reference);
            };
            Transform(j, type, func);

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
