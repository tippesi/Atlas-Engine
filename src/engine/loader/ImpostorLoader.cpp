#include "ImpostorLoader.h"
#include "AssetLoader.h"
#include "KtxLoader.h"
#include "graphics/Format.h"
#include "mesh/MeshSerializer.h"

#include <cstring>

namespace Atlas::Loader {

    bool ImpostorLoader::ImpostorTextures::IsComplete() const {

        return baseColor && roughnessMetalnessAo && normal && depth;

    }

    Ref<Mesh::Impostor> ImpostorLoader::LoadImpostorFromMemory(const std::vector<char>& data, const std::string& filename,
        bool binaryJson, int32_t maxResolution) {

        auto j = ParseFile(data, binaryJson);

        auto impostor = CreateRef<Mesh::Impostor>();
        from_json(j, *impostor);

        impostor->RefreshViewPlaneBuffer();

        auto textures = LoadTextures(j, filename, impostor->resolution, impostor->views, maxResolution);
        impostor->baseColorTexture = textures.baseColor;
        impostor->roughnessMetalnessAoTexture = textures.roughnessMetalnessAo;
        impostor->normalTexture = textures.normal;
        impostor->depthTexture = textures.depth;

        impostor->isGenerated = textures.IsComplete();

        return impostor;

    }

    ImpostorLoader::ImpostorTextures ImpostorLoader::LoadImpostorTextures(const std::vector<char>& data,
        const std::string& filename, bool binaryJson, int32_t maxResolution) {

        auto j = ParseFile(data, binaryJson);

        return LoadTextures(j, filename, j.at("resolution").get<int32_t>(), j.at("views").get<int32_t>(), maxResolution);

    }

    json ImpostorLoader::ParseFile(const std::vector<char>& data, bool binaryJson) {

        return binaryJson ? json::from_msgpack(data) : json::parse(data.begin(), data.end());

    }

    ImpostorLoader::ImpostorTextures ImpostorLoader::LoadTextures(json& j, const std::string& filename,
        int32_t fileResolution, int32_t views, int32_t maxResolution) {

        auto jsonToTexture = [&](const char* name, Ref<Texture::Texture2DArray>& texture, bool depth) {

            auto& t = j[name];

            // A compressed texture is one ktx2 array with all views
            if (t.is_object()) {
                std::vector<char> ktxData;
                try_get_binary_json(t, "ktx2", ktxData);

                auto image = KtxLoader::LoadImageFromMemory(ktxData, filename, false, maxResolution);
                texture = CreateRef<Texture::Texture2DArray>(image,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Anisotropic);
                return;
            }

            // Only uncompressed textures are uploaded into allocated arrays, a compressed one would hold both
            // Below the resolution of the file every step-th texel is kept, averages would mix in empty texels
            auto resolution = fileResolution;
            while (resolution > maxResolution && resolution > 1)
                resolution /= 2;
            auto step = fileResolution / resolution;
            auto texelSize = depth ? sizeof(uint16_t) : 4 * sizeof(uint8_t);
            auto layerSize = size_t(resolution) * size_t(resolution) * texelSize;

            // All layers are uploaded at once, every upload transitions the whole array
            std::vector<uint8_t> layerData(layerSize * t.size());
            auto copyLayer = [&](const uint8_t* texels, size_t layer) {
                auto destination = layerData.data() + layer * layerSize;
                if (step == 1) {
                    std::memcpy(destination, texels, layerSize);
                    return;
                }
                for (int32_t y = 0; y < resolution; y++) {
                    for (int32_t x = 0; x < resolution; x++) {
                        auto source = (size_t(y * step) * size_t(fileResolution) + size_t(x * step)) * texelSize;
                        std::memcpy(destination + (size_t(y) * size_t(resolution) + size_t(x)) * texelSize,
                            texels + source, texelSize);
                    }
                }
                };

            auto fileLayerSize = size_t(fileResolution) * size_t(fileResolution) * texelSize;
            size_t layerCount = 0;
            for (auto& jsonLayer : t) {
                const auto& binary = jsonLayer["data"].get_binary();
                if (depth) {
                    if (binary.size() != fileLayerSize)
                        throw ResourceLoadException(filename, "Impostor depth layer has the wrong size");
                    copyLayer(binary.data(), layerCount++);
                }
                else {
                    int32_t width, height, channels;
                    auto pixels = stbi_load_from_memory(binary.data(), int32_t(binary.size()), &width, &height, &channels, 4);
                    if (!pixels)
                        throw ResourceLoadException(filename, "Couldn't decode impostor layer");
                    if (width != fileResolution || height != fileResolution) {
                        stbi_image_free(pixels);
                        throw ResourceLoadException(filename, "Impostor layer has the wrong size");
                    }
                    copyLayer(pixels, layerCount++);
                    stbi_image_free(pixels);
                }
            }

            texture = CreateRef<Texture::Texture2DArray>(resolution, resolution,
                views * views, depth ? VK_FORMAT_R16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Anisotropic);

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            Graphics::MemoryTransferManager transferManager(device, device->memoryManager);

            transferManager.BeginMultiTransfer();

            VkOffset3D offset{ 0, 0, 0 };
            VkExtent3D extent{ texture->image->width, texture->image->height, 1 };
            transferManager.UploadImageData(layerData.data(), texture->image.get(), offset, extent, 0, uint32_t(layerCount));

            transferManager.EndMultiTransfer();
            };

        ImpostorTextures textures;
        if (j.contains("baseColor"))
            jsonToTexture("baseColor", textures.baseColor, false);
        if (j.contains("roughnessMetalnessAo"))
            jsonToTexture("roughnessMetalnessAo", textures.roughnessMetalnessAo, false);
        if (j.contains("normal"))
            jsonToTexture("normal", textures.normal, false);
        if (j.contains("depth"))
            jsonToTexture("depth", textures.depth, true);

        return textures;

    }

