#include "Compiler.h"

#include "../common/ColorConverter.h"
#include "../loader/ShaderLoader.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <unordered_set>

namespace Atlas::MaterialGraph {

    Ref<CompiledGraph> Compiler::Compile(const Graph& graph) {

        auto compiled = CreateRef<CompiledGraph>();

        // Also with errors, nodes using the subgraph keep their pins
        if (IsSubgraph(graph)) {
            compiled->subgraphNodes = graph.nodes;
            compiled->subgraphLinks = graph.links;
        }

        Expansion expansion;
        const auto& expanded = Expand(graph, expansion, *compiled);

        State state = { .graph = expanded, .compiled = *compiled, .expansion = expansion };
        if (compiled->IsSubgraph())
            CheckSubgraph(state);
        else
            GenerateShaders(state);

        MapToSource(expansion, *compiled);

        return compiled;

    }

    void Compiler::GenerateShaders(State& state) {

        const auto& graph = state.graph;
        auto compiled = &state.compiled;

        if (compiled->errors.empty())
            ValidateGraph(state);
        if (!compiled->errors.empty())
            return;

        const Node* vertexOutput = nullptr;
        const Node* surfaceOutput = nullptr;
        for (const auto& node : graph.nodes) {
            if (node.type == NodeLibrary::vertexOutputType)
                vertexOutput = &node;
            else if (node.type == NodeLibrary::surfaceOutputType)
                surfaceOutput = &node;
        }

        auto isLinked = [&](const Node* node, const std::string& pin) {
            return node && graph.GetInputLink(node->id, pin) != nullptr;
            };
        // Vertex outputs which only reach a zero, e.g. wind behind a disabled static switch, don't move the vertices
        auto isVertexOutputUsed = [&](const std::string& pin) {
            if (!isLinked(vertexOutput, pin))
                return false;
            std::vector<uint32_t> visiting;
            return !IsDisconnected(state, *vertexOutput, *NodeLibrary::Find(*vertexOutput)->FindInput(pin), visiting);
            };

        const auto& surfaceInputs = NodeLibrary::Find(NodeLibrary::surfaceOutputType)->inputs;

        auto hasPositionOffset = isVertexOutputUsed(NodeLibrary::positionOffsetPin);
        compiled->hasVertexOffset = hasPositionOffset;
        compiled->hasSurface = std::any_of(surfaceInputs.begin(), surfaceInputs.end(),
            [&](const auto& input) { return isLinked(surfaceOutput, input.name); });
        auto getInlineValue = [&](const std::string& pin) {
            auto iter = surfaceOutput->inputValues.find(pin);
            return iter != surfaceOutput->inputValues.end() ? iter->second.x :
                NodeLibrary::Find(NodeLibrary::surfaceOutputType)->FindInput(pin)->defaultValue.x;
            };
        // An opacity typed into the output counts as well, it is uniform
        compiled->hasOpacity = surfaceOutput && (isLinked(surfaceOutput, NodeLibrary::opacityPin) ||
            getInlineValue(NodeLibrary::opacityPin) != 1.0f);
        compiled->writesNormal = isLinked(surfaceOutput, NodeLibrary::normalPin);
        compiled->refracts = surfaceOutput && (isLinked(surfaceOutput, NodeLibrary::refractiveIndexPin) ||
            getInlineValue(NodeLibrary::refractiveIndexPin) != 1.0f) && (isLinked(surfaceOutput, NodeLibrary::thicknessPin) ||
            getInlineValue(NodeLibrary::thicknessPin) > 0.0f);

        if (compiled->hasOpacity) {
            std::vector<uint32_t> visiting;
            auto opacityInput = NodeLibrary::Find(NodeLibrary::surfaceOutputType)->FindInput(NodeLibrary::opacityPin);
            compiled->hasUniformOpacity = IsUniform(state, *surfaceOutput, *opacityInput, visiting);
        }

        auto hasVertexNormal = isVertexOutputUsed(NodeLibrary::vertexNormalPin);

        std::string mainSurface, shadowSurface, vertexOpacity;
        std::vector<uint32_t> mainVaryings, shadowVaryings;
        if (compiled->hasSurface) {
            // Translucent materials blend by the opacity, opaque ones only read it to cut out fragments
            std::vector<std::string> pins;
            for (const auto& input : surfaceInputs) {
                if (input.name != NodeLibrary::opacityPin || compiled->hasOpacity)
                    pins.push_back(input.name);
            }
            mainSurface = GetSurfaceFunction(state, *surfaceOutput, pins, mainVaryings);
        }
        if (compiled->CutsOutFragments())
            shadowSurface = GetSurfaceFunction(state, *surfaceOutput, { NodeLibrary::opacityPin }, shadowVaryings);
        if (compiled->hasUniformOpacity)
            vertexOpacity = GetVertexOpacityFunction(state, *surfaceOutput);

        auto usesVertexStage = compiled->hasVertexOffset || hasVertexNormal || !mainVaryings.empty() || compiled->hasUniformOpacity;
        auto usesShadow = compiled->hasVertexOffset || compiled->hasOpacity;

        // A normal without any movement still has to be written
        compiled->hasVertexOffset = compiled->hasVertexOffset || hasVertexNormal;

        auto mainVertexFunctions = GetVertexFunctions(state, vertexOutput, mainVaryings, compiled->hasVertexOffset, true);
        auto shadowVertexFunctions = GetVertexFunctions(state, vertexOutput, shadowVaryings, hasPositionOffset, false);
        auto reads = [&](std::initializer_list<const char*> names) {
            return std::any_of(names.begin(), names.end(), [&](const char* name) {
                return shadowVertexFunctions.find(name) != std::string::npos || shadowSurface.find(name) != std::string::npos;
                });
            };
        compiled->animatesShadow = reads({ ".time", "globalData.", ".viewDirection", ".viewPosition", ".viewNormal",
            ".tangentFrame", ".screenCoord" });
        compiled->shadowReadsTexCoords = reads({ ".texCoord" });
        // Hardware ray tracing deforms the vertices in a compute pass, which has no varyings
        auto deformVertexFunctions = compiled->hasVertexOffset ? GetVertexFunctions(state, vertexOutput, {}, true, true) : std::string();

        // Ray tracing pipelines call the surface per hit, which has no varyings either, the stages of the nodes stay the
        // ones of the rasterizer. The any hit stage passes translucent surfaces by their opacity, refracting ones are
        // hit and blend it like glass
        std::string hitSurface;
        if (compiled->hasSurface) {
            auto nodeStages = compiled->nodeStages;
            std::vector<std::string> pins;
            for (const auto& input : surfaceInputs) {
                if (input.name != NodeLibrary::opacityPin || compiled->refracts)
                    pins.push_back(input.name);
            }
            std::vector<uint32_t> hitVaryings;
            hitSurface = GetSurfaceFunction(state, *surfaceOutput, pins, hitVaryings, AllStages);
            compiled->nodeStages = nodeStages;
        }

        std::string hitOpacitySurface;
        if (compiled->hasOpacity) {
            auto nodeStages = compiled->nodeStages;
            std::vector<uint32_t> hitVaryings;
            hitOpacitySurface = GetSurfaceFunction(state, *surfaceOutput, { NodeLibrary::opacityPin }, hitVaryings, AllStages);
            compiled->nodeStages = nodeStages;
        }

        if (!compiled->errors.empty())
            return;

        compiled->valid = true;
        if (!usesVertexStage && !compiled->hasSurface)
            return;

        auto define = [](const char* macro, bool enable = true) {
            return enable ? "#define " + std::string(macro) + "\n" : std::string();
            };

        // Reading the frame needs it as well if the normal isn't written
        auto usesTangents = !compiled->writesNormal && mainSurface.find("tangentFrame") != std::string::npos;

        auto vertexMacros = define("MATERIAL_GRAPH") + define("MATERIAL_GRAPH_STAGE_VERTEX");
        auto surfaceMacros = define("MATERIAL_GRAPH") + define("MATERIAL_GRAPH_STAGE_SURFACE");
        const std::string common = "#include <common.hsh>\n";

        compiled->mainVertexCode = vertexMacros +
            define("MATERIAL_GRAPH_VERTEX_OFFSET", compiled->hasVertexOffset) +
            define("MATERIAL_GRAPH_SURFACE", compiled->hasSurface) +
            define("MATERIAL_GRAPH_WRITES_NORMAL", compiled->writesNormal) +
            define("MATERIAL_GRAPH_USES_TANGENTS", usesTangents) +
            define("MATERIAL_GRAPH_VERTEX_OPACITY", compiled->hasUniformOpacity) + common +
            GetVaryingDeclarations(state, mainVaryings, "out") + mainVertexFunctions + vertexOpacity +
            "#include <../deferred/geometry.vsh>\n";
        compiled->mainFragmentCode = surfaceMacros +
            define("MATERIAL_GRAPH_SURFACE", compiled->hasSurface) +
            define("MATERIAL_GRAPH_WRITES_NORMAL", compiled->writesNormal) +
            define("MATERIAL_GRAPH_USES_TANGENTS", usesTangents) +
            define("MATERIAL_GRAPH_OPACITY", compiled->CutsOutFragments()) +
            define("MATERIAL_GRAPH_HAS_OPACITY", compiled->hasOpacity) + common +
            GetVaryingDeclarations(state, mainVaryings, "in") + mainSurface +
            "#include <../deferred/geometry.fsh>\n";

        if (usesShadow) {
            compiled->shadowVertexCode = vertexMacros +
                define("MATERIAL_GRAPH_VERTEX_OFFSET", hasPositionOffset) +
                define("MATERIAL_GRAPH_SURFACE", compiled->CutsOutFragments()) +
                define("MATERIAL_GRAPH_VERTEX_OPACITY", compiled->hasUniformOpacity) + common +
                GetVaryingDeclarations(state, shadowVaryings, "out") + shadowVertexFunctions + vertexOpacity +
                "#include <../shadowMapping.vsh>\n";
            compiled->shadowFragmentCode = surfaceMacros +
                define("MATERIAL_GRAPH_SURFACE", compiled->CutsOutFragments()) + common +
                GetVaryingDeclarations(state, shadowVaryings, "in") + shadowSurface +
                "#include <../shadowMapping.fsh>\n";
        }

        if (compiled->hasVertexOffset) {
            compiled->deformCode = vertexMacros + define("MATERIAL_GRAPH_VERTEX_OFFSET") + common +
                deformVertexFunctions + "#include <../raytracer/deform.csh>\n";
        }

        if (compiled->hasSurface) {
            compiled->hitCode = surfaceMacros + define("MATERIAL_GRAPH_STAGE_EVALUATE") + define("MATERIAL_GRAPH_STAGE_HIT") +
                common + hitSurface + "#include <../raytracer/graphHit.rchit>\n";
        }

        if (compiled->hasOpacity) {
            compiled->hitOpacityCode = surfaceMacros + define("MATERIAL_GRAPH_STAGE_EVALUATE") + define("MATERIAL_GRAPH_STAGE_HIT") +
                common + hitOpacitySurface + "#include <../raytracer/graphOpacity.rahit>\n";
        }

        Hash hash = 0;
        HashCombine(hash, compiled->mainVertexCode);
        HashCombine(hash, compiled->mainFragmentCode);
        HashCombine(hash, compiled->shadowVertexCode);
        HashCombine(hash, compiled->shadowFragmentCode);
        HashCombine(hash, compiled->deformCode);
        HashCombine(hash, compiled->hitCode);
        HashCombine(hash, compiled->hitOpacityCode);
        compiled->hash = hash;

        // The name contains the content since the SPIR-V cache only knows names and macros
        auto prefix = "materialGraph/" + std::to_string(hash);
        auto registerFile = [&](std::string& file, const std::string& suffix, const std::string& code) {
            if (code.empty())
                return;
            file = prefix + suffix;
            Loader::ShaderLoader::AddVirtualFile(file, code);
            };

        registerFile(compiled->mainVertexFile, "_main.vsh", compiled->mainVertexCode);
        registerFile(compiled->mainFragmentFile, "_main.fsh", compiled->mainFragmentCode);
        registerFile(compiled->shadowVertexFile, "_shadow.vsh", compiled->shadowVertexCode);
        registerFile(compiled->shadowFragmentFile, "_shadow.fsh", compiled->shadowFragmentCode);
        registerFile(compiled->deformFile, "_deform.csh", compiled->deformCode);
        registerFile(compiled->hitFile, "_hit.rchit", compiled->hitCode);
        registerFile(compiled->hitOpacityFile, "_opacity.rahit", compiled->hitOpacityCode);

    }

