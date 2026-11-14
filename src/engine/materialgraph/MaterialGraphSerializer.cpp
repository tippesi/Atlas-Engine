#include "MaterialGraphSerializer.h"
#include "NodeLibrary.h"

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
            NodeLibrary::UpdateCodeDefinition(p);
        }

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
            {"nodes", p.nodes},
            {"links", p.links},
        };

    }

    void from_json(const json& j, Graph& p) {

        j.at("name").get_to(p.name);
        j.at("nodes").get_to(p.nodes);
        j.at("links").get_to(p.links);

        try_get_json(j, "maxDisplacement", p.maxDisplacement);

        p.nextNodeId = 1;
        for (const auto& node : p.nodes)
            p.nextNodeId = std::max(p.nextNodeId, node.id + 1);

    }

}