    void ImpostorLoader::SaveImpostor(const Ref<Mesh::Impostor>& impostor, const std::string& filename, bool binaryJson) {

        WriteImpostor(ReadImpostor(impostor, filename, binaryJson));

    }

    Ref<ImpostorLoader::ImpostorFileData> ImpostorLoader::ReadImpostor(const Ref<Mesh::Impostor>& impostor,
        const std::string& filename, bool binaryJson) {

        auto path = Loader::AssetLoader::GetFullPath(filename);

        auto data = CreateRef<ImpostorFileData>();
        data->filename = filename;
        data->binaryJson = binaryJson;
        data->compressTextures = impostor->compressTextures;

        auto& j = data->j;
        to_json(j, *impostor);

        // Block compressed textures can't be converted again, their data stays as it is in the file
        json savedJson;
        auto getSavedTexture = [&](const char* name) -> json {
            if (savedJson.is_null()) {
                auto savedStream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);
                if (savedStream.is_open()) {
                    auto content = Loader::AssetLoader::GetFileContent(savedStream);
                    savedStream.close();
                    if (!content.empty())
                        savedJson = binaryJson ? json::from_msgpack(content) :
                            json::parse(content.begin(), content.end());
                }
            }
            if (!savedJson.contains(name)) {
                Log::Error("Couldn't keep the compressed " + std::string(name) + " texture of impostor " + filename);
                return json();
            }
            return savedJson[name];
            };

        if (impostor->isGenerated) {
            auto readTexture = [&](const char* name, const Ref<Texture::Texture2DArray>& texture, bool depth) {
                auto& t = j[name];
                if (Graphics::IsFormatCompressed(texture->format)) {
                    t = getSavedTexture(name);
                    return;
                }

                if (depth) {
                    for (int32_t i = 0; i < texture->depth; i++) {
                        t.push_back(json());
                        set_binary_json(t.back(), "data", texture->GetData<float16>(i));
                    }
                    return;
                }

                auto& layers = data->layers[name];
                for (int32_t i = 0; i < texture->depth; i++) {
                    auto& layer = layers.emplace_back(CreateRef<Common::Image<uint8_t>>(
                        texture->width, texture->height, texture->channels));
                    layer->SetData(texture->GetData<uint8_t>(i));
                    layer->fileName = filename;
                }
                };

            readTexture("baseColor", impostor->baseColorTexture, false);
            readTexture("roughnessMetalnessAo", impostor->roughnessMetalnessAoTexture, false);
            readTexture("normal", impostor->normalTexture, false);
            readTexture("depth", impostor->depthTexture, true);
        }

        // A compressed texture that couldn't be kept would get lost, the file stays as it is
        for (auto name : { "baseColor", "roughnessMetalnessAo", "normal" })
            if (!data->layers.contains(name) && j.contains(name) && j[name].is_null())
                data->write = false;

        return data;

    }

    void ImpostorLoader::WriteImpostor(const Ref<ImpostorFileData>& data) {

        if (!data->write)
            return;

        // Stbi callback
        auto writePngToMemory = [](void* context, void* data, int32_t size) {
            auto imageData = static_cast<std::vector<uint8_t>*>(context);
            auto imageBytes = static_cast<uint8_t*>(data);

            for (int i = 0; i < size; i++)
                imageData->push_back(imageBytes[i]);
            };

        auto& j = data->j;
        for (const auto& [name, layers] : data->layers) {
            auto& t = j[name];
            if (data->compressTextures) {
                // An impostor which can't be encoded keeps its previous file
                auto fileData = KtxLoader::EncodeImage(layers);
                if (fileData.empty()) {
                    Log::Error("Couldn't write impostor file " + data->filename);
                    return;
                }
                set_binary_json(t, "ktx2", fileData);
                continue;
            }

            for (const auto& layer : layers) {
                std::vector<uint8_t> imageData;
                stbi_write_png_to_func(writePngToMemory, &imageData, layer->width, layer->height,
                    layer->channels, layer->GetData().data(), layer->channels * layer->width);

                t.push_back(json());
                set_binary_json(t.back(), "data", imageData);
            }
        }

        auto path = Loader::AssetLoader::GetFullPath(data->filename);
        auto fileStream = Loader::AssetLoader::WriteFile(path, std::ios::out | std::ios::binary);

        if (!fileStream.is_open()) {
            Log::Error("Couldn't write impostor file " + data->filename);
            return;
        }

        if (data->binaryJson) {
            auto msgpack = json::to_msgpack(j);
            fileStream.write(reinterpret_cast<const char*>(msgpack.data()), msgpack.size());
        }
        else {
            fileStream << j.dump();
        }

        fileStream.close();

    }

}