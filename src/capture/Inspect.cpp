#include "Inspect.h"
#include "SceneFile.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>

namespace Atlas::Capture {

    static void Print(const std::string& line) {

        std::printf("%s\n", line.c_str());

    }

    static std::string FormatSize(const Volume::AABB& bounds) {

        auto size = bounds.max - bounds.min;
        return FormatFloat(size.x, 2) + " x " + FormatFloat(size.y, 2) + " x " + FormatFloat(size.z, 2);

    }

    // Where the origin lies in the bounds, e.g. "x center, y bottom, z center"
    static std::string DescribeOrigin(const Volume::AABB& bounds) {

        auto describe = [](float min, float max, const char* low, const char* high) -> std::string {

            if (max - min < 1e-5f)
                return "flat";
            auto t = -min / (max - min);
            if (std::abs(t) < 0.05f)
                return low;
            if (std::abs(t - 1.0f) < 0.05f)
                return high;
            if (std::abs(t - 0.5f) < 0.05f)
                return "center";
            return FormatFloat(t * 100.0f, 0) + "%";

        };

        return "x " + describe(bounds.min.x, bounds.max.x, "min", "max") + ", y " + describe(bounds.min.y, bounds.max.y, "bottom", "top") +
            ", z " + describe(bounds.min.z, bounds.max.z, "min", "max");

    }

    static std::string DescribePlacement(const mat4& matrix) {

        auto position = vec3(matrix[3]);
        auto scale = vec3(glm::length(vec3(matrix[0])), glm::length(vec3(matrix[1])), glm::length(vec3(matrix[2])));
        auto forward = vec3(matrix[2]) / std::max(scale.z, 1e-6f);
        auto yaw = glm::degrees(std::atan2(forward.x, forward.z));

        auto text = FormatVec3(position, 2);
        if (std::abs(yaw) > 0.05f)
            text += ", yaw " + FormatFloat(yaw, 1);
        if (glm::length(scale - vec3(1.0f)) > 1e-3f)
            text += std::abs(scale.x - scale.y) < 1e-3f && std::abs(scale.y - scale.z) < 1e-3f ? ", scale " + FormatFloat(scale.x, 3) :
                ", scale " + FormatVec3(scale, 3);
        return text;

    }

    struct InspectOptions {

        fs::path file;
        std::vector<std::string> filters;
        std::string assembly;
        size_t limit = 40;
        fs::path output;

    };

    static int32_t InspectMesh(const InspectOptions& options) {

        auto info = ReadMeshFile(options.file);
        json result = {
            { "name", info.name }, { "bounds", { { "min", FromVec3(info.bounds.min) }, { "max", FromVec3(info.bounds.max) } } },
            { "origin", DescribeOrigin(info.bounds) }, { "vertices", info.vertexCount }, { "triangles", info.indexCount / 3 },
            { "materials", info.materials }, { "impostor", info.impostor }, { "settings", info.settings }
        };

        Print(info.name + ": " + FormatSize(info.bounds) + " m, origin " + DescribeOrigin(info.bounds) + ", bounds " +
            FormatVec3(info.bounds.min, 2) + " to " + FormatVec3(info.bounds.max, 2));
        Print("  " + std::to_string(info.vertexCount) + " vertices, " + std::to_string(info.indexCount / 3) + " triangles, " +
            std::to_string(info.subData.size()) + " submeshes" + (info.impostor.empty() ? ", no impostor" : ", impostor " + info.impostor));
        for (size_t i = 0; i < info.materials.size(); i++) {
            std::string submeshes;
            for (const auto& [name, index] : info.subData)
                if (index == int32_t(i))
                    submeshes += (submeshes.empty() ? "" : ", ") + name;
            Print("  material " + std::to_string(i) + ": " + info.materials[i] + (submeshes.empty() ? "" : " (" + submeshes + ")"));
        }
        std::string settings;
        for (const auto& key : { "mobility", "castShadow", "rayTrace", "cullBackFaces", "vegetation", "distanceCulling",
            "impostorDistance", "minScreenSize", "invertUVs" })
            if (info.settings.contains(key))
                settings += std::string(settings.empty() ? "" : ", ") + key + " " + info.settings[key].dump();
        Print("  " + settings);

        if (!options.output.empty())
            WriteFileAtomic(options.output, result.dump(2));
        return 0;

    }

