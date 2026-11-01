#pragma once

#include "System.h"
#include "mesh/Impostor.h"
#include "common/Image.h"
#include "common/SerializationHelper.h"

#include <map>

namespace Atlas::Loader {

    class ImpostorLoader {

    public:
        // An impostor read back from the GPU, WriteImpostor encodes and writes it on any thread
        struct ImpostorFileData {
            std::string filename;
            bool binaryJson = true;
            bool compressTextures = false;
            bool write = true;

            json j;
            std::map<std::string, std::vector<Ref<Common::Image<uint8_t>>>> layers;
        };

        static Ref<Mesh::Impostor> LoadImpostor(const std::string& filename, bool binaryJson = true);

        static void SaveImpostor(const Ref<Mesh::Impostor>& impostor, const std::string& filename, bool binaryJson = true);

        // Reads the textures back from the GPU, has to run where GPU work can be submitted
        static Ref<ImpostorFileData> ReadImpostor(const Ref<Mesh::Impostor>& impostor, const std::string& filename,
            bool binaryJson = true);

        static void WriteImpostor(const Ref<ImpostorFileData>& data);

    };

}