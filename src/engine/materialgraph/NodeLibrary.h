#pragma once

#include "MaterialGraph.h"

#include <map>
#include <string>
#include <vector>

namespace Atlas::MaterialGraph {

    enum class NodeKind {
        // Outputs are generated from the expressions of the definition
        Expression = 0,
        // Baked into the shader, changing the value generates a new shader
        Constant,
        // Read from the values of the material, can change at any time
        Parameter,
        // Hands a value from the vertex to the surface stage
        Varying,
        VertexOutput,
        SurfaceOutput,
        // Only a note in the editor
        Comment,
        // Only compiles one of its inputs, the other one isn't part of the shaders
        StaticSwitch,
        // Passes the value on, keeps links tidy in the editor
        Reroute,
        // Stands for the nodes of another graph, the compiler replaces it with them
        Subgraph,
        // A value a subgraph gets from the node using it
        SubgraphInput,
        // A value a subgraph hands to the node using it
        SubgraphOutput,
        // Only a box around nodes in the editor
        Frame
    };

    struct InputDefinition {
        std::string name;
        ValueType type = ValueType::Float;
        vec4 defaultValue = vec4(0.0f);
        // Used instead of the inline value if nothing is connected, e.g. the texture coordinates of the mesh
        std::string defaultExpression;

        bool operator==(const InputDefinition& that) const = default;
    };

    struct OutputDefinition {
        std::string name;
        ValueType type = ValueType::Float;
        // $0, $1, ... are the inputs, $IN the input structure of the stage
        std::string expression;

        bool operator==(const OutputDefinition& that) const = default;
    };

    struct NodeDefinition {
        std::string type;
        std::string category;
        std::string tooltip;

        NodeKind kind = NodeKind::Expression;
        uint32_t stages = AllStages;

        std::vector<InputDefinition> inputs;
        std::vector<OutputDefinition> outputs;

        // GLSL functions the expressions call, guarded such that a shader can contain them several times
        std::string functions;

        // Initial value of Node::value
        vec4 defaultValue = vec4(0.0f);
        // Constants and parameters
        ValueType valueType = ValueType::Float;
        bool isColor = false;

        const InputDefinition* FindInput(const std::string& name) const;

        const OutputDefinition* FindOutput(const std::string& name) const;
    };

    class NodeLibrary {

    public:
        static const NodeDefinition* Find(const std::string& type);

        // Code and subgraph nodes have their own pins, all other nodes the ones of their type
        static const NodeDefinition* Find(const Node& node);

        // Creates the own pins of a node, e.g. after the code, the type or the subgraph of the node changed
        static void UpdateDefinition(Node& node);

        // The inputs or outputs of a subgraph in the order nodes using it show them
        static std::vector<const Node*> GetSubgraphPins(const std::vector<Node>& nodes, const std::string& pinType);

        // The parameters of the function of a code node, e.g. "vec3 color, float strength"
        static std::string GetCodeParameters(const Node& node);

        // Names of the types in files and jobs, float to vec4 are also the GLSL types of code nodes
        static std::string GetTypeName(ValueType type);

        // Unknown names are generic
        static ValueType FindType(const std::string& name);

        // All definitions in the order they are presented
        static const std::vector<NodeDefinition>& GetDefinitions();

        static const std::string codeType;
        static const std::string subgraphType;
        static const std::string subgraphInputType;
        static const std::string subgraphOutputType;

        // The pin of a subgraph input with the value while nodes using the subgraph aren't connected
        static const std::string subgraphDefaultPin;
        // Stands for defaults which come from nodes of the subgraph
        static const std::string subgraphDefaultExpression;
        static const std::string vertexOutputType;
        static const std::string surfaceOutputType;

        // Names of the pins of the output nodes
        static const std::string positionOffsetPin;
        static const std::string vertexNormalPin;
        static const std::string baseColorPin;
        static const std::string normalPin;
        static const std::string roughnessPin;
        static const std::string metalnessPin;
        static const std::string aoPin;
        static const std::string emissivePin;
        static const std::string opacityPin;
        static const std::string reflectancePin;
        static const std::string transmissionPin;
        static const std::string refractiveIndexPin;
        static const std::string thicknessPin;
        static const std::string attenuationColorPin;
        static const std::string attenuationDistancePin;

    private:
        static std::vector<NodeDefinition> CreateDefinitions();

        // The function of a code node from its code, inputs and output type
        static void UpdateCodeDefinition(Node& node);

        static void UpdateSubgraphDefinition(Node& node);

    };

}
