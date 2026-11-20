#pragma once

#include "../System.h"
#include "../common/Hash.h"
#include "../common/Ref.h"
#include "../resource/Resource.h"
#include "../texture/Texture2D.h"

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Atlas::MaterialGraph {

    enum class ValueType {
        Float = 0,
        Vec2,
        Vec3,
        Vec4,
        Texture,
        // Takes the largest type of the connected generic inputs
        Generic
    };

    enum StageBits : uint32_t {
        VertexStage = (1 << 0),
        SurfaceStage = (1 << 1),
        AllStages = VertexStage | SurfaceStage
    };

    struct NodeDefinition;

    // Fallback of a texture parameter without a texture
    enum class DefaultTexture {
        White = 0,
        Black,
        Normal
    };

    struct CodeInput {
        std::string name;
        ValueType type = ValueType::Float;
    };

    struct Node {
        uint32_t id = 0;
        // Name of the definition in the node library
        std::string type;

        vec2 position = vec2(0.0f);

        // Inputs without an entry use the default of their definition
        std::unordered_map<std::string, vec4> inputValues;

        // Constant or default of a parameter. Texture parameters: coordinate scale in x, fallback in y. Vertex output:
        // whether it derives normals in x, sampling distance in y
        vec4 value = vec4(0.0f);
        // Name of a parameter or the text of a comment
        std::string name;
        ResourceHandle<Texture::Texture2D> texture;

        // Parameters only, without limits if x isn't smaller than y
        vec2 range = vec2(0.0f);
        std::string group;
        int32_t order = 0;

        // Size of frames, in the units of the canvas
        vec2 size = vec2(0.0f);

        // Code nodes: the body of a GLSL function, which gets the inputs as arguments and returns the output
        std::string code;
        std::vector<CodeInput> codeInputs;
        ValueType codeOutputType = ValueType::Float;
        // Pins of a code node, NodeLibrary::UpdateCodeDefinition replaces it after the code or its pins changed
        Ref<const NodeDefinition> codeDefinition;
    };

    struct Link {
        uint32_t fromNode = 0;
        std::string fromPin;
        uint32_t toNode = 0;
        std::string toPin;
    };

    struct Parameter {
        std::string name;
        ValueType type = ValueType::Float;
        // Colors are authored in sRGB, the renderer converts them
        bool isColor = false;

        // Position in the values of the material, in 32 bit words
        uint32_t offset = 0;

        vec4 defaultValue = vec4(0.0f);
        ResourceHandle<Texture::Texture2D> defaultTexture;
        DefaultTexture fallback = DefaultTexture::White;
        // Texture coordinates of the graph relative to the ones of the mesh, for the texture streaming
        float coordinateScale = 1.0f;

        vec2 range = vec2(0.0f);
        std::string group;
        int32_t order = 0;

        uint32_t nodeId = 0;

        bool HasRange() const { return range.x < range.y; }

        // Writes a value in the form the shaders read it
        void WriteValue(vec4 value, uint32_t* words) const;
    };

    struct CompileError {
        // 0 for errors of the whole graph
        uint32_t nodeId = 0;
        std::string message;
    };

    // Result of compiling a graph. Shader files are registered with the shader loader
    struct CompiledGraph {
        bool valid = false;
        std::vector<CompileError> errors;

        bool hasVertexOffset = false;
        // Otherwise the fragments are the ones of the regular material
        bool hasSurface = false;
        bool hasOpacity = false;
        // The opacity only depends on parameters, it's evaluated per vertex and cuts out whole draws
        bool hasUniformOpacity = false;
        bool writesNormal = false;

        // Shadow files are empty if the regular shadow pipeline can be used
        std::string mainVertexFile;
        std::string mainFragmentFile;
        std::string shadowVertexFile;
        std::string shadowFragmentFile;
        // Deforms the vertices for hardware ray tracing, empty without a vertex offset
        std::string deformFile;
        // Evaluates the surface of hits in ray tracing pipelines, empty without a surface
        std::string hitFile;
        // Alpha tests the candidate hits in ray tracing pipelines, empty without an opacity
        std::string hitOpacityFile;

        std::string mainVertexCode;
        std::string mainFragmentCode;
        std::string shadowVertexCode;
        std::string shadowFragmentCode;
        std::string deformCode;
        std::string hitCode;
        std::string hitOpacityCode;

        std::vector<Parameter> parameters;
        uint32_t parameterWords = 0;

        std::unordered_map<uint32_t, uint32_t> nodeStages;

        // Hash of the generated code, graphs generating the same code share pipelines
        Hash hash = 0;

        bool UsesShadowPipeline() const { return !shadowVertexFile.empty(); }

        // A discard in the fragments disables early depth tests
        bool CutsOutFragments() const { return hasOpacity && !hasUniformOpacity; }
    };

    // A fragment shader evaluating a part of a graph outside of the regular passes, see Evaluator
    struct EvaluationShader {
        bool valid = false;
        std::vector<CompileError> errors;

        std::string fragmentFile;

        // The layout doesn't depend on the evaluated part, all shaders of a graph share the same parameter values
        std::vector<Parameter> parameters;
        uint32_t parameterWords = 0;

        bool usesTime = false;
    };

    enum class BakeMode {
        // Maps with the surface at one point in time for the positions of one mesh
        Textures = 0,
        // A single value per property, which works for every mesh
        Average
    };

    // The properties of a material from before a bake replaced them
    struct UnbakedProperties {
        vec3 baseColor = vec3(1.0f);
        vec3 emissiveColor = vec3(0.0f);
        float emissiveIntensity = 1.0f;
        float opacity = 1.0f;
        float roughness = 1.0f;
        float metalness = 0.0f;
        float ao = 1.0f;
        float normalScale = 0.5f;
        // Paths by the name of the property, textures which aren't in use shouldn't stay loaded
        std::unordered_map<std::string, std::string> maps;
    };

    // How the surface of a graph gets into the properties of a material, which ray tracing uses
    struct BakeSettings {
        BakeMode mode = BakeMode::Textures;
        int32_t resolution = 1024;
        float time = 0.0f;
        // Empty for the sphere of the material editor
        std::string meshPath;

        // 0 if it wasn't baked yet
        Hash bakedState = 0;
        UnbakedProperties unbaked;
    };

    /**
     * A material graph describes vertices and surfaces of a material with nodes, from which shaders are generated.
     * The structure of a graph decides on the pipelines, parameter values and their textures are part of the
     * materials using the graph and can change without any compilation.
     */
    class Graph {

    public:
        Graph() = default;

        Graph(const Graph& that);

        Graph& operator=(const Graph& that);

        // Only consists of the two output nodes
        static Ref<Graph> CreateDefault();

        Node& AddNode(const std::string& type, vec2 position = vec2(0.0f));

        void RemoveNode(uint32_t id);

        Node* GetNode(uint32_t id);

        const Node* GetNode(uint32_t id) const;

        // An input only has one link, a new link replaces the existing one
        bool Connect(uint32_t fromNode, const std::string& fromPin, uint32_t toNode, const std::string& toPin);

        void Disconnect(uint32_t toNode, const std::string& toPin);

        const Link* GetInputLink(uint32_t toNode, const std::string& toPin) const;

        // Generates the shaders from the current state, has to be called after a structural change
        Ref<const CompiledGraph> Compile();

        // Thread safe, null if the graph was never compiled
        Ref<const CompiledGraph> GetCompiled() const;

        std::string name;

        std::vector<Node> nodes;
        std::vector<Link> links;

        // Largest distance a vertex moves in object space, culling needs it since the bounds stay those of the mesh
        float maxDisplacement = 0.0f;

        // With any user pipelines are created in place instead of in the background, e.g. while editing to not show
        // the fallback
        std::atomic_int32_t blockingPipelineUsers = 0;

        // The next node gets this id, ids stay unique over the lifetime of a graph
        uint32_t nextNodeId = 1;

    private:
        mutable std::mutex compiledMutex;
        Ref<const CompiledGraph> compiled;

    };

}
