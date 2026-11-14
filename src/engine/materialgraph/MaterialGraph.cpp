#include "MaterialGraph.h"
#include "Compiler.h"
#include "NodeLibrary.h"
#include "../common/ColorConverter.h"

#include <algorithm>
#include <cstring>

namespace Atlas::MaterialGraph {

    void Parameter::WriteValue(vec4 value, uint32_t* words) const {

        if (isColor)
            value = vec4(Common::ColorConverter::ConvertSRGBToLinear(vec3(value)), value.a);

        std::memcpy(words, glm::value_ptr(value), (uint32_t(type) + 1u) * sizeof(float));

    }

    Graph::Graph(const Graph& that) {

        *this = that;

    }

    Graph& Graph::operator=(const Graph& that) {

        if (this != &that) {
            name = that.name;
            nodes = that.nodes;
            links = that.links;
            maxDisplacement = that.maxDisplacement;
            nextNodeId = that.nextNodeId;

            auto thatCompiled = that.GetCompiled();
            std::scoped_lock lock(compiledMutex);
            compiled = thatCompiled;
        }

        return *this;

    }

    Ref<Graph> Graph::CreateDefault() {

        auto graph = CreateRef<Graph>();

        graph->AddNode(NodeLibrary::vertexOutputType, vec2(400.0f, -260.0f));
        graph->AddNode(NodeLibrary::surfaceOutputType, vec2(400.0f, 0.0f));
        graph->Compile();

        return graph;

    }

    Node& Graph::AddNode(const std::string& type, vec2 position) {

        auto& node = nodes.emplace_back();
        node.id = nextNodeId++;
        node.type = type;
        node.position = position;

        if (auto definition = NodeLibrary::Find(type))
            node.value = definition->defaultValue;

        if (type == NodeLibrary::codeType) {
            node.code = "return 0.0;";
            NodeLibrary::UpdateCodeDefinition(node);
        }

        return node;

    }

    void Graph::RemoveNode(uint32_t id) {

        std::erase_if(links, [&](const Link& link) { return link.fromNode == id || link.toNode == id; });
        std::erase_if(nodes, [&](const Node& node) { return node.id == id; });

    }

    Node* Graph::GetNode(uint32_t id) {

        auto iter = std::find_if(nodes.begin(), nodes.end(), [&](const Node& node) { return node.id == id; });
        return iter != nodes.end() ? &*iter : nullptr;

    }

    const Node* Graph::GetNode(uint32_t id) const {

        auto iter = std::find_if(nodes.begin(), nodes.end(), [&](const Node& node) { return node.id == id; });
        return iter != nodes.end() ? &*iter : nullptr;

    }

    bool Graph::Connect(uint32_t fromNode, const std::string& fromPin, uint32_t toNode, const std::string& toPin) {

        auto from = GetNode(fromNode);
        auto to = GetNode(toNode);
        if (!from || !to || fromNode == toNode)
            return false;

        auto fromDefinition = NodeLibrary::Find(*from);
        auto toDefinition = NodeLibrary::Find(*to);
        if (!fromDefinition || !toDefinition)
            return false;

        auto output = fromDefinition->FindOutput(fromPin);
        auto input = toDefinition->FindInput(toPin);
        if (!output || !input)
            return false;

        // Textures only connect to textures, all other values convert to each other
        if ((output->type == ValueType::Texture) != (input->type == ValueType::Texture))
            return false;

        Disconnect(toNode, toPin);
        links.push_back({ fromNode, fromPin, toNode, toPin });

        return true;

    }

    void Graph::Disconnect(uint32_t toNode, const std::string& toPin) {

        std::erase_if(links, [&](const Link& link) { return link.toNode == toNode && link.toPin == toPin; });

    }

    const Link* Graph::GetInputLink(uint32_t toNode, const std::string& toPin) const {

        auto iter = std::find_if(links.begin(), links.end(),
            [&](const Link& link) { return link.toNode == toNode && link.toPin == toPin; });
        return iter != links.end() ? &*iter : nullptr;

    }

    Ref<const CompiledGraph> Graph::Compile() {

        Ref<const CompiledGraph> result = Compiler::Compile(*this);

        std::scoped_lock lock(compiledMutex);
        compiled = result;

        return result;

    }

    Ref<const CompiledGraph> Graph::GetCompiled() const {

        std::scoped_lock lock(compiledMutex);
        return compiled;

    }

}
