#pragma once

#include "../System.h"
#include "../common/Image.h"
#include "../graphics/Common.h"

#include <vector>

namespace Atlas::Loader {

    struct KtxImage {
        int32_t width = 0;
        int32_t height = 0;
        int32_t layers = 1;
        uint32_t mipLevels = 1;
        // Levels of the file above the loaded resolution and the size of its largest level
        uint32_t skippedMipLevels = 0;
        int32_t fileWidth = 0;
        int32_t fileHeight = 0;

        VkFormat format = VK_FORMAT_UNDEFINED;

        std::vector<uint8_t> data;
        std::vector<VkBufferImageCopy> copyRegions;

        std::vector<char> fileData;

        std::string fileName;
    };

    class KtxLoader {

    public:
        static Ref<KtxImage> LoadImage(const std::string& filename, bool colorSpaceConversion = false,
            int32_t maxImageResolution = 8192);

        static Ref<KtxImage> LoadImageFromMemory(const std::vector<char>& buffer, const std::string& filename,
            bool colorSpaceConversion = false, int32_t maxImageResolution = 8192);

        // The level of the file closest to maxImageResolution as an RGBA image, e.g. for previews. Nullptr for files
        // which can't be decoded on the CPU, i.e. block compressed ones which aren't supercompressed
        static Ref<Common::Image<uint8_t>> DecodeImage(const std::vector<char>& buffer, int32_t maxImageResolution);

        static Ref<KtxImage> ConvertImage(const Ref<Common::Image<uint8_t>>& image);

        // All layers need the same size
        static Ref<KtxImage> ConvertImage(const std::vector<Ref<Common::Image<uint8_t>>>& layers);

        // The ktx2 file of the layers without loading it, empty if the encoding failed. With the cache (.cache/ktx2/ in
        // the data directory) identical images are encoded once, e.g. a texture imported by models in several directories
        static std::vector<char> EncodeImage(const std::vector<Ref<Common::Image<uint8_t>>>& layers, bool useCache = false);

        static void SaveImage(const Ref<KtxImage>& image, const std::string& filename);

        static bool IsKtxFile(const std::string& filename);

    private:
        static std::vector<char> Encode(const std::vector<Ref<Common::Image<uint8_t>>>& layers);

    };

}
