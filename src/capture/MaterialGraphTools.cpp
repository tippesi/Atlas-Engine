#include "MaterialGraphTools.h"
#include "SceneFile.h"

#include "loader/AssetReferences.h"
#include "materialgraph/MaterialGraphSerializer.h"
#include "materialgraph/NodeLibrary.h"
#include "resource/ResourceManager.h"
#include "streaming/TextureStreaming.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <fstream>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace Atlas::Capture {

    using namespace MaterialGraph;
    using Loader::AssetReferences;

    // Without trailing zeros, e.g. 8 or 0.25
    static std::string FormatNumber(float value) {

        auto text = FormatFloat(value, 3);
        text.erase(text.find_last_not_of('0') + 1);
        if (text.ends_with('.'))
            text.pop_back();
        return text == "-0" ? "0" : text;

    }

    static std::string GetTypeName(ValueType type, bool isColor = false) {

        return isColor ? "color" : NodeLibrary::GetTypeName(type);

    }

    // Components a value of the type uses, generic values use all of them
    static int32_t GetComponentCount(ValueType type) {

        if (type == ValueType::Texture)
            return 1;
        return type == ValueType::Generic ? 4 : int32_t(type) + 1;

    }

    static std::string FormatValue(vec4 value, int32_t components) {

        if (components == 1)
            return FormatNumber(value.x);

        std::string text = "(";
        for (int32_t i = 0; i < components; i++)
            text += (i > 0 ? ", " : "") + FormatNumber(value[i]);
        return text + ")";

    }

    static json ToJson(vec4 value, int32_t components) {

        if (components == 1)
            return Round(value.x, 4);

        json array = json::array();
        for (int32_t i = 0; i < components; i++)
            array.push_back(Round(value[i], 4));
        return array;

    }

    // A number sets all components (only the first one without splat), an array the first ones, objects use the keys of
    // the files ({ "x", "y", "z", "w" }). Components which aren't given keep their current value
    static vec4 ToVec4(const json& value, vec4 current, bool splat, int32_t components, const std::string& what) {

        if (value.is_array() && value.size() > size_t(components))
            throw std::runtime_error(what + ": " + std::to_string(value.size()) + " components for a value with " + std::to_string(components));

        if (value.is_number())
            return splat ? vec4(value.get<float>()) : vec4(value.get<float>(), current.y, current.z, current.w);

        if (value.is_array() && !value.empty() && value.size() <= 4 &&
            std::all_of(value.begin(), value.end(), [](const json& component) { return component.is_number(); })) {
            for (size_t i = 0; i < value.size(); i++)
                current[int32_t(i)] = value[i].get<float>();
            return current;
        }

        if (value.is_object()) {
            const std::string names[] = { "x", "y", "z", "w" };
            for (const auto& [key, component] : value.items()) {
                auto index = int32_t(std::find(std::begin(names), std::end(names), key) - std::begin(names));
                if (index >= components || !component.is_number())
                    throw std::runtime_error(what + ": '" + key + "' isn't a number for a component of a value with " +
                        std::to_string(components) + " components");
                current[index] = component.get<float>();
            }
            return current;
        }

        throw std::runtime_error(what + ": expected a number or an array of up to 4 numbers, got " + value.dump());

    }

    static vec2 ToVec2Array(const json& value, const std::string& what) {

        if (!value.is_array() || value.size() != 2 || !value[0].is_number() || !value[1].is_number())
            throw std::runtime_error(what + ": expected [x, y], got " + value.dump());
        return vec2(value[0].get<float>(), value[1].get<float>());

    }

    static std::string Join(const std::vector<std::string>& names) {

        std::string text;
        for (const auto& name : names)
            text += (text.empty() ? "'" : ", '") + name + "'";
        return text.empty() ? "none" : text;

    }

    static std::string GetLabel(const Node& node) {

        return "#" + std::to_string(node.id) + " " + node.type + (node.name.empty() ? "" : " '" + node.name + "'");

    }

    // Comments and frames keep their text in the name, it doesn't name the node
    static bool IsNote(const Node& node) {

        auto definition = NodeLibrary::Find(node);
        return definition && (definition->kind == NodeKind::Comment || definition->kind == NodeKind::Frame);

    }

    static std::vector<std::string> GetPinNames(const NodeDefinition& definition, bool inputs) {

        std::vector<std::string> names;
        if (inputs) {
            for (const auto& input : definition.inputs)
                names.push_back(input.name);
        }
        else {
            for (const auto& output : definition.outputs)
                names.push_back(output.name);
        }
        return names;

    }

    static GraphDocument ReadDocument(const std::string& path) {

        std::ifstream stream(GetAssetDirectory() / path);
        if (!stream.is_open())
            throw std::runtime_error("couldn't open the graph " + path);

        auto j = json::parse(stream);
        AssetReferences::ResolveReferences(j, AssetReferences::Type::MaterialGraph, path);

        GraphDocument document;
        for (auto& node : j.at("nodes")) {
            if (!node.contains("texturePath"))
                continue;
            document.texturePaths[node.at("id").get<uint32_t>()] = node["texturePath"].get<std::string>();
            node.erase("texturePath");
        }

        document.graph = CreateRef<Graph>();
        from_json(j, *document.graph);
        return document;

    }

    static GraphDocument& GetDocument(const std::string& path, std::map<std::string, GraphDocument>& documents) {

        auto iter = documents.find(path);
        if (iter == documents.end())
            iter = documents.emplace(path, ReadDocument(path)).first;
        return iter->second;

    }

    static json SerializeDocument(const GraphDocument& document, const std::string& path) {

        json j;
        to_json(j, *document.graph);
        for (auto& node : j["nodes"]) {
            auto iter = document.texturePaths.find(node["id"].get<uint32_t>());
            if (iter != document.texturePaths.end())
                node["texturePath"] = iter->second;
        }

        AssetReferences::MakeReferences(j, AssetReferences::Type::MaterialGraph, path);
        return j;

    }

    // A #id, a key of the operation, a parameter name or a node type which occurs once
    static Node* FindNode(Graph& graph, const std::map<std::string, uint32_t>& keys, const std::string& reference, std::string& error) {

        if (reference.starts_with("#")) {
            uint32_t id = 0;
            auto result = std::from_chars(reference.data() + 1, reference.data() + reference.size(), id);
            auto node = result.ec == std::errc() ? graph.GetNode(id) : nullptr;
            if (!node)
                error = "no node " + reference;
            return node;
        }

        if (auto iter = keys.find(reference); iter != keys.end()) {
            auto node = graph.GetNode(iter->second);
            if (!node)
                error = "the node '" + reference + "' was removed";
            return node;
        }

        std::vector<Node*> matches;
        for (auto& node : graph.nodes)
            if (node.name == reference && !IsNote(node))
                matches.push_back(&node);
        if (matches.empty()) {
            for (auto& node : graph.nodes)
                if (node.type == reference)
                    matches.push_back(&node);
        }

        if (matches.size() == 1)
            return matches.front();

        if (matches.size() > 1) {
            std::vector<std::string> labels;
            for (auto node : matches)
                labels.push_back(GetLabel(*node));
            error = "'" + reference + "' matches " + std::to_string(matches.size()) + " nodes (" + Join(labels) + "), use a #id or a key";
            return nullptr;
        }

        std::vector<std::string> candidates;
        for (const auto& [key, _] : keys)
            candidates.push_back(key);
        for (const auto& node : graph.nodes) {
            if (!node.name.empty() && !IsNote(node))
                candidates.push_back(node.name);
            candidates.push_back(node.type);
        }
        error = "no node '" + reference + "' (a #id, a key of the operation, a parameter name or a node type which occurs once), closest: " +
            Join(FindClosestNames(candidates, reference));
        return nullptr;

    }

    static Node& GetNode(Graph& graph, const std::map<std::string, uint32_t>& keys, const std::string& reference) {

        std::string error;
        auto node = FindNode(graph, keys, reference, error);
        if (!node)
            throw std::runtime_error(error);
        return *node;

    }

    // "reference" for nodes with one output or "reference.Pin"
    static std::pair<uint32_t, std::string> FindOutput(Graph& graph, const std::map<std::string, uint32_t>& keys, const std::string& text) {

        std::string error;
        auto node = FindNode(graph, keys, text, error);
        std::string pin;
        if (!node) {
            auto dot = text.rfind('.');
            if (dot == std::string::npos)
                throw std::runtime_error(error);
            node = &GetNode(graph, keys, text.substr(0, dot));
            pin = text.substr(dot + 1);
        }

        auto definition = NodeLibrary::Find(*node);
        if (!definition)
            throw std::runtime_error(GetLabel(*node) + " has an unknown type");
        auto outputs = GetPinNames(*definition, false);

        if (pin.empty()) {
            if (outputs.size() != 1)
                throw std::runtime_error(GetLabel(*node) + " has the outputs " + Join(outputs) + ", name one, e.g. '" + text + "." +
                    (outputs.empty() ? "" : outputs.front()) + "'");
            pin = outputs.front();
        }
        else if (!definition->FindOutput(pin)) {
            throw std::runtime_error(GetLabel(*node) + " has no output '" + pin + "', its outputs: " + Join(outputs));
        }

        return { node->id, pin };

    }

    static void ApplyNodeEntry(Graph& graph, GraphDocument& document, const std::map<std::string, uint32_t>& keys, uint32_t id, const json& entry) {

        static const std::set<std::string> known = { "key", "type", "node", "remove", "name", "value", "position", "size", "range", "group",
            "order", "texture", "inputs", "code", "codeInputs", "codeOutputType" };
        CheckKeys(entry, known, "materialGraph node " + entry.dump());

        auto node = graph.GetNode(id);
        auto definition = NodeLibrary::Find(*node);
        auto label = GetLabel(*node);

        if (entry.contains("name"))
            node->name = entry["name"].get<std::string>();
        if (entry.contains("value")) {
            // Texture parameters keep their coordinate scale in x and the fallback in y, the vertex output its settings
            auto isValue = definition && (definition->kind == NodeKind::Constant || definition->kind == NodeKind::Parameter);
            auto splat = isValue && definition->valueType != ValueType::Texture;
            auto components = 4;
            if (isValue)
                components = definition->valueType == ValueType::Texture ? 2 : GetComponentCount(definition->valueType);
            node->value = ToVec4(entry["value"], node->value, splat, components, label + " value");
        }
        if (entry.contains("position"))
            node->position = ToVec2Array(entry["position"], label + " position");
        if (entry.contains("size"))
            node->size = ToVec2Array(entry["size"], label + " size");
        if (entry.contains("range"))
            node->range = ToVec2Array(entry["range"], label + " range");
        if (entry.contains("group"))
            node->group = entry["group"].get<std::string>();
        if (entry.contains("order"))
            node->order = entry["order"].get<int32_t>();

        if (entry.contains("code") || entry.contains("codeInputs") || entry.contains("codeOutputType")) {
            if (node->type != NodeLibrary::codeType)
                throw std::runtime_error(label + " isn't a code node, only those have code");

            auto checkType = [&](ValueType type, const std::string& what) {
                if (type == ValueType::Texture || type == ValueType::Generic)
                    throw std::runtime_error(label + ": " + what + " needs one of the types float, vec2, vec3, vec4");
                };

            if (entry.contains("code"))
                node->code = entry["code"].get<std::string>();
            if (entry.contains("codeOutputType")) {
                node->codeOutputType = NodeLibrary::FindType(entry["codeOutputType"].get<std::string>());
                checkType(node->codeOutputType, "the result");
            }
            if (entry.contains("codeInputs")) {
                // The format of graph files, [["name", "vec3"], ...] in the order of the arguments
                node->codeInputs = entry["codeInputs"].get<std::vector<CodeInput>>();
                for (const auto& input : node->codeInputs)
                    checkType(input.type, "the input " + input.name);
            }
            NodeLibrary::UpdateCodeDefinition(*node);
            definition = NodeLibrary::Find(*node);
        }

        if (entry.contains("texture")) {
            if (!definition || definition->valueType != ValueType::Texture)
                throw std::runtime_error(label + " has no texture, only texture parameters have one");
            if (entry["texture"].is_null()) {
                document.texturePaths.erase(id);
            }
            else {
                auto path = ToAssetPath(entry["texture"].get<std::string>());
                if (!fs::exists(GetAssetDirectory() / path))
                    throw std::runtime_error(label + ": no texture file " + path);
                document.texturePaths[id] = path;
            }
        }

        if (!entry.contains("inputs"))
            return;

        for (const auto& [pin, value] : entry["inputs"].items()) {
            node = graph.GetNode(id);
            auto input = definition ? definition->FindInput(pin) : nullptr;
            if (!input)
                throw std::runtime_error(label + " has no input '" + pin + "', its inputs: " +
                    (definition ? Join(GetPinNames(*definition, true)) : "unknown"));

            if (value.is_string()) {
                auto [fromId, fromPin] = FindOutput(graph, keys, value.get<std::string>());
                auto from = graph.GetNode(fromId);
                if (!graph.Connect(fromId, fromPin, id, pin)) {
                    auto output = NodeLibrary::Find(*from)->FindOutput(fromPin);
                    auto reason = fromId == id ? "a node can't use its own output" : (output->type == ValueType::Texture) != (input->type == ValueType::Texture) ?
                        "textures only connect to textures" : "the link isn't valid";
                    throw std::runtime_error("can't connect " + GetLabel(*from) + "." + fromPin + " to " + label + "." + pin + ": " + reason);
                }
                continue;
            }

            graph.Disconnect(id, pin);
            if (value.is_null()) {
                node->inputValues.erase(pin);
                continue;
            }
            if (input->type == ValueType::Texture)
                throw std::runtime_error(label + "." + pin + " is a texture input, it only takes a link");
            auto current = node->inputValues.contains(pin) ? node->inputValues[pin] : input->defaultValue;
            node->inputValues[pin] = ToVec4(value, current, true, GetComponentCount(input->type), label + "." + pin);
        }

    }

    // New nodes without a position go into the column of their way to the outputs (like Arrange in the editor), below
    // the nodes already there
    static void PlaceNodes(Graph& graph, const std::vector<uint32_t>& unplaced) {

        std::set<uint32_t> pending(unplaced.begin(), unplaced.end());
        if (pending.empty())
            return;

        std::unordered_map<uint32_t, int32_t> depths;
        for (const auto& node : graph.nodes)
            depths[node.id] = 0;
        for (size_t i = 0; i < graph.nodes.size(); i++) {
            auto changed = false;
            for (const auto& link : graph.links) {
                auto depth = depths[link.toNode] + 1;
                if (depth > depths[link.fromNode]) {
                    depths[link.fromNode] = depth;
                    changed = true;
                }
            }
            if (!changed)
                break;
        }

        auto right = 0.0f;
        auto hasPlaced = false;
        for (const auto& node : graph.nodes) {
            if (pending.contains(node.id) || IsNote(node))
                continue;
            right = hasPlaced ? std::max(right, node.position.x) : node.position.x;
            hasPlaced = true;
        }

        const float columnSpacing = 310.0f;
        const float nodeHeight = 240.0f;
        for (auto id : unplaced) {
            auto node = graph.GetNode(id);
            if (!node)
                continue;

            auto x = right - float(depths[id]) * columnSpacing;
            auto y = 0.0f;
            auto hasNeighbour = false;
            for (const auto& other : graph.nodes) {
                if (pending.contains(other.id) || IsNote(other) || std::abs(other.position.x - x) >= 0.5f * columnSpacing)
                    continue;
                y = hasNeighbour ? std::max(y, other.position.y + nodeHeight) : other.position.y + nodeHeight;
                hasNeighbour = true;
            }
            node->position = vec2(x, y);
            pending.erase(id);
        }

    }

    static json DescribeParameters(const CompiledGraph& compiled) {

        json parameters = json::array();
        for (const auto& parameter : compiled.parameters) {
            json description = { { "name", parameter.name }, { "type", GetTypeName(parameter.type, parameter.isColor) } };
            if (parameter.type != ValueType::Texture)
                description["default"] = ToJson(parameter.defaultValue, GetComponentCount(parameter.type));
            if (parameter.HasRange())
                description["range"] = { parameter.range.x, parameter.range.y };
            if (!parameter.group.empty())
                description["group"] = parameter.group;
            parameters.push_back(description);
        }
        return parameters;

    }

    static json DescribeCompiled(const CompiledGraph& compiled) {

        return { { "vertexOffset", compiled.hasVertexOffset }, { "surface", compiled.hasSurface }, { "opacity", compiled.hasOpacity },
            { "uniformOpacity", compiled.hasUniformOpacity }, { "writesNormal", compiled.writesNormal }, { "shadowPipeline", compiled.UsesShadowPipeline() } };

    }

    static std::string FormatErrors(const Graph& graph, const CompiledGraph& compiled) {

        std::string text;
        for (const auto& error : compiled.errors) {
            auto node = graph.GetNode(error.nodeId);
            text += "\n  " + (node ? GetLabel(*node) + ": " : std::string()) + error.message;
        }
        return text;

    }

    json EditMaterialGraph(const json& operation, MaterialGraphEdits& edits) {

        CheckKeys(operation, { "op", "graph", "create", "from", "overwrite", "name", "maxDisplacement", "nodes" }, "materialGraph");

        auto path = ToAssetPath(operation.at("graph").get<std::string>());
        if (fs::path(path).extension() != ".aematgraph")
            throw std::runtime_error(path + " isn't a .aematgraph file");

        auto& documents = edits.documents;
        auto create = operation.value("create", false) || operation.contains("from");
        auto exists = documents.contains(path) || fs::exists(GetAssetDirectory() / path);
        if (create && exists && !operation.value("overwrite", false))
            throw std::runtime_error(path + " already exists, set \"overwrite\": true or leave out \"create\"");
        if (!create && !exists)
            throw std::runtime_error(path + " doesn't exist, set \"create\": true");

        // Edits a copy, a failing operation leaves the document as it was
        GraphDocument document;
        if (operation.contains("from")) {
            document = GetDocument(ToAssetPath(operation["from"].get<std::string>()), documents);
            document.graph = CreateRef<Graph>(*document.graph);
            document.graph->name = fs::path(path).stem().string();
        }
        else if (create) {
            document.graph = Graph::CreateDefault();
            document.graph->name = fs::path(path).stem().string();
        }
        else {
            document = GetDocument(path, documents);
            document.graph = CreateRef<Graph>(*document.graph);
        }

        auto& graph = *document.graph;
        if (operation.contains("name"))
            graph.name = operation["name"].get<std::string>();
        if (operation.contains("maxDisplacement"))
            graph.maxDisplacement = operation["maxDisplacement"].get<float>();

        // Nodes are added and found first, links may point to nodes of later entries
        auto entries = operation.value("nodes", json::array());
        std::map<std::string, uint32_t> keys;
        std::vector<uint32_t> entryNodes, added, unplaced;
        for (const auto& entry : entries) {
            if (entry.contains("type") == entry.contains("node"))
                throw std::runtime_error("a node entry has either \"type\" (adds a node) or \"node\" (changes one): " + entry.dump());

            uint32_t id = 0;
            if (entry.contains("type")) {
                auto type = entry["type"].get<std::string>();
                if (!NodeLibrary::Find(type)) {
                    std::vector<std::string> types;
                    for (const auto& definition : NodeLibrary::GetDefinitions())
                        types.push_back(definition.type);
                    throw std::runtime_error("unknown node type '" + type + "', closest: " + Join(FindClosestNames(types, type)) +
                        ". 'inspect nodes' lists all of them");
                }
                id = graph.AddNode(type).id;
                added.push_back(id);
                if (!entry.contains("position"))
                    unplaced.push_back(id);
            }
            else {
                id = GetNode(graph, keys, entry["node"].get<std::string>()).id;
            }

            if (entry.contains("key")) {
                auto key = entry["key"].get<std::string>();
                if (!keys.emplace(key, id).second)
                    throw std::runtime_error("the key '" + key + "' is used twice");
            }
            entryNodes.push_back(id);
        }

        int32_t removed = 0;
        for (size_t i = 0; i < entries.size(); i++) {
            auto id = entryNodes[i];
            if (!graph.GetNode(id))
                throw std::runtime_error("the node of " + entries[i].dump() + " was removed by an earlier entry");

            if (entries[i].value("remove", false)) {
                graph.RemoveNode(id);
                document.texturePaths.erase(id);
                removed++;
                continue;
            }
            ApplyNodeEntry(graph, document, keys, id, entries[i]);
        }
        PlaceNodes(graph, unplaced);

        auto compiled = graph.Compile();
        if (!compiled->errors.empty())
            throw std::runtime_error(path + " doesn't compile:" + FormatErrors(graph, *compiled));

        documents[path] = document;
        edits.changed.insert(path);

        json nodes = json::object();
        for (const auto& [key, id] : keys)
            nodes[key] = id;

        return { { "graph", path }, { "created", create }, { "nodes", nodes }, { "added", added.size() }, { "removed", removed },
            { "parameters", DescribeParameters(*compiled) }, { "compiled", DescribeCompiled(*compiled) } };

    }

    json SaveMaterialGraphs(MaterialGraphEdits& edits) {

        json saved = json::array();
        for (const auto& path : edits.changed) {
            auto file = GetAssetDirectory() / path;
            json entry = { { "graph", path } };
            if (fs::exists(file)) {
                auto backup = file;
                backup += ".backup";
                if (!fs::exists(backup))
                    fs::copy_file(file, backup);
                entry["backup"] = backup.string();
            }

            fs::create_directories(file.parent_path());
            if (!WriteFileAtomic(file, SerializeDocument(edits.documents.at(path), path).dump(2)))
                throw std::runtime_error("Couldn't write the material graph " + path);
            saved.push_back(entry);
        }
        edits.changed.clear();

        return saved;

    }

    Ref<Graph> CreateGraph(const GraphDocument& document) {

        // Parameters take their default textures from the nodes, so the graph is compiled with them
        auto graph = CreateRef<Graph>(*document.graph);
        for (const auto& [id, path] : document.texturePaths) {
            if (auto node = graph->GetNode(id))
                node->texture = ResourceManager<Texture::Texture2D>::GetOrLoadResourceWithLoaderAsync(path,
                    Streaming::TextureStreaming::LoadTexture, false, Texture::Wrapping::Repeat, Texture::Filtering::Anisotropic);
        }
        graph->Compile();

        return graph;

    }

    std::vector<std::string> FindMaterialFiles(const json& patterns) {

        auto paths = FindAssetFiles(patterns, ".aematerial");
        if (paths.empty())
            throw std::runtime_error("no material matches " + patterns.dump());
        return paths;

    }

    void CheckAssignMaterialGraph(const json& operation) {

        CheckKeys(operation, { "op", "materials", "graph", "values", "textures", "reset" }, "assignMaterialGraph");
        if (operation.contains("graph") && operation["graph"].is_null() && (operation.contains("values") || operation.contains("textures")))
            throw std::runtime_error("values and textures need a graph, \"graph\" is null");

    }

    static Ref<const CompiledGraph> GetCompiledGraph(const std::string& path, MaterialGraphEdits& edits) {

        auto& document = GetDocument(path, edits.documents);
        auto compiled = document.graph->GetCompiled();
        return compiled ? compiled : document.graph->Compile();

    }

    json ToParameterJson(const Parameter& parameter, vec4 value) {

        return ToJson(value, GetComponentCount(parameter.type));

    }

    const Parameter* FindParameter(const CompiledGraph& compiled, const std::string& name) {

        auto iter = std::find_if(compiled.parameters.begin(), compiled.parameters.end(), [&](const Parameter& parameter) {
            return parameter.name == name; });
        return iter != compiled.parameters.end() ? &*iter : nullptr;

    }

    vec4 ToParameterValue(const Parameter& parameter, const json& value, vec4 current) {

        if (parameter.type == ValueType::Texture)
            throw std::runtime_error("'" + parameter.name + "' is a texture parameter, set it with \"textures\"");
        return ToVec4(value, current, true, GetComponentCount(parameter.type), "the value of '" + parameter.name + "'");

    }

    void CheckGraphValues(const json& operation, const CompiledGraph& compiled, const std::string& graphPath) {

        auto find = [&](const std::string& name) -> const Parameter& {

            auto parameter = FindParameter(compiled, name);
            if (!parameter) {
                std::vector<std::string> names;
                for (const auto& candidate : compiled.parameters)
                    names.push_back(candidate.name);
                throw std::runtime_error("the graph " + graphPath + " has no parameter '" + name + "', its parameters: " + Join(names));
            }
            return *parameter;

        };

        for (const auto& [name, value] : operation.value("values", json::object()).items()) {
            const auto& parameter = find(name);
            ToParameterValue(parameter, value, parameter.defaultValue);
        }

        for (const auto& [name, value] : operation.value("textures", json::object()).items()) {
            const auto& parameter = find(name);
            if (parameter.type != ValueType::Texture)
                throw std::runtime_error("'" + name + "' isn't a texture parameter, set it with \"values\"");
            if (!value.is_null() && !fs::exists(GetAssetDirectory() / ToAssetPath(value.get<std::string>())))
                throw std::runtime_error("no texture file " + value.get<std::string>() + " for '" + name + "'");
        }

        auto reset = operation.value("reset", json());
        if (reset.is_array()) {
            for (const auto& name : reset)
                find(name.get<std::string>());
        }

    }

    void CheckMaterialGraphOperations(const json& operations, std::vector<std::string>& issues) {

        MaterialGraphEdits edits;
        for (size_t i = 0; i < operations.size(); i++) {
            const auto& operation = operations[i];
            auto type = operation.value("op", std::string());
            if (type != "materialGraph" && type != "assignMaterialGraph")
                continue;

            auto where = "edit operation " + std::to_string(i + 1) + " (" + type + ")";
            try {
                if (type == "materialGraph") {
                    EditMaterialGraph(operation, edits);
                    continue;
                }

                CheckAssignMaterialGraph(operation);
                auto patterns = ToAssetPatterns(operation.at("materials"));
                auto missingDirectory = std::any_of(patterns.begin(), patterns.end(), [](const std::string& pattern) {
                    return !fs::is_directory(GetAssetDirectory() / fs::path(pattern).parent_path()); });
                auto materials = missingDirectory ? std::vector<std::string>() : FindAssetFiles(patterns, ".aematerial");
                if (materials.empty()) {
                    issues.push_back("NOTE: " + where + ": no material matches " + operation["materials"].dump() + ", earlier operations may create it");
                    continue;
                }

                std::map<std::string, std::string> assigned;
                for (const auto& material : materials) {
                    std::string graphPath;
                    if (operation.contains("graph")) {
                        graphPath = operation["graph"].is_null() ? std::string() : ToAssetPath(operation["graph"].get<std::string>());
                        assigned[material] = graphPath;
                    }
                    else if (edits.assigned.contains(material)) {
                        graphPath = edits.assigned[material];
                    }
                    else {
                        std::ifstream stream(GetAssetDirectory() / material);
                        auto j = json::parse(stream);
                        AssetReferences::ResolveReferences(j, AssetReferences::Type::Material, material);
                        graphPath = j.value("graphPath", std::string());
                    }

                    if (graphPath.empty()) {
                        if (operation.contains("values") || operation.contains("textures"))
                            throw std::runtime_error(material + " has no graph, set \"graph\"");
                        continue;
                    }

                    auto compiled = GetCompiledGraph(graphPath, edits);
                    if (!compiled->errors.empty())
                        throw std::runtime_error("the graph " + graphPath + " doesn't compile");
                    CheckGraphValues(operation, *compiled, graphPath);
                }
                // A failing operation ends the edit, only valid ones assign their graph for the next ones
                for (const auto& [material, graphPath] : assigned)
                    edits.assigned[material] = graphPath;
            }
            catch (const std::exception& e) {
                issues.push_back("ERROR: " + where + ": " + e.what());
            }
        }

    }

    static std::string DescribeStages(const CompiledGraph& compiled, uint32_t nodeId) {

        auto iter = compiled.nodeStages.find(nodeId);
        if (iter == compiled.nodeStages.end() || iter->second == 0)
            return "unused";
        if (iter->second == AllStages)
            return "vertex + surface";
        return iter->second == VertexStage ? "vertex" : "surface";

    }

    int32_t InspectMaterialGraph(const fs::path& file, const std::vector<std::string>& filters, bool printCode, const fs::path& output) {

        // References in packages are relative to the file, which needs its path below the data directory
        auto relative = file.lexically_normal().lexically_relative(GetAssetDirectory());
        auto path = !relative.empty() && relative.native()[0] != '.' ? relative.generic_string() : file.generic_string();

        auto document = ReadDocument(path);
        auto& graph = *document.graph;
        auto compiled = graph.Compile();

        Print(path + ": '" + graph.name + "', " + std::to_string(graph.nodes.size()) + " nodes, " + std::to_string(graph.links.size()) +
            " links, largest displacement " + FormatNumber(graph.maxDisplacement));

        if (!compiled->errors.empty()) {
            Print("  doesn't compile:" + FormatErrors(graph, *compiled));
        }
        else {
            std::vector<std::string> parts;
            if (compiled->hasVertexOffset)
                parts.push_back("moves vertices");
            if (compiled->hasSurface)
                parts.push_back(compiled->writesNormal ? "surface with normals" : "surface");
            if (compiled->hasOpacity)
                parts.push_back(compiled->hasUniformOpacity ? "opacity per draw" : "cutout opacity");
            if (parts.empty())
                parts.push_back("nothing, the material renders like a regular one");
            std::string text;
            for (const auto& part : parts)
                text += (text.empty() ? "" : ", ") + part;
            Print("  compiles: " + text);
        }

        if (!compiled->parameters.empty())
            Print("  parameters:");
        for (const auto& parameter : compiled->parameters) {
            auto line = "    " + parameter.name + ": " + GetTypeName(parameter.type, parameter.isColor);
            if (parameter.type == ValueType::Texture) {
                auto texture = document.texturePaths.find(parameter.nodeId);
                const char* fallbacks[] = { "white", "black", "normal" };
                line += texture != document.texturePaths.end() ? " " + texture->second : std::string(" ") + fallbacks[int32_t(parameter.fallback)];
            }
            else {
                line += " " + FormatValue(parameter.defaultValue, GetComponentCount(parameter.type));
            }
            if (parameter.HasRange())
                line += " [" + FormatNumber(parameter.range.x) + ", " + FormatNumber(parameter.range.y) + "]";
            if (!parameter.group.empty())
                line += ", group " + parameter.group;
            Print(line);
        }

        Print("  nodes (stages they run in):");
        for (const auto& node : graph.nodes) {
            if (!filters.empty() && !MatchAnyPattern(filters, node.type) && !MatchAnyPattern(filters, node.name) &&
                !MatchAnyPattern(filters, "#" + std::to_string(node.id)))
                continue;

            auto definition = NodeLibrary::Find(node);
            auto line = "    " + GetLabel(node);
            if (definition && (definition->kind == NodeKind::Constant || (definition->kind == NodeKind::Parameter && definition->valueType != ValueType::Texture)))
                line += " = " + FormatValue(node.value, GetComponentCount(definition->valueType));
            if (auto texture = document.texturePaths.find(node.id); texture != document.texturePaths.end())
                line += " " + texture->second;
            if (definition && !IsNote(node))
                line += " (" + DescribeStages(*compiled, node.id) + ")";
            Print(line);

            if (!definition)
                continue;
            for (const auto& input : definition->inputs) {
                if (auto link = graph.GetInputLink(node.id, input.name)) {
                    auto from = graph.GetNode(link->fromNode);
                    Print("      " + input.name + " <- " + (from ? GetLabel(*from) : "#" + std::to_string(link->fromNode)) + "." + link->fromPin);
                }
                else if (auto value = node.inputValues.find(input.name); value != node.inputValues.end()) {
                    Print("      " + input.name + " = " + FormatValue(value->second, GetComponentCount(input.type)));
                }
            }
        }

        std::vector<std::pair<std::string, std::string>> code = {
            { "main vertex", compiled->mainVertexCode }, { "main fragment", compiled->mainFragmentCode },
            { "shadow vertex", compiled->shadowVertexCode }, { "shadow fragment", compiled->shadowFragmentCode } };
        if (printCode) {
            for (const auto& [stage, text] : code)
                if (!text.empty())
                    Print("  // " + stage + "\n" + text);
        }

        if (!output.empty()) {
            json errors = json::array();
            for (const auto& error : compiled->errors)
                errors.push_back({ { "node", error.nodeId }, { "message", error.message } });
            json result = { { "graph", SerializeDocument(document, path) }, { "errors", errors },
                { "parameters", DescribeParameters(*compiled) }, { "compiled", DescribeCompiled(*compiled) } };
            json stages = json::object();
            for (const auto& node : graph.nodes)
                stages[std::to_string(node.id)] = DescribeStages(*compiled, node.id);
            result["stages"] = stages;
            if (printCode) {
                for (const auto& [stage, text] : code)
                    result["code"][stage] = text;
            }
            if (!WriteFileAtomic(output, result.dump(2)))
                throw std::runtime_error("Couldn't write " + output.string());
            Print("  written to " + output.string());
        }

        return compiled->errors.empty() ? 0 : 1;

    }

    int32_t InspectMaterialGraphNodes(const std::vector<std::string>& filters) {

        const char* stageNames[] = { "", "vertex only", "surface only", "" };

        // Unconnected inputs with an expression read what an input node gives, e.g. the object position
        std::unordered_map<std::string, std::string> expressionNames;
        for (const auto& definition : NodeLibrary::GetDefinitions())
            if (definition.inputs.empty() && definition.outputs.size() == 1 && !definition.outputs.front().expression.empty())
                expressionNames.emplace(definition.outputs.front().expression, definition.type);

        std::string category;
        for (const auto& definition : NodeLibrary::GetDefinitions()) {
            if (!filters.empty() && !MatchAnyPattern(filters, definition.type) && !MatchAnyPattern(filters, definition.category))
                continue;

            if (definition.category != category) {
                category = definition.category;
                Print(category + ":");
            }

            std::string inputs;
            for (const auto& input : definition.inputs) {
                auto text = input.name + " " + GetTypeName(input.type);
                if (!input.defaultExpression.empty())
                    text += " = " + (expressionNames.contains(input.defaultExpression) ? expressionNames[input.defaultExpression] : input.defaultExpression);
                else if (input.type != ValueType::Texture)
                    text += " = " + FormatValue(input.defaultValue, GetComponentCount(input.type));
                inputs += (inputs.empty() ? "" : ", ") + text;
            }

            std::string outputs;
            for (const auto& output : definition.outputs)
                outputs += (outputs.empty() ? "" : ", ") + output.name + " " + GetTypeName(output.type);

            auto line = "  " + definition.type;
            if (definition.kind == NodeKind::Constant || definition.kind == NodeKind::Parameter)
                line += " (value " + GetTypeName(definition.valueType, definition.isColor) + " " +
                    FormatValue(definition.defaultValue, definition.valueType == ValueType::Texture ? 2 : GetComponentCount(definition.valueType)) + ")";
            if (definition.stages != AllStages)
                line += std::string(" [") + stageNames[definition.stages & AllStages] + "]";
            line += ": " + (inputs.empty() ? std::string("-") : inputs) + " -> " + (outputs.empty() ? std::string("-") : outputs);
            Print(line);
            if (!filters.empty() && !definition.tooltip.empty())
                Print("      " + definition.tooltip);
        }

        return 0;

    }

}
