#include "AssetLoader.h"
#include "../common/Path.h"
#include "../Log.h"

#include <SDL3/SDL.h>

#include <vector>
#include <sys/stat.h>

#ifdef AE_OS_WINDOWS
#include <direct.h>
#endif

namespace Atlas {

    namespace Loader {

        std::string AssetLoader::assetDirectory;
        std::string AssetLoader::dataDirectory;

        std::mutex AssetLoader::assetLoaderMutex;

        bool AssetLoader::alwaysReload = false;
        std::unordered_set<std::string> AssetLoader::readFiles;

        void AssetLoader::Init() {

#ifdef AE_OS_ANDROID
            dataDirectory = std::string(SDL_GetAndroidInternalStoragePath());
#elif defined(AE_OS_APPLE_MOBILE)
            auto prefPath = SDL_GetPrefPath("AtlasEngine", "AtlasEngine");
            if (prefPath != nullptr) {
                dataDirectory = Common::Path::Normalize(Common::Path::GetAbsolute(prefPath));
                SDL_free(prefPath);
            }
#endif

        }

        void AssetLoader::SetAssetDirectory(const std::string& directory) {

            assetDirectory = directory;

#if !defined(AE_OS_ANDROID) && !defined(AE_OS_APPLE_MOBILE)
            dataDirectory = Common::Path::Normalize(Common::Path::GetAbsolute(directory));
#endif

        }

        std::string AssetLoader::GetAssetDirectory() {

            return assetDirectory;

        }

        std::string AssetLoader::GetDataDirectory() {

            return dataDirectory;

        }

        bool AssetLoader::FileExists(const std::string& filename) {

            auto path = Common::Path::IsAbsolute(filename) ? filename : GetDataPath(filename);
            std::ifstream stream(path);
#ifdef AE_OS_APPLE_MOBILE
            if (!stream.good()) {
                std::ifstream assetStream(GetAssetPath(filename));
                return assetStream.good();
            }
#endif
            return stream.good();

        }

        std::ifstream AssetLoader::ReadFile(std::string filename, std::ios_base::openmode mode) {

            std::ifstream stream;
            std::string path;

            if (!Common::Path::IsAbsolute(filename)) {
                path = GetFullPath(filename);
            }
            else
                path = filename;

            stream.open(path, mode);

            // It might be that the file is not unpacked
#if defined(AE_OS_ANDROID) || defined(AE_OS_APPLE_MOBILE)
            if (alwaysReload && stream.is_open()) {
                // We only want to unpack a file once per app execution
                if (readFiles.find(path) == readFiles.end()) {
                    readFiles.insert(path);
                    stream.close();
                    RemoveFile(path);
                }
            }

            if (!stream.is_open()) {
                UnpackFile(filename);
                stream.open(path, mode);
            }
#endif

            return stream;

        }

        std::ofstream AssetLoader::WriteFile(std::string filename, std::ios_base::openmode mode) {

            std::ofstream stream;

            std::string path;

            if (!Common::Path::IsAbsolute(filename))
                path = GetDataPath(filename);
            else
                path = filename;

            stream.open(path, mode);

            // If file couldn't be opened we try again after we created the
            // directories which point to that file
            if (!stream.is_open()) {
                auto parentPath = std::filesystem::path(path).parent_path();
                if (!parentPath.empty()) {
                    std::error_code errorCode;
                    std::filesystem::create_directories(parentPath, errorCode);
                    stream.open(path, mode);
                }
            }

            return stream;

        }

        bool AssetLoader::RemoveFile(std::string filename) {

            std::string path;

            if (!Common::Path::IsAbsolute(filename))
                path = GetDataPath(filename);
            else
                path = filename;

            if (remove(path.c_str()) != 0)
                return false;

            return true;

        }

        size_t AssetLoader::GetFileSize(std::ifstream& stream) {

            stream.seekg(0, stream.end);
            auto size = stream.tellg();
            stream.clear();
            stream.seekg(0, stream.beg);

            return size;

        }

