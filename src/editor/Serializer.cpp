#include "Serializer.h"
#include "Singletons.h"
#include "FileImporter.h"
#include "common/SerializationHelper.h"

#include "Log.h"
#include "loader/AssetLoader.h"

namespace Atlas::Editor {

    const std::string Serializer::configFilename = "config.json";

    void Serializer::SerializeConfig() {

        auto fileStream = Loader::AssetLoader::WriteFile(configFilename, std::ios::out | std::ios::binary);

        if (!fileStream.is_open()) {
            Log::Error("Couldn't write config file");
            return;
        }

        const auto& config = Singletons::config;

        std::vector<std::string> scenePaths;
        for (const auto& scene : config->openedScenes)
            scenePaths.push_back(scene.GetResource()->path);

        json j = {
            { "pathTrace", config->pathTrace },
            { "scenes", scenePaths }
        };

        fileStream << to_string(j);

        fileStream.close();

    }

    void Serializer::DeserializeConfig() {

        Loader::AssetLoader::UnpackFile(configFilename);
        auto path = Loader::AssetLoader::GetFullPath(configFilename);

        auto fileStream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);

        if (!fileStream.is_open()) {
            return;
        }

        std::string serialized((std::istreambuf_iterator<char>(fileStream)),
            std::istreambuf_iterator<char>());

        fileStream.close();

        if (serialized.empty())
            return;

        json j = json::parse(serialized);

        const auto& config = Singletons::config;

        std::vector<std::string> scenePaths;

        if (j.contains("pathTrace"))
            j.at("pathTrace").get_to(config->pathTrace);

        if (j.contains("scenes"))
            j.at("scenes").get_to(scenePaths);

        // No need to add it to the config, will be done through resource events
        for (const auto& scenePath : scenePaths)
            FileImporter::ImportFile(scenePath);

    }

}