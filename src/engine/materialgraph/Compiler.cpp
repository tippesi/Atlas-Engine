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

        State state = { .graph = graph, .compiled = *compiled };

        ValidateGraph(state);
        if (!compiled->errors.empty())
            return compiled;

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

        const auto& surfaceInputs = NodeLibrary::Find(NodeLibrary::surfaceOutputType)->inputs;

        compiled->hasVertexOffset = isLinked(vertexOutput, NodeLibrary::positionOffsetPin);
        compiled->hasSurface = std::any_of(surfaceInputs.begin(), surfaceInputs.end(),
            [&](const auto& input) { return isLinked(surfaceOutput, input.name); });
        compiled->hasOpacity = isLinked(surfaceOutput, NodeLibrary::opacityPin);
        compiled->writesNormal = isLinked(surfaceOutput, NodeLibrary::normalPin);

        if (compiled->hasOpacity) {
            std::vector<uint32_t> visiting;
            auto opacityInput = NodeLibrary::Find(NodeLibrary::surfaceOutputType)->FindInput(NodeLibrary::opacityPin);
            compiled->hasUniformOpacity = IsUniform(state, *surfaceOutput, *opacityInput, visiting);
        }

        auto hasVertexNormal = isLinked(vertexOutput, NodeLibrary::vertexNormalPin);

        std::string mainSurface, shadowSurface, vertexOpacity;
        std::vector<uint32_t> mainVaryings, shadowVaryings;
        if (compiled->hasSurface) {
            std::vector<std::string> pins;
            for (const auto& input : surfaceInputs) {
                if (input.name != NodeLibrary::opacityPin || compiled->CutsOutFragments())
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
        auto shadowVertexFunctions = GetVertexFunctions(state, vertexOutput, shadowVaryings, isLinked(vertexOutput, NodeLibrary::positionOffsetPin), false);
        // Hardware ray tracing deforms the vertices in a compute pass, which has no varyings
        auto deformVertexFunctions = compiled->hasVertexOffset ? GetVertexFunctions(state, vertexOutput, {}, true, true) : std::string();

        // Ray tracing pipelines call the surface per hit, which has no varyings either, the stages of the nodes stay the
        // ones of the rasterizer
        std::string hitSurface;
        if (compiled->hasSurface) {
            auto nodeStages = compiled->nodeStages;
            std::vector<std::string> pins;
            for (const auto& input : surfaceInputs) {
                if (input.name != NodeLibrary::opacityPin)
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
            return compiled;

        compiled->valid = true;
        if (!usesVertexStage && !compiled->hasSurface)
            return compiled;

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
            define("MATERIAL_GRAPH_OPACITY", compiled->CutsOutFragments()) + common +
            GetVaryingDeclarations(state, mainVaryings, "in") + mainSurface +
            "#include <../deferred/geometry.fsh>\n";

        if (usesShadow) {
            compiled->shadowVertexCode = vertexMacros +
                define("MATERIAL_GRAPH_VERTEX_OFFSET", isLinked(vertexOutput, NodeLibrary::positionOffsetPin)) +
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

        return compiled;

    }

    EvaluationShader Compiler::CompileBake(const Graph& graph) {

        CompiledGraph compiled;
        State state = { .graph = graph, .compiled = compiled };

        ValidateGraph(state);
        if (!compiled.errors.empty())
            return { .errors = compiled.errors };

        AddEvaluationParameters(state);

        auto surfaceOutput = std::find_if(graph.nodes.begin(), graph.nodes.end(),
            [](const Node& node) { return node.type == NodeLibrary::surfaceOutputType; });
        if (surfaceOutput == graph.nodes.end())
            return { .errors = { { 0, "The graph has no surface output" } } };

        std::vector<std::string> pins;
        for (const auto& input : NodeLibrary::Find(NodeLibrary::surfaceOutputType)->inputs)
            pins.push_back(input.name);

        std::vector<uint32_t> usedVaryings;
        auto code = GetSurfaceFunction(state, *surfaceOutput, pins, usedVaryings, AllStages);

        return FinishEvaluation(compiled, code, "");

    }

    EvaluationShader Compiler::CompilePreview(const Graph& graph, uint32_t nodeId) {

        CompiledGraph compiled;
        State state = { .graph = graph, .compiled = compiled };

        ValidateGraph(state);
        if (!compiled.errors.empty())
            return { .errors = compiled.errors };

        AddEvaluationParameters(state);

        auto node = graph.GetNode(nodeId);
        if (!node)
            return { .errors = { { nodeId, "The node doesn't exist" } } };

        return FinishEvaluation(compiled, GetPreviewFunction(state, *node), "#define EVALUATE_PREVIEW\n");

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
            auto definition = NodeLibrary::Find(node->type);
            if (definition->kind == NodeKind::Parameter)
                EmitParameter(state, function, *node, *definition);
        }

    }

    std::string Compiler::GetPreviewFunction(State& state, const Node& node) {

        const std::string signature = "vec4 EvaluateMaterialPreview(MaterialSurfaceInput surfaceIn) {\n";

        auto definition = NodeLibrary::Find(node.type);

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
        if (definition->kind == NodeKind::VertexOutput) {
            value = EmitInput(state, function, node, *definition->FindInput(NodeLibrary::positionOffsetPin), ValueType::Vec3);
        }
        else {
            if (definition->outputs.empty()) {
                Error(state, node.id, "The node has no value");
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
                value = "MGSampleTexture(" + variable + ", surfaceIn.texCoord).rgb";
            else
                value = Convert(variable, type, ValueType::Vec3);

            // Tangent space normals are shown like in a normal map
            if (definition->category == "Normal" || output->name == "Normal")
                value = "0.5 * (" + value + ") + 0.5";
        }

        return signature + "\n" + function.code + "\n    return vec4(" + value + ", 1.0);\n\n}\n\n";

    }

    void Compiler::Error(State& state, uint32_t nodeId, const std::string& message) {

        // A node might be emitted for several functions, which would report the same error again
        auto& errors = state.compiled.errors;
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
            auto definition = NodeLibrary::Find(node.type);
            if (!definition) {
                Error(state, node.id, "Unknown node type " + node.type);
                continue;
            }

            vertexOutputs += definition->kind == NodeKind::VertexOutput ? 1 : 0;
            surfaceOutputs += definition->kind == NodeKind::SurfaceOutput ? 1 : 0;

            if (definition->kind != NodeKind::Parameter)
                continue;

            if (node.name.empty()) {
                Error(state, node.id, "A parameter needs a name");
                continue;
            }

            auto [iter, inserted] = parameterNames.try_emplace(node.name, &node);
            if (!inserted && iter->second->type != node.type)
                Error(state, node.id, "The parameter " + node.name + " already exists with another type");
        }

        if (vertexOutputs > 1)
            Error(state, 0, "A graph can only have one vertex output");
        if (surfaceOutputs > 1)
            Error(state, 0, "A graph can only have one surface output");

        std::unordered_set<std::string> connectedInputs;
        for (const auto& link : graph.links) {
            auto from = graph.GetNode(link.fromNode);
            auto to = graph.GetNode(link.toNode);
            if (!from || !to) {
                Error(state, 0, "A link references a node which doesn't exist");
                continue;
            }

            auto fromDefinition = NodeLibrary::Find(from->type);
            auto toDefinition = NodeLibrary::Find(to->type);
            if (!fromDefinition || !toDefinition)
                continue;

            auto output = fromDefinition->FindOutput(link.fromPin);
            auto input = toDefinition->FindInput(link.toPin);
            if (!output)
                Error(state, from->id, "The node has no output " + link.fromPin);
            if (!input)
                Error(state, to->id, "The node has no input " + link.toPin);
            if (!output || !input)
                continue;

            if ((output->type == ValueType::Texture) != (input->type == ValueType::Texture))
                Error(state, to->id, "The input " + link.toPin + " can't be connected to " + link.fromPin);

            if (!connectedInputs.insert(std::to_string(link.toNode) + "/" + link.toPin).second)
                Error(state, to->id, "The input " + link.toPin + " has several links");
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

        auto definition = NodeLibrary::Find(node.type);

        auto staticInput = definition->kind == NodeKind::StaticSwitch ? GetStaticSwitchInput(node, *definition) : nullptr;

        auto type = ValueType::Float;
        for (const auto& input : definition->inputs) {
            if (input.type != ValueType::Generic || (staticInput && &input != staticInput))
                continue;

            auto link = state.graph.GetInputLink(node.id, input.name);
            if (!link)
                continue;

            auto from = state.graph.GetNode(link->fromNode);
            auto fromDefinition = NodeLibrary::Find(from->type);
            auto output = fromDefinition->FindOutput(link->fromPin);

            auto linkType = output->type;
            if (linkType == ValueType::Generic)
                linkType = ResolveGeneric(state, *from, visiting);

            type = ValueType(std::max(int32_t(type), int32_t(linkType)));
        }

        visiting.pop_back();

        state.genericTypes[node.id] = type;
        return type;

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
        auto definition = NodeLibrary::Find(node.type);

        if (std::find(function.visiting.begin(), function.visiting.end(), node.id) != function.visiting.end()) {
            Error(state, node.id, "The node is part of a cycle");
            return;
        }

        if (!(definition->stages & function.stage)) {
            Error(state, node.id, function.stage == VertexStage ? "The node isn't available in the vertex stage" :
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
                        Error(state, node.id, "A graph can hand at most " + std::to_string(maxVaryings) + " values to the surface");
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
                Error(state, node.id, "The input " + input.name + " needs a texture parameter");
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
        auto fromDefinition = NodeLibrary::Find(from->type);
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
        case ValueType::Float: return "MGReadFloat(" + address + ")";
        case ValueType::Vec2: return "MGReadVec2(" + address + ")";
        case ValueType::Vec3: return "MGReadVec3(" + address + ")";
        case ValueType::Vec4: return "MGReadVec4(" + address + ")";
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
            { NodeLibrary::opacityPin, "opacity" }
        };

        auto definition = NodeLibrary::Find(output.type);
        state.compiled.nodeStages[output.id] |= SurfaceStage;

        std::string assignments;
        for (const auto& pin : pins) {
            auto input = definition->FindInput(pin);
            assignments += "    surfaceOut." + pinMembers.at(pin) + " = " + EmitInput(state, function, output, *input, input->type) + ";\n";
        }

        usedVaryings = function.usedVaryings;

        return "\nMaterialSurfaceOutput EvaluateMaterialSurface(MaterialSurfaceInput surfaceIn) {\n\n"
            "    MaterialSurfaceOutput surfaceOut = CreateMaterialSurfaceOutput();\n\n" + function.code + (function.code.empty() ? "" : "\n") + assignments +
            "\n    return surfaceOut;\n\n}\n\n";

    }

    std::string Compiler::GetVertexFunctions(State& state, const Node* output, const std::vector<uint32_t>& usedVaryings,
        bool hasOffset, bool withNormal) {

        std::string code;

        if (hasOffset && output) {
            auto definition = NodeLibrary::Find(output->type);
            state.compiled.nodeStages[output->id] |= VertexStage;

            Function offset = { .stage = VertexStage, .inputName = "vertexIn" };
            auto input = definition->FindInput(NodeLibrary::positionOffsetPin);
            auto offsetValue = EmitInput(state, offset, *output, *input, ValueType::Vec3);
            code += "\nvec3 EvaluateMaterialVertexOffset(MaterialVertexInput vertexIn) {\n\n" + offset.code +
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
                    code += "\nvec3 EvaluateMaterialVertexNormal(MaterialVertexInput vertexIn) {\n\n" + normal.code +
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
            auto definition = NodeLibrary::Find(node->type);
            auto type = GetOutputType(state, *node, definition->outputs.front());
            auto value = EmitInput(state, varyings, *node, definition->inputs.front(), type);
            assignments += "    graphVarying" + std::to_string(state.varyings[nodeId]) + " = " + Convert(value, type, ValueType::Vec4) + ";\n";
        }
        code += "\n// Values handed to the surface\nvoid WriteMaterialVaryings(MaterialVertexInput vertexIn) {\n\n" + varyings.code +
            (varyings.code.empty() ? "" : "\n") + assignments + (assignments.empty() ? "" : "\n") + "}\n\n";

        return code;

    }

    bool Compiler::IsUniform(State& state, const Node& node, const InputDefinition& input, std::vector<uint32_t>& visiting) {

        auto link = state.graph.GetInputLink(node.id, input.name);
        if (!link)
            return input.defaultExpression.find("$IN") == std::string::npos;

        auto from = state.graph.GetNode(link->fromNode);
        auto definition = NodeLibrary::Find(from->type);
        if (std::find(visiting.begin(), visiting.end(), from->id) != visiting.end())
            return false;

        switch (definition->kind) {
        case NodeKind::Constant:
            return true;
        case NodeKind::Parameter:
            return definition->valueType != ValueType::Texture;
        case NodeKind::StaticSwitch:
        case NodeKind::Reroute:
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

    std::string Compiler::GetVertexOpacityFunction(State& state, const Node& output) {

        Function function = { .stage = VertexStage, .inputName = "vertexIn" };

        auto input = NodeLibrary::Find(output.type)->FindInput(NodeLibrary::opacityPin);
        auto value = EmitInput(state, function, output, *input, ValueType::Float);

        return "\nfloat EvaluateMaterialOpacity(MaterialVertexInput vertexIn) {\n\n" + function.code +
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

            auto index = size_t(expression[i + 1] - '0');
            AE_ASSERT(index < arguments.size() && "Expression of a node uses an input it doesn't have");
            if (index < arguments.size())
                result += Enclose(arguments[index]);
            i++;
        }

        return result;

    }

}
