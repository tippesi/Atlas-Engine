#pragma once

#include <vector>
#include <string>
#include <map>
#include <type_traits>

#include "resource/ResourceManager.h"

#include "mesh/Mesh.h"
#include "scene/Scene.h"
#include "audio/AudioData.h"
#include "scripting/Script.h"
#include "Font.h"

#include "loader/ModelLoader.h"
#include "Serializer.h"

namespace Atlas::Editor {

    enum class FileType {
        Audio = 0,
        Mesh,
        Terrain,
        Scene,
        Script,
        Font,
        Prefab
    };

    class FileImporter {

    public:
        static void ImportFiles(const std::vector<std::string>& filenames);

        static void ImportFile(const std::string& filename);

        template<class T>
        static ResourceHandle<T> ImportFile(const std::string& filename);

        template<class T>
        static bool AreCompatible(const std::string& filename);

        static const std::map<const std::string, FileType> fileTypeMapping;

    };

    template<class T>
    ResourceHandle<T> FileImporter::ImportFile(const std::string& filename) {

        ResourceHandle<T> handle;

        if constexpr (std::is_same_v<T, Audio::AudioData>) {
            handle = ResourceManager<Audio::AudioData>::GetOrLoadResourceAsync(
                filename, ResourceOrigin::User);
        }
        else if constexpr (std::is_same_v<T, Mesh::Mesh>) {
            handle = ResourceManager<Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(filename,
                ResourceOrigin::User, Loader::ModelLoader::LoadMesh, false, 2048);
        }
        else if constexpr (std::is_same_v<T, Scene::Scene>) {
            handle = ResourceManager<Scene::Scene>::GetOrLoadResourceWithLoaderAsync(filename,
                ResourceOrigin::User, Serializer::DeserializeScene, false);
        }
        else if constexpr (std::is_same_v<T, Scripting::Script>) {
            handle = ResourceManager<Scripting::Script>::GetOrLoadResourceAsync(
                filename, ResourceOrigin::User);
        }
        else if constexpr (std::is_same_v<T, Font>) {
            handle = ResourceManager<Font>::GetOrLoadResourceAsync(filename,
                ResourceOrigin::User, 32.0f, 8, 127);
        }

        return handle;

    }

    template<class T>
    bool FileImporter::AreCompatible(const std::string& filename) {

        std::string fileType = Common::Path::GetFileType(filename);
        std::transform(fileType.begin(), fileType.end(), fileType.begin(), ::tolower);

        if (!fileTypeMapping.contains(fileType))
            return false;

        auto type = fileTypeMapping.at(fileType);

        if constexpr (std::is_same_v<T, Audio::AudioData>) {
            return type == FileType::Audio;
        }
        else if constexpr (std::is_same_v<T, Mesh::Mesh>) {
            return type == FileType::Mesh;
        }
        else if constexpr (std::is_same_v<T, Scene::Scene>) {
            return type == FileType::Scene;
        }
        else if constexpr (std::is_same_v<T, Scripting::Script>) {
            return type == FileType::Script;
        }
        else if constexpr (std::is_same_v<T, Font>) {
            return type == FileType::Font;
        }
        else if constexpr (std::is_same_v<T, Scene::Entity>) {
            return type == FileType::Prefab;
        }
        else {
            return false;
        }

    }

}