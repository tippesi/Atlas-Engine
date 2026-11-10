#pragma once

#include "MaterialGraph.h"
#include "NodeLibrary.h"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace Atlas::MaterialGraph {

    /**
     * Generates shaders from a graph. Every stage of every pass gets its own function with only the nodes this stage
     * needs, e.g. shadows only evaluate the displacement and the opacity. The generated files include the regular
     * geometry and shadow shaders, which call the functions of the graph behind macros.
     */
    class Compiler {

    public:
        static Ref<CompiledGraph> Compile(const Graph& graph);

        // All pins of the surface output, without a vertex stage. Values the graph calculates in the vertex
        // stage are calculated per fragment here
        static EvaluationShader CompileBake(const Graph& graph);

        // The value of a node, the first used output of nodes with several
        static EvaluationShader CompilePreview(const Graph& graph, uint32_t nodeId);

        // The largest number of values handed from the vertex to the surface stage
        static constexpr int32_t maxVaryings = 4;
        // Location of the first value handed from the vertex to the surface stage
        static constexpr int32_t varyingLocation = 13;

    private:
        // A generated function, nodes are emitted once and in the order of their dependencies
        struct Function {
            uint32_t stage = VertexStage;
            std::string inputName;
            std::string code;
            // Variable per output of the emitted nodes. They are named after their node and numbered in the order they
            // are emitted, such that the position of nodes in the graph or their ids don't change the generated code
            std::unordered_map<uint32_t, std::unordered_map<std::string, std::string>> variables;
            std::vector<uint32_t> visiting;
            std::unordered_map<std::string, int32_t> variableNames;
            // The varyings the function reads, in the order they were found
            std::vector<uint32_t> usedVaryings;
            // An output which is calculated without a link
            uint32_t previewNode = 0;
            std::string previewPin;
        };

        struct State {
            const Graph& graph;
            CompiledGraph& compiled;
            std::unordered_map<uint32_t, ValueType> genericTypes;
            // Index of the nodes handing values to the surface
            std::unordered_map<uint32_t, int32_t> varyings;
            std::map<std::string, size_t> parameterIndices;
        };

        static void Error(State& state, uint32_t nodeId, const std::string& message);

        static void ValidateGraph(State& state);

        static const InputDefinition* GetStaticSwitchInput(const Node& node, const NodeDefinition& definition);

        static EvaluationShader FinishEvaluation(const CompiledGraph& compiled, const std::string& code, const char* macros);

        static void AddEvaluationParameters(State& state);

        static std::string GetPreviewFunction(State& state, const Node& node);

        static ValueType ResolveGeneric(State& state, const Node& node, std::vector<uint32_t>& visiting);

        static ValueType GetOutputType(State& state, const Node& node, const OutputDefinition& output);

        static void EmitNode(State& state, Function& function, const Node& node);

        static std::string EmitInput(State& state, Function& function, const Node& node, const InputDefinition& input,
            ValueType targetType);

        static std::string EmitParameter(State& state, Function& function, const Node& node, const NodeDefinition& definition);

        static std::string GetSurfaceFunction(State& state, const Node& output, const std::vector<std::string>& pins,
            std::vector<uint32_t>& usedVaryings, uint32_t stage = SurfaceStage);

        static std::string GetVertexFunctions(State& state, const Node* output, const std::vector<uint32_t>& usedVaryings,
            bool hasOffset, bool withNormal);

        // Inputs which only depend on parameters and constants are the same for a whole draw
        static bool IsUniform(State& state, const Node& node, const InputDefinition& input, std::vector<uint32_t>& visiting);

        static std::string GetVertexOpacityFunction(State& state, const Node& output);

        static std::string GetVaryingDeclarations(State& state, const std::vector<uint32_t>& usedVaryings, const char* qualifier);

        // A readable name, e.g. fractalNoiseResult or glowParameter. The suffix keeps names from hiding GLSL functions
        static std::string GetVariableName(Function& function, const Node& node, const NodeDefinition& definition, const std::string& pin);

        // Only expressions which aren't a single term need parentheses
        static std::string Enclose(const std::string& expression);

        static std::string GetTypeName(ValueType type);

        static std::string GetLiteral(float value);

        static std::string GetLiteral(vec4 value, ValueType type);

        static std::string Convert(const std::string& expression, ValueType from, ValueType to);

        static std::string ExpandExpression(const std::string& expression, const std::vector<std::string>& arguments,
            const std::string& inputName);

    };

}
