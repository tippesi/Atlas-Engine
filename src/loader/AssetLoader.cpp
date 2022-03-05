#include "AssetLoader.h"
#include "../common/Path.h"
#include "../Log.h"

#include <vector>
#include <sys/stat.h>

namespace Atlas {

	namespace Loader {

		std::string AssetLoader::assetDirectory;
		std::string AssetLoader::dataDirectory;

		std::mutex AssetLoader::assetLoaderMutex;

#ifdef AE_OS_ANDROID
		AAssetManager* AssetLoader::manager;
#endif

		bool AssetLoader::alwaysReload = false;
        std::unordered_set<std::string> AssetLoader::readFiles;

		void AssetLoader::Init() {

#ifdef AE_OS_ANDROID
			auto interface = (JNIEnv*)SDL_AndroidGetJNIEnv();

			JNIEnv* env = interface;

			jobject activity = (jobject)SDL_AndroidGetActivity();

			jclass Activity = interface->GetObjectClass(activity);

			// We use the activity to call the Java methods and obtain the classes to get the AssetManager
			jmethodID getResources = env->GetMethodID(Activity, "getResources",
				"()Landroid/content/res/Resources;");
			jobject ressource = env->CallObjectMethod(activity, getResources);

			jclass resourcesClazz = env->FindClass("android/content/res/Resources");
			jmethodID getAssetManager = env->GetMethodID(resourcesClazz, "getAssets",
				"()Landroid/content/res/AssetManager;");
			jobject assetManager = env->CallObjectMethod(ressource, getAssetManager);

			manager = AAssetManager_fromJava(interface, assetManager);

			dataDirectory = std::string(SDL_AndroidGetInternalStoragePath());
#endif

		}

		void AssetLoader::SetAssetDirectory(const std::string& directory) {

			assetDirectory = directory;

#ifndef AE_OS_ANDROID
			dataDirectory = Common::Path::GetAbsolute(directory);
#endif

		}

		bool AssetLoader::FileExists(const std::string& filename) {

			auto assetDir = Common::Path::GetAbsolute(assetDirectory);

			std::ifstream stream(assetDir + "/" + filename);
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
#ifdef AE_OS_ANDROID
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
				path = GetFullPath(filename);
			else
				path = filename;

			stream.open(path, mode);

			// If file couldn't be opened we try again after we created the
			// directories which point to that file
			if (!stream.is_open()) {
				size_t directoryPosition = filename.find_last_of("/");
				if (directoryPosition != std::string::npos) {
					MakeDirectory(filename.substr(0, directoryPosition));
					stream.open(path, mode);
				}
			}

			return stream;

		}

		bool AssetLoader::RemoveFile(std::string filename) {

			std::string path;

			if (!Common::Path::IsAbsolute(filename))
				path = GetFullPath(filename);
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

		std::vector<char> AssetLoader::GetFileContent(std::ifstream& stream) {

			auto size = AssetLoader::GetFileSize(stream);

			std::vector<char> buffer(size);

			stream.read(buffer.data(), size);

			return buffer;

		}

		void AssetLoader::MakeDirectory(std::string directory) {

			directory = Common::Path::Normalize(directory);

			directory += "/";

			for (int32_t i = 0; i < directory.length(); i++) {
				if (directory[i] == '/') {
					auto subPath = directory.substr(0, i);
					auto path = GetFullPath(subPath);
#ifdef AE_OS_WINDOWS
					_mkdir(path.c_str());
#else
					mkdir(path.c_str(), S_IRUSR | S_IWUSR | S_IXUSR);
#endif
				}
			}

		}

		void AssetLoader::UnpackFile(const std::string& filename) {

			std::lock_guard<std::mutex> guard(assetLoaderMutex);

#ifdef AE_OS_ANDROID
			auto assetPath = Common::Path::Normalize(GetAssetPath(filename));

			auto asset = AAssetManager_open(manager, assetPath.c_str(), AASSET_MODE_UNKNOWN);

			if (!asset) {
				Log::Error("Asset not found " + assetPath);
				return;
			}

			auto stream = WriteFile(filename, std::ios::out);

			if (!stream.is_open()) {
				AAsset_close(asset);
				Log::Error("Unable to copy asset " + assetPath);
				return;
			}

			int32_t assetLength = AAsset_getLength(asset);

			std::vector<char> buffer(assetLength);

			int32_t readLength = 0;

			while ((readLength = AAsset_read(asset, buffer.data(), assetLength)) > 0)
				stream.write(buffer.data(), readLength);

			stream.close();

			AAsset_close(asset);
#endif

		}

		std::string AssetLoader::GetFullPath(std::string path) {

			if (Common::Path::IsAbsolute(path))
				return path;

			return dataDirectory + "/" + path;

		}

		bool AssetLoader::IsFileInAssetDirectory(std::string path) {

			auto assetDir = Common::Path::GetAbsolute(assetDirectory);

			path = Common::Path::GetAbsolute(path);

			// File not in asset directory
			if (path.find(assetDir) == std::string::npos)
				return false;

			return true;

		}

		void AssetLoader::SetReloadBehaviour(bool alwaysReload) {

            std::lock_guard<std::mutex> guard(assetLoaderMutex);

		    AssetLoader::alwaysReload = alwaysReload;

		}

		std::string AssetLoader::GetAssetPath(std::string path) {

			if (assetDirectory.length() > 0)
				return assetDirectory + "/" + path;

			return path;

		}

	}

}