#pragma once

#include "../System.h"

#include <string>

namespace Atlas {

    namespace Common {

        class Path {

        public:
            /**
             * Calculates the path from one file to another.
             * @param src A path to a file.
             * @param dest A path to a file.
             * @return The path from src to dest.
             * @note A relative path is assumed to be relative to the working directory.
             * @warning Both paths must contain a filename.
             */
            static std::string GetRelative(std::string src, std::string dest);

            /**
             * Returns the parent directory of a file.
             * @param path A path to a file.
             * @return The parent directory of the file.
             * @warning The paths must contain a filename.
             */
            static std::string GetDirectory(std::string path);

            /**
             * Returns the file name of a path.
             * @param path A path to a file.
             * @return The file name in the path.
             * @warning The paths must contain a filename.
             */
            static std::string GetFileName(std::string path);

            /**
             * Returns the file name of a path without its extension.
             * @param path A path to a file.
             * @return The file name in the path.
             * @warning The paths must contain a filename.
             */
            static std::string GetFileNameWithoutExtension(std::string path);

            /**
             * Returns the file type of a path.
             * @param path A path to a file.
             * @return The file name in the path.
             * @warning The paths must contain a filename.
             */
            static std::string GetFileType(std::string path);

            /**
             * Gets the absolute and normalized path to for a relative path
             * @param path A relative path.
             * @return The absolute path.
             * @note It is assumed that the path is relative to the working directory.
             */
            static std::string GetAbsolute(const std::string& path);

            /**
             * Checks whether a path is absolute.
             * @param path The path that needs to be checked
             * @return True if absolute, false otherwise.
             */
            static bool IsAbsolute(std::string path);

            /**
             * Normalizes a given path
             * @param path A path.
             * @return The normalized path.
             * @note Normalizing means resolving all "../"
             */
            static std::string Normalize(std::string path);

            /**
             * Converts the letters of a path to lower case, e.g. to compare paths of case-insensitive file systems.
             * @param path A path.
             * @return The path in lower case.
             */
            static std::string ToLower(std::string path);

            /**
             * Returns a path after a file or directory was renamed.
             * @param path A path to a file or directory.
             * @param oldPath The old path of the renamed file or directory.
             * @param newPath The new path of the renamed file or directory.
             * @return The path with its new beginning, the same path if it isn't the renamed one or in it.
             * @note Upper and lower case don't matter, like on the file systems of Windows and macOS.
             */
            static std::string Rename(const std::string& path, const std::string& oldPath, const std::string& newPath);

        };

    }

}