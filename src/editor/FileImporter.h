#pragma once

#include <vector>
#include <string>
#include <map>

namespace Atlas::Editor {

    class FileImporter {

    public:
        static void ImportFiles(const std::vector<std::string>& filenames);

        static void ImportFile(const std::string& filename);

    private:
        enum class FileType {
            Audio = 0,
            Mesh,
            Terrain,
            Scene,
            Script,
            Font
        };

        static const std::map<const std::string, FileType> fileTypeMapping;


    };

}