#include "Script.h"

#include "loader/AssetLoader.h"
#include "resource/ResourceManager.h"

namespace Atlas::Scripting {

    Script::Script(const std::string& filename) : filename(filename) {

        Reload();

    }

    void Script::Reload() {

        auto stream = Loader::AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);

        if (!stream.is_open()) {
            throw ResourceLoadException(filename, "Couldn't open file stream");
        }

        auto filedata = Loader::AssetLoader::GetFileContent(stream);

        code = std::string(filedata.begin(), filedata.end());

    }

}