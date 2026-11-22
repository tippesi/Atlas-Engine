#include "MaterialLoader.h"
#include "AssetLoader.h"
#include "AssetReferences.h"

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
            auto data = Loader::AssetLoader::GetFileContent(fileStream);
            j = json::parse(data);
        }

        fileStream.close();

        AssetReferences::ResolveReferences(j, AssetReferences::Type::Material, filename);

        auto material = CreateRef<Material>();
        from_json(j, *material);

        return material;

    }

    void MaterialLoader::SaveMaterial(const Ref<Material>& material, const std::string& filename,
        bool binaryJson, bool formatJson) {

        json j;
        to_json(j, *material);
        AssetReferences::MakeReferences(j, AssetReferences::Type::Material, filename);

        auto format = binaryJson ? JsonFormat::BJData : formatJson ? JsonFormat::Indented : JsonFormat::Compact;
        if (!AssetLoader::WriteJson(filename, j, format))
            Log::Error("Couldn't write material file " + filename);

    }

}