    void Compiler::CheckSubgraph(State& state) {

        auto& compiled = state.compiled;

        if (compiled.errors.empty())
            ValidateGraph(state);
        if (!compiled.errors.empty())
            return;

        Function function = { .stage = AllStages, .inputName = "surfaceIn" };
        for (auto output : NodeLibrary::GetSubgraphPins(state.graph.nodes, NodeLibrary::subgraphOutputType)) {
            const auto& input = NodeLibrary::Find(*output)->inputs.front();
            EmitInput(state, function, *output, input, input.type);
            compiled.nodeStages[output->id] = AllStages;
        }

        // The stages nodes are available in, the graphs using the subgraph decide which ones they end up in
        for (auto& [nodeId, stages] : compiled.nodeStages)
            stages &= NodeLibrary::Find(*state.graph.GetNode(nodeId))->stages;

        compiled.valid = compiled.errors.empty();

    }

    bool Compiler::IsSubgraph(const Graph& graph) {

        return std::any_of(graph.nodes.begin(), graph.nodes.end(), [](const Node& node) {
            return node.type == NodeLibrary::subgraphInputType || node.type == NodeLibrary::subgraphOutputType;
            });

    }

    void Compiler::ValidateSubgraphPins(const Graph& graph, CompiledGraph& compiled) {

        auto outputCount = 0;
        for (const auto& node : graph.nodes) {
            if (node.type == NodeLibrary::vertexOutputType || node.type == NodeLibrary::surfaceOutputType)
                Error(compiled, node.id, "A graph with subgraph inputs or outputs can't have the outputs of a material");
            outputCount += node.type == NodeLibrary::subgraphOutputType ? 1 : 0;
        }
        if (!outputCount)
            Error(compiled, 0, "A subgraph needs an output");

        for (const auto& pinType : { NodeLibrary::subgraphInputType, NodeLibrary::subgraphOutputType }) {
            std::unordered_set<std::string> names;
            for (auto pin : NodeLibrary::GetSubgraphPins(graph.nodes, pinType)) {
                if (pin->name.empty())
                    Error(compiled, pin->id, "The pin of a subgraph needs a name");
                else if (!names.insert(pin->name).second)
                    Error(compiled, pin->id, "The pin " + pin->name + " exists twice");
                if (pin->pinType == ValueType::Generic)
                    Error(compiled, pin->id, "The pin " + pin->name + " needs a type");
            }
        }

    }

