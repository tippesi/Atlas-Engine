#include "MaterialGraphBindings.h"
#include "ResourceBindings.h"

#include "Log.h"
#include "common/StringHelper.h"
#include "materialgraph/MaterialGraph.h"
#include "materialgraph/NodeLibrary.h"

#include <stdexcept>

namespace Atlas::Scripting::Bindings {

    using MaterialGraph::Graph;
    using MaterialGraph::Node;

    // Sol turns the exceptions of the functions into Lua errors, the script fails with the message
    static Node& GetGraphNode(Graph& graph, uint32_t id) {

        auto node = graph.GetNode(id);
        if (!node)
            throw std::runtime_error("The material graph " + graph.name + " has no node " + std::to_string(id));
        return *node;

    }

    static const MaterialGraph::NodeDefinition& GetNodeDefinition(const Node& node) {

        auto definition = MaterialGraph::NodeLibrary::Find(node);
        if (!definition)
            throw std::runtime_error("Node " + std::to_string(node.id) + " has the unknown type " + node.type);
        return *definition;

    }

    // Changes a node and counts up the revision
    template<class Change>
    static void ChangeGraphNode(Graph& graph, uint32_t id, Change change) {

        change(GetGraphNode(graph, id));
        graph.revision++;

    }

    template<class Change>
    static void ChangeGraphInput(Graph& graph, uint32_t id, const std::string& pin, Change change) {

        auto& node = GetGraphNode(graph, id);
        if (!GetNodeDefinition(node).FindInput(pin))
            throw std::runtime_error("Node " + std::to_string(id) + " (" + node.type + ") has no input " + pin);
        change(node);
        graph.revision++;

    }

