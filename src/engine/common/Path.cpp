#include "Path.h"

#include <algorithm>
#include <filesystem>

#ifdef AE_OS_WINDOWS
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Atlas {

    namespace Common {

        std::string Path::GetRelative(std::string src, std::string dest) {

            std::string result;

            src = GetAbsolute(src);
            dest = GetAbsolute(dest);

            // Find the first character that doesn't match
            size_t count = 0;
            size_t max = dest.find_last_of('/');

            while (count <= max && src[count] == dest[count])
                count++;

            if (!count)
                return dest;

            dest = dest.substr(count, dest.size());

            for (size_t i = count; i < src.size(); i++)
                if (src[i] == '/' || src[i] == '\\')
                    result.append("../");

            result.append(dest);

            return result;

        }

        std::string Path::GetDirectory(std::string path) {

            auto pos = path.find_last_of("/\\");

            if (pos == std::string::npos)
                return "";

            return path.substr(0, pos);

        }

        std::string Path::GetFileName(std::string path) {

            auto offset = path.find_last_of("/\\") + 1;

            if (offset == std::string::npos)
                return path;

            return path.substr(offset, path.end() - path.begin() - offset);

        }

        std::string Path::GetFileType(std::string path) {

            std::string fileType = "";

            auto filename = GetFileName(path);
            auto offset = filename.find('.');

            if (offset != std::string::npos) {
                fileType = filename.substr(offset + 1,
                    filename.end() - filename.begin() - offset);
            }
            
            return fileType;

        }

        std::string Path::GetAbsolute(const std::string& path) {

            auto absolute = std::filesystem::absolute(path);
            return absolute.string();

        }

        bool Path::IsAbsolute(std::string path) {

            return std::filesystem::path(path).is_absolute();

        }

        std::string Path::Normalize(std::string path) {

            size_t backPosition;

            for (size_t i = 0; i < path.size(); i++) {
                if (path[i] == '\\')
                    path[i] = '/';
            }

            while ((backPosition = path.find("/..")) != std::string::npos) {
                auto parentPath = path.substr(0, backPosition);
                auto childPath = path.substr(backPosition + 3, path.length());
                size_t parentBackPosition = parentPath.find_last_of('/');
                if (parentBackPosition == std::string::npos) {
                    return path;
                }
                path = parentPath.substr(0, parentBackPosition) + childPath;
            }

            return path;

        }

    }

}