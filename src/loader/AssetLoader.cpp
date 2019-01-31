#include "AssetLoader.h"
#include <SDL/include/SDL.h>

string AssetLoader::assetDirectory;
string AssetLoader::dataDirectory;

mutex AssetLoader::assetLoaderMutex;

#ifdef ENGINE_ANDROID

#endif

void AssetLoader::Init() {

#ifdef ENGINE_ANDROID
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

#ifndef ENGINE_ANDROID
    dataDirectory = directory;
#endif

}

ifstream AssetLoader::ReadFile(string filename, ios_base::openmode mode) {

	ifstream file;

	string path = GetFullPath(filename);

	file.open(path, mode);

	// It might be that the file is not unpacked
#ifdef ENGINE_ANDROID
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

	return file;

}

void AssetLoader::UnpackFile(string filename) {

	lock_guard<mutex> guard(assetLoaderMutex);



}

void AssetLoader::UnpackDirectory(string directory) {

	lock_guard<mutex> guard(assetLoaderMutex);

}

string AssetLoader::GetFullPath(string path) {
	
	return dataDirectory + "/" + path;

}