    void GenerateMaterialGraphBindings(Binder* ns) {

        // Nodes are addressed by their id and pins by their names in the node library, the ids stay valid while nodes
        // are added. Changes count up the revision, Compile() applies them to the materials
        auto graphType = ns->new_usertype<Graph>("MaterialGraph", sol::no_constructor,
            "name", &Graph::name,
            "maxDisplacement", &Graph::maxDisplacement,
            "blendMode", &Graph::blendMode,
            "twoSided", &Graph::twoSided,
            "revision", sol::readonly(&Graph::revision),
            "GetNodes", [](Graph& graph, sol::this_state state) {
                sol::state_view lua(state);
                sol::table nodes = lua.create_table();
                for (const auto& node : graph.nodes)
                    nodes.add(lua.create_table_with("id", node.id, "type", node.type, "name", node.name,
                        "position", node.position, "value", node.value, "code", node.code));
                return nodes;
            },
            "GetLinks", [](Graph& graph, sol::this_state state) {
                sol::state_view lua(state);
                sol::table links = lua.create_table();
                for (const auto& link : graph.links)
                    links.add(lua.create_table_with("from", link.fromNode, "fromPin", link.fromPin, "to", link.toNode,
                        "toPin", link.toPin));
                return links;
            },
            // Ids of the nodes whose type or name matches, e.g. "Fractal noise" or "Glow*"
            "FindNodes", [](Graph& graph, const std::string& pattern) {
                std::vector<uint32_t> ids;
                for (const auto& node : graph.nodes)
                    if (Helper::String::MatchWildcard(pattern, node.type) || Helper::String::MatchWildcard(pattern, node.name))
                        ids.push_back(node.id);
                return sol::as_table(ids);
            },
            // The id of the new node
            "AddNode", [](Graph& graph, const std::string& type, sol::optional<vec2> position) {
                if (!MaterialGraph::NodeLibrary::Find(type))
                    throw std::runtime_error("Unknown material graph node type " + type + ", Atlas.MaterialGraph.GetNodeTypes() lists them");
                auto id = graph.AddNode(type, position.value_or(vec2(0.0f))).id;
                graph.revision++;
                return id;
            },
            "RemoveNode", [](Graph& graph, uint32_t id) {
                GetGraphNode(graph, id);
                graph.RemoveNode(id);
                graph.revision++;
            },
            "Connect", [](Graph& graph, uint32_t fromNode, const std::string& fromPin, uint32_t toNode, const std::string& toPin) {
                const auto& from = GetGraphNode(graph, fromNode);
                const auto& to = GetGraphNode(graph, toNode);
                if (fromNode == toNode)
                    throw std::runtime_error("Node " + std::to_string(fromNode) + " can't be connected to itself");
                if (!GetNodeDefinition(from).FindOutput(fromPin))
                    throw std::runtime_error("Node " + std::to_string(fromNode) + " (" + from.type + ") has no output " + fromPin);
                if (!GetNodeDefinition(to).FindInput(toPin))
                    throw std::runtime_error("Node " + std::to_string(toNode) + " (" + to.type + ") has no input " + toPin);
                if (!graph.Connect(fromNode, fromPin, toNode, toPin))
                    throw std::runtime_error("Textures only connect to texture inputs, " + fromPin + " can't connect to " + toPin);
                graph.revision++;
            },
            "Disconnect", [](Graph& graph, uint32_t toNode, const std::string& toPin) {
                GetGraphNode(graph, toNode);
                graph.Disconnect(toNode, toPin);
                graph.revision++;
            },
            // The value of an input without a link
            "SetInput", sol::overload(
                [](Graph& graph, uint32_t id, const std::string& pin, float value) {
                    ChangeGraphInput(graph, id, pin, [&](Node& node) { node.inputValues[pin] = vec4(value); });
                },
                [](Graph& graph, uint32_t id, const std::string& pin, const vec4& value) {
                    ChangeGraphInput(graph, id, pin, [&](Node& node) { node.inputValues[pin] = value; });
                },
                [](Graph& graph, uint32_t id, const std::string& pin, const vec3& value) {
                    ChangeGraphInput(graph, id, pin, [&](Node& node) { node.inputValues[pin] = vec4(value, 0.0f); });
                },
                [](Graph& graph, uint32_t id, const std::string& pin, const vec2& value) {
                    ChangeGraphInput(graph, id, pin, [&](Node& node) { node.inputValues[pin] = vec4(value, 0.0f, 0.0f); });
                }),
            // Constants and the defaults of parameters
            "SetValue", sol::overload(
                [](Graph& graph, uint32_t id, float value) {
                    ChangeGraphNode(graph, id, [&](Node& node) { node.value = vec4(value); });
                },
                [](Graph& graph, uint32_t id, const vec4& value) {
                    ChangeGraphNode(graph, id, [&](Node& node) { node.value = value; });
                },
                [](Graph& graph, uint32_t id, const vec3& value) {
                    ChangeGraphNode(graph, id, [&](Node& node) { node.value = vec4(value, 0.0f); });
                },
                [](Graph& graph, uint32_t id, const vec2& value) {
                    ChangeGraphNode(graph, id, [&](Node& node) { node.value = vec4(value, 0.0f, 0.0f); });
                }),
            // The name of a parameter or a pin of a subgraph, the text of a comment
            "SetName", [](Graph& graph, uint32_t id, const std::string& name) {
                ChangeGraphNode(graph, id, [&](Node& node) { node.name = name; });
            },
            "SetPosition", [](Graph& graph, uint32_t id, const vec2& position) {
                ChangeGraphNode(graph, id, [&](Node& node) { node.position = position; });
            },
            // The body of a code node, its pins follow the code
            "SetCode", [](Graph& graph, uint32_t id, const std::string& code) {
                ChangeGraphNode(graph, id, [&](Node& node) {
                    node.code = code;
                    MaterialGraph::NodeLibrary::UpdateDefinition(node);
                    });
            },
            // The graph of a subgraph node
            "SetSubgraph", [](Graph& graph, uint32_t id, const ResourceHandle<Graph>& subgraph) {
                if (GetGraphNode(graph, id).type != MaterialGraph::NodeLibrary::subgraphType)
                    throw std::runtime_error("Node " + std::to_string(id) + " isn't a subgraph node");
                if (subgraph.IsLoaded() && (subgraph.Get().get() == &graph || subgraph->Uses(graph)))
                    throw std::runtime_error("The subgraph would contain itself");
                graph.SetSubgraph(id, subgraph);
                graph.revision++;
            },
            // The type of a subgraph input or output: float, vec2, vec3, vec4 or texture. Its name is set by SetName
            "SetPinType", [](Graph& graph, uint32_t id, const std::string& type) {
                auto pinType = MaterialGraph::NodeLibrary::FindType(type);
                if (pinType == MaterialGraph::ValueType::Generic)
                    throw std::runtime_error("Unknown type " + type + ", use float, vec2, vec3, vec4 or texture");
                const auto& nodeType = GetGraphNode(graph, id).type;
                if (nodeType != MaterialGraph::NodeLibrary::subgraphInputType && nodeType != MaterialGraph::NodeLibrary::subgraphOutputType)
                    throw std::runtime_error("Node " + std::to_string(id) + " isn't an input or output of a subgraph");
                ChangeGraphNode(graph, id, [&](Node& node) {
                    node.pinType = pinType;
                    MaterialGraph::NodeLibrary::UpdateDefinition(node);
                    });
            },
            // Generates the shaders of the graph and of loaded graphs using it, returns the errors as { node, message }
            "Compile", [](Graph& graph, sol::this_state state) {
                auto compiled = graph.Compile();
                if (compiled->IsSubgraph())
                    graph.CompileUsers();
                sol::state_view lua(state);
                sol::table errors = lua.create_table();
                for (const auto& error : compiled->errors)
                    errors.add(lua.create_table_with("node", error.nodeId, "message", error.message));
                return errors;
            }
            );

        graphType.set_function("GetNodeTypes", []() {
            std::vector<std::string> types;
            for (const auto& definition : MaterialGraph::NodeLibrary::GetDefinitions())
                types.push_back(definition.type);
            return sol::as_table(types);
            });

        GenerateResourceBinding<Graph>(ns, "MaterialGraphResourceHandle");

    }

}
