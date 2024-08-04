#include "Content.h"

namespace Atlas::Editor {

    const std::map<const std::string, ContentType> Content::contentTypeMapping = {
        { "wav", ContentType::Audio },
        { "gltf", ContentType::Mesh },
        { "glb", ContentType::Mesh },
        { "obj", ContentType::Mesh },
        { "fbx", ContentType::Mesh },
        { "usd", ContentType::Mesh },
        { "usda", ContentType::Mesh },
        { "aeterrain", ContentType::Terrain },
        { "aescene", ContentType::Scene },
        { "lua", ContentType::Script },
        { "ttf", ContentType::Font },
        { "aeprefab", ContentType::Prefab },
    };

}