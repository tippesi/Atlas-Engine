#include "KtxLoader.h"
#include "AssetLoader.h"

#include "../common/Path.h"
#include "../graphics/Format.h"
#include "../graphics/GraphicsDevice.h"
#include "../resource/ResourceLoadException.h"

#include <ktx.h>

#include <cstring>
#include <cstdlib>
#include <numeric>
#include <algorithm>
#include <mutex>
#include <atomic>

namespace Atlas::Loader {

    Ref<KtxImage> KtxLoader::LoadImage(const std::string& filename, bool colorSpaceConversion,
        int32_t maxImageResolution) {

        const auto normalizedFileName = Common::Path::Normalize(filename);

        auto fileStream = AssetLoader::ReadFile(normalizedFileName, std::ios::in | std::ios::binary);

        if (!fileStream.is_open()) {
            throw ResourceLoadException(normalizedFileName, "Couldn't open ktx file stream");
        }

        auto buffer = AssetLoader::GetFileContent(fileStream);
        fileStream.close();

        auto image = LoadImageFromMemory(buffer, normalizedFileName, colorSpaceConversion, maxImageResolution);

        Log::Message("Loaded image " + normalizedFileName);

        return image;

    }

    Ref<KtxImage> KtxLoader::LoadImageFromMemory(const std::vector<char>& buffer, const std::string& filename,
        bool colorSpaceConversion, int32_t maxImageResolution) {

        const auto normalizedFileName = Common::Path::Normalize(filename);

        ktxTexture2* texture = nullptr;
        auto result = ktxTexture2_CreateFromMemory(reinterpret_cast<const ktx_uint8_t*>(buffer.data()),
            buffer.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);

        if (result != KTX_SUCCESS) {
            throw ResourceLoadException(normalizedFileName, "Couldn't read ktx2 file: " + std::string(ktxErrorString(result)));
        }

        if (texture->numDimensions != 2 || texture->numLayers > 1 || texture->numFaces > 1) {
            ktxTexture_Destroy(ktxTexture(texture));
            throw ResourceLoadException(normalizedFileName, "Only 2D ktx2 textures without layers or faces are supported");
        }

        auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;
        auto featureFlags = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

        if (ktxTexture2_NeedsTranscoding(texture)) {
            ktx_transcode_fmt_e transcodeFormat;
            if (graphicsDevice->CheckFormatSupport(VK_FORMAT_BC7_UNORM_BLOCK, featureFlags))
                transcodeFormat = KTX_TTF_BC7_RGBA;
            else if (graphicsDevice->CheckFormatSupport(VK_FORMAT_ASTC_4x4_UNORM_BLOCK, featureFlags))
                transcodeFormat = KTX_TTF_ASTC_4x4_RGBA;
            else if (graphicsDevice->CheckFormatSupport(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, featureFlags))
                transcodeFormat = KTX_TTF_ETC2_RGBA;
            else
                transcodeFormat = KTX_TTF_RGBA32;

            // The first transcode initializes global basis tables without synchronization
            static std::mutex transcoderMutex;
            static std::atomic_bool transcoderInitialized = false;
            if (!transcoderInitialized) {
                std::scoped_lock lock(transcoderMutex);
                result = ktxTexture2_TranscodeBasis(texture, transcodeFormat, 0);
                transcoderInitialized = true;
            }
            else {
                result = ktxTexture2_TranscodeBasis(texture, transcodeFormat, 0);
            }

            if (result != KTX_SUCCESS) {
                ktxTexture_Destroy(ktxTexture(texture));
                throw ResourceLoadException(normalizedFileName, "Couldn't transcode ktx2 file: " + std::string(ktxErrorString(result)));
            }
        }

        auto format = colorSpaceConversion ? Graphics::GetFormatSRGB(VkFormat(texture->vkFormat)) :
            Graphics::GetFormatUNORM(VkFormat(texture->vkFormat));

        if (!Graphics::FormatTable.contains(format) || !graphicsDevice->CheckFormatSupport(format, featureFlags)) {
            ktxTexture_Destroy(ktxTexture(texture));
            throw ResourceLoadException(normalizedFileName, "Format of ktx2 file isn't supported by the device");
        }

        uint32_t baseLevel = 0;
        while (baseLevel + 1 < texture->numLevels &&
            int32_t(glm::max(texture->baseWidth, texture->baseHeight) >> baseLevel) > maxImageResolution) {
            baseLevel++;
        }

        auto image = CreateRef<KtxImage>();
        image->width = int32_t(glm::max(texture->baseWidth >> baseLevel, 1u));
        image->height = int32_t(glm::max(texture->baseHeight >> baseLevel, 1u));
        image->mipLevels = texture->numLevels - baseLevel;
        image->format = format;
        image->fileName = normalizedFileName;

        auto levelAlignment = std::lcm(Graphics::GetFormatSize(format), size_t(4));

        size_t dataSize = 0;
        for (uint32_t i = baseLevel; i < texture->numLevels; i++) {
            auto levelSize = ktxTexture_GetImageSize(ktxTexture(texture), i);
            dataSize += (levelSize + levelAlignment - 1) / levelAlignment * levelAlignment;
        }
        image->data.resize(dataSize);

        size_t dataOffset = 0;
        for (uint32_t i = baseLevel; i < texture->numLevels; i++) {
            ktx_size_t levelOffset;
            ktxTexture_GetImageOffset(ktxTexture(texture), i, 0, 0, &levelOffset);
            auto levelSize = ktxTexture_GetImageSize(ktxTexture(texture), i);

            std::memcpy(image->data.data() + dataOffset, ktxTexture_GetData(ktxTexture(texture)) + levelOffset, levelSize);

            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = dataOffset;
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = i - baseLevel;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent = {
                glm::max(texture->baseWidth >> i, 1u),
                glm::max(texture->baseHeight >> i, 1u),
                1
            };
            image->copyRegions.push_back(copyRegion);

            dataOffset += (levelSize + levelAlignment - 1) / levelAlignment * levelAlignment;
        }

        ktxTexture_Destroy(ktxTexture(texture));

        return image;

    }

