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

		/**
		 * The asset loader is an abstraction for the operating system reads and writes to
		 * files. It allows the access to a data folder on every operating system.
		 * @note On Android the asset loader will copy data from the asset folder to the cache
		 * of the application in order to allow read and writes.
		 */
        class AssetLoader {

        public:
        	/**
        	 * Initializes the asset loader. Is called by the engine.
        	 */
            static void Init();

            /**
             * Saves a path to an asset directory in order to access
             * @param directory
             * @note All file paths handed over to the engine should be relative the asset directory.
             * @warning All files that are loaded with the engine should be stored in
             * the asset directory.
             */
            static void SetAssetDirectory(std::string directory);

            /**
             * Opens a file for reading from the asset directory.
             * @param filename A path to the file relative to the asset directory.
             * @param mode The mode in which the file should be opened, e.g. std::ios::binary
             * @return An ifstream object. See
             * <a href="http://www.cplusplus.com/reference/fstream/ifstream/">cplusplus.com</a> for more.
             */
            static std::ifstream ReadFile(std::string filename, std::ios_base::openmode mode);

            /**
             * Opens a file for writing from the asset directory.
             * @param filename A path to the file relative to the asset directory.
             * @param mode The mode in which the file should be opened, e.g. std::ios::binary
             * @return An ofstream object. See
             * <a href="http://www.cplusplus.com/reference/fstream/ofstream/">cplusplus.com</a> for more.
             */
            static std::ofstream WriteFile(std::string filename, std::ios_base::openmode mode);

            /**
             * Returns a size for a readable opened file.
             * @param stream An ifstream object.
             * @return The size of the file in bytes.
			 * @note Assumes the stream is opened in binary mode.
             */
            static size_t GetFileSize(std::ifstream& stream);

            /**
             * Returns the content of the file as a byte vector.
             * @param stream An ifstream object.
             * @return The content of tha file.
			 * @note Assumes the stream is opened in binary mode.
             */
            static std::vector<char> GetFileContent(std::ifstream& stream);

            /**
             * Creates a new directory in the asset folder.
             * @param directory The path of the directory relative to the asset directory.
             */
            static void MakeDirectory(std::string directory);

            /**
             * Unpacks a file from the asset directory to the applications cache.
             * @param filename A file path relative to the asset directory
             * @note This is only needed for Android right now and is done automatically
             * by the asset loader.
             */
            static void UnpackFile(std::string filename);

            /**
             * Returns the full path of a path relative to the asset directory.
             * @param path A path relative to the asset directory.
             * @return The full path as a string.
             */
            static std::string GetFullPath(std::string path);

        private:
            static std::string GetAssetPath(std::string path);

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
