#include "MaterialGraphLoader.h"
#include "AssetLoader.h"
#include "AssetReferences.h"

#include "../materialgraph/MaterialGraphSerializer.h"
#include "../resource/Resource.h"

#include <filesystem>
#include <unordered_set>
#include <vector>

namespace Atlas::Loader {

    Ref<MaterialGraph::Graph> MaterialGraphLoader::LoadGraph(const std::string& filename) {

        Loader::AssetLoader::UnpackFile(filename);
        auto path = Loader::AssetLoader::GetFullPath(filename);

        auto fileStream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);

        if (!fileStream.is_open()) {
            throw ResourceLoadException(filename, "Couldn't open material graph file stream");
        }

        auto data = Loader::AssetLoader::GetFileContent(fileStream);
        auto j = json::parse(data);

        fileStream.close();

        AssetReferences::ResolveReferences(j, AssetReferences::Type::MaterialGraph, filename);

        CheckSubgraphCycle(j, filename);

        auto graph = CreateRef<MaterialGraph::Graph>();
        from_json(j, *graph);

        auto compiled = graph->Compile();
        for (const auto& error : compiled->errors)
            Log::Error("Material graph " + filename + ": " + error.message);

        return graph;

    }

    void MaterialGraphLoader::SaveGraph(const Ref<MaterialGraph::Graph>& graph, const std::string& filename) {

        json j;
        to_json(j, *graph);
        AssetReferences::MakeReferences(j, AssetReferences::Type::MaterialGraph, filename);

        if (!AssetLoader::WriteJson(filename, j, JsonFormat::Indented))
            Log::Error("Couldn't write material graph file " + filename);

    }

    void MaterialGraphLoader::CheckSubgraphCycle(const json& j, const std::string& filename) {

        auto getFullPath = [](const std::string& path) {
            return std::filesystem::path(AssetLoader::GetFullPath(path)).lexically_normal().generic_string();
            };

        std::vector<std::string> pending;
        auto addSubgraphs = [&](const json& graph) {
            if (!graph.contains("nodes"))
                return;
            for (const auto& node : graph["nodes"]) {
                if (node.contains("subgraphPath"))
                    pending.push_back(node["subgraphPath"]);
            }
            };

        auto fullPath = getFullPath(filename);
        std::unordered_set<std::string> visited;

        addSubgraphs(j);
        while (!pending.empty()) {
            auto path = pending.back();
            pending.pop_back();

            auto subgraphPath = getFullPath(path);
            if (subgraphPath == fullPath)
                throw ResourceLoadException(filename, "The material graph contains itself as a subgraph");
            if (!visited.insert(subgraphPath).second)
                continue;

            AssetLoader::UnpackFile(path);
            auto fileStream = AssetLoader::ReadFile(AssetLoader::GetFullPath(path), std::ios::in | std::ios::binary);
            if (!fileStream.is_open())
                continue;

            auto subgraph = json::parse(AssetLoader::GetFileContent(fileStream), nullptr, false);
            if (subgraph.is_discarded())
                continue;

            AssetReferences::ResolveReferences(subgraph, AssetReferences::Type::MaterialGraph, path);
            addSubgraphs(subgraph);
        }

    }

}
