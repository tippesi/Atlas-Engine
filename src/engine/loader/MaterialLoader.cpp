#include "MaterialLoader.h"
#include "AssetLoader.h"

#include "mesh/MeshSerializer.h"

namespace Atlas::Loader {

    Ref<Material> MaterialLoader::LoadMaterial(const std::string& filename, bool binaryJson) {

        Loader::AssetLoader::UnpackFile(filename);
        auto path = Loader::AssetLoader::GetFullPath(filename);

        auto fileStream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);

        if (!fileStream.is_open()) {
            throw ResourceLoadException(filename, "Couldn't open material file stream");
        }

        json j;
        if (binaryJson) {
            auto data = Loader::AssetLoader::GetFileContent(fileStream);
            j = json::from_bjdata(data);
        }
        else {
            std::string serialized((std::istreambuf_iterator<char>(fileStream)),
                std::istreambuf_iterator<char>());
            j = json::parse(serialized);
        }

        fileStream.close();

        auto material = CreateRef<Material>();
        from_json(j, *material);

        return material;

    }

    void MaterialLoader::SaveMaterial(const Ref<Material>& material, const std::string& filename,
        bool binaryJson, bool formatJson) {

        auto path = Loader::AssetLoader::GetFullPath(filename);
        auto fileStream = Loader::AssetLoader::WriteFile(path, std::ios::out | std::ios::binary);

        if (!fileStream.is_open()) {
            Log::Error("Couldn't write material file " + filename);
            return;
        }

        json j;
        to_json(j, *material);

        if (binaryJson) {
            auto data = json::to_bjdata(j);
            fileStream.write(reinterpret_cast<const char*>(data.data()), data.size());
        }
        else {
            fileStream << (formatJson ? j.dump(2) : j.dump());
        }

        fileStream.close();

    }

}