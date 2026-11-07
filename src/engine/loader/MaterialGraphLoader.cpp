#include "MaterialGraphLoader.h"
#include "AssetLoader.h"
#include "AssetReferences.h"

#include "../materialgraph/MaterialGraphSerializer.h"
#include "../resource/Resource.h"

namespace Atlas::Loader {

    Ref<MaterialGraph::Graph> MaterialGraphLoader::LoadGraph(const std::string& filename) {

        Loader::AssetLoader::UnpackFile(filename);
        auto path = Loader::AssetLoader::GetFullPath(filename);

        auto fileStream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);

        if (!fileStream.is_open()) {
            throw ResourceLoadException(filename, "Couldn't open material graph file stream");
        }

        std::string serialized((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
        auto j = json::parse(serialized);

        fileStream.close();

        AssetReferences::ResolveReferences(j, AssetReferences::Type::MaterialGraph, filename);

        auto graph = CreateRef<MaterialGraph::Graph>();
        from_json(j, *graph);

        auto compiled = graph->Compile();
        for (const auto& error : compiled->errors)
            Log::Error("Material graph " + filename + ": " + error.message);

        return graph;

    }

    void MaterialGraphLoader::SaveGraph(const Ref<MaterialGraph::Graph>& graph, const std::string& filename) {

        auto path = Loader::AssetLoader::GetFullPath(filename);
        auto fileStream = Loader::AssetLoader::WriteFile(path, std::ios::out | std::ios::binary);

        if (!fileStream.is_open()) {
            Log::Error("Couldn't write material graph file " + filename);
            return;
        }

        json j;
        to_json(j, *graph);
        AssetReferences::MakeReferences(j, AssetReferences::Type::MaterialGraph, filename);

        fileStream << j.dump(2);

        fileStream.close();

    }

}
