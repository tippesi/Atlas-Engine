#include "FileImporter.h"

#include <cctype>

#include "common/Path.h"

#include "mesh/Mesh.h"
#include "scene/Scene.h"
#include "audio/AudioData.h"

#include "loader/ModelLoader.h"
#include "scene/SceneSerializer.h"

namespace Atlas::Editor {

    void FileImporter::ImportFiles(const std::vector<std::string> &filenames) {

        for (const auto& filename : filenames)
            ImportFile(filename);

    }

    void FileImporter::ImportFile(const std::string &filename) {

        std::string fileType = Common::Path::GetFileType(filename);
        std::transform(fileType.begin(), fileType.end(), fileType.begin(), ::tolower);

        if (!fileTypeMapping.contains(fileType)) {
            Log::Warning("Incompatible file type " + fileType);
            return;
        }

        auto type = fileTypeMapping.at(fileType);
        switch(type) {
            case FileType::Audio: {
                    auto handle = ResourceManager<Audio::AudioData>::GetOrLoadResourceAsync(
                        filename, ResourceOrigin::User);
                    handle.GetResource()->permanent = true;
                }
                break;
            case FileType::Mesh: {
                    auto handle = ResourceManager<Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(filename,
                        ResourceOrigin::User, Loader::ModelLoader::LoadMesh, false, glm::mat4(1.0f), 8192);
                    handle.GetResource()->permanent = true;
                }
                break;
            case FileType::Scene: {
                    auto handle = ResourceManager<Scene::Scene>::GetOrLoadResourceWithLoaderAsync(filename,
                        ResourceOrigin::User, Scene::SceneSerializer::DeserializeScene);
                }
                break;
            default:
                break;
        }

    }

    const std::map<const std::string, FileImporter::FileType> FileImporter::fileTypeMapping = {
        { "wav", FileType::Audio },
        { "gltf", FileType::Mesh },
        { "glb", FileType::Mesh },
        { "obj", FileType::Mesh },
        { "fbx", FileType::Mesh },
        { "aeterrain", FileType::Terrain },
        { "aescene", FileType::Scene },
    };

}