    const Graph& Compiler::Expand(const Graph& graph, Expansion& expansion, CompiledGraph& compiled) {

        if (IsSubgraph(graph))
            ValidateSubgraphPins(graph, compiled);

        std::vector<uint32_t> subgraphNodes;
        for (const auto& node : graph.nodes) {
            if (node.type == NodeLibrary::subgraphType)
                subgraphNodes.push_back(node.id);
        }
        if (subgraphNodes.empty())
            return graph;

        auto& expanded = expansion.graph;
        expanded.name = graph.name;
        expanded.nodes = graph.nodes;
        expanded.links = graph.links;
        expanded.nextNodeId = graph.nextNodeId;
        for (const auto& node : graph.nodes)
            expanded.nextNodeId = std::max(expanded.nextNodeId, node.id + 1);

        // The nodes change their order when clicking them, the new ids must not depend on it
        std::sort(subgraphNodes.begin(), subgraphNodes.end());

        std::vector<const Graph*> parents = { &graph };
        for (auto nodeId : subgraphNodes)
            ExpandNode(nodeId, expansion, compiled, parents);

        return expanded;

    }

    void Compiler::ExpandNode(uint32_t nodeId, Expansion& expansion, CompiledGraph& compiled, std::vector<const Graph*>& parents) {

        auto& graph = expansion.graph;

        const auto subgraph = graph.GetNode(nodeId)->subgraph;
        if (subgraph.IsLoaded() && std::find(parents.begin(), parents.end(), subgraph.Get().get()) != parents.end()) {
            Error(compiled, nodeId, "The subgraph " + GetSubgraphName(subgraph) + " contains itself");
            return;
        }

        auto inlined = InlineSubgraph(graph, nodeId);
        for (const auto& error : inlined.errors)
            Error(compiled, nodeId, error);
        if (!inlined.errors.empty())
            return;

        auto origin = expansion.origins.contains(nodeId) ? expansion.origins[nodeId] : Expansion::Origin{ .nodeId = nodeId };
        origin.path += (origin.path.empty() ? "" : " / ") + GetSubgraphName(subgraph);
        for (auto id : inlined.nodes)
            expansion.origins[id] = origin;
        expansion.outputs[nodeId] = inlined.outputs;

        parents.push_back(subgraph.Get().get());
        for (auto id : inlined.nodes) {
            if (graph.GetNode(id)->type == NodeLibrary::subgraphType)
                ExpandNode(id, expansion, compiled, parents);
        }
        parents.pop_back();

    }

    Compiler::InlinedSubgraph Compiler::InlineSubgraph(Graph& graph, uint32_t nodeId) {

        InlinedSubgraph inlined;

        const auto node = *graph.GetNode(nodeId);
        auto name = GetSubgraphName(node.subgraph);

        auto compiled = node.subgraph.IsLoaded() ? node.subgraph->GetCompiled() : nullptr;
        if (!node.subgraph.IsValid())
            inlined.errors.push_back("The node has no subgraph");
        else if (!node.subgraph.IsLoaded())
            inlined.errors.push_back("The subgraph " + name + " couldn't be loaded");
        else if (!compiled || !compiled->IsSubgraph())
            inlined.errors.push_back("The graph " + name + " has no inputs or outputs of a subgraph");
        else if (!compiled->valid)
            inlined.errors.push_back("The subgraph " + name + " has errors: " + (compiled->errors.empty() ? std::string() :
                compiled->errors.front().message));
        if (!inlined.errors.empty())
            return inlined;

        auto hasPin = [&](const std::string& pinType, const std::string& pin) {
            auto pins = NodeLibrary::GetSubgraphPins(compiled->subgraphNodes, pinType);
            return std::any_of(pins.begin(), pins.end(), [&](const Node* pinNode) { return pinNode->name == pin; });
            };
        for (const auto& link : graph.links) {
            if (link.toNode == nodeId && !hasPin(NodeLibrary::subgraphInputType, link.toPin))
                inlined.errors.push_back("The subgraph " + name + " has no input " + link.toPin);
            if (link.fromNode == nodeId && !hasPin(NodeLibrary::subgraphOutputType, link.fromPin))
                inlined.errors.push_back("The subgraph " + name + " has no output " + link.fromPin);
        }
        if (!inlined.errors.empty())
            return inlined;

        // In the order of their ids, the nodes change their order when clicking them
        std::vector<const Node*> subgraphNodes;
        for (const auto& subgraphNode : compiled->subgraphNodes)
            subgraphNodes.push_back(&subgraphNode);
        std::sort(subgraphNodes.begin(), subgraphNodes.end(), [](const Node* a, const Node* b) { return a->id < b->id; });

        std::unordered_map<uint32_t, uint32_t> ids;
        std::unordered_set<uint32_t> outputNodes;
        for (auto subgraphNode : subgraphNodes) {
            auto copy = *subgraphNode;
            copy.id = graph.nextNodeId++;
            ids[subgraphNode->id] = copy.id;
            inlined.nodes.push_back(copy.id);

            if (copy.type == NodeLibrary::subgraphInputType) {
                inlined.inputs[copy.name] = copy.id;
            }
            else if (copy.type == NodeLibrary::subgraphOutputType) {
                // Both only convert to the type of the pin
                inlined.outputs[copy.name] = copy.id;
                outputNodes.insert(copy.id);
                copy.type = NodeLibrary::subgraphInputType;
                if (auto value = copy.inputValues.find("Value"); value != copy.inputValues.end())
                    copy.inputValues = { { NodeLibrary::subgraphDefaultPin, value->second } };
                NodeLibrary::UpdateDefinition(copy);
            }

            graph.nodes.push_back(copy);
        }

        // The links and values of the node replace the defaults of the inputs
        std::unordered_set<uint32_t> linkedInputs;
        for (auto& link : graph.links) {
            if (link.toNode == nodeId) {
                link.toNode = inlined.inputs[link.toPin];
                link.toPin = NodeLibrary::subgraphDefaultPin;
                linkedInputs.insert(link.toNode);
            }
            if (link.fromNode == nodeId) {
                link.fromNode = inlined.outputs[link.fromPin];
                link.fromPin = "Value";
            }
        }
        for (const auto& [pin, value] : node.inputValues) {
            if (inlined.inputs.contains(pin))
                graph.GetNode(inlined.inputs[pin])->inputValues[NodeLibrary::subgraphDefaultPin] = value;
        }

        for (auto link : compiled->subgraphLinks) {
            if (!ids.contains(link.fromNode) || !ids.contains(link.toNode) || linkedInputs.contains(ids[link.toNode]))
                continue;
            link.fromNode = ids[link.fromNode];
            link.toNode = ids[link.toNode];
            if (outputNodes.contains(link.toNode))
                link.toPin = NodeLibrary::subgraphDefaultPin;
            graph.links.push_back(link);
        }

        std::erase_if(graph.nodes, [&](const Node& item) { return item.id == nodeId; });

        return inlined;

    }

    std::string Compiler::GetSubgraphName(const ResourceHandle<Graph>& subgraph) {

        if (subgraph.IsValid() && !subgraph.IsGenerated())
            return subgraph.GetResource()->GetFileName();

        return subgraph.IsLoaded() ? subgraph->name : "";

    }

