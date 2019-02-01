#include "AssetLoader.h"
#include <SDL/include/SDL.h>

#include <vector>

#ifdef AE_OS_WINDOWS
#include <direct.h>
#else
#include <sys/stat.h>
#endif

string AssetLoader::assetDirectory;
string AssetLoader::dataDirectory;

mutex AssetLoader::assetLoaderMutex;

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

    dataDirectory = string(SDL_AndroidGetInternalStoragePath());
#endif

}

void AssetLoader::SetAssetDirectory(string directory) {

    assetDirectory = directory;

#ifndef AE_OS_ANDROID
    dataDirectory = directory;
#endif

}

ifstream AssetLoader::ReadFile(string filename, ios_base::openmode mode) {

	ifstream file;

	string path = GetFullPath(filename);

	file.open(path, mode);

	// It might be that the file is not unpacked
#ifdef AE_OS_ANDROID
	if (!file.is_open()) {
		UnpackFile(filename);
		file.open(path, mode);
	}
#endif

	return file;

}

ofstream AssetLoader::WriteFile(string filename, ios_base::openmode mode) {

	ofstream file;

	string path = GetFullPath(filename);

	file.open(path, mode);

	// If file couldn't be opened we try again after we created the 
	// directories which point to that file
	if (!file.is_open()) {
		size_t directoryPosition = filename.find_last_of("/");
		if (directoryPosition != string::npos) {
			MakeDirectory(filename.substr(0, directoryPosition));
			file.open(path, mode);
		}
	}

	return file;

}

size_t AssetLoader::GetFileSize(ifstream& stream) {

	stream.seekg(0, stream.end);
	auto size = stream.tellg();
	stream.seekg(0);

	return size - stream.tellg();

}

vector<char> AssetLoader::GetFileContent(ifstream& stream) {

	auto size = AssetLoader::GetFileSize(stream);

	vector<char> buffer(size);

	if (!stream.read(buffer.data(), size)) {
		buffer.resize(0);
	}

	return buffer;

}

void AssetLoader::MakeDirectory(string directory) {

    directory = GetAbsolutePath(directory);

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

void AssetLoader::UnpackFile(string filename) {

	lock_guard<mutex> guard(assetLoaderMutex);

#ifdef AE_OS_ANDROID
	auto assetPath = GetAssetPath(filename);
	assetPath = GetAbsolutePath(assetPath);

	auto asset = AAssetManager_open(manager, assetPath.c_str(), AASSET_MODE_UNKNOWN);

	if (!asset) {
	    EngineLog("Asset not found: %s", assetPath.c_str());
        return;
    }

	auto stream = WriteFile(filename, ios::out);

	if (!stream.is_open()) {
		AAsset_close(asset);
		EngineLog("Couldn't open stream file");
		return;
	}

	int32_t assetLength = AAsset_getLength(asset);

	vector<char> buffer(assetLength);

	int32_t readLength = 0;

	while ((readLength = AAsset_read(asset, buffer.data(), assetLength)) > 0)
		stream.write(buffer.data(), readLength);

	stream.close();

	AAsset_close(asset);
#endif

}

void AssetLoader::UnpackDirectory(string directory) {

	lock_guard<mutex> guard(assetLoaderMutex);

}

string AssetLoader::GetFullPath(string path) {
	
	return dataDirectory + "/" + path;

}

string AssetLoader::GetAssetPath(string path) {

    if (assetDirectory.length() > 0)
        return assetDirectory + "/" + path;

    return path;

}

string AssetLoader::GetAbsolutePath(string path) {

    size_t backPosition;

    while ((backPosition = path.find("/..")) != string::npos) {
        auto parentPath = path.substr(0, backPosition);
        auto childPath = path.substr(backPosition + 3, path.length());
        size_t parentBackPostion = parentPath.find_last_of('/');
        if (parentBackPostion == string::npos) {
            throw EngineException("Trying to access data outside the assets folder");
        }
        path = parentPath.substr(0, parentBackPostion) + childPath;
    }

    return path;

}