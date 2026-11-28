#include "MaterialGraph.h"
#include "Compiler.h"
#include "NodeLibrary.h"
#include "../common/ColorConverter.h"
#include "../resource/ResourceManager.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_set>

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
            blendMode = that.blendMode;
            twoSided = that.twoSided;
            legacyMaterialSettings = that.legacyMaterialSettings.load();
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

    Ref<Graph> Graph::CreateDefaultSubgraph() {

        auto graph = CreateRef<Graph>();

        auto inputId = graph->AddNode(NodeLibrary::subgraphInputType).id;
        auto outputId = graph->AddNode(NodeLibrary::subgraphOutputType, vec2(400.0f, 0.0f)).id;

        for (auto& node : graph->nodes) {
            node.name = node.id == inputId ? "Value" : "Result";
            node.pinType = ValueType::Vec3;
            NodeLibrary::UpdateDefinition(node);
        }

        graph->Connect(inputId, "Value", outputId, "Value");
        graph->Compile();

        return graph;

    }

    Ref<Graph> Graph::CreateSubgraphPreview(const ResourceHandle<Graph>& subgraph) {

        auto graph = CreateDefault();
        auto vertexOutput = graph->nodes[0].id;
        auto surfaceOutput = graph->nodes[1].id;

        auto subgraphNode = graph->AddNode(NodeLibrary::subgraphType).id;
        graph->SetSubgraph(subgraphNode, subgraph);

        // Without its textures a subgraph still has to show something
        auto compiled = subgraph.IsLoaded() ? subgraph->GetCompiled() : nullptr;
        if (compiled) {
            for (auto pin : NodeLibrary::GetSubgraphPins(compiled->subgraphNodes, NodeLibrary::subgraphInputType)) {
                auto hasDefault = std::any_of(compiled->subgraphLinks.begin(), compiled->subgraphLinks.end(),
                    [&](const Link& link) { return link.toNode == pin->id; });
                if (pin->pinType != ValueType::Texture || hasDefault)
                    continue;
                auto& texture = graph->AddNode("Texture parameter");
                auto isTaken = std::any_of(compiled->parameters.begin(), compiled->parameters.end(),
                    [&](const Parameter& parameter) { return parameter.name == pin->name; });
                texture.name = pin->name + (isTaken ? " input" : "");
                graph->Connect(texture.id, "Value", subgraphNode, pin->name);
            }
        }

        // Textures are sampled to show them
        auto connect = [&](const OutputDefinition& output, uint32_t toNode, const std::string& toPin) {
            auto fromNode = subgraphNode;
            auto fromPin = output.name;
            if (output.type == ValueType::Texture) {
                fromNode = graph->AddNode("Sample texture").id;
                graph->Connect(subgraphNode, output.name, fromNode, "Texture");
                fromPin = "Color";
            }
            graph->Connect(fromNode, fromPin, toNode, toPin);
            };

        auto outputs = NodeLibrary::Find(*graph->GetNode(subgraphNode))->outputs;
        auto connected = false;
        for (const auto& output : outputs) {
            auto isSurface = NodeLibrary::Find(NodeLibrary::surfaceOutputType)->FindInput(output.name) != nullptr;
            auto isVertex = output.name == NodeLibrary::positionOffsetPin;
            if (isSurface || isVertex) {
                connect(output, isSurface ? surfaceOutput : vertexOutput, output.name);
                connected = true;
            }
        }
        if (subgraph.IsLoaded())
            graph->maxDisplacement = subgraph->maxDisplacement;

        if (!connected && !outputs.empty()) {
            connect(outputs.front(), surfaceOutput, NodeLibrary::baseColorPin);
            if (graph->Compile()->valid)
                return graph;

            // Values of the vertex stage can't be a color
            graph->Disconnect(surfaceOutput, NodeLibrary::baseColorPin);
            connect(outputs.front(), vertexOutput, NodeLibrary::positionOffsetPin);
        }
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

        if (type == NodeLibrary::codeType)
            node.code = "return 0.0;";
        NodeLibrary::UpdateDefinition(node);

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

    void Graph::SetSubgraph(uint32_t nodeId, const ResourceHandle<Graph>& subgraph) {

        auto node = GetNode(nodeId);
        if (!node)
            return;

        // The ids of the pins belong to the previous subgraph
        node->subgraph = subgraph;
        node->subgraphPins.clear();
        node->definition = nullptr;
        NodeLibrary::UpdateDefinition(*node);

        auto definition = NodeLibrary::Find(*node);
        std::erase_if(links, [&](const Link& link) {
            return (link.toNode == nodeId && !definition->FindInput(link.toPin)) ||
                (link.fromNode == nodeId && !definition->FindOutput(link.fromPin));
            });
        std::erase_if(node->inputValues, [&](const auto& item) { return !definition->FindInput(item.first); });

        UpdateSubgraphNodes();

    }

    Ref<Graph> Graph::ExtractSubgraph(const std::unordered_set<uint32_t>& nodeIds, std::vector<Link>& inputLinks,
        std::vector<Link>& outputLinks) const {

        auto subgraph = CreateRef<Graph>();
        subgraph->nextNodeId = nextNodeId;

        vec2 boundsMin(std::numeric_limits<float>::max()), boundsMax(-std::numeric_limits<float>::max());
        for (const auto& node : nodes) {
            if (!nodeIds.contains(node.id))
                continue;
            subgraph->nodes.push_back(node);
            boundsMin = glm::min(boundsMin, node.position);
            boundsMax = glm::max(boundsMax, node.position);
        }

        std::unordered_set<std::string> inputNames, outputNames;
        auto addPin = [&](const std::string& type, std::string name, ValueType pinType, std::unordered_set<std::string>& names) {
            auto uniqueName = name;
            for (int32_t index = 2; !names.insert(uniqueName).second; index++)
                uniqueName = name + " " + std::to_string(index);

            auto isInput = type == NodeLibrary::subgraphInputType;
            auto position = vec2(isInput ? boundsMin.x - 300.0f : boundsMax.x + 300.0f, boundsMin.y + float(names.size() - 1) * 120.0f);
            auto& pin = subgraph->AddNode(type, position);
            pin.name = uniqueName;
            pin.pinType = pinType;
            pin.order = int32_t(names.size());
            NodeLibrary::UpdateDefinition(pin);
            return pin.id;
            };

        // Pins called value say nothing without their node
        auto getPinName = [&](const Node& node, const std::string& pin) {
            auto isUnclear = pin == "Value" || pin == "Result" || pin.size() == 1;
            return isUnclear ? (node.name.empty() ? node.type : node.name) : pin;
            };

        // Several links from the same output share their pin
        std::unordered_map<std::string, uint32_t> pins;
        for (const auto& link : links) {
            auto fromInside = nodeIds.contains(link.fromNode);
            auto toInside = nodeIds.contains(link.toNode);
            auto source = std::to_string(link.fromNode) + "/" + link.fromPin;

            if (fromInside && toInside) {
                subgraph->links.push_back(link);
            }
            else if (toInside) {
                if (!pins.contains(source)) {
                    // The name of a parameter tells more than the pin it is connected to
                    auto from = GetNode(link.fromNode);
                    auto isNamed = from && NodeLibrary::Find(*from) && NodeLibrary::Find(*from)->kind == NodeKind::Parameter;
                    auto name = isNamed ? from->name : getPinName(*GetNode(link.toNode), link.toPin);
                    pins[source] = addPin(NodeLibrary::subgraphInputType, name,
                        Compiler::GetOutputType(*this, link.fromNode, link.fromPin), inputNames);
                    inputLinks.push_back({ link.fromNode, link.fromPin, 0, subgraph->GetNode(pins[source])->name });
                }
                subgraph->links.push_back({ pins[source], "Value", link.toNode, link.toPin });
            }
            else if (fromInside) {
                if (!pins.contains(source)) {
                    // Outputs for a pin of a material are named after it, previews of the subgraph show it there as well
                    auto to = GetNode(link.toNode);
                    auto isMaterialPin = to && (to->type == NodeLibrary::surfaceOutputType || to->type == NodeLibrary::vertexOutputType);
                    pins[source] = addPin(NodeLibrary::subgraphOutputType, isMaterialPin ? link.toPin : getPinName(*GetNode(link.fromNode), link.fromPin),
                        Compiler::GetOutputType(*this, link.fromNode, link.fromPin), outputNames);
                    subgraph->links.push_back({ link.fromNode, link.fromPin, pins[source], "Value" });
                }
                outputLinks.push_back({ 0, subgraph->GetNode(pins[source])->name, link.toNode, link.toPin });
            }
        }

        subgraph->Compile();

        return subgraph;

    }

    uint32_t Graph::ReplaceWithSubgraph(const std::unordered_set<uint32_t>& nodeIds, const ResourceHandle<Graph>& subgraph,
        const std::vector<Link>& inputLinks, const std::vector<Link>& outputLinks) {

        vec2 center(0.0f);
        for (auto id : nodeIds) {
            if (auto node = GetNode(id))
                center += node->position / float(nodeIds.size());
        }

        for (auto id : nodeIds)
            RemoveNode(id);

        auto nodeId = AddNode(NodeLibrary::subgraphType, center).id;
        SetSubgraph(nodeId, subgraph);

        for (const auto& link : inputLinks)
            Connect(link.fromNode, link.fromPin, nodeId, link.toPin);
        for (const auto& link : outputLinks)
            Connect(nodeId, link.fromPin, link.toNode, link.toPin);

        return nodeId;

    }

    std::vector<uint32_t> Graph::DissolveSubgraph(uint32_t nodeId) {

        auto node = GetNode(nodeId);
        if (!node)
            return {};

        auto position = node->position;
        auto inlined = Compiler::InlineSubgraph(*this, nodeId);
        if (!inlined.errors.empty())
            return {};

        vec2 center(0.0f);
        for (auto id : inlined.nodes)
            center += GetNode(id)->position / float(inlined.nodes.size());

        // The types while all pins are there, generic values depend on them
        std::unordered_map<uint32_t, ValueType> sourceTypes;
        for (auto id : inlined.nodes) {
            GetNode(id)->position += position - center;
            if (auto link = GetInputLink(id, NodeLibrary::subgraphDefaultPin))
                sourceTypes[id] = Compiler::GetOutputType(*this, link->fromNode, link->fromPin);
        }

        for (auto id : inlined.nodes) {
            if (GetNode(id)->type == NodeLibrary::subgraphInputType)
                DissolvePin(id, sourceTypes);
        }

        std::erase_if(inlined.nodes, [&](uint32_t id) { return GetNode(id) == nullptr; });

        return inlined.nodes;

    }

    void Graph::DissolvePin(uint32_t pinId, const std::unordered_map<uint32_t, ValueType>& sourceTypes) {

        const auto pin = *GetNode(pinId);

        auto isUsed = std::any_of(links.begin(), links.end(), [&](const Link& link) { return link.fromNode == pinId; });
        auto sourceLink = GetInputLink(pinId, NodeLibrary::subgraphDefaultPin);

        auto replaceSource = [&](uint32_t fromNode, const std::string& fromPin) {
            for (auto& link : links) {
                if (link.fromNode == pinId) {
                    link.fromNode = fromNode;
                    link.fromPin = fromPin;
                }
            }
            };

        if (sourceLink && sourceTypes.at(pinId) == pin.pinType) {
            const auto source = *sourceLink;
            replaceSource(source.fromNode, source.fromPin);
            RemoveNode(pinId);
            return;
        }
        if (!isUsed || (!sourceLink && pin.pinType == ValueType::Texture)) {
            RemoveNode(pinId);
            return;
        }

        Node replacement = { .id = pinId, .position = pin.position };
        if (sourceLink) {
            // Code converts the value like the pin did
            replacement.type = NodeLibrary::codeType;
            replacement.code = "return value;";
            replacement.codeInputs = { { "value", pin.pinType } };
            replacement.codeOutputType = pin.pinType;
            std::find_if(links.begin(), links.end(), [&](const Link& link) { return link.toNode == pinId; })->toPin = "value";
            replaceSource(pinId, "Result");
        }
        else {
            static const std::string constants[] = { "Float", "Vector 2", "Vector 3", "Vector 4" };
            replacement.type = constants[int32_t(pin.pinType)];
            auto value = pin.inputValues.find(NodeLibrary::subgraphDefaultPin);
            replacement.value = value != pin.inputValues.end() ? value->second : vec4(0.0f);
            replaceSource(pinId, "Value");
        }
        NodeLibrary::UpdateDefinition(replacement);
        *GetNode(pinId) = replacement;

    }

    bool Graph::Uses(const Graph& subgraph) const {

        std::unordered_set<const Graph*> visited;
        std::vector<const Graph*> pending = { this };
        while (!pending.empty()) {
            auto graph = pending.back();
            pending.pop_back();

            for (const auto& node : graph->nodes) {
                if (!node.subgraph.IsLoaded())
                    continue;
                auto used = node.subgraph.Get().get();
                if (used == &subgraph)
                    return true;
                if (visited.insert(used).second)
                    pending.push_back(used);
            }
        }

        return false;

    }

    std::vector<ResourceHandle<Graph>> Graph::CompileUsers() const {

        std::vector<ResourceHandle<Graph>> users, materialGraphs;
        for (const auto& graph : ResourceManager<Graph>::GetResources()) {
            if (!graph.IsLoaded() || graph.Get().get() == this || !graph->Uses(*this))
                continue;
            auto compiled = graph->GetCompiled();
            (compiled && compiled->IsSubgraph() ? users : materialGraphs).push_back(graph);
        }

        // Graphs read the compiled state of their subgraphs, so these come first. Nothing uses the graphs of materials
        for (size_t i = 0; i < users.size(); i++) {
            auto next = std::find_if(users.begin() + i, users.end(), [&](const auto& candidate) {
                return std::none_of(users.begin() + i, users.end(), [&](const auto& other) {
                    return other.Get() != candidate.Get() && candidate->Uses(*other.Get());
                    });
                });
            if (next != users.end())
                std::swap(users[i], *next);
        }

        users.insert(users.end(), materialGraphs.begin(), materialGraphs.end());
        for (const auto& user : users)
            user.Get()->Compile();

        return users;

    }

    vec4 Graph::GetSurfaceValue(const std::string& pin) const {

        auto definition = NodeLibrary::Find(NodeLibrary::surfaceOutputType);
        auto surface = std::find_if(nodes.begin(), nodes.end(),
            [](const Node& node) { return node.type == NodeLibrary::surfaceOutputType; });
        if (surface != nodes.end()) {
            auto iter = surface->inputValues.find(pin);
            if (iter != surface->inputValues.end())
                return iter->second;
        }
        return definition->FindInput(pin)->defaultValue;

    }

    void Graph::UpdateSubgraphNodes() {

        for (auto& node : nodes) {
            if (node.type != NodeLibrary::subgraphType)
                continue;

            // Pins are only followed between valid states, names might exist twice meanwhile
            auto compiled = node.subgraph.IsLoaded() ? node.subgraph->GetCompiled() : nullptr;
            if (compiled && compiled->IsSubgraph() && compiled->valid)
                UpdateSubgraphPins(node, *compiled);

            // Links to pins the subgraph doesn't have anymore stay visible, such that they can be removed
            auto updated = node;
            NodeLibrary::UpdateDefinition(updated);
            auto definition = CreateRef<NodeDefinition>(*NodeLibrary::Find(updated));
            for (const auto& link : links) {
                if (link.toNode == node.id && !definition->FindInput(link.toPin))
                    definition->inputs.push_back({ .name = link.toPin, .type = ValueType::Generic });
                if (link.fromNode == node.id && !definition->FindOutput(link.fromPin))
                    definition->outputs.push_back({ .name = link.fromPin, .type = ValueType::Generic });
            }
            // Compiling a graph without any change doesn't write to it, other threads might read it
            if (!node.definition || node.definition->inputs != definition->inputs || node.definition->outputs != definition->outputs)
                node.definition = definition;
        }

    }

    void Graph::UpdateSubgraphPins(Node& node, const CompiledGraph& compiledSubgraph) {

        std::vector<SubgraphPin> pins;
        std::unordered_map<std::string, std::string> inputNames, outputNames;
        for (const auto& pinNode : compiledSubgraph.subgraphNodes) {
            auto isInput = pinNode.type == NodeLibrary::subgraphInputType;
            if (!isInput && pinNode.type != NodeLibrary::subgraphOutputType)
                continue;

            pins.push_back({ pinNode.id, pinNode.name });
            auto known = std::find_if(node.subgraphPins.begin(), node.subgraphPins.end(),
                [&](const SubgraphPin& pin) { return pin.id == pinNode.id; });
            if (known != node.subgraphPins.end() && known->name != pinNode.name)
                (isInput ? inputNames : outputNames)[known->name] = pinNode.name;
        }
        if (node.subgraphPins != pins)
            node.subgraphPins = std::move(pins);

        if (inputNames.empty() && outputNames.empty())
            return;

        // A pin which got the name of a removed one replaces its links and its value
        auto isReplaced = [](const auto& names, const std::string& name) {
            return !names.contains(name) && std::any_of(names.begin(), names.end(), [&](const auto& item) { return item.second == name; });
            };
        std::erase_if(links, [&](const Link& link) {
            return (link.toNode == node.id && isReplaced(inputNames, link.toPin)) ||
                (link.fromNode == node.id && isReplaced(outputNames, link.fromPin));
            });
        std::erase_if(node.inputValues, [&](const auto& item) { return isReplaced(inputNames, item.first); });

        for (auto& link : links) {
            if (link.toNode == node.id && inputNames.contains(link.toPin))
                link.toPin = inputNames[link.toPin];
            if (link.fromNode == node.id && outputNames.contains(link.fromPin))
                link.fromPin = outputNames[link.fromPin];
        }

        std::unordered_map<std::string, vec4> values;
        for (const auto& [name, value] : node.inputValues)
            values[inputNames.contains(name) ? inputNames[name] : name] = value;
        node.inputValues = std::move(values);

    }

    Ref<const CompiledGraph> Graph::Compile() {

        UpdateSubgraphNodes();

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