    void Compiler::MapToSource(const Expansion& expansion, CompiledGraph& compiled) {

        if (expansion.origins.empty())
            return;

        auto errors = std::move(compiled.errors);
        compiled.errors.clear();
        for (const auto& error : errors) {
            auto iter = expansion.origins.find(error.nodeId);
            if (iter == expansion.origins.end())
                Error(compiled, error.nodeId, error.message);
            else
                Error(compiled, iter->second.nodeId, iter->second.path + ": " + error.message);
        }

        for (const auto& [nodeId, origin] : expansion.origins) {
            auto iter = compiled.nodeStages.find(nodeId);
            if (iter == compiled.nodeStages.end())
                continue;
            auto stages = iter->second;
            compiled.nodeStages.erase(iter);
            compiled.nodeStages[origin.nodeId] |= stages;
        }

        for (auto& parameter : compiled.parameters) {
            auto iter = expansion.origins.find(parameter.nodeId);
            if (iter != expansion.origins.end())
                parameter.nodeId = iter->second.nodeId;
        }

    }

    EvaluationShader Compiler::CompileBake(const Graph& graph) {

        CompiledGraph compiled;
        Expansion expansion;
        const auto& expanded = Expand(graph, expansion, compiled);

        State state = { .graph = expanded, .compiled = compiled, .expansion = expansion };

        if (compiled.errors.empty())
            ValidateGraph(state);
        if (!compiled.errors.empty()) {
            MapToSource(expansion, compiled);
            return { .errors = compiled.errors };
        }

        AddEvaluationParameters(state);

        auto surfaceOutput = std::find_if(state.graph.nodes.begin(), state.graph.nodes.end(),
            [](const Node& node) { return node.type == NodeLibrary::surfaceOutputType; });
        if (surfaceOutput == state.graph.nodes.end())
            return { .errors = { { 0, "The graph has no surface output" } } };

        std::vector<std::string> pins;
        for (const auto& input : NodeLibrary::Find(NodeLibrary::surfaceOutputType)->inputs)
            pins.push_back(input.name);

        std::vector<uint32_t> usedVaryings;
        auto code = GetSurfaceFunction(state, *surfaceOutput, pins, usedVaryings, AllStages);
        MapToSource(expansion, compiled);

        return FinishEvaluation(compiled, code, "");

    }

    EvaluationShader Compiler::CompilePreview(const Graph& graph, uint32_t nodeId) {

        CompiledGraph compiled;
        Expansion expansion;
        const auto& expanded = Expand(graph, expansion, compiled);

        State state = { .graph = expanded, .compiled = compiled, .expansion = expansion };

        if (compiled.errors.empty())
            ValidateGraph(state);
        if (!compiled.errors.empty()) {
            MapToSource(expansion, compiled);
            return { .errors = compiled.errors };
        }

        AddEvaluationParameters(state);

        // The first used output of a subgraph stands for the node
        if (auto outputs = expansion.outputs.find(nodeId); outputs != expansion.outputs.end()) {
            auto definition = NodeLibrary::Find(*graph.GetNode(nodeId));
            if (definition->outputs.empty())
                return { .errors = { { nodeId, "The node has no value" } } };

            auto pin = definition->outputs.front().name;
            for (const auto& output : definition->outputs) {
                auto isLinked = std::any_of(graph.links.begin(), graph.links.end(),
                    [&](const Link& link) { return link.fromNode == nodeId && link.fromPin == output.name; });
                if (isLinked) {
                    pin = output.name;
                    break;
                }
            }
            nodeId = outputs->second.contains(pin) ? outputs->second[pin] : 0;
        }

        auto node = state.graph.GetNode(nodeId);
        if (!node)
            return { .errors = { { nodeId, "The node doesn't exist" } } };

        auto code = GetPreviewFunction(state, *node);
        MapToSource(expansion, compiled);

        return FinishEvaluation(compiled, code, "#define EVALUATE_PREVIEW\n");

    }

    EvaluationShader Compiler::FinishEvaluation(const CompiledGraph& compiled, const std::string& code, const char* macros) {

        EvaluationShader shader = {
            .errors = compiled.errors,
            .parameters = compiled.parameters,
            .parameterWords = compiled.parameterWords,
            .usesTime = code.find("surfaceIn.time") != std::string::npos
        };

        if (!shader.errors.empty())
            return shader;

        auto file = std::string(macros) + "#define MATERIAL_GRAPH\n#define MATERIAL_GRAPH_STAGE_SURFACE\n"
            "#define MATERIAL_GRAPH_STAGE_EVALUATE\n#include <common.hsh>\n" + code + "#include <evaluate.fsh>\n";

        Hash hash = 0;
        HashCombine(hash, file);

        shader.fragmentFile = "materialGraph/" + std::to_string(hash) + "_evaluate.fsh";
        Loader::ShaderLoader::AddVirtualFile(shader.fragmentFile, file);
        shader.valid = true;

        return shader;

    }

    void Compiler::AddEvaluationParameters(State& state) {

        // By id, the order of the nodes changes when clicking them
        std::vector<const Node*> nodes;
        for (const auto& node : state.graph.nodes)
            nodes.push_back(&node);
        std::sort(nodes.begin(), nodes.end(), [](const Node* a, const Node* b) { return a->id < b->id; });

        Function function = { .stage = AllStages, .inputName = "surfaceIn" };
        for (auto node : nodes) {
            auto definition = NodeLibrary::Find(*node);
            if (definition->kind == NodeKind::Parameter)
                EmitParameter(state, function, *node, *definition);
        }

    }

    std::string Compiler::GetPreviewFunction(State& state, const Node& node) {

        const std::string signature = "vec4 EvaluateMaterialPreview(MaterialSurfaceInput surfaceIn) {\n";

        auto definition = NodeLibrary::Find(node);

        if (definition->kind == NodeKind::SurfaceOutput) {
            std::vector<std::string> pins;
            for (const auto& input : definition->inputs)
                pins.push_back(input.name);

            std::vector<uint32_t> usedVaryings;
            return GetSurfaceFunction(state, node, pins, usedVaryings, AllStages) + signature + "\n"
                "    MaterialSurfaceOutput surface = EvaluateMaterialSurface(surfaceIn);\n"
                "    return vec4(surface.baseColor + surface.emissive, 1.0);\n\n}\n\n";
        }

        Function function = { .stage = AllStages, .inputName = "surfaceIn", .previewNode = node.id };

        std::string value;
        // Tangent space normals are shown like in a normal map
        auto toColor = [](const std::string& color, bool isNormal) {
            return isNormal ? "0.5 * (" + color + ") + 0.5" : color;
            };

        if (definition->kind == NodeKind::VertexOutput) {
            value = EmitInput(state, function, node, *definition->FindInput(NodeLibrary::positionOffsetPin), ValueType::Vec3);
        }
        else if (definition->kind == NodeKind::SubgraphInput || definition->kind == NodeKind::SubgraphOutput) {
            const auto& input = definition->inputs.front();
            if (input.type == ValueType::Texture)
                value = "MaterialGraphSampleTexture(" + EmitInput(state, function, node, input, input.type) + ", surfaceIn.texCoord).rgb";
            else
                value = Convert(EmitInput(state, function, node, input, input.type), input.type, ValueType::Vec3);
            value = toColor(value, node.name == NodeLibrary::normalPin);
        }
        else {
            if (definition->outputs.empty()) {
                Error(state.compiled, node.id, "The node has no value");
                return "";
            }

            auto output = &definition->outputs.front();
            for (const auto& candidate : definition->outputs) {
                auto isLinked = std::any_of(state.graph.links.begin(), state.graph.links.end(),
                    [&](const Link& link) { return link.fromNode == node.id && link.fromPin == candidate.name; });
                if (isLinked) {
                    output = &candidate;
                    break;
                }
            }
            function.previewPin = output->name;

            EmitNode(state, function, node);

            auto variables = function.variables.find(node.id);
            if (variables == function.variables.end() || !variables->second.contains(output->name))
                return "";

            auto type = GetOutputType(state, node, *output);
            const auto& variable = variables->second[output->name];

            if (type == ValueType::Texture)
                value = "MaterialGraphSampleTexture(" + variable + ", surfaceIn.texCoord).rgb";
            else
                value = Convert(variable, type, ValueType::Vec3);

            value = toColor(value, definition->category == "Normal" || output->name == "Normal");
        }

        return function.functions + signature + "\n" + function.code + "\n    return vec4(" + value + ", 1.0);\n\n}\n\n";

    }

