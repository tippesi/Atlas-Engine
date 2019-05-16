#ifndef AE_PATH_H
#define AE_PATH_H

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
			 * Returns the filename of a path.
			 * @param path A path to a file.
			 * @return The filename in the path.
			 * @warning The paths must contain a filename.
			 */
			static std::string GetFilename(std::string path);

			/**
			 * Gets the absolute and normalized path to for a relative path
			 * @param path A relative path.
			 * @return The absolute path.
			 * @note It is assumed that the path is relative to the working directory.
			 */
			static std::string GetAbsolute(std::string path);

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

		};

	}

}

#endif