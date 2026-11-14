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
        // Only a box around nodes in the editor
        Frame
    };

    struct InputDefinition {
        std::string name;
        ValueType type = ValueType::Float;
        vec4 defaultValue = vec4(0.0f);
        // Used instead of the inline value if nothing is connected, e.g. the texture coordinates of the mesh
        std::string defaultExpression;
    };

    struct OutputDefinition {
        std::string name;
        ValueType type = ValueType::Float;
        // $0, $1, ... are the inputs, $IN the input structure of the stage
        std::string expression;
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

        // Code nodes have their own pins, all other nodes the ones of their type
        static const NodeDefinition* Find(const Node& node);

        // Creates the pins and the function of a code node from its code, inputs and output type
        static void UpdateCodeDefinition(Node& node);

        // Names of the types in files and jobs, float to vec4 are also the GLSL types of code nodes
        static std::string GetTypeName(ValueType type);

        // Unknown names are generic
        static ValueType FindType(const std::string& name);

        // All definitions in the order they are presented
        static const std::vector<NodeDefinition>& GetDefinitions();

        static const std::string codeType;
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

    private:
        static std::vector<NodeDefinition> CreateDefinitions();

    };

}
