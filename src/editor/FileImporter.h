#pragma once

#include <vector>
#include <string>

namespace Atlas::Editor {

    class FileImporter {

        static void ImportFiles(const std::vector<std::string>& filenames);

        static void ImportFile(const std::string& filename);

    };

}