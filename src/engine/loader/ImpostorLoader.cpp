#include "ImpostorLoader.h"
#include "AssetLoader.h"
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
        impostor->AllocateTextures();

        // We define these lambdas inline and not as a function to be able to not spill the json
        // definitions via the header files to other parts of the code
        auto jsonToTexture = [&](const char* name, Ref<Texture::Texture2DArray>& texture, bool depth) {    

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            Graphics::MemoryTransferManager transferManager(device, device->memoryManager);

            transferManager.BeginMultiTransfer();

            auto& t = j[name];

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

        auto path = Loader::AssetLoader::GetFullPath(filename);
        auto fileStream = Loader::AssetLoader::WriteFile(path, std::ios::out | std::ios::binary);

        if (!fileStream.is_open()) {
            Log::Error("Couldn't write impostor file " + filename);
            return;
        }

        json j;
        to_json(j, *impostor);

        if (impostor->isGenerated) {
            // Stbi callback
            auto writePngToMemory = [](void* context, void* data, int32_t size) {
                auto imageData = static_cast<std::vector<uint8_t>*>(context);
                auto imageBytes = static_cast<uint8_t*>(data);

                for (int i = 0; i < size; i++)
                    imageData->push_back(imageBytes[i]);
                };

            auto textureToJson = [&](const char* name, const Ref<Texture::Texture2DArray>& texture, bool depth) {
                auto& t = j[name];
                for (int32_t i = 0; i < texture->depth; i++) {
                    t.push_back(json());
                    if (depth) {
                        set_binary_json(t.back(), "data", texture->GetData<float16>(i));
                    }
                    else {
                        std::vector<uint8_t> imageData;

                        auto data = texture->GetData<uint8_t>(i);
                        stbi_write_png_to_func(writePngToMemory, &imageData, texture->width, texture->height,
                            texture->channels, data.data(), texture->channels * texture->width);

                        set_binary_json(t.back(), "data", imageData);
                    }
                }
                };

            textureToJson("baseColor", impostor->baseColorTexture, false);
            textureToJson("roughnessMetalnessAo", impostor->roughnessMetalnessAoTexture, false);
            textureToJson("normal", impostor->normalTexture, false);
            textureToJson("depth", impostor->depthTexture, true);
        }

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