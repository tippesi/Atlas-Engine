#include "FileImporter.h"

#include "mesh/Mesh.h"
#include "scene/Scene.h"
#include "audio/AudioData.h"

namespace Atlas::Editor {

    void FileImporter::ImportFiles(const std::vector<std::string> &filenames) {

        for (const auto& filename : filenames)
            ImportFile(filename);

    }

    void FileImporter::ImportFile(const std::string &filename) {



    }

}