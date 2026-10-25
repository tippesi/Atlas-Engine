#pragma once

#include "../System.h"
#include "../common/Image.h"
#include "../graphics/Common.h"

#include <vector>

namespace Atlas::Loader {

    struct KtxImage {
        int32_t width = 0;
        int32_t height = 0;
        uint32_t mipLevels = 1;

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

        static Ref<KtxImage> ConvertImage(const Ref<Common::Image<uint8_t>>& image);

        static void SaveImage(const Ref<KtxImage>& image, const std::string& filename);

        static bool IsKtxFile(const std::string& filename);

    };

}