    const std::string& Compiler::GetPinLabel(const Node& node, const std::string& pin) {

        return node.type == NodeLibrary::subgraphInputType ? node.name : pin;

    }

    void Compiler::Error(CompiledGraph& compiled, uint32_t nodeId, const std::string& message) {

        // A node might be emitted for several functions, which would report the same error again
        auto& errors = compiled.errors;
        auto exists = std::any_of(errors.begin(), errors.end(),
            [&](const auto& error) { return error.nodeId == nodeId && error.message == message; });
        if (!exists)
            errors.push_back({ nodeId, message });

    }

    void Compiler::ValidateGraph(State& state) {

        const auto& graph = state.graph;

        int32_t vertexOutputs = 0, surfaceOutputs = 0;
        std::unordered_map<std::string, const Node*> parameterNames;

        for (const auto& node : graph.nodes) {
            auto definition = NodeLibrary::Find(node);
            if (!definition) {
                Error(state.compiled, node.id, "Unknown node type " + node.type);
                continue;
            }

            vertexOutputs += definition->kind == NodeKind::VertexOutput ? 1 : 0;
            surfaceOutputs += definition->kind == NodeKind::SurfaceOutput ? 1 : 0;

            if (node.type == NodeLibrary::codeType) {
                auto isNumber = [](ValueType type) { return type != ValueType::Texture && type != ValueType::Generic; };
                if (!isNumber(node.codeOutputType))
                    Error(state.compiled, node.id, "The result needs a number or vector type");

                std::unordered_set<std::string> inputNames;
                for (const auto& input : node.codeInputs) {
                    auto isName = !input.name.empty() && !std::isdigit(static_cast<unsigned char>(input.name.front())) &&
                        std::all_of(input.name.begin(), input.name.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; });
                    if (!isName)
                        Error(state.compiled, node.id, "The input " + input.name + " needs a name of letters, digits and underscores");
                    if (!inputNames.insert(input.name).second)
                        Error(state.compiled, node.id, "The input " + input.name + " exists twice");
                    if (!isNumber(input.type))
                        Error(state.compiled, node.id, "The input " + input.name + " needs a number or vector type");
                }
                if (node.code.find("return") == std::string::npos)
                    Error(state.compiled, node.id, "The code has to return the result");
            }

            if (definition->kind != NodeKind::Parameter)
                continue;

            if (node.name.empty()) {
                Error(state.compiled, node.id, "A parameter needs a name");
                continue;
            }

            auto [iter, inserted] = parameterNames.try_emplace(node.name, &node);
            if (!inserted && iter->second->type != node.type)
                Error(state.compiled, node.id, "The parameter " + node.name + " already exists with another type");
        }

        if (vertexOutputs > 1)
            Error(state.compiled, 0, "A graph can only have one vertex output");
        if (surfaceOutputs > 1)
            Error(state.compiled, 0, "A graph can only have one surface output");