    Ref<KtxImage> KtxLoader::ConvertImage(const Ref<Common::Image<uint8_t>>& image) {

        auto channels = image->channels;
        const auto& imageData = image->GetData();

        std::vector<uint8_t> rgbaData(size_t(image->width) * size_t(image->height) * 4);
        for (size_t i = 0; i < rgbaData.size() / 4; i++) {
            for (int32_t j = 0; j < 4; j++) {
                rgbaData[i * 4 + j] = j < channels ? imageData[i * channels + j] : 255;
            }
        }

        Common::Image<uint8_t> rgbaImage(image->width, image->height, 4);
        rgbaImage.SetData(rgbaData);
        rgbaImage.GenerateMipmap();

        ktxTextureCreateInfo createInfo = {};
        createInfo.vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
        createInfo.baseWidth = uint32_t(rgbaImage.width);
        createInfo.baseHeight = uint32_t(rgbaImage.height);
        createInfo.baseDepth = 1;
        createInfo.numDimensions = 2;
        createInfo.numLevels = uint32_t(rgbaImage.GetMipmapLevelCount());
        createInfo.numLayers = 1;
        createInfo.numFaces = 1;
        createInfo.isArray = KTX_FALSE;
        createInfo.generateMipmaps = KTX_FALSE;

        ktxTexture2* texture = nullptr;
        auto result = ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
        if (result != KTX_SUCCESS) {
            throw ResourceLoadException(image->fileName, "Couldn't create ktx2 texture: " + std::string(ktxErrorString(result)));
        }

        for (int32_t i = 0; i < rgbaImage.GetMipmapLevelCount(); i++) {
            auto& level = rgbaImage.GetMipLevel(i);
            ktxTexture_SetImageFromMemory(ktxTexture(texture), uint32_t(i), 0, 0, level.data.data(), level.data.size());
        }

        ktxBasisParams params = {};
        params.structSize = sizeof(params);
        params.uastc = KTX_TRUE;
        params.uastcFlags = KTX_PACK_UASTC_LEVEL_DEFAULT;
        params.threadCount = 1;

        result = ktxTexture2_CompressBasisEx(texture, &params);
        if (result == KTX_SUCCESS)
            result = ktxTexture2_DeflateZstd(texture, 10);

        ktx_uint8_t* fileData = nullptr;
        ktx_size_t fileSize = 0;
        if (result == KTX_SUCCESS)
            result = ktxTexture_WriteToMemory(ktxTexture(texture), &fileData, &fileSize);

        ktxTexture_Destroy(ktxTexture(texture));

        if (result != KTX_SUCCESS) {
            throw ResourceLoadException(image->fileName, "Couldn't convert image to ktx2: " + std::string(ktxErrorString(result)));
        }

        std::vector<char> buffer(reinterpret_cast<char*>(fileData), reinterpret_cast<char*>(fileData) + fileSize);
        free(fileData);

        auto ktxImage = LoadImageFromMemory(buffer, image->fileName);
        ktxImage->fileData = std::move(buffer);

        Log::Message("Converted image " + image->fileName + " to ktx2 format");

        return ktxImage;

    }

    void KtxLoader::SaveImage(const Ref<KtxImage>& image, const std::string& filename) {

        const auto normalizedFileName = Common::Path::Normalize(filename);

        if (image->fileData.empty()) {
            Log::Error("Couldn't write ktx image " + normalizedFileName + ": Image wasn't converted");
            return;
        }

        auto fileStream = AssetLoader::WriteFile(normalizedFileName, std::ios::out | std::ios::binary);

        if (!fileStream.is_open()) {
            Log::Error("Couldn't write ktx image " + normalizedFileName);
            return;
        }

        fileStream.write(image->fileData.data(), std::streamsize(image->fileData.size()));
        fileStream.close();

    }

    bool KtxLoader::IsKtxFile(const std::string& filename) {

        auto fileType = Common::Path::GetFileType(filename);
        std::transform(fileType.begin(), fileType.end(), fileType.begin(), ::tolower);

        return fileType == "ktx2";

    }

}
