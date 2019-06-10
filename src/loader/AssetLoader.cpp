#include "AssetLoader.h"
#include "../common/Path.h"
#include <SDL/include/SDL.h>

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

		void AssetLoader::SetAssetDirectory(std::string directory) {

			assetDirectory = directory;

#ifndef AE_OS_ANDROID
			dataDirectory = Common::Path::GetAbsolute(directory);
#endif

		}

		std::ifstream AssetLoader::ReadFile(std::string filename, std::ios_base::openmode mode) {

			std::ifstream stream;
			std::string path;

			if (!Common::Path::IsAbsolute(filename))
				path = GetFullPath(filename);
			else
				path = filename;

			stream.open(path, mode);

			// It might be that the file is not unpacked
#ifdef AE_OS_ANDROID
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

			auto position = filename.find(dataDirectory);

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

		void AssetLoader::UnpackFile(std::string filename) {

			std::lock_guard<std::mutex> guard(assetLoaderMutex);

#ifdef AE_OS_ANDROID
			auto assetPath = Common::Path::Normalize(GetAssetPath(filename));

			auto asset = AAssetManager_open(manager, assetPath.c_str(), AASSET_MODE_UNKNOWN);

			if (!asset) {
				AtlasLog("Asset not found: %s", assetPath.c_str());
				return;
			}

			auto stream = WriteFile(filename, std::ios::out);

			if (!stream.is_open()) {
				AAsset_close(asset);
				AtlasLog("Unable to copy asset");
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

			return dataDirectory + "/" + path;

		}

		std::string AssetLoader::GetAssetPath(std::string path) {

			if (assetDirectory.length() > 0)
				return assetDirectory + "/" + path;

			return path;

		}

	}

}