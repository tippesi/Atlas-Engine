#pragma once

#include "System.h"
#include "mesh/Impostor.h"
#include "common/Image.h"
#include "common/SerializationHelper.h"

#include <limits>
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

        struct ImpostorTextures {
            Ref<Texture::Texture2DArray> baseColor;
            Ref<Texture::Texture2DArray> roughnessMetalnessAo;
            Ref<Texture::Texture2DArray> normal;
            Ref<Texture::Texture2DArray> depth;

            bool IsComplete() const;
        };

        // The views of the textures are loaded with at most maxResolution, the impostor keeps the resolution of the file
        static Ref<Mesh::Impostor> LoadImpostorFromMemory(const std::vector<char>& data, const std::string& filename,
            bool binaryJson = true, int32_t maxResolution = std::numeric_limits<int32_t>::max());

        // Only the textures of the file
        static ImpostorTextures LoadImpostorTextures(const std::vector<char>& data, const std::string& filename,
            bool binaryJson, int32_t maxResolution);

        static void SaveImpostor(const Ref<Mesh::Impostor>& impostor, const std::string& filename, bool binaryJson = true);

        // Reads the textures back from the GPU, has to run where GPU work can be submitted
        static Ref<ImpostorFileData> ReadImpostor(const Ref<Mesh::Impostor>& impostor, const std::string& filename,
            bool binaryJson = true);

        static void WriteImpostor(const Ref<ImpostorFileData>& data);

    private:
        static json ParseFile(const std::vector<char>& data, bool binaryJson);

        static ImpostorTextures LoadTextures(json& j, const std::string& filename, int32_t fileResolution, int32_t views,
            int32_t maxResolution);

    };

}