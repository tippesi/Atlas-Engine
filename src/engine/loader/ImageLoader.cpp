#include "ImageLoader.h"
#include "AssetLoader.h"

namespace Atlas::Loader {
    
    std::vector<char> ImageLoader::ReadFileContent(const std::string& filename) {

        std::vector<char> buffer;
        auto fileStream = AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);

        if (fileStream.is_open()) {           
            buffer = AssetLoader::GetFileContent(fileStream);

            fileStream.close();
        }

        return buffer;

    }

    std::ofstream ImageLoader::OpenWriteFileStream(const std::string& filename, Common::ImageFormat format) {

        if (format == Common::ImageFormat::PGM) {
            return AssetLoader::WriteFile(filename, std::ios::out);
        }
        
        return AssetLoader::WriteFile(filename, std::ios::out | std::ios::binary);

    }

}