        std::unordered_set<std::string> connectedInputs;
        for (const auto& link : graph.links) {
            auto from = graph.GetNode(link.fromNode);
            auto to = graph.GetNode(link.toNode);
            if (!from || !to) {
                Error(state.compiled, 0, "A link references a node which doesn't exist");
                continue;
            }

            auto fromDefinition = NodeLibrary::Find(*from);
            auto toDefinition = NodeLibrary::Find(*to);
            if (!fromDefinition || !toDefinition)
                continue;

            auto output = fromDefinition->FindOutput(link.fromPin);
            auto input = toDefinition->FindInput(link.toPin);
            if (!output)
                Error(state.compiled, from->id, "The node has no output " + link.fromPin);
            if (!input)
                Error(state.compiled, to->id, "The node has no input " + link.toPin);
            if (!output || !input)
                continue;

            if ((output->type == ValueType::Texture) != (input->type == ValueType::Texture))
                Error(state.compiled, to->id, "The input " + GetPinLabel(*to, link.toPin) + " can't be connected to " + GetPinLabel(*from, link.fromPin));

            if (!connectedInputs.insert(std::to_string(link.toNode) + "/" + link.toPin).second)
                Error(state.compiled, to->id, "The input " + GetPinLabel(*to, link.toPin) + " has several links");
        }

    }

    const InputDefinition* Compiler::GetStaticSwitchInput(const Node& node, const NodeDefinition& definition) {

        return &definition.inputs[node.value.x > 0.0f ? 0 : 1];

    }

    ValueType Compiler::ResolveGeneric(State& state, const Node& node, std::vector<uint32_t>& visiting) {

        auto cached = state.genericTypes.find(node.id);
        if (cached != state.genericTypes.end())
            return cached->second;

        // Cycles are reported when emitting the nodes
        if (std::find(visiting.begin(), visiting.end(), node.id) != visiting.end())
            return ValueType::Float;

        visiting.push_back(node.id);

        auto definition = NodeLibrary::Find(node);

        auto staticInput = definition->kind == NodeKind::StaticSwitch ? GetStaticSwitchInput(node, *definition) : nullptr;

        auto type = ValueType::Float;
        for (const auto& input : definition->inputs) {
            if (input.type != ValueType::Generic || (staticInput && &input != staticInput))
                continue;

            auto link = state.graph.GetInputLink(node.id, input.name);
            if (!link)
                continue;

            auto from = state.graph.GetNode(link->fromNode);
            auto fromDefinition = from ? NodeLibrary::Find(*from) : nullptr;
            auto output = fromDefinition ? fromDefinition->FindOutput(link->fromPin) : nullptr;
            // Types are also asked for graphs with errors
            if (!output)
                continue;

            auto linkType = output->type;
            if (linkType == ValueType::Generic)
                linkType = ResolveGeneric(state, *from, visiting);

            type = ValueType(std::max(int32_t(type), int32_t(linkType)));
        }

        visiting.pop_back();

        state.genericTypes[node.id] = type;
        return type;

    }

    ValueType Compiler::GetOutputType(const Graph& graph, uint32_t nodeId, const std::string& pin) {

        auto node = graph.GetNode(nodeId);
        auto definition = node ? NodeLibrary::Find(*node) : nullptr;
        auto output = definition ? definition->FindOutput(pin) : nullptr;
        if (!output)
            return ValueType::Float;

        CompiledGraph compiled;
        Expansion expansion;
        State state = { .graph = graph, .compiled = compiled, .expansion = expansion };

        return GetOutputType(state, *node, *output);

    }

    ValueType Compiler::GetOutputType(State& state, const Node& node, const OutputDefinition& output) {

        if (output.type != ValueType::Generic)
            return output.type;

        std::vector<uint32_t> visiting;
        return ResolveGeneric(state, node, visiting);

    }

    void Compiler::EmitNode(State& state, Function& function, const Node& node) {

        if (function.variables.contains(node.id))
            return;

        auto& compiled = state.compiled;
        auto definition = NodeLibrary::Find(node);

        if (std::find(function.visiting.begin(), function.visiting.end(), node.id) != function.visiting.end()) {
            Error(state.compiled, node.id, "The node is part of a cycle");
            return;
        }

        if (!(definition->stages & function.stage)) {
            Error(state.compiled, node.id, function.stage == VertexStage ? "The node isn't available in the vertex stage" :
                "The node isn't available in the surface stage");
            return;
        }

        compiled.nodeStages[node.id] |= function.stage;

        function.visiting.push_back(node.id);

        auto addVariable = [&](const std::string& pin, ValueType type, const std::string& expression) {
            auto variable = GetVariableName(function, node, *definition, pin);
            function.code += "    " + GetTypeName(type) + " " + variable + " = " + expression + ";\n";
            function.variables[node.id][pin] = variable;
            };

        // Outputs nothing reads don't have to be calculated, e.g. the other channels of a texture
        auto isUsed = [&](const std::string& pin) {
            if (node.id == function.previewNode && pin == function.previewPin)
                return true;
            return std::any_of(state.graph.links.begin(), state.graph.links.end(),
                [&](const Link& link) { return link.fromNode == node.id && link.fromPin == pin; });
            };

        switch (definition->kind) {
        case NodeKind::Constant: {
            auto value = node.value;
            if (definition->isColor)
                value = vec4(Common::ColorConverter::ConvertSRGBToLinear(vec3(value)), value.a);
            addVariable("Value", definition->valueType, GetLiteral(value, definition->valueType));
            break;
        }
        case NodeKind::Parameter:
            addVariable("Value", definition->valueType == ValueType::Texture ? ValueType::Texture : definition->valueType,
                EmitParameter(state, function, node, *definition));
            break;
        case NodeKind::Varying: {
            auto type = GetOutputType(state, node, definition->outputs.front());
            // Evaluations have a single function, which calculates the value in place
            if (function.stage != SurfaceStage) {
                addVariable("Value", type, EmitInput(state, function, node, definition->inputs.front(), type));
            }
            else {
                auto iter = state.varyings.find(node.id);
                if (iter == state.varyings.end()) {
                    if (int32_t(state.varyings.size()) >= maxVaryings) {
                        Error(state.compiled, node.id, "A graph can hand at most " + std::to_string(maxVaryings) + " values to the surface");
                        break;
                    }
                    iter = state.varyings.emplace(node.id, int32_t(state.varyings.size())).first;
                }
                if (std::find(function.usedVaryings.begin(), function.usedVaryings.end(), node.id) == function.usedVaryings.end())
                    function.usedVaryings.push_back(node.id);
                addVariable("Value", type, Convert("graphVarying" + std::to_string(iter->second), ValueType::Vec4, type));
            }
            break;
        }
        case NodeKind::StaticSwitch: {
            auto type = GetOutputType(state, node, definition->outputs.front());
            addVariable("Result", type, EmitInput(state, function, node, *GetStaticSwitchInput(node, *definition), type));
            break;
        }
        case NodeKind::SubgraphInput: {
            // Only converts to the type of the pin, which doesn't need a variable
            const auto& input = definition->inputs.front();
            auto value = EmitInput(state, function, node, input, input.type);
            function.variables[node.id]["Value"] = value;
            break;
        }
        case NodeKind::Reroute:
        case NodeKind::Expression: {
            auto genericType = ValueType::Float;
            std::vector<uint32_t> visiting;
            genericType = ResolveGeneric(state, node, visiting);

            std::vector<std::string> arguments;
            for (const auto& input : definition->inputs) {
                auto targetType = input.type == ValueType::Generic ? genericType : input.type;
                arguments.push_back(EmitInput(state, function, node, input, targetType));
            }

            for (const auto& output : definition->outputs) {
                if (!isUsed(output.name))
                    continue;
                auto type = output.type == ValueType::Generic ? genericType : output.type;
                addVariable(output.name, type, ExpandExpression(output.expression, arguments, function.inputName));
            }

            if (!definition->functions.empty() && function.functions.find(definition->functions) == std::string::npos)
                function.functions += definition->functions;

            // Marks the node as emitted even without any used output
            function.variables[node.id];
            break;
        }
        default:
            function.variables[node.id];
            break;
        }

        function.visiting.pop_back();

    }

    std::string Compiler::EmitInput(State& state, Function& function, const Node& node, const InputDefinition& input,
        ValueType targetType) {

        auto link = state.graph.GetInputLink(node.id, input.name);
        if (!link) {
            if (targetType == ValueType::Texture) {
                // A subgraph on its own doesn't get its textures yet, the graphs using it have to connect them
                auto isOpenInput = node.type == NodeLibrary::subgraphInputType && !state.expansion.origins.contains(node.id);
                if (!isOpenInput)
                    Error(state.compiled, node.id, "The input " + GetPinLabel(node, input.name) + " needs a texture parameter");
                return "0u";
            }

            if (!input.defaultExpression.empty()) {
                // Default expressions can be of any type, they are written for their target
                return ExpandExpression(input.defaultExpression, {}, function.inputName);
            }

            auto value = input.defaultValue;
            auto iter = node.inputValues.find(input.name);
            if (iter != node.inputValues.end())
                value = iter->second;

            // Generic inputs without a link hold a single number
            if (input.type == ValueType::Generic)
                return Convert(GetLiteral(value.x), ValueType::Float, targetType);
            return GetLiteral(value, targetType);
        }

        auto from = state.graph.GetNode(link->fromNode);
        auto fromDefinition = NodeLibrary::Find(*from);
        auto output = fromDefinition->FindOutput(link->fromPin);

        EmitNode(state, function, *from);

        auto fromType = GetOutputType(state, *from, *output);

        auto variables = function.variables.find(from->id);
        if (variables == function.variables.end() || !variables->second.contains(link->fromPin)) {
            // Only happens after an error, which stops the compilation later on
            return GetLiteral(vec4(0.0f), targetType == ValueType::Texture ? ValueType::Float : targetType);
        }

        return Convert(variables->second[link->fromPin], fromType, targetType);

    }

    std::string Compiler::EmitParameter(State& state, Function& function, const Node& node, const NodeDefinition& definition) {

        auto& compiled = state.compiled;

        auto iter = state.parameterIndices.find(node.name);
        if (iter == state.parameterIndices.end()) {
            Parameter parameter = {
                .name = node.name,
                .type = definition.valueType,
                .isColor = definition.isColor,
                .offset = compiled.parameterWords,
                .defaultValue = node.value,
                .range = node.range,
                .group = node.group,
                .order = node.order,
                .nodeId = node.id
            };

            if (definition.valueType == ValueType::Texture) {
                parameter.defaultTexture = node.texture;
                parameter.coordinateScale = node.value.x;
                parameter.fallback = DefaultTexture(int32_t(node.value.y));
            }

            uint32_t words = 1;
            switch (definition.valueType) {
            case ValueType::Vec2: words = 2; break;
            case ValueType::Vec3: words = 3; break;
            case ValueType::Vec4: words = 4; break;
            default: break;
            }

            compiled.parameterWords += words;
            compiled.parameters.push_back(parameter);
            iter = state.parameterIndices.emplace(node.name, compiled.parameters.size() - 1).first;
        }

        auto offset = std::to_string(compiled.parameters[iter->second].offset);
        auto address = function.inputName + ".parameterOffset + " + offset + "u";

        switch (definition.valueType) {
        case ValueType::Float: return "MaterialGraphReadFloat(" + address + ")";
        case ValueType::Vec2: return "MaterialGraphReadVec2(" + address + ")";
        case ValueType::Vec3: return "MaterialGraphReadVec3(" + address + ")";
        case ValueType::Vec4: return "MaterialGraphReadVec4(" + address + ")";
        default: return "materialGraphParameters[" + address + "]";
        }

    }

    std::string Compiler::GetSurfaceFunction(State& state, const Node& output, const std::vector<std::string>& pins,
        std::vector<uint32_t>& usedVaryings, uint32_t stage) {

        Function function = { .stage = stage, .inputName = "surfaceIn" };

        static const std::unordered_map<std::string, std::string> pinMembers = {
            { NodeLibrary::baseColorPin, "baseColor" },
            { NodeLibrary::normalPin, "normal" },
            { NodeLibrary::roughnessPin, "roughness" },
            { NodeLibrary::metalnessPin, "metalness" },
            { NodeLibrary::aoPin, "ao" },
            { NodeLibrary::emissivePin, "emissive" },
            { NodeLibrary::opacityPin, "opacity" },
            { NodeLibrary::reflectancePin, "reflectance" },
            { NodeLibrary::transmissionPin, "transmission" },
            { NodeLibrary::refractiveIndexPin, "refractiveIndex" },
            { NodeLibrary::thicknessPin, "thickness" },
            { NodeLibrary::attenuationColorPin, "attenuationColor" },
            { NodeLibrary::attenuationDistancePin, "attenuationDistance" }
        };

        auto definition = NodeLibrary::Find(output);
        state.compiled.nodeStages[output.id] |= SurfaceStage;

        std::string assignments;
        for (const auto& pin : pins) {
            auto input = definition->FindInput(pin);
            assignments += "    surfaceOut." + pinMembers.at(pin) + " = " + EmitInput(state, function, output, *input, input->type) + ";\n";
        }

        usedVaryings = function.usedVaryings;

        return function.functions + "\nMaterialSurfaceOutput EvaluateMaterialSurface(MaterialSurfaceInput surfaceIn) {\n\n"
            "    MaterialSurfaceOutput surfaceOut = CreateMaterialSurfaceOutput();\n\n" + function.code + (function.code.empty() ? "" : "\n") + assignments +
            "\n    return surfaceOut;\n\n}\n\n";

    }

    std::string Compiler::GetVertexFunctions(State& state, const Node* output, const std::vector<uint32_t>& usedVaryings,
        bool hasOffset, bool withNormal) {

        std::string code;

        if (hasOffset && output) {
            auto definition = NodeLibrary::Find(*output);
            state.compiled.nodeStages[output->id] |= VertexStage;

            Function offset = { .stage = VertexStage, .inputName = "vertexIn" };
            auto input = definition->FindInput(NodeLibrary::positionOffsetPin);
            auto offsetValue = EmitInput(state, offset, *output, *input, ValueType::Vec3);
            code += offset.functions + "\nvec3 EvaluateMaterialVertexOffset(MaterialVertexInput vertexIn) {\n\n" + offset.code +
                (offset.code.empty() ? "" : "\n") + "    return " + offsetValue + ";\n\n}\n";

            if (withNormal) {
                if (output->value.x > 0.0f) {
                    // The surface next to the vertex gives the normal, which only needs the displacement of the graph
                    code += "\nvec3 EvaluateMaterialVertexNormal(MaterialVertexInput vertexIn) {\n\n"
                        "    vec3 up = abs(vertexIn.normal.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);\n"
                        "    vec3 tangent = normalize(cross(up, vertexIn.normal));\n"
                        "    vec3 bitangent = cross(vertexIn.normal, tangent);\n"
                        "    float dist = " + GetLiteral(output->value.y) + ";\n"
                        "    vec3 center = EvaluateMaterialVertexOffset(vertexIn);\n"
                        "    vec3 alongTangent = dist * tangent + EvaluateMaterialVertexOffset(MoveMaterialVertexInput(vertexIn, dist * tangent));\n"
                        "    vec3 alongBitangent = dist * bitangent + EvaluateMaterialVertexOffset(MoveMaterialVertexInput(vertexIn, dist * bitangent));\n"
                        "\n    return normalize(cross(alongTangent - center, alongBitangent - center));\n\n}\n";
                }
                else {
                    Function normal = { .stage = VertexStage, .inputName = "vertexIn" };
                    auto normalInput = definition->FindInput(NodeLibrary::vertexNormalPin);
                    auto normalValue = EmitInput(state, normal, *output, *normalInput, ValueType::Vec3);
                    code += normal.functions + "\nvec3 EvaluateMaterialVertexNormal(MaterialVertexInput vertexIn) {\n\n" + normal.code +
                        (normal.code.empty() ? "" : "\n") + "    return " + normalValue + ";\n\n}\n";
                }
            }
        }
        else if (hasOffset) {
            code += "vec3 EvaluateMaterialVertexOffset(MaterialVertexInput vertexIn) {\n    return vec3(0.0);\n}\n";
            code += "vec3 EvaluateMaterialVertexNormal(MaterialVertexInput vertexIn) {\n    return vertexIn.normal;\n}\n";
        }

        Function varyings = { .stage = VertexStage, .inputName = "vertexIn" };
        std::string assignments;
        for (auto nodeId : usedVaryings) {
            auto node = state.graph.GetNode(nodeId);
            auto definition = NodeLibrary::Find(*node);
            auto type = GetOutputType(state, *node, definition->outputs.front());
            auto value = EmitInput(state, varyings, *node, definition->inputs.front(), type);
            assignments += "    graphVarying" + std::to_string(state.varyings[nodeId]) + " = " + Convert(value, type, ValueType::Vec4) + ";\n";
        }
        code += varyings.functions + "\n// Values handed to the surface\nvoid WriteMaterialVaryings(MaterialVertexInput vertexIn) {\n\n" + varyings.code +
            (varyings.code.empty() ? "" : "\n") + assignments + (assignments.empty() ? "" : "\n") + "}\n\n";

        return code;

    }

    bool Compiler::IsUniform(State& state, const Node& node, const InputDefinition& input, std::vector<uint32_t>& visiting) {

        auto link = state.graph.GetInputLink(node.id, input.name);
        if (!link)
            return input.defaultExpression.find("$IN") == std::string::npos;

        auto from = state.graph.GetNode(link->fromNode);
        auto definition = NodeLibrary::Find(*from);
        if (std::find(visiting.begin(), visiting.end(), from->id) != visiting.end())
            return false;

        switch (definition->kind) {
        case NodeKind::Constant:
            return true;
        case NodeKind::Parameter:
            return definition->valueType != ValueType::Texture;
        case NodeKind::StaticSwitch:
        case NodeKind::Reroute:
        case NodeKind::SubgraphInput:
        case NodeKind::Expression: {
            auto output = definition->FindOutput(link->fromPin);
            if (!(definition->stages & VertexStage) || output->expression.find("$IN") != std::string::npos)
                return false;

            const auto staticInput = definition->kind == NodeKind::StaticSwitch ? GetStaticSwitchInput(*from, *definition) : nullptr;

            visiting.push_back(from->id);
            auto uniform = std::all_of(definition->inputs.begin(), definition->inputs.end(), [&](const InputDefinition& fromInput) {
                return (staticInput && &fromInput != staticInput) || IsUniform(state, *from, fromInput, visiting);
                });
            visiting.pop_back();

            return uniform;
        }
        default:
            return false;
        }

    }

    bool Compiler::IsDisconnected(State& state, const Node& node, const InputDefinition& input, std::vector<uint32_t>& visiting) {

        auto link = state.graph.GetInputLink(node.id, input.name);
        if (!link) {
            auto value = input.defaultValue;
            auto iter = node.inputValues.find(input.name);
            if (iter != node.inputValues.end())
                value = iter->second;
            return input.defaultExpression.empty() && value == vec4(0.0f);
        }

        auto from = state.graph.GetNode(link->fromNode);
        auto definition = NodeLibrary::Find(*from);
        if (std::find(visiting.begin(), visiting.end(), from->id) != visiting.end())
            return false;

        const InputDefinition* fromInput = nullptr;
        if (definition->kind == NodeKind::StaticSwitch)
            fromInput = GetStaticSwitchInput(*from, *definition);
        else if (definition->kind == NodeKind::Reroute || definition->kind == NodeKind::SubgraphInput)
            fromInput = &definition->inputs.front();
        if (!fromInput)
            return false;

        visiting.push_back(from->id);
        auto disconnected = IsDisconnected(state, *from, *fromInput, visiting);
        visiting.pop_back();

        return disconnected;

    }

    std::string Compiler::GetVertexOpacityFunction(State& state, const Node& output) {

        Function function = { .stage = VertexStage, .inputName = "vertexIn" };

        auto input = NodeLibrary::Find(output)->FindInput(NodeLibrary::opacityPin);
        auto value = EmitInput(state, function, output, *input, ValueType::Float);

        return function.functions + "\nfloat EvaluateMaterialOpacity(MaterialVertexInput vertexIn) {\n\n" + function.code +
            (function.code.empty() ? "" : "\n") + "    return " + value + ";\n\n}\n\n";

    }

    std::string Compiler::GetVaryingDeclarations(State& state, const std::vector<uint32_t>& usedVaryings, const char* qualifier) {

        std::string code;
        for (auto nodeId : usedVaryings) {
            auto index = state.varyings[nodeId];
            code += "layout(location = " + std::to_string(varyingLocation + index) + ") " + qualifier +
                " vec4 graphVarying" + std::to_string(index) + ";\n";
        }

        return code;

    }

    std::string Compiler::GetVariableName(Function& function, const Node& node, const NodeDefinition& definition, const std::string& pin) {

        // The name of a parameter tells more than its type
        auto isParameter = definition.kind == NodeKind::Parameter;
        const auto& words = isParameter && !node.name.empty() ? node.name : definition.type;

        std::string name;
        auto startWord = false;
        for (auto character : words) {
            if (!std::isalnum(static_cast<unsigned char>(character))) {
                startWord = !name.empty();
                continue;
            }
            name += startWord ? char(std::toupper(static_cast<unsigned char>(character))) :
                name.empty() ? char(std::tolower(static_cast<unsigned char>(character))) : character;
            startWord = false;
        }

        if (name.empty() || std::isdigit(static_cast<unsigned char>(name.front())))
            name = "value" + name;

        if (isParameter) {
            name += "Parameter";
        }
        else {
            auto capitalized = true;
            for (auto character : pin) {
                if (!std::isalnum(static_cast<unsigned char>(character))) {
                    capitalized = true;
                    continue;
                }
                name += capitalized ? char(std::toupper(static_cast<unsigned char>(character))) : character;
                capitalized = false;
            }
        }

        auto count = ++function.variableNames[name];
        return count > 1 ? name + std::to_string(count) : name;

    }

    std::string Compiler::Enclose(const std::string& expression) {

        int32_t depth = 0;
        for (auto character : expression) {
            if (character == '(' || character == '[') {
                depth++;
            }
            else if (character == ')' || character == ']') {
                depth--;
            }
            else if (depth == 0 && !std::isalnum(static_cast<unsigned char>(character)) && character != '_' && character != '.') {
                return "(" + expression + ")";
            }
        }

        return expression;

    }

    std::string Compiler::GetTypeName(ValueType type) {

        switch (type) {
        case ValueType::Float: return "float";
        case ValueType::Vec2: return "vec2";
        case ValueType::Vec3: return "vec3";
        case ValueType::Vec4: return "vec4";
        case ValueType::Texture: return "uint";
        default: return "float";
        }

    }

    std::string Compiler::GetLiteral(float value) {

        // The shortest form which keeps the value, the same value always generates the same code
        char buffer[64];
        auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        std::string literal(buffer, result.ptr);

        if (literal.find_first_of(".en") == std::string::npos)
            literal += ".0";

        return literal.find_first_of("n") != std::string::npos ? "0.0" : literal;

    }

    std::string Compiler::GetLiteral(vec4 value, ValueType type) {

        switch (type) {
        case ValueType::Vec2: return "vec2(" + GetLiteral(value.x) + ", " + GetLiteral(value.y) + ")";
        case ValueType::Vec3: return "vec3(" + GetLiteral(value.x) + ", " + GetLiteral(value.y) + ", " + GetLiteral(value.z) + ")";
        case ValueType::Vec4: return "vec4(" + GetLiteral(value.x) + ", " + GetLiteral(value.y) + ", " + GetLiteral(value.z) + ", " + GetLiteral(value.w) + ")";
        default: return GetLiteral(value.x);
        }

    }

    std::string Compiler::Convert(const std::string& expression, ValueType from, ValueType to) {

        if (from == to)
            return expression;

        if (from == ValueType::Float)
            return GetTypeName(to) + "(" + expression + ")";

        switch (to) {
        case ValueType::Float:
            return Enclose(expression) + ".x";
        case ValueType::Vec2:
            return Enclose(expression) + ".xy";
        case ValueType::Vec3:
            if (from == ValueType::Vec4)
                return Enclose(expression) + ".xyz";
            return "vec3(" + expression + ", 0.0)";
        case ValueType::Vec4:
            if (from == ValueType::Vec3)
                return "vec4(" + expression + ", 1.0)";
            return "vec4(" + expression + ", 0.0, 1.0)";
        default:
            return expression;
        }

    }

    std::string Compiler::ExpandExpression(const std::string& expression, const std::vector<std::string>& arguments,
        const std::string& inputName) {

        const std::string inputToken = "$IN";

        std::string result;
        for (size_t i = 0; i < expression.size(); i++) {
            if (expression[i] != '$') {
                result += expression[i];
                continue;
            }

            if (expression.compare(i, inputToken.size(), inputToken) == 0) {
                result += inputName;
                i += inputToken.size() - 1;
                continue;
            }

            auto start = i;
            size_t index = 0;
            while (i + 1 < expression.size() && std::isdigit(static_cast<unsigned char>(expression[i + 1])))
                index = index * 10 + size_t(expression[++i] - '0');
            AE_ASSERT(i > start && index < arguments.size() && "Expression of a node uses an input it doesn't have");
            if (index < arguments.size())
                result += Enclose(arguments[index]);
        }

        return result;

    }

}
