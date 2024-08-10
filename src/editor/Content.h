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
        None
    };

    class Content {

    public:
        std::string name;

        std::filesystem::path path;
        std::string assetPath;

        ContentType type;

        static const std::map<const std::string, ContentType> contentTypeMapping;
    };

}