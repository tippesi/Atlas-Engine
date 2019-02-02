#ifndef ASSETLOADER_H
#define ASSETLOADER_H

#include "../System.h"

#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <vector>
#include <mutex>

class AssetLoader {

public:
    static void Init();

    static void SetAssetDirectory(string directory);

    static ifstream ReadFile(string filename, ios_base::openmode mode);

    static ofstream WriteFile(string filename, ios_base::openmode mode);

	static size_t GetFileSize(ifstream& stream);

	static vector<char> GetFileContent(ifstream& stream);

	static void MakeDirectory(string directory);
	
	static void UnpackFile(string filename);

    static void UnpackDirectory(string directory);

	static string GetFullPath(string path);

private:
    static string GetAssetPath(string path);

    static string GetAbsolutePath(string path);

    static string assetDirectory;

    static string dataDirectory;

    static mutex assetLoaderMutex;

#ifdef AE_OS_ANDROID
    static AAssetManager* manager;
#endif

};


#endif
