#include "Content.h"

namespace Atlas::Editor {

    const std::map<const std::string, ContentType> Content::contentTypeMapping = {
        { "wav", ContentType::Audio },
        { "aemesh", ContentType::Mesh },
        { "gltf", ContentType::MeshSource },
        { "glb", ContentType::MeshSource },
        { "obj", ContentType::MeshSource },
        { "fbx", ContentType::MeshSource },
        { "usd", ContentType::MeshSource },
        { "usda", ContentType::MeshSource },
        { "aematerial", ContentType::Material },
        { "aeterrain", ContentType::Terrain },
        { "aescene", ContentType::Scene },
        { "lua", ContentType::Script },
        { "ttf", ContentType::Font },
        { "aeprefab", ContentType::Prefab },
    };

}