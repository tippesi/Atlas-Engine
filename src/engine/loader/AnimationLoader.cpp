#include "AnimationLoader.h"
#include "AssetLoader.h"
#include "animation/AnimationSerializer.h"
#include "resource/Resource.h"
#include "Log.h"

namespace Atlas::Loader {

    Ref<Animation::Animation> AnimationLoader::LoadAnimation(const std::string& filename, bool binaryJson) {

        Loader::AssetLoader::UnpackFile(filename);
        auto path = Loader::AssetLoader::GetFullPath(filename);

        auto fileStream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);

        if (!fileStream.is_open()) {
            throw ResourceLoadException(filename, "Couldn't open animation file stream: " + std::string(strerror(errno)));
        }

        json j;
        if (binaryJson) {
            auto data = Loader::AssetLoader::GetFileContent(fileStream);
            // We don't want to keep the file stream open longer than we need
            fileStream.close();
            j = json::from_msgpack(data);
        }
        else {
            std::string serialized((std::istreambuf_iterator<char>(fileStream)),
                std::istreambuf_iterator<char>());
            fileStream.close();
            j = json::parse(serialized);
        }

        auto animation = CreateRef<Animation::Animation>();
        from_json(j, *animation);

        return animation;

    }

    void AnimationLoader::SaveAnimation(const Ref<Animation::Animation>& animation, const std::string& filename, bool binaryJson) {

        auto path = Loader::AssetLoader::GetFullPath(filename);
        auto fileStream = Loader::AssetLoader::WriteFile(path, std::ios::out | std::ios::binary);

        if (!fileStream.is_open()) {
            Log::Error("Couldn't write animation file " + filename);
            return;
        }

        json j;
        to_json(j, *animation);

        if (binaryJson) {
            auto data = json::to_msgpack(j);
            fileStream.write(reinterpret_cast<const char*>(data.data()), data.size());
        }
        else {
            fileStream << j.dump();
        }

        fileStream.close();

    }

}
