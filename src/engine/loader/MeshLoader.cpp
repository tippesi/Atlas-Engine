#include "MeshLoader.h"
#include "AssetLoader.h"
#include "AssetReferences.h"
#include "ImpostorLoader.h"
#include "mesh/MeshSerializer.h"

namespace Atlas::Loader {

    Ref<Mesh::Mesh> MeshLoader::LoadMesh(const std::string& filename, bool binaryJson) {

        Loader::AssetLoader::UnpackFile(filename);
        auto path = Loader::AssetLoader::GetFullPath(filename);

        auto fileStream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);

        if (!fileStream.is_open()) {
            throw ResourceLoadException(filename, "Couldn't open mesh file stream: " + std::string(strerror(errno)));
        }

        json j;
        if (binaryJson) {
            auto data = Loader::AssetLoader::GetFileContent(fileStream);
            // We don't want to keep the file stream open longer than we need
            fileStream.close();
            j = json::from_msgpack(data);
        }
        else {
            auto data = Loader::AssetLoader::GetFileContent(fileStream);
            fileStream.close();
            j = json::parse(data);
        }

        AssetReferences::ResolveReferences(j, AssetReferences::Type::Mesh, filename);

        auto mesh = CreateRef<Mesh::Mesh>();
        from_json(j, *mesh);

        return mesh;

    }

    void MeshLoader::SaveMesh(const Ref<Mesh::Mesh>& mesh, const std::string& filename, 
        bool binaryJson, bool formatJson, bool saveImpostor) {

        json j;
        to_json(j, *mesh);
        AssetReferences::MakeReferences(j, AssetReferences::Type::Mesh, filename);

        auto format = binaryJson ? JsonFormat::MessagePack : formatJson ? JsonFormat::Indented : JsonFormat::Compact;
        if (!AssetLoader::WriteJson(filename, j, format)) {
            Log::Error("Couldn't write mesh file " + filename);
            return;
        }

        // Impostors the streaming reduced keep the file they were loaded from
        const auto& impostor = mesh->impostor;
        if (saveImpostor && impostor.IsLoaded() && impostor->IsResident() &&
            impostor->baseColorTexture->width == impostor->resolution) {
            ImpostorLoader::SaveImpostor(mesh->impostor.Get(), mesh->impostor.GetResource()->path, binaryJson);
        }

    }

}
