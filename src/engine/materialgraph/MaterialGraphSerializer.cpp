#include "MaterialGraphSerializer.h"
#include "NodeLibrary.h"

#include "../loader/MaterialGraphLoader.h"
#include "../resource/ResourceManager.h"
#include "../streaming/TextureStreaming.h"

namespace Atlas::MaterialGraph {

    void to_json(json& j, const CodeInput& p) {

        j = json::array({ p.name, NodeLibrary::GetTypeName(p.type) });

    }

    void from_json(const json& j, CodeInput& p) {

        j.at(0).get_to(p.name);
        p.type = NodeLibrary::FindType(j.at(1).get<std::string>());

    }

    void to_json(json& j, const SubgraphPin& p) {

        j = json::array({ p.id, p.name });

    }

    void from_json(const json& j, SubgraphPin& p) {

        j.at(0).get_to(p.id);
        j.at(1).get_to(p.name);

    }

    void to_json(json& j, const Node& p) {

        j = json{
            {"id", p.id},
            {"type", p.type},
            {"position", p.position},
            {"value", p.value},
        };

        if (!p.name.empty())
            j["name"] = p.name;

        if (p.texture.IsValid() && !p.texture.IsGenerated())
            j["texturePath"] = p.texture.GetResource()->path;

        if (!p.inputValues.empty()) {
            auto values = json::object();
            for (const auto& [name, value] : p.inputValues)
                values[name] = value;
            j["inputValues"] = values;
        }

        if (p.range.x < p.range.y)
            j["range"] = p.range;
        if (!p.group.empty())
            j["group"] = p.group;
        if (p.order != 0)
            j["order"] = p.order;
        if (p.size.x > 0.0f)
            j["size"] = p.size;

        if (p.type == NodeLibrary::codeType) {
            j["code"] = p.code;
            j["codeOutputType"] = NodeLibrary::GetTypeName(p.codeOutputType);
            j["codeInputs"] = p.codeInputs;
        }

        if (p.type == NodeLibrary::subgraphInputType || p.type == NodeLibrary::subgraphOutputType)
            j["pinType"] = NodeLibrary::GetTypeName(p.pinType);

        if (p.type == NodeLibrary::subgraphType) {
            if (p.subgraph.IsValid() && !p.subgraph.IsGenerated())
                j["subgraphPath"] = p.subgraph.GetResource()->path;
            j["subgraphPins"] = p.subgraphPins;
        }

    }

    void from_json(const json& j, Node& p) {

        j.at("id").get_to(p.id);
        j.at("type").get_to(p.type);
        j.at("position").get_to(p.position);
        j.at("value").get_to(p.value);

        try_get_json(j, "name", p.name);
        try_get_json(j, "range", p.range);
        try_get_json(j, "group", p.group);
        try_get_json(j, "order", p.order);
        try_get_json(j, "size", p.size);

        p.texture = j.contains("texturePath") ? ResourceManager<Texture::Texture2D>::GetOrLoadResourceWithLoaderAsync(
            j["texturePath"], Streaming::TextureStreaming::LoadTexture, false, Texture::Wrapping::Repeat,
            Texture::Filtering::Anisotropic) : ResourceHandle<Texture::Texture2D>();

        p.inputValues.clear();
        if (j.contains("inputValues")) {
            for (const auto& [name, value] : j["inputValues"].items())
                p.inputValues[name] = value.get<vec4>();
        }

        if (p.type == NodeLibrary::codeType) {
            std::string outputType = NodeLibrary::GetTypeName(p.codeOutputType);
            try_get_json(j, "code", p.code);
            try_get_json(j, "codeOutputType", outputType);
            try_get_json(j, "codeInputs", p.codeInputs);
            p.codeOutputType = NodeLibrary::FindType(outputType);
        }

        if (j.contains("pinType"))
            p.pinType = NodeLibrary::FindType(j["pinType"].get<std::string>());

        // The pins of the node depend on the subgraph, so it is loaded right away like graphs are for materials
        p.subgraph = j.contains("subgraphPath") ? ResourceManager<Graph>::GetOrLoadResourceWithLoader(j["subgraphPath"],
            ResourceOrigin::User, Loader::MaterialGraphLoader::LoadGraph) : ResourceHandle<Graph>();
        try_get_json(j, "subgraphPins", p.subgraphPins);

        NodeLibrary::UpdateDefinition(p);

    }

    void to_json(json& j, const Link& p) {

        j = json{
            {"fromNode", p.fromNode},
            {"fromPin", p.fromPin},
            {"toNode", p.toNode},
            {"toPin", p.toPin},
        };

    }

    void from_json(const json& j, Link& p) {

        j.at("fromNode").get_to(p.fromNode);
        j.at("fromPin").get_to(p.fromPin);
        j.at("toNode").get_to(p.toNode);
        j.at("toPin").get_to(p.toPin);

    }

    void to_json(json& j, const Graph& p) {

        j = json{
            {"name", p.name},
            {"maxDisplacement", p.maxDisplacement},
            {"blendMode", int32_t(p.blendMode)},
            {"twoSided", p.twoSided},
            {"nextNodeId", p.nextNodeId},
            {"nodes", p.nodes},
            {"links", p.links},
        };

    }

    void from_json(const json& j, Graph& p) {

        j.at("name").get_to(p.name);
        j.at("nodes").get_to(p.nodes);
        j.at("links").get_to(p.links);

        try_get_json(j, "maxDisplacement", p.maxDisplacement);
        p.legacyMaterialSettings = !j.contains("blendMode");
        int32_t blendMode = 0;
        try_get_json(j, "blendMode", blendMode);
        p.blendMode = MaterialBlendMode(blendMode);
        try_get_json(j, "twoSided", p.twoSided);

        // Ids aren't used again, the pins of subgraphs are told apart by them
        p.nextNodeId = 1;
        try_get_json(j, "nextNodeId", p.nextNodeId);
        for (const auto& node : p.nodes)
            p.nextNodeId = std::max(p.nextNodeId, node.id + 1);

    }

}