    static int32_t InspectModel(const InspectOptions& options) {

        Assimp::Importer importer;
        auto scene = importer.ReadFile(options.file.string(), aiProcess_Triangulate | aiProcess_GenBoundingBoxes);
        if (!scene) {
            Print("Couldn't read " + options.file.string() + ": " + importer.GetErrorString());
            return 2;
        }

        auto toMatrix = [](const aiMatrix4x4& m) {

            return glm::transpose(glm::make_mat4(&m.a1));

        };

        Volume::AABB bounds(vec3(std::numeric_limits<float>::max()), vec3(-std::numeric_limits<float>::max()));
        int64_t vertices = 0, triangles = 0;
        std::vector<std::string> nodeLines;
        std::function<void(const aiNode*, const mat4&, int32_t)> visit = [&](const aiNode* node, const mat4& parent, int32_t depth) {

            auto global = parent * toMatrix(node->mTransformation);
            for (uint32_t i = 0; i < node->mNumMeshes; i++) {
                const auto mesh = scene->mMeshes[node->mMeshes[i]];
                vertices += mesh->mNumVertices;
                triangles += mesh->mNumFaces;
                const auto& box = mesh->mAABB;
                for (int32_t corner = 0; corner < 8; corner++) {
                    vec3 point((corner & 1) ? box.mMax.x : box.mMin.x, (corner & 2) ? box.mMax.y : box.mMin.y, (corner & 4) ? box.mMax.z : box.mMin.z);
                    auto transformed = vec3(global * vec4(point, 1.0f));
                    bounds.min = glm::min(bounds.min, transformed);
                    bounds.max = glm::max(bounds.max, transformed);
                }
            }
            if (depth <= 3 && nodeLines.size() < options.limit)
                nodeLines.push_back(std::string(size_t(depth) * 2 + 2, ' ') + node->mName.C_Str() + ": " + DescribePlacement(toMatrix(node->mTransformation)) +
                    (node->mNumMeshes ? ", " + std::to_string(node->mNumMeshes) + " meshes" : "") +
                    (node->mNumChildren ? ", " + std::to_string(node->mNumChildren) + " children" : ""));
            for (uint32_t i = 0; i < node->mNumChildren; i++)
                visit(node->mChildren[i], global, depth + 1);

        };
        visit(scene->mRootNode, mat4(1.0f), 0);

        double unitScale = 0.0;
        if (scene->mMetaData)
            scene->mMetaData->Get("UnitScaleFactor", unitScale);

        Print(options.file.filename().string() + ": " + std::to_string(scene->mNumMeshes) + " meshes, " + std::to_string(vertices) +
            " vertices, " + std::to_string(triangles) + " triangles, bounds " + FormatSize(bounds) + " (" + FormatVec3(bounds.min, 2) +
            " to " + FormatVec3(bounds.max, 2) + "), origin " + DescribeOrigin(bounds) + (unitScale > 0.0 ? ", unit scale " +
            FormatFloat(float(unitScale), 2) : ""));
        for (uint32_t i = 0; i < scene->mNumMaterials && i < options.limit; i++) {
            auto material = scene->mMaterials[i];
            aiString texture;
            std::string line = "  material " + std::string(material->GetName().C_Str());
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texture) == AI_SUCCESS || material->GetTexture(aiTextureType_BASE_COLOR, 0, &texture) == AI_SUCCESS)
                line += ": " + std::string(texture.C_Str());
            Print(line);
        }
        for (uint32_t i = 0; i < scene->mNumAnimations; i++) {
            auto animation = scene->mAnimations[i];
            auto ticks = animation->mTicksPerSecond > 0.0 ? animation->mTicksPerSecond : 25.0;
            Print("  animation " + std::string(animation->mName.C_Str()) + ": " + FormatFloat(float(animation->mDuration / ticks), 2) + " s, " +
                std::to_string(animation->mNumChannels) + " channels");
        }
        Print("  nodes:");
        for (const auto& line : nodeLines)
            Print(line);
        return 0;

    }

    static int32_t InspectScene(const InspectOptions& options) {

        auto entities = ReadFileEntities(options.file);

        std::map<std::string, int32_t> components, meshes;
        for (const auto& entity : entities) {
            for (const auto& component : entity.components)
                components[component]++;
            if (!entity.mesh.empty())
                meshes[entity.mesh]++;
        }

        std::string componentText;
        for (const auto& [name, count] : components)
            componentText += (componentText.empty() ? "" : ", ") + name + " " + std::to_string(count);
        Print(options.file.filename().string() + ": " + std::to_string(entities.size()) + " entities, " + std::to_string(meshes.size()) +
            " meshes; components: " + componentText);

        std::vector<std::pair<int32_t, std::string>> meshList;
        for (const auto& [path, count] : meshes)
            meshList.emplace_back(count, path);
        std::sort(meshList.rbegin(), meshList.rend());
        Print("  most used meshes:");
        for (size_t i = 0; i < std::min(meshList.size(), size_t(12)); i++)
            Print("    " + std::to_string(meshList[i].first) + "x " + meshList[i].second);

        json result = { { "entities", entities.size() }, { "components", components }, { "meshes", meshes } };

        if (!options.filters.empty()) {
            json matches = json::array();
            size_t count = 0;
            Print("  entities matching the filter:");
            for (const auto& entity : entities) {
                if (!MatchAnyPattern(options.filters, entity.name))
                    continue;
                count++;
                std::string parent = entity.parent >= 0 ? entities[entity.parent].name : "";
                matches.push_back({ { "name", entity.name }, { "position", FromVec3(vec3(entity.global[3])) }, { "mesh", entity.mesh },
                    { "parent", parent }, { "components", entity.components } });
                if (count <= options.limit)
                    Print("    " + entity.name + " at " + DescribePlacement(entity.global) + (entity.mesh.empty() ? "" : ", " + entity.mesh) +
                        (parent.empty() ? "" : ", in " + parent));
            }
            if (count > options.limit)
                Print("    ... " + std::to_string(count - options.limit) + " more (--limit, --out)");
            Print("  " + std::to_string(count) + " entities match");
            result["matches"] = matches;
        }

        if (!options.assembly.empty()) {
            auto root = std::find_if(entities.begin(), entities.end(), [&](const FileEntity& entity) {
                return Helper::String::MatchWildcard(options.assembly, entity.name); });
            if (root == entities.end()) {
                std::vector<std::string> names;
                for (const auto& entity : entities)
                    names.push_back(entity.name);
                std::string closest;
                for (const auto& name : FindClosestNames(names, options.assembly))
                    closest += (closest.empty() ? "'" : ", '") + name + "'";
                Print("No entity matches '" + options.assembly + "', closest: " + closest);
                return 2;
            }

            // Mesh bounds are read once per mesh file, paths in packages are relative to the scene
            std::map<std::string, std::optional<Volume::AABB>> meshBounds;
            auto getBounds = [&](const std::string& mesh) -> std::optional<Volume::AABB> {

                if (auto it = meshBounds.find(mesh); it != meshBounds.end())
                    return it->second;
                auto path = mesh.starts_with(".") ? options.file.parent_path() / mesh : GetAssetDirectory() / mesh;
                std::optional<Volume::AABB> bounds;
                try {
                    bounds = ReadMeshFile(path.lexically_normal()).bounds;
                }
                catch (const std::exception&) {}
                return meshBounds[mesh] = bounds;

            };

            auto rootIndex = int32_t(root - entities.begin());
            auto inverse = glm::inverse(root->global);
            json parts = json::array();
            Print("  assembly " + root->name + " at " + DescribePlacement(root->global) + ", parts relative to it:");
            std::vector<int32_t> depths(entities.size(), -1);
            depths[rootIndex] = 0;
            size_t printed = 0;
            for (size_t i = size_t(rootIndex) + 1; i < entities.size(); i++) {
                const auto& entity = entities[i];
                if (entity.parent < 0 || depths[entity.parent] < 0)
                    continue;
                depths[i] = depths[entity.parent] + 1;
                auto relative = inverse * entity.global;
                std::string boundsText;
                json part = { { "name", entity.name }, { "offset", FromVec3(vec3(relative[3])) }, { "mesh", entity.mesh } };
                if (!entity.mesh.empty()) {
                    if (auto bounds = getBounds(entity.mesh)) {
                        auto transformed = bounds->Transform(relative);
                        boundsText = ", bounds " + FormatVec3(transformed.min, 2) + " to " + FormatVec3(transformed.max, 2);
                        part["bounds"] = { { "min", FromVec3(transformed.min) }, { "max", FromVec3(transformed.max) } };
                    }
                }
                parts.push_back(part);
                if (printed++ < options.limit)
                    Print(std::string(size_t(depths[i]) * 2 + 2, ' ') + entity.name + ": " + DescribePlacement(relative) +
                        (entity.mesh.empty() ? "" : ", " + fs::path(entity.mesh).filename().string()) + boundsText);
            }
            if (printed > options.limit)
                Print("    ... " + std::to_string(printed - options.limit) + " more parts (--limit, --out)");
            result["assembly"] = { { "root", root->name }, { "parts", parts } };
        }

        if (!options.output.empty()) {
            WriteFileAtomic(options.output, result.dump(2));
            Print("  written to " + options.output.string());
        }
        return 0;

    }

    int32_t Inspect(const std::vector<std::string>& args) {

        if (args.size() < 3)
            throw std::runtime_error("inspect expects a scene, prefab, mesh or model file");

        InspectOptions options;
        auto file = ResolveUserPath(args[2]);
        std::error_code error;
        options.file = fs::exists(file, error) ? file : GetAssetDirectory() / args[2];
        for (size_t i = 3; i < args.size(); i++) {
            if (args[i] == "--filter" && i + 1 < args.size())
                options.filters.push_back(args[++i]);
            else if (args[i] == "--assembly" && i + 1 < args.size())
                options.assembly = args[++i];
            else if (args[i] == "--limit" && i + 1 < args.size())
                options.limit = size_t(std::stoul(args[++i]));
            else if (args[i] == "--out" && i + 1 < args.size())
                options.output = ResolveUserPath(args[++i]);
            else
                throw std::runtime_error("Unknown inspect option '" + args[i] + "' (--filter, --assembly, --limit, --out)");
        }
        if (!fs::exists(options.file, error))
            throw std::runtime_error("No file " + args[2] + " in the launch or the data directory");

        auto extension = options.file.extension().string();
        if (extension == ".aemesh")
            return InspectMesh(options);
        if (extension == ".aescene" || extension == ".aeprefab" || extension == ".json")
            return InspectScene(options);
        return InspectModel(options);

    }

}
