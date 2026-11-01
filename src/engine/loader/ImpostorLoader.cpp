#include "ImpostorLoader.h"
#include "AssetLoader.h"
#include "KtxLoader.h"
#include "graphics/Format.h"
#include "mesh/MeshSerializer.h"


namespace Atlas::Loader {

    Ref<Mesh::Impostor> ImpostorLoader::LoadImpostor(const std::string& filename, bool binaryJson) {

        Loader::AssetLoader::UnpackFile(filename);
        auto path = Loader::AssetLoader::GetFullPath(filename);

        auto fileStream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);

        if (!fileStream.is_open()) {
            throw ResourceLoadException(filename, "Couldn't open impostor file stream: " + std::string(strerror(errno)));
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

        auto impostor = CreateRef<Mesh::Impostor>();
        from_json(j, *impostor);

        impostor->RefreshViewPlaneBuffer();

        auto jsonToTexture = [&](const char* name, Ref<Texture::Texture2DArray>& texture, bool depth) {    

            auto& t = j[name];

            // A compressed texture is one ktx2 array with all views
            if (t.is_object()) {
                std::vector<char> ktxData;
                try_get_binary_json(t, "ktx2", ktxData);

                auto image = KtxLoader::LoadImageFromMemory(ktxData, filename);
                texture = CreateRef<Texture::Texture2DArray>(image,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Anisotropic);
                return;
            }

            // Only uncompressed textures are uploaded into allocated arrays, a compressed one would hold both
            texture = CreateRef<Texture::Texture2DArray>(impostor->resolution, impostor->resolution,
                impostor->views * impostor->views, depth ? VK_FORMAT_R16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Anisotropic);

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            Graphics::MemoryTransferManager transferManager(device, device->memoryManager);

            transferManager.BeginMultiTransfer();

            VkOffset3D offset{ 0, 0, 0 };
            VkExtent3D extent{ texture->image->width, texture->image->height, 1 };

            std::vector<uint8_t> jsonData;

            int32_t layerCount = 0;
            for (auto& jsonLayer : t) {                
                if (depth) {
                    try_get_binary_json(jsonLayer, "data", jsonData);

                    transferManager.UploadImageData(jsonData.data(), texture->image.get(),
                        offset, extent, layerCount++, 1);
                }
                else {
                    try_get_binary_json(jsonLayer, "data", jsonData);

                    int32_t width, height, channels;
                    void* data = stbi_load_from_memory(jsonData.data(),
                        (int32_t)jsonData.size(), &width, &height, &channels, 0);

                    transferManager.UploadImageData(data, texture->image.get(),
                        offset, extent, layerCount++, 1);

                    stbi_image_free(data);
                }                
            }
            
            transferManager.EndMultiTransfer();
            };
        
        if (j.contains("baseColor")) {
            jsonToTexture("baseColor", impostor->baseColorTexture, false);
        }
        if (j.contains("roughnessMetalnessAo")) {
            jsonToTexture("roughnessMetalnessAo", impostor->roughnessMetalnessAoTexture, false);
        }
        if (j.contains("normal")) {
            jsonToTexture("normal", impostor->normalTexture, false);
        }
        if (j.contains("depth")) {
            jsonToTexture("depth", impostor->depthTexture, true);
        }

        impostor->isGenerated = impostor->baseColorTexture && impostor->roughnessMetalnessAoTexture &&
            impostor->normalTexture && impostor->depthTexture;

        return impostor;

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