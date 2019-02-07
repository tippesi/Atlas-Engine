#ifndef AE_ASSETLOADER_H
#define AE_ASSETLOADER_H

#include "../System.h"

#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <vector>
#include <mutex>

namespace Atlas {

	namespace Loader {

        class AssetLoader {

        public:
            static void Init();

            static void SetAssetDirectory(std::string directory);

            static std::ifstream ReadFile(std::string filename, std::ios_base::openmode mode);

            static std::ofstream WriteFile(std::string filename, std::ios_base::openmode mode);

            static size_t GetFileSize(std::ifstream& stream);

            static std::vector<char> GetFileContent(std::ifstream& stream);

            static void MakeDirectory(std::string directory);

            static void UnpackFile(std::string filename);

            static void UnpackDirectory(std::string directory);

            static std::string GetFullPath(std::string path);

        private:
            static std::string GetAssetPath(std::string path);

            static std::string GetAbsolutePath(std::string path);

            static std::string assetDirectory;

            static std::string dataDirectory;

            static std::mutex assetLoaderMutex;

#ifdef AE_OS_ANDROID
            static AAssetManager* manager;
#endif

        };

	}

}

#endif
