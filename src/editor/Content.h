#pragma once

#include <map>
#include <string>
#include <filesystem>

namespace Atlas::Editor {

    enum class ContentType {
        Audio = 0,
        Mesh,
        MeshSource,
        Material,
        Terrain,
        Scene,
        Script,
        Font,
        Prefab,
        Texture,
        EnvironmentTexture,
        Animation,
        MaterialGraph,
        None
    };

    class Content {

    public:
        std::string name;

        std::filesystem::path path;
        std::string assetPath;

        ContentType type = ContentType::None;
        uintmax_t size = 0;

        static const std::map<const std::string, ContentType> contentTypeMapping;
    };

}