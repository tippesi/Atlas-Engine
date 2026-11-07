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

    // Copies a ktx2 file without the levels above maxImageResolution, so they are neither inflated nor transcoded
    // Only for supercompressed levels without global data, their level data needs no alignment
    static bool TrimLevels(const std::vector<char>& buffer, int32_t maxImageResolution,
        std::vector<char>& trimmed, uint32_t& skippedLevels) {

        const size_t headerSize = 80;
        const size_t levelIndexEntrySize = 24;
        const uint8_t identifier[12] = { 0xAB, 'K', 'T', 'X', ' ', '2', '0', 0xBB, '\r', '\n', 0x1A, '\n' };

        if (buffer.size() < headerSize || std::memcmp(buffer.data(), identifier, sizeof(identifier)) != 0)
            return false;

        auto read32 = [&](size_t offset) {
            uint32_t value;
            std::memcpy(&value, buffer.data() + offset, sizeof(value));
            return value;
            };
        auto read64 = [&](size_t offset) {
            uint64_t value;
            std::memcpy(&value, buffer.data() + offset, sizeof(value));
            return value;
            };

        auto width = read32(20);
        auto height = read32(24);
        auto depth = read32(28);
        auto faceCount = read32(36);
        auto levelCount = read32(40);
        auto supercompression = read32(44);
        auto dfdOffset = read32(48);
        auto dfdLength = read32(52);
        auto kvdOffset = read32(56);
        auto kvdLength = read32(60);
        auto sgdLength = read64(72);

        const uint32_t zstd = 2, zlib = 3;
        if ((supercompression != zstd && supercompression != zlib) || sgdLength > 0 ||
            depth > 0 || faceCount != 1 || levelCount < 2)
            return false;

        if (buffer.size() < headerSize + levelCount * levelIndexEntrySize ||
            size_t(dfdOffset) + dfdLength > buffer.size() || size_t(kvdOffset) + kvdLength > buffer.size())
            return false;

        skippedLevels = 0;
        while (skippedLevels + 1 < levelCount &&
            int32_t(glm::max(width, height) >> skippedLevels) > maxImageResolution) {
            skippedLevels++;
        }

        if (skippedLevels == 0)
            return false;

        auto newLevelCount = levelCount - skippedLevels;
        auto newDfdOffset = uint32_t(headerSize + newLevelCount * levelIndexEntrySize);
        auto newKvdOffset = kvdLength > 0 ? newDfdOffset + dfdLength : 0u;
        auto dataOffset = size_t(newDfdOffset) + dfdLength + kvdLength;

        size_t dataSize = 0;
        for (uint32_t i = skippedLevels; i < levelCount; i++) {
            auto entry = headerSize + i * levelIndexEntrySize;
            auto levelOffset = read64(entry);
            auto levelLength = read64(entry + 8);
            if (levelOffset + levelLength > buffer.size())
                return false;
            dataSize += levelLength;
        }

        trimmed.assign(dataOffset + dataSize, 0);

        auto write32 = [&](size_t offset, uint32_t value) {
            std::memcpy(trimmed.data() + offset, &value, sizeof(value));
            };
        auto write64 = [&](size_t offset, uint64_t value) {
            std::memcpy(trimmed.data() + offset, &value, sizeof(value));
            };

        std::memcpy(trimmed.data(), buffer.data(), headerSize);
        write32(20, glm::max(width >> skippedLevels, 1u));
        write32(24, height > 0 ? glm::max(height >> skippedLevels, 1u) : 0u);
        write32(40, newLevelCount);
        write32(48, newDfdOffset);
        write32(56, newKvdOffset);
        write64(64, 0);

        std::memcpy(trimmed.data() + newDfdOffset, buffer.data() + dfdOffset, dfdLength);
        if (kvdLength > 0)
            std::memcpy(trimmed.data() + newKvdOffset, buffer.data() + kvdOffset, kvdLength);

        // Levels are stored from the smallest to the largest one
        for (uint32_t i = levelCount; i-- > skippedLevels;) {
            auto entry = headerSize + i * levelIndexEntrySize;
            auto levelOffset = read64(entry);
            auto levelLength = read64(entry + 8);
            auto uncompressedLength = read64(entry + 16);

            auto newEntry = headerSize + (i - skippedLevels) * levelIndexEntrySize;
            write64(newEntry, dataOffset);
            write64(newEntry + 8, levelLength);
            write64(newEntry + 16, uncompressedLength);

            std::memcpy(trimmed.data() + dataOffset, buffer.data() + levelOffset, levelLength);
            dataOffset += levelLength;
        }

        return true;

    }

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

        std::vector<char> trimmedBuffer;
        uint32_t trimmedLevels = 0;
        auto trimmed = TrimLevels(buffer, maxImageResolution, trimmedBuffer, trimmedLevels);
        const auto& fileBuffer = trimmed ? trimmedBuffer : buffer;

        ktxTexture2* texture = nullptr;
        auto result = ktxTexture2_CreateFromMemory(reinterpret_cast<const ktx_uint8_t*>(fileBuffer.data()),
            fileBuffer.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);

        if (result != KTX_SUCCESS) {
            throw ResourceLoadException(normalizedFileName, "Couldn't read ktx2 file: " + std::string(ktxErrorString(result)));
        }

        if (texture->numDimensions != 2 || texture->numFaces > 1) {
            ktxTexture_Destroy(ktxTexture(texture));
            throw ResourceLoadException(normalizedFileName, "Only 2D ktx2 textures without faces are supported");
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
        image->layers = int32_t(texture->numLayers);
        image->mipLevels = texture->numLevels - baseLevel;
        image->skippedMipLevels = trimmedLevels + baseLevel;
        image->fileWidth = int32_t(texture->baseWidth);
        image->fileHeight = int32_t(texture->baseHeight);
        if (trimmed) {
            std::memcpy(&image->fileWidth, buffer.data() + 20, sizeof(int32_t));
            std::memcpy(&image->fileHeight, buffer.data() + 24, sizeof(int32_t));
        }
        image->format = format;
        image->fileName = normalizedFileName;

        auto levelAlignment = std::lcm(Graphics::GetFormatSize(format), size_t(4));

        // The layers of a level are stored one after another
        size_t dataSize = 0;
        for (uint32_t i = baseLevel; i < texture->numLevels; i++) {
            auto levelSize = ktxTexture_GetImageSize(ktxTexture(texture), i) * texture->numLayers;
            dataSize += (levelSize + levelAlignment - 1) / levelAlignment * levelAlignment;
        }
        image->data.resize(dataSize);

        size_t dataOffset = 0;
        for (uint32_t i = baseLevel; i < texture->numLevels; i++) {
            ktx_size_t levelOffset;
            ktxTexture_GetImageOffset(ktxTexture(texture), i, 0, 0, &levelOffset);
            auto levelSize = ktxTexture_GetImageSize(ktxTexture(texture), i) * texture->numLayers;

            std::memcpy(image->data.data() + dataOffset, ktxTexture_GetData(ktxTexture(texture)) + levelOffset, levelSize);

            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = dataOffset;
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = i - baseLevel;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = texture->numLayers;
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

        return ConvertImage(std::vector<Ref<Common::Image<uint8_t>>>{ image });

    }

    Ref<KtxImage> KtxLoader::ConvertImage(const std::vector<Ref<Common::Image<uint8_t>>>& layers) {

        const auto& image = layers.front();

        auto buffer = EncodeImage(layers);
        if (buffer.empty())
            throw ResourceLoadException(image->fileName, "Couldn't convert image to ktx2");

        auto ktxImage = LoadImageFromMemory(buffer, image->fileName);
        ktxImage->fileData = std::move(buffer);

        Log::Message("Converted image " + image->fileName + " to ktx2 format");

        return ktxImage;

    }

    std::vector<char> KtxLoader::EncodeImage(const std::vector<Ref<Common::Image<uint8_t>>>& layers) {

        AE_ASSERT(!layers.empty() && "No layers to convert");

        const auto& image = layers.front();

        std::vector<Common::Image<uint8_t>> rgbaImages;
        for (const auto& layer : layers) {
            AE_ASSERT(layer->width == image->width && layer->height == image->height && "Layers differ in size");

            auto channels = layer->channels;
            const auto& imageData = layer->GetData();

            std::vector<uint8_t> rgbaData(size_t(layer->width) * size_t(layer->height) * 4);
            for (size_t i = 0; i < rgbaData.size() / 4; i++) {
                for (int32_t j = 0; j < 4; j++) {
                    rgbaData[i * 4 + j] = j < channels ? imageData[i * channels + j] : 255;
                }
            }

            auto& rgbaImage = rgbaImages.emplace_back(layer->width, layer->height, 4);
            rgbaImage.SetData(rgbaData);
            rgbaImage.GenerateMipmap();
        }

        ktxTextureCreateInfo createInfo = {};
        createInfo.vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
        createInfo.baseWidth = uint32_t(image->width);
        createInfo.baseHeight = uint32_t(image->height);
        createInfo.baseDepth = 1;
        createInfo.numDimensions = 2;
        createInfo.numLevels = uint32_t(rgbaImages.front().GetMipmapLevelCount());
        createInfo.numLayers = uint32_t(layers.size());
        createInfo.numFaces = 1;
        createInfo.isArray = layers.size() > 1 ? KTX_TRUE : KTX_FALSE;
        createInfo.generateMipmaps = KTX_FALSE;

        ktxTexture2* texture = nullptr;
        auto result = ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
        if (result != KTX_SUCCESS) {
            Log::Error("Couldn't create ktx2 texture for " + image->fileName + ": " + ktxErrorString(result));
            return {};
        }

        for (size_t layer = 0; layer < rgbaImages.size(); layer++) {
            for (int32_t i = 0; i < rgbaImages[layer].GetMipmapLevelCount(); i++) {
                auto& level = rgbaImages[layer].GetMipLevel(i);
                ktxTexture_SetImageFromMemory(ktxTexture(texture), uint32_t(i), uint32_t(layer), 0,
                    level.data.data(), level.data.size());
            }
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
            Log::Error("Couldn't encode " + image->fileName + " as ktx2: " + ktxErrorString(result));
            return {};
        }

        std::vector<char> buffer(reinterpret_cast<char*>(fileData), reinterpret_cast<char*>(fileData) + fileSize);
        free(fileData);

        return buffer;

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
