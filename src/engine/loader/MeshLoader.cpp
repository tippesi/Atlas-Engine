#include "MeshLoader.h"
#include "mesh/MeshSerializer.h"

namespace Atlas::Loader {

    Ref<Mesh::Mesh> MeshLoader::LoadMesh(const std::string& filename, bool binaryJson) {

        Loader::AssetLoader::UnpackFile(filename);
        auto path = Loader::AssetLoader::GetFullPath(filename);

        auto fileStream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);

        if (!fileStream.is_open()) {
            throw ResourceLoadException(filename, "Couldn't open mesh file stream");
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

        auto mesh = CreateRef<Mesh::Mesh>();
        from_json(j, *mesh);

        return mesh;

    }

    void MeshLoader::SaveMesh(const Ref<Mesh::Mesh>& mesh, const std::string& filename, 
        bool binaryJson, bool formatJson) {

        auto path = Loader::AssetLoader::GetFullPath(filename);
        auto fileStream = Loader::AssetLoader::WriteFile(path, std::ios::out | std::ios::binary);

        if (!fileStream.is_open()) {
            Log::Error("Couldn't write mesh file " + filename);
            return;
        }

        json j;
        to_json(j, *mesh);
        
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