#include "TextureStreaming.h"
#include "StreamingManager.h"
#include "../Material.h"
#include "../common/Path.h"
#include "../graphics/Image.h"
#include "../loader/AssetLoader.h"
#include "../loader/KtxLoader.h"
#include "../resource/ResourceLoadException.h"

#include <algorithm>
#include <limits>

namespace Atlas::Streaming {

    class TextureStreaming::Entry : public MipResidencyEntry {

    public:
        bool Prepare() override {

            if (texture.expired())
                return false;

            PrepareLevels(StreamingManager::settings.textures);
            return true;

        }

        bool Load(int32_t level) override {

            auto image = Loader::KtxLoader::LoadImageFromMemory(data, filename, colorSpaceConversion,
                GetResolution(level));
            loaded = CreateRef<Texture::Texture2D>(image, wrapping, filtering);
            return true;

        }

        void Apply() override {

            if (auto resident = texture.lock()) {
                resident->image = loaded->image;
                resident->sampler = loaded->sampler;
                resident->width = loaded->width;
                resident->height = loaded->height;
            }
            loaded.reset();

        }

        std::weak_ptr<Texture::Texture2D> texture;

        std::string filename;
        std::vector<char> data;
        bool colorSpaceConversion = false;
        Texture::Wrapping wrapping = Texture::Wrapping::Repeat;
        Texture::Filtering filtering = Texture::Filtering::Anisotropic;

        Ref<Texture::Texture2D> loaded;

    };

    Residency TextureStreaming::residency{ "Texture streaming", 0 };

    Ref<Texture::Texture2D> TextureStreaming::LoadTexture(const std::string& filename, bool colorSpaceConversion,
        Texture::Wrapping wrapping, Texture::Filtering filtering) {

        auto mipMapped = filtering == Texture::Filtering::MipMapNearest || filtering == Texture::Filtering::MipMapLinear ||
            filtering == Texture::Filtering::Anisotropic;
        if (!mipMapped || !Loader::KtxLoader::IsKtxFile(filename))
            return CreateRef<Texture::Texture2D>(filename, colorSpaceConversion, wrapping, filtering);

        auto normalizedFileName = Common::Path::Normalize(filename);
        auto fileStream = Loader::AssetLoader::ReadFile(normalizedFileName, std::ios::in | std::ios::binary);
        if (!fileStream.is_open())
            throw ResourceLoadException(normalizedFileName, "Couldn't open ktx file stream");

        auto data = Loader::AssetLoader::GetFileContent(fileStream);
        fileStream.close();

        auto texture = LoadTextureFromMemory(std::move(data), normalizedFileName, colorSpaceConversion, wrapping, filtering);

        Log::Message("Loaded image " + normalizedFileName);

        return texture;

    }

    Ref<Texture::Texture2D> TextureStreaming::LoadTextureFromMemory(std::vector<char> data, const std::string& filename,
        bool colorSpaceConversion, Texture::Wrapping wrapping, Texture::Filtering filtering) {

        auto normalizedFileName = Common::Path::Normalize(filename);

        auto entry = CreateRef<Entry>();
        entry->data = std::move(data);

        // Without streaming the textures are still registered, so it can be enabled later
        const auto& settings = StreamingManager::settings.textures;
        auto image = Loader::KtxLoader::LoadImageFromMemory(entry->data, normalizedFileName, colorSpaceConversion,
            settings.enable ? settings.minResolution : std::numeric_limits<int32_t>::max());
        if (image->layers > 1)
            throw ResourceLoadException(filename, "Ktx2 texture arrays can't be loaded as a 2D texture");

        auto texture = CreateRef<Texture::Texture2D>(image, wrapping, filtering);

        entry->texture = texture;
        entry->filename = normalizedFileName;
        entry->colorSpaceConversion = colorSpaceConversion;
        entry->wrapping = wrapping;
        entry->filtering = filtering;
        entry->width = image->fileWidth;
        entry->height = image->fileHeight;
        entry->mipCount = int32_t(image->mipLevels + image->skippedMipLevels);
        entry->residentLevel = int32_t(image->skippedMipLevels);
        entry->AddLevelSizes(*texture->image);

        // Files without levels above the minimum resolution are always complete
        if (entry->GetMinLevel(settings.minResolution) == 0)
            return texture;

        std::scoped_lock lock(residency.mutex);
        residency.Insert(texture.get(), entry);

        return texture;

    }

    void TextureStreaming::Request(std::span<const MaterialRequest> requests) {

        std::scoped_lock lock(residency.mutex);

        auto request = [&](const ResourceHandle<Texture::Texture2D>& map, float resolution) {
            if (!map.IsLoaded())
                return;
            if (auto entry = static_cast<Entry*>(residency.Find(map.Get().get())))
                entry->request = std::max(entry->request, resolution);
            };

        for (const auto& [material, resolution] : requests) {
            request(material->baseColorMap, resolution);
            request(material->opacityMap, resolution);
            request(material->normalMap, resolution);
            request(material->roughnessMap, resolution);
            request(material->metalnessMap, resolution);
            request(material->aoMap, resolution);
            request(material->displacementMap, resolution);
            request(material->emissiveMap, resolution);

            if (!material->HasGraph())
                continue;

            auto compiled = material->graph->GetCompiled();
            if (!compiled)
                continue;

            // Graphs scale the coordinates per texture, e.g. several layers of a surface
            for (const auto& parameter : compiled->parameters) {
                if (parameter.type != MaterialGraph::ValueType::Texture)
                    continue;

                request(material->GetGraphTexture(parameter), resolution * parameter.coordinateScale);
            }
        }

    }

    float TextureStreaming::GetResolution(float uvScale, float distance, float focalLength) {

        return distance > 0.0f ? uvScale * focalLength / distance : std::numeric_limits<float>::max();

    }

    void TextureStreaming::LoadFullResolution(const Material& material) {

        auto loadNow = [&](ResourceHandle<Texture::Texture2D> map) {
            map.WaitForLoad();
            if (map.IsLoaded())
                residency.LoadNow(map.Get().get(), 0);
            };

        for (const auto& map : material.GetMaps())
            loadNow(map);
        for (const auto& map : material.GetGraphTextures())
            loadNow(map);

    }

}
