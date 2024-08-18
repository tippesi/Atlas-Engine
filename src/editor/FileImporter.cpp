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

        if (!Content::contentTypeMapping.contains(fileType)) {
            Log::Warning("Incompatible file type " + fileType);
            return;
        }

        // Prefabs can't be imported via the resource manager (need to be created on scene by scene basis)
        auto type = Content::contentTypeMapping.at(fileType);
        switch(type) {
        case ContentType::Audio: ImportFile<Audio::AudioData>(filename); break;
        case ContentType::Mesh: ImportFile<Mesh::Mesh>(filename); break;
        case ContentType::Material: ImportFile<Mesh::Mesh>(filename); break;
        case ContentType::Texture: ImportFile<Texture::Texture2D>(filename); break;
        case ContentType::EnvironmentTexture: ImportFile<Texture::Cubemap>(filename); break;
        case ContentType::Scene: ImportFile<Scene::Scene>(filename); break;
        case ContentType::Script: ImportFile<Scripting::Script>(filename); break;
        case ContentType::Font: ImportFile<Font>(filename); break;
        default: break;
        }

    }

}