        std::filesystem::file_time_type AssetLoader::GetFileLastModifiedTime(const std::string& path,
            const std::filesystem::file_time_type defaultTime) {

            const int32_t tryCount = 2;

            auto fileTime = defaultTime;
            for (int32_t i = 0; i < tryCount; i++) {
                try {
                    auto fullPath = Common::Path::IsAbsolute(path) ? path : GetFullPath(path);
                    fileTime = std::filesystem::last_write_time(fullPath);
                }
                catch (...) {}
            }

            return fileTime;

        }

        std::vector<char> AssetLoader::GetFileContent(std::ifstream& stream) {

            auto size = AssetLoader::GetFileSize(stream);

            std::vector<char> buffer(size);

            stream.read(buffer.data(), size);

            return buffer;

        }

        void AssetLoader::MakeDirectory(std::string directory) {

            auto path = Common::Path::IsAbsolute(directory) ? directory : GetDataPath(directory);
            std::filesystem::create_directories(path);

        }

        void AssetLoader::UnpackFile(const std::string& filename) {
            
#if defined(AE_OS_ANDROID) || defined(AE_OS_APPLE_MOBILE)
            std::string path;
            if (Common::Path::IsAbsolute(filename)) {
                path = GetRelativePath(filename);
            }
            else {
                path = filename;
            }

            std::lock_guard<std::mutex> guard(assetLoaderMutex);

            // On Android SDL looks for relative paths in the internal storage first, then in the APK assets
            auto assetPath = Common::Path::Normalize(GetAssetPath(path));

            auto asset = SDL_IOFromFile(assetPath.c_str(), "rb");
            if (!asset) {
                Log::Error("Asset not found " + assetPath);
                return;
            }

            auto stream = WriteFile(path, std::ios::out | std::ios::binary);
            if (!stream.is_open()) {
                SDL_CloseIO(asset);
                Log::Error("Unable to copy asset " + assetPath);
                return;
            }

            std::vector<char> buffer(1 << 16);

            size_t readLength = 0;

            while ((readLength = SDL_ReadIO(asset, buffer.data(), buffer.size())) > 0)
                stream.write(buffer.data(), std::streamsize(readLength));

            SDL_CloseIO(asset);
#endif

        }

        std::string AssetLoader::GetFullPath(const std::string& path) {

            if (Common::Path::IsAbsolute(path))
                return path;

            auto dataPath = GetDataPath(path);
            return dataPath;

        }

        std::string AssetLoader::GetRelativePath(const std::string& path) {

            auto pos = path.find(dataDirectory);
            if (pos != std::string::npos)
                return path.substr(pos + dataDirectory.length() + 1);

#if defined(AE_OS_ANDROID) || defined(AE_OS_APPLE_MOBILE)
            pos = path.find(assetDirectory);
            if (pos != std::string::npos)
                return path.substr(pos + assetDirectory.length() + 1);
#endif

            return path;

        }

        bool AssetLoader::IsFileInAssetDirectory(std::string path) {

            path = Common::Path::GetAbsolute(path);

            if (path.find(Common::Path::GetAbsolute(dataDirectory)) != std::string::npos)
                return true;

            auto assetDir = Common::Path::GetAbsolute(assetDirectory);
            if (path.find(assetDir) != std::string::npos)
                return true;

            return false;

        }

        void AssetLoader::SetReloadBehaviour(bool alwaysReload) {

            std::lock_guard<std::mutex> guard(assetLoaderMutex);

            AssetLoader::alwaysReload = alwaysReload;

        }

        std::string AssetLoader::GetAssetPath(const std::string& path) {

            if (assetDirectory.length() > 0)
                return assetDirectory + "/" + path;

            return path;

        }

        std::string AssetLoader::GetDataPath(const std::string& path) {

            if (dataDirectory.length() > 0)
                return dataDirectory + "/" + path;

            return path;

        }

        bool AssetLoader::ExistsAtPath(const std::string& path) {

            std::ifstream stream(path);
            return stream.good();

        }

    }

}
