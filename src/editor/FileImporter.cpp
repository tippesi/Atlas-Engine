#include "FileImporter.h"

#include <cctype>

#include "common/Path.h"

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

        // Prefabs can't be imported via the resource manager (need to be created on scene by scene basis)
        auto type = fileTypeMapping.at(fileType);
        switch(type) {
        case FileType::Audio: ImportFile<Audio::AudioData>(filename); break;
        case FileType::Mesh: ImportFile<Mesh::Mesh>(filename); break;
        case FileType::Scene: ImportFile<Scene::Scene>(filename); break;
        case FileType::Script: ImportFile<Scripting::Script>(filename); break;
        case FileType::Font: ImportFile<Font>(filename); break;
        default: break;
        }

    }

    const std::map<const std::string, FileType> FileImporter::fileTypeMapping = {
        { "wav", FileType::Audio },
        { "gltf", FileType::Mesh },
        { "glb", FileType::Mesh },
        { "obj", FileType::Mesh },
        { "fbx", FileType::Mesh },
        { "aeterrain", FileType::Terrain },
        { "aescene", FileType::Scene },
        { "lua", FileType::Script },
        { "ttf", FileType::Font },
        { "aeprefab", FileType::Prefab },
    };

}