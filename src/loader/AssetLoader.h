#ifndef ASSETLOADER_H
#define ASSETLOADER_H

#include "../System.h"

#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <mutex>

class AssetLoader {

public:
    static void Init();

    static void SetAssetDirectory(string directory);

    static ifstream ReadFile(string filename);

    static ofstream WriteFile(string filename);

    static void UnpackFile(string filename);

    static void UnpackDirectory(string directory);

	static string GetFullPath(string path);

private:
    static string assetDirectory;

    static string dataDirectory;

    static mutex assetLoaderMutex;

#ifdef ENGINE_ANDROID
    static AAssetManager* manager;
#endif

};


#endif
