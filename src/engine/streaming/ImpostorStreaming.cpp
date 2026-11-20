#include "ImpostorStreaming.h"
#include "StreamingManager.h"
#include "../graphics/Image.h"
#include "../loader/AssetLoader.h"
#include "../loader/ImpostorLoader.h"
#include "../resource/ResourceLoadException.h"

#include <algorithm>
#include <limits>

namespace Atlas::Streaming {

    class ImpostorStreaming::Entry : public MipResidencyEntry {

    public:
        bool Prepare() override {

            if (impostor.expired())
                return false;

            // Impostors nobody requests leave the GPU
            PrepareLevels(StreamingManager::settings.impostors);
            if (!requested)
                desiredLevel = mipCount;
            requested = false;
            return true;

        }

        bool Load(int32_t level) override {

            loaded = Loader::ImpostorLoader::LoadImpostorTextures(data, filename, binaryJson, GetResolution(level));
            return loaded.IsComplete();

        }

        void Apply() override {

            if (auto resident = impostor.lock()) {
                resident->baseColorTexture = loaded.baseColor;
                resident->roughnessMetalnessAoTexture = loaded.roughnessMetalnessAo;
                resident->normalTexture = loaded.normal;
                resident->depthTexture = loaded.depth;
            }
            loaded = {};

        }

        Release ReleaseTo(int32_t level) override {

            if (level < mipCount)
                return Release::NeedsLoad;

            if (auto resident = impostor.lock()) {
                resident->baseColorTexture.reset();
                resident->roughnessMetalnessAoTexture.reset();
                resident->normalTexture.reset();
                resident->depthTexture.reset();
            }
            return Release::Done;

        }

        std::weak_ptr<Mesh::Impostor> impostor;

        std::string filename;
        std::vector<char> data;
        bool binaryJson = true;

        bool requested = false;

        Loader::ImpostorLoader::ImpostorTextures loaded;

    };

    Residency ImpostorStreaming::residency{ "Impostor streaming", 1 };

    Ref<Mesh::Impostor> ImpostorStreaming::LoadImpostor(const std::string& filename, bool binaryJson) {

        Loader::AssetLoader::UnpackFile(filename);
        auto fileStream = Loader::AssetLoader::ReadFile(Loader::AssetLoader::GetFullPath(filename),
            std::ios::in | std::ios::binary);
        if (!fileStream.is_open())
            throw ResourceLoadException(filename, "Couldn't open impostor file stream");

        auto entry = CreateRef<Entry>();
        entry->data = Loader::AssetLoader::GetFileContent(fileStream);
        fileStream.close();

        // Without streaming the impostors are still registered, so it can be enabled later
        const auto& settings = StreamingManager::settings.impostors;
        auto impostor = Loader::ImpostorLoader::LoadImpostorFromMemory(entry->data, filename, binaryJson,
            settings.enable ? settings.minResolution : std::numeric_limits<int32_t>::max());
        if (!impostor->isGenerated)
            return impostor;

        entry->impostor = impostor;
        entry->filename = filename;
        entry->binaryJson = binaryJson;
        entry->width = impostor->resolution;
        entry->height = impostor->resolution;
        while ((impostor->resolution >> entry->mipCount) > 0)
            entry->mipCount++;

        while (entry->GetResolution(entry->residentLevel) > impostor->baseColorTexture->width)
            entry->residentLevel++;

        // The alignment of small views over many layers needs more memory than their texels
        for (const auto& texture : { impostor->baseColorTexture, impostor->roughnessMetalnessAoTexture,
            impostor->normalTexture, impostor->depthTexture })
            entry->AddLevelSizes(*texture->image);

        std::scoped_lock lock(residency.mutex);
        residency.Insert(impostor.get(), entry);

        return impostor;

    }

    void ImpostorStreaming::Request(std::span<const ImpostorRequest> requests) {

        std::scoped_lock lock(residency.mutex);

        for (const auto& [impostor, resolution] : requests) {
            if (auto entry = static_cast<Entry*>(residency.Find(impostor))) {
                entry->requested = true;
                entry->request = std::max(entry->request, resolution);
            }
        }

    }

    float ImpostorStreaming::GetResolution(float radius, float distance, float focalLength) {

        return distance > 0.0f ? 2.0f * radius * focalLength / distance : std::numeric_limits<float>::max();

    }

}
