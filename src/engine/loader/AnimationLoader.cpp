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
            auto data = Loader::AssetLoader::GetFileContent(fileStream);
            fileStream.close();
            j = json::parse(data);
        }

        auto animation = CreateRef<Animation::Animation>();
        from_json(j, *animation);

        return animation;

    }

    void AnimationLoader::SaveAnimation(const Ref<Animation::Animation>& animation, const std::string& filename, bool binaryJson) {

        json j;
        to_json(j, *animation);

        auto format = binaryJson ? Loader::JsonFormat::MessagePack : Loader::JsonFormat::Compact;
        if (!Loader::AssetLoader::WriteJson(filename, j, format))
            Log::Error("Couldn't write animation file " + filename);

    }

}
