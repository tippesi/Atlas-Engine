#pragma once

#include "Common.h"

#include "volume/AABB.h"

namespace Atlas::Capture {

    // An entity of a scene or prefab file read without loading it
    struct FileEntity {

        std::string name;
        int32_t parent = -1;
        mat4 local = mat4(1.0f);
        mat4 global = mat4(1.0f);
        std::string mesh;
        // The components by their key in the file, e.g. mesh, rigidBody, script
        std::vector<std::string> components;

    };

    // Reads the entity tree of a scene (.aescene, .json) or prefab (.aeprefab) with a SAX parser, which takes a few
    // seconds for the world scene of a million entities. Parents come before their children
    std::vector<FileEntity> ReadFileEntities(const fs::path& path);

    // The entity names of a scene or prefab file, cached next to the executable by path, size and write time
    std::vector<std::string> ReadEntityNames(const fs::path& path);

    struct MeshFileInfo {

        std::string name;
        Volume::AABB bounds;
        int64_t vertexCount = 0;
        int64_t indexCount = 0;
        std::vector<std::string> materials;
        // Submeshes with their material index
        std::vector<std::pair<std::string, int32_t>> subData;
        std::string impostor;
        json settings = json::object();

    };

    // Reads an .aemesh file without loading it
    MeshFileInfo ReadMeshFile(const fs::path& path);

    // Names that differ least from the text, ignoring case, for "did you mean" hints
    std::vector<std::string> FindClosestNames(const std::vector<std::string>& names, const std::string& text, size_t count = 3);

}
