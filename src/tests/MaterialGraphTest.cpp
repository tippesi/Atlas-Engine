#include <gtest/gtest.h>

#include "materialgraph/Baker.h"
#include "materialgraph/Compiler.h"
#include "materialgraph/MaterialGraph.h"
#include "materialgraph/MaterialGraphSerializer.h"
#include "materialgraph/NodeLibrary.h"
#include "loader/ShaderLoader.h"
#include "mesh/MeshSerializer.h"

#include <string>

using namespace Atlas;
using namespace Atlas::MaterialGraph;

// The two outputs and helpers to wire up small graphs
class TestGraph {

public:
    TestGraph() {

        vertexOutput = graph.AddNode(NodeLibrary::vertexOutputType).id;
        surfaceOutput = graph.AddNode(NodeLibrary::surfaceOutputType).id;

    }

    uint32_t Add(const std::string& type, const std::string& name = "") {

        auto& node = graph.AddNode(type);
        node.name = name;
        return node.id;

    }

    void Link(uint32_t from, const std::string& fromPin, uint32_t to, const std::string& toPin) {

        EXPECT_TRUE(graph.Connect(from, fromPin, to, toPin)) << fromPin << " -> " << toPin;

    }

    void LinkOffset(uint32_t from, const std::string& pin = "Value") {

        Link(from, pin, vertexOutput, NodeLibrary::positionOffsetPin);

    }

    void LinkSurface(uint32_t from, const std::string& surfacePin, const std::string& pin = "Value") {

        Link(from, pin, surfaceOutput, surfacePin);

    }

    Ref<const CompiledGraph> Compile() {

        return graph.Compile();

    }

    bool HasError(const CompiledGraph& compiled, const std::string& text) {

        for (const auto& error : compiled.errors) {
            if (error.message.find(text) != std::string::npos)
                return true;
        }
        return false;

    }

    Graph graph;
    uint32_t vertexOutput = 0;
    uint32_t surfaceOutput = 0;

};

static bool Contains(const std::string& text, const std::string& part) {

    return text.find(part) != std::string::npos;

}

TEST(MaterialGraphLibrary, DefinitionsAreConsistent) {

    for (const auto& definition : NodeLibrary::GetDefinitions()) {
        EXPECT_FALSE(definition.category.empty()) << definition.type;
        EXPECT_FALSE(definition.tooltip.empty()) << definition.type;
        EXPECT_EQ(NodeLibrary::Find(definition.type), &definition) << "Names of nodes have to be unique: " << definition.type;

        for (const auto& output : definition.outputs) {
            if (definition.kind != NodeKind::Expression)
                continue;

            EXPECT_FALSE(output.expression.empty()) << definition.type;

            // All referenced inputs have to exist
            for (size_t i = 0; i + 1 < output.expression.size(); i++) {
                if (output.expression[i] == '$' && std::isdigit(output.expression[i + 1])) {
                    EXPECT_LT(size_t(output.expression[i + 1] - '0'), definition.inputs.size()) << definition.type;
                }
            }
        }
    }

}

TEST(MaterialGraphCompiler, GraphWithoutLinksIsValidButGeneratesNothing) {

    TestGraph test;
    auto compiled = test.Compile();

    EXPECT_TRUE(compiled->valid);
    EXPECT_FALSE(compiled->hasVertexOffset);
    EXPECT_FALSE(compiled->hasSurface);
    EXPECT_TRUE(compiled->mainVertexFile.empty());
    EXPECT_FALSE(compiled->UsesShadowPipeline());

}

TEST(MaterialGraphCompiler, VertexOnly) {

    TestGraph test;
    test.LinkOffset(test.Add("Object normal"));
    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    EXPECT_TRUE(compiled->hasVertexOffset);
    EXPECT_FALSE(compiled->hasSurface);
    EXPECT_FALSE(compiled->hasOpacity);

    // Moved vertices need their own shadows, the fragments stay the ones of the regular material
    EXPECT_TRUE(compiled->UsesShadowPipeline());
    EXPECT_TRUE(Contains(compiled->mainVertexCode, "MATERIAL_GRAPH_VERTEX_OFFSET"));
    EXPECT_FALSE(Contains(compiled->mainVertexCode, "#define MATERIAL_GRAPH_SURFACE"));
    EXPECT_FALSE(Contains(compiled->mainFragmentCode, "#define MATERIAL_GRAPH_SURFACE"));
    EXPECT_FALSE(Contains(compiled->mainFragmentCode, "EvaluateMaterialSurface"));
    EXPECT_TRUE(Contains(compiled->shadowVertexCode, "EvaluateMaterialVertexOffset"));

}

TEST(MaterialGraphCompiler, FragmentOnly) {

    TestGraph test;
    test.LinkSurface(test.Add("Color"), NodeLibrary::baseColorPin);
    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    EXPECT_FALSE(compiled->hasVertexOffset);
    EXPECT_TRUE(compiled->hasSurface);

    // Shadows of surfaces without a cutout are the regular ones, which keeps the number of pipelines down
    EXPECT_FALSE(compiled->UsesShadowPipeline());
    EXPECT_TRUE(compiled->shadowVertexCode.empty());
    EXPECT_FALSE(Contains(compiled->mainVertexCode, "EvaluateMaterialVertexOffset"));
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "EvaluateMaterialSurface"));

}

TEST(MaterialGraphCompiler, VertexAndFragment) {

    TestGraph test;
    test.LinkOffset(test.Add("Object normal"));
    test.LinkSurface(test.Add("Color"), NodeLibrary::baseColorPin);
    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    EXPECT_TRUE(compiled->hasVertexOffset);
    EXPECT_TRUE(compiled->hasSurface);
    EXPECT_TRUE(compiled->UsesShadowPipeline());
    EXPECT_TRUE(Contains(compiled->mainVertexCode, "#define MATERIAL_GRAPH_SURFACE"));

}

TEST(MaterialGraphCompiler, OpacityNeedsShadowFragments) {

    TestGraph test;
    test.LinkSurface(test.Add("Fractal noise"), NodeLibrary::opacityPin, "Result");
    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    EXPECT_TRUE(compiled->hasOpacity);
    EXPECT_FALSE(compiled->hasUniformOpacity);
    EXPECT_TRUE(compiled->UsesShadowPipeline());
    EXPECT_TRUE(Contains(compiled->shadowFragmentCode, "EvaluateMaterialSurface"));
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "MATERIAL_GRAPH_OPACITY"));

}

TEST(MaterialGraphCompiler, OpacityOfParametersIsEvaluatedPerVertex) {

    TestGraph test;
    auto alpha = test.Add("Float parameter", "Alpha");
    auto inverted = test.Add("One minus");
    test.Link(alpha, "Value", inverted, "Value");
    test.LinkSurface(inverted, NodeLibrary::opacityPin, "Result");
    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    EXPECT_TRUE(compiled->hasUniformOpacity);
    EXPECT_FALSE(compiled->CutsOutFragments());

    // Fragments without a discard keep early depth tests, the vertices cut out the whole draw
    EXPECT_FALSE(Contains(compiled->mainFragmentCode, "MATERIAL_GRAPH_OPACITY"));
    EXPECT_FALSE(Contains(compiled->mainFragmentCode, "alphaParameter"));
    EXPECT_TRUE(Contains(compiled->mainVertexCode, "EvaluateMaterialOpacity"));
    EXPECT_TRUE(Contains(compiled->shadowVertexCode, "EvaluateMaterialOpacity"));
    EXPECT_FALSE(Contains(compiled->shadowFragmentCode, "#define MATERIAL_GRAPH_SURFACE"));

}

TEST(MaterialGraphCompiler, PassesOnlyContainTheirNodes) {

    TestGraph test;
    test.LinkSurface(test.Add("Fractal noise"), NodeLibrary::baseColorPin, "Result");
    test.LinkSurface(test.Add("Voronoi"), NodeLibrary::opacityPin, "Distance");
    test.LinkOffset(test.Add("Simplex noise"), "Result");
    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);

    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "MGFractalNoise"));
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "MGVoronoi"));
    EXPECT_FALSE(Contains(compiled->mainFragmentCode, "MGSimplex"));

    // Shadows don't calculate the color
    EXPECT_FALSE(Contains(compiled->shadowFragmentCode, "MGFractalNoise"));
    EXPECT_TRUE(Contains(compiled->shadowFragmentCode, "MGVoronoi"));

    EXPECT_TRUE(Contains(compiled->shadowVertexCode, "MGSimplex"));
    EXPECT_FALSE(Contains(compiled->shadowVertexCode, "MGFractalNoise"));

}

TEST(MaterialGraphCompiler, UnusedOutputsAreNotCalculated) {

    TestGraph test;
    auto voronoi = test.Add("Voronoi");
    test.LinkSurface(voronoi, NodeLibrary::roughnessPin, "Cell");
    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, ").z"));
    EXPECT_FALSE(Contains(compiled->mainFragmentCode, ").y"));

}

TEST(MaterialGraphCompiler, NodesOutsideOfOutputsDontCount) {

    TestGraph test;
    test.LinkSurface(test.Add("Color"), NodeLibrary::baseColorPin);
    auto unused = test.Add("Float parameter", "Unused");
    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    EXPECT_TRUE(compiled->parameters.empty());
    EXPECT_FALSE(compiled->nodeStages.contains(unused));

}

TEST(MaterialGraphCompiler, NodeStagesAreReported) {

    TestGraph test;
    auto shared = test.Add("Time");
    auto vertexOnly = test.Add("Object tangent");
    auto surfaceOnly = test.Add("View position");
    auto add = test.Add("Add");
    test.Link(shared, "Value", add, "A");
    test.Link(vertexOnly, "Value", add, "B");
    test.LinkOffset(add, "Result");
    auto mix = test.Add("Add");
    test.Link(shared, "Value", mix, "A");
    test.Link(surfaceOnly, "Value", mix, "B");
    test.LinkSurface(mix, NodeLibrary::baseColorPin, "Result");

    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid) << (compiled->errors.empty() ? "" : compiled->errors.front().message);
    EXPECT_EQ(compiled->nodeStages.at(shared), uint32_t(AllStages));
    EXPECT_EQ(compiled->nodeStages.at(vertexOnly), uint32_t(VertexStage));
    EXPECT_EQ(compiled->nodeStages.at(surfaceOnly), uint32_t(SurfaceStage));

}

TEST(MaterialGraphCompiler, CyclesAreReported) {

    TestGraph test;
    auto a = test.Add("Add");
    auto b = test.Add("Add");
    test.Link(a, "Result", b, "A");
    test.Link(b, "Result", a, "A");
    test.LinkSurface(a, NodeLibrary::baseColorPin, "Result");

    auto compiled = test.Compile();

    EXPECT_FALSE(compiled->valid);
    EXPECT_TRUE(test.HasError(*compiled, "cycle"));

}

TEST(MaterialGraphCompiler, NodesInTheWrongStageAreReported) {

    {
        TestGraph test;
        test.LinkOffset(test.Add("View position"));
        auto compiled = test.Compile();
        EXPECT_FALSE(compiled->valid);
        EXPECT_TRUE(test.HasError(*compiled, "vertex stage"));
    }
    {
        TestGraph test;
        test.LinkSurface(test.Add("Object tangent"), NodeLibrary::baseColorPin);
        auto compiled = test.Compile();
        EXPECT_FALSE(compiled->valid);
        EXPECT_TRUE(test.HasError(*compiled, "surface stage"));
    }

}

TEST(MaterialGraphCompiler, InvalidGraphsAreReported) {

    {
        TestGraph test;
        test.graph.AddNode("Node which doesn't exist");
        auto compiled = test.Compile();
        EXPECT_FALSE(compiled->valid);
        EXPECT_TRUE(test.HasError(*compiled, "Unknown node"));
    }
    {
        TestGraph test;
        test.graph.AddNode(NodeLibrary::surfaceOutputType);
        auto compiled = test.Compile();
        EXPECT_FALSE(compiled->valid);
        EXPECT_TRUE(test.HasError(*compiled, "one surface output"));
    }
    {
        TestGraph test;
        test.Add("Float parameter", "");
        auto compiled = test.Compile();
        EXPECT_FALSE(compiled->valid);
        EXPECT_TRUE(test.HasError(*compiled, "needs a name"));
    }
    {
        TestGraph test;
        test.Add("Float parameter", "Same");
        test.Add("Color parameter", "Same");
        auto compiled = test.Compile();
        EXPECT_FALSE(compiled->valid);
        EXPECT_TRUE(test.HasError(*compiled, "another type"));
    }
    {
        TestGraph test;
        test.graph.links.push_back({ 999, "Value", test.surfaceOutput, NodeLibrary::baseColorPin });
        auto compiled = test.Compile();
        EXPECT_FALSE(compiled->valid);
        EXPECT_TRUE(test.HasError(*compiled, "doesn't exist"));
    }
    {
        TestGraph test;
        auto a = test.Add("Color");
        auto b = test.Add("Color");
        test.graph.links.push_back({ a, "Value", test.surfaceOutput, NodeLibrary::baseColorPin });
        test.graph.links.push_back({ b, "Value", test.surfaceOutput, NodeLibrary::baseColorPin });
        auto compiled = test.Compile();
        EXPECT_FALSE(compiled->valid);
        EXPECT_TRUE(test.HasError(*compiled, "several links"));
    }

}

TEST(MaterialGraphCompiler, SamplingNeedsATexture) {

    TestGraph test;
    test.LinkSurface(test.Add("Sample texture"), NodeLibrary::baseColorPin, "Color");
    auto compiled = test.Compile();

    EXPECT_FALSE(compiled->valid);
    EXPECT_TRUE(test.HasError(*compiled, "texture parameter"));

}

TEST(MaterialGraphCompiler, TooManyVaryingsAreReported) {

    auto build = [](int32_t count) {
        TestGraph test;
        uint32_t sum = 0;
        for (int32_t i = 0; i < count; i++) {
            auto time = test.Add("Time");
            auto varying = test.Add("Vertex to surface");
            test.Link(time, "Value", varying, "Value");
            if (sum) {
                auto add = test.Add("Add");
                test.Link(sum, sum == varying ? "Value" : (test.graph.GetNode(sum)->type == "Add" ? "Result" : "Value"), add, "A");
                test.Link(varying, "Value", add, "B");
                sum = add;
            }
            else {
                sum = varying;
            }
        }
        test.LinkSurface(sum, NodeLibrary::roughnessPin, "Result");
        auto compiled = test.Compile();
        return std::make_pair(compiled, test.HasError(*compiled, "at most"));
        };

    EXPECT_TRUE(build(Compiler::maxVaryings).first->valid);
    EXPECT_FALSE(build(Compiler::maxVaryings + 1).first->valid);
    EXPECT_TRUE(build(Compiler::maxVaryings + 1).second);

}

TEST(MaterialGraphCompiler, VaryingsConnectBothStages) {

    TestGraph test;
    auto color = test.Add("Object position");
    auto varying = test.Add("Vertex to surface");
    test.Link(color, "Value", varying, "Value");
    test.LinkSurface(varying, NodeLibrary::baseColorPin);

    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    EXPECT_TRUE(Contains(compiled->mainVertexCode, "out vec4 graphVarying0"));
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "in vec4 graphVarying0"));
    EXPECT_TRUE(Contains(compiled->mainVertexCode, "graphVarying0 = "));
    EXPECT_EQ(compiled->nodeStages.at(color), uint32_t(VertexStage));

}

TEST(MaterialGraphCompiler, GenericTypesFollowTheInputs) {

    TestGraph test;
    auto vector = test.Add("Vector 3");
    auto number = test.Add("Float");
    auto multiply = test.Add("Multiply");
    test.Link(vector, "Value", multiply, "A");
    test.Link(number, "Value", multiply, "B");
    test.LinkSurface(multiply, NodeLibrary::baseColorPin, "Result");

    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    // The number is converted to the vector, the result has the type of the vector
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "vec3 multiplyResult = "));
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "vec3(floatValue)"));

}

TEST(MaterialGraphCompiler, ValuesConvertBetweenTypes) {

    TestGraph test;
    auto position = test.Add("Vector 4");
    auto coordinates = test.Add("Vector 2");
    auto combined = test.Add("Add");
    test.Link(position, "Value", combined, "A");
    test.Link(coordinates, "Value", combined, "B");
    test.LinkSurface(combined, NodeLibrary::roughnessPin, "Result");

    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "vec4(vector2Value"));
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, ", 0.0, 1.0)"));
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "addResult.x"));

}

TEST(MaterialGraphCompiler, ParametersGetALayout) {

    TestGraph test;
    auto number = test.Add("Float parameter", "Number");
    auto color = test.Add("Color parameter", "Tint");
    auto texture = test.Add("Texture parameter", "Map");
    auto sample = test.Add("Sample texture");
    test.Link(texture, "Value", sample, "Texture");
    auto multiply = test.Add("Multiply");
    test.Link(color, "Value", multiply, "A");
    test.Link(number, "Value", multiply, "B");
    auto add = test.Add("Add");
    test.Link(multiply, "Result", add, "A");
    test.Link(sample, "Color", add, "B");
    test.LinkSurface(add, NodeLibrary::baseColorPin, "Result");

    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    ASSERT_EQ(compiled->parameters.size(), size_t(3));
    EXPECT_EQ(compiled->parameterWords, uint32_t(1 + 3 + 1));

    uint32_t words = 0;
    for (const auto& parameter : compiled->parameters) {
        EXPECT_EQ(parameter.offset, words) << parameter.name;
        words += parameter.type == ValueType::Vec3 ? 3 : 1;
    }

    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "surfaceIn.parameterOffset"));

}

TEST(MaterialGraphCompiler, ParametersAreSharedByName) {

    TestGraph test;
    auto a = test.Add("Float parameter", "Same");
    auto b = test.Add("Float parameter", "Same");
    test.LinkSurface(a, NodeLibrary::roughnessPin);
    test.LinkSurface(b, NodeLibrary::metalnessPin);

    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    EXPECT_EQ(compiled->parameters.size(), size_t(1));
    EXPECT_EQ(compiled->parameterWords, uint32_t(1));

}

TEST(MaterialGraphCompiler, AnyNumberOfTextures) {

    TestGraph test;
    uint32_t last = 0;
    const int32_t textureCount = 64;
    for (int32_t i = 0; i < textureCount; i++) {
        auto texture = test.Add("Texture parameter", "Texture " + std::to_string(i));
        auto sample = test.Add("Sample normal map");
        test.Link(texture, "Value", sample, "Texture");
        if (last) {
            auto blend = test.Add("Blend normals");
            test.Link(last, last == sample ? "Normal" : (test.graph.GetNode(last)->type == "Blend normals" ? "Result" : "Normal"), blend, "A");
            test.Link(sample, "Normal", blend, "B");
            last = blend;
        }
        else {
            last = sample;
        }
    }
    test.LinkSurface(last, NodeLibrary::normalPin, "Result");

    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    EXPECT_EQ(compiled->parameters.size(), size_t(textureCount));
    EXPECT_EQ(compiled->parameterWords, uint32_t(textureCount));

}

TEST(MaterialGraphCompiler, HashOnlyDependsOnTheCode) {

    auto build = [](vec2 position, float parameterDefault, float constant) {
        TestGraph test;
        auto parameter = test.Add("Float parameter", "Value");
        test.graph.GetNode(parameter)->value.x = parameterDefault;
        test.graph.GetNode(parameter)->position = position;
        auto number = test.Add("Float");
        test.graph.GetNode(number)->value.x = constant;
        auto add = test.Add("Add");
        test.Link(parameter, "Value", add, "A");
        test.Link(number, "Value", add, "B");
        test.LinkSurface(add, NodeLibrary::roughnessPin, "Result");
        return test.Compile()->hash;
        };

    auto hash = build(vec2(0.0f), 1.0f, 1.0f);

    EXPECT_EQ(hash, build(vec2(0.0f), 1.0f, 1.0f));
    // Moving nodes and changing a parameter doesn't need any new shader
    EXPECT_EQ(hash, build(vec2(100.0f, 50.0f), 1.0f, 1.0f));
    EXPECT_EQ(hash, build(vec2(0.0f), 3.0f, 1.0f));
    // Constants are baked into the shader
    EXPECT_NE(hash, build(vec2(0.0f), 1.0f, 2.0f));

}

TEST(MaterialGraphCompiler, FileNamesContainTheHash) {

    TestGraph test;
    test.LinkOffset(test.Add("Object normal"));
    test.LinkSurface(test.Add("Color"), NodeLibrary::baseColorPin);
    auto first = test.Compile();

    test.graph.GetNode(test.graph.nodes.back().id)->value = vec4(0.5f);
    auto second = test.Compile();

    // The SPIR-V cache only knows file names and macros, so new code needs a new name
    EXPECT_NE(first->hash, second->hash);
    EXPECT_NE(first->mainFragmentFile, second->mainFragmentFile);

}

TEST(MaterialGraphCompiler, LiteralsAreValidGlsl) {

    TestGraph test;
    auto vector = test.Add("Vector 3");
    test.graph.GetNode(vector)->value = vec4(1.0f, -0.25f, 1e-7f, 0.0f);
    test.LinkSurface(vector, NodeLibrary::baseColorPin);

    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "vec3(1.0, -0.25, 1e-07)")) << compiled->mainFragmentCode;

}

TEST(MaterialGraphEditing, InputsOnlyHaveOneLink) {

    TestGraph test;
    auto a = test.Add("Color");
    auto b = test.Add("Color");
    test.LinkSurface(a, NodeLibrary::baseColorPin);
    test.LinkSurface(b, NodeLibrary::baseColorPin);

    ASSERT_EQ(test.graph.links.size(), size_t(1));
    EXPECT_EQ(test.graph.links.front().fromNode, b);

}

TEST(MaterialGraphEditing, TexturesOnlyConnectToTextures) {

    TestGraph test;
    auto texture = test.Add("Texture parameter", "Map");
    auto number = test.Add("Float");
    auto sample = test.Add("Sample texture");

    EXPECT_FALSE(test.graph.Connect(texture, "Value", test.surfaceOutput, NodeLibrary::roughnessPin));
    EXPECT_FALSE(test.graph.Connect(number, "Value", sample, "Texture"));
    EXPECT_TRUE(test.graph.Connect(texture, "Value", sample, "Texture"));

}

TEST(MaterialGraphEditing, InvalidConnectionsAreRejected) {

    TestGraph test;
    auto a = test.Add("Add");

    EXPECT_FALSE(test.graph.Connect(a, "Result", a, "A"));
    EXPECT_FALSE(test.graph.Connect(a, "Doesn't exist", test.surfaceOutput, NodeLibrary::roughnessPin));
    EXPECT_FALSE(test.graph.Connect(a, "Result", test.surfaceOutput, "Doesn't exist"));
    EXPECT_FALSE(test.graph.Connect(12345, "Result", test.surfaceOutput, NodeLibrary::roughnessPin));

}

TEST(MaterialGraphEditing, RemovingANodeRemovesItsLinks) {

    TestGraph test;
    auto a = test.Add("Color");
    test.LinkSurface(a, NodeLibrary::baseColorPin);

    test.graph.RemoveNode(a);

    EXPECT_TRUE(test.graph.links.empty());
    EXPECT_EQ(test.graph.GetNode(a), nullptr);

}

TEST(MaterialGraphEditing, NodeIdsAreNotReused) {

    TestGraph test;
    auto a = test.Add("Color");
    test.graph.RemoveNode(a);

    EXPECT_NE(test.Add("Color"), a);

}

TEST(MaterialGraphSerialization, RoundTripKeepsTheShaders) {

    TestGraph test;
    test.graph.name = "Test";
    test.graph.maxDisplacement = 0.5f;
    auto parameter = test.Add("Float parameter", "Amount");
    test.graph.GetNode(parameter)->value.x = 2.0f;
    auto noise = test.Add("Simplex noise");
    test.graph.GetNode(noise)->inputValues["Scale"] = vec4(4.0f);
    auto multiply = test.Add("Multiply");
    test.Link(parameter, "Value", multiply, "A");
    test.Link(noise, "Result", multiply, "B");
    test.LinkOffset(multiply, "Result");
    test.LinkSurface(multiply, NodeLibrary::roughnessPin, "Result");

    auto compiled = test.Compile();

    json j;
    to_json(j, test.graph);

    // Through the text form of the files
    auto loaded = CreateRef<Graph>();
    from_json(json::parse(j.dump()), *loaded);
    auto loadedCompiled = loaded->Compile();

    EXPECT_EQ(loaded->name, "Test");
    EXPECT_EQ(loaded->maxDisplacement, 0.5f);
    EXPECT_EQ(loaded->nodes.size(), test.graph.nodes.size());
    EXPECT_EQ(loaded->links.size(), test.graph.links.size());
    ASSERT_TRUE(loadedCompiled->valid);
    EXPECT_EQ(loadedCompiled->hash, compiled->hash);
    EXPECT_EQ(loadedCompiled->parameters.front().defaultValue.x, 2.0f);

    // New nodes don't take the ids of loaded ones
    EXPECT_GT(loaded->AddNode("Float").id, test.graph.nodes.back().id);

}

TEST(MaterialGraphSerialization, CopiesAreIndependent) {

    TestGraph test;
    test.LinkSurface(test.Add("Color"), NodeLibrary::baseColorPin);
    test.Compile();

    Graph copy = test.graph;
    copy.AddNode("Float");

    EXPECT_EQ(copy.nodes.size(), test.graph.nodes.size() + 1);
    EXPECT_NE(copy.GetCompiled(), nullptr);

}

TEST(MaterialGraphBounds, DefaultGraphHasOutputsAndCompiles) {

    auto graph = Graph::CreateDefault();

    EXPECT_EQ(graph->nodes.size(), size_t(2));
    ASSERT_NE(graph->GetCompiled(), nullptr);
    EXPECT_TRUE(graph->GetCompiled()->valid);

}

TEST(MaterialGraphCompiler, GeneratedCodeIsReadable) {

    TestGraph graph;
    auto glow = graph.Add("Float parameter", "Seam glow");
    auto noise = graph.Add("Fractal noise");
    auto multiply = graph.Add("Multiply");
    auto multiply2 = graph.Add("Multiply");
    graph.Link(noise, "Result", multiply, "A");
    graph.Link(glow, "Value", multiply, "B");
    graph.Link(multiply, "Result", multiply2, "A");
    graph.LinkSurface(multiply2, NodeLibrary::emissivePin, "Result");

    auto compiled = graph.Compile();
    ASSERT_TRUE(compiled->valid);

    const auto& code = compiled->mainFragmentCode;

    // Named after nodes and parameters, numbered if a node type is used several times
    EXPECT_NE(code.find("float fractalNoiseResult = "), std::string::npos);
    EXPECT_NE(code.find("float seamGlowParameter = "), std::string::npos);
    EXPECT_NE(code.find("float multiplyResult = fractalNoiseResult * seamGlowParameter;"), std::string::npos);
    EXPECT_NE(code.find("float multiplyResult2 = "), std::string::npos);
    // No parentheses around single terms
    EXPECT_EQ(code.find("(fractalNoiseResult)"), std::string::npos);

}

TEST(MaterialGraphCompiler, VariableNamesDontHideFunctions) {

    TestGraph graph;
    auto floorNode = graph.Add("Floor");
    auto clampNode = graph.Add("Clamp");
    auto parameter = graph.Add("Float parameter", "clamp");
    graph.Link(parameter, "Value", floorNode, "Value");
    graph.Link(floorNode, "Result", clampNode, "Value");
    graph.LinkSurface(clampNode, NodeLibrary::emissivePin, "Result");

    auto compiled = graph.Compile();
    ASSERT_TRUE(compiled->valid);

    EXPECT_EQ(compiled->mainFragmentCode.find("float floor ="), std::string::npos);
    EXPECT_EQ(compiled->mainFragmentCode.find("float clamp ="), std::string::npos);
    EXPECT_NE(compiled->mainFragmentCode.find("clampParameter"), std::string::npos);

}

TEST(MaterialGraphCompiler, StaticSwitchOnlyCompilesTheSelectedInput) {

    TestGraph graph;
    auto voronoi = graph.Add("Voronoi");
    auto checker = graph.Add("Checker");
    auto staticSwitch = graph.Add("Static switch");
    graph.Link(voronoi, "Distance", staticSwitch, "On");
    graph.Link(checker, "Result", staticSwitch, "Off");
    graph.LinkSurface(staticSwitch, NodeLibrary::emissivePin, "Result");

    auto on = graph.Compile();
    ASSERT_TRUE(on->valid);
    EXPECT_NE(on->mainFragmentCode.find("MGVoronoi"), std::string::npos);
    EXPECT_EQ(on->mainFragmentCode.find("MGChecker"), std::string::npos);

    graph.graph.GetNode(staticSwitch)->value.x = 0.0f;

    auto off = graph.Compile();
    ASSERT_TRUE(off->valid);
    EXPECT_EQ(off->mainFragmentCode.find("MGVoronoi"), std::string::npos);
    EXPECT_NE(off->mainFragmentCode.find("MGChecker"), std::string::npos);
    EXPECT_NE(on->hash, off->hash);

}

TEST(MaterialGraphCompiler, ReroutesPassValuesOnAndFramesGenerateNothing) {

    TestGraph graph;
    auto color = graph.Add("Color");
    auto reroute = graph.Add("Reroute");
    graph.Add("Frame", "Title");
    graph.Link(color, "Value", reroute, "Value");
    graph.LinkSurface(reroute, NodeLibrary::baseColorPin);

    auto compiled = graph.Compile();
    ASSERT_TRUE(compiled->valid);
    EXPECT_NE(compiled->mainFragmentCode.find("vec3 rerouteValue = colorValue;"), std::string::npos);

}

TEST(MaterialGraphCompiler, ReadingTheTangentFrameRequestsIt) {

    TestGraph graph;
    auto height = graph.Add("Texture parameter", "Height");
    auto parallax = graph.Add("Parallax coordinates");
    auto split = graph.Add("Split vector");
    graph.Link(height, "Value", parallax, "Height");
    graph.Link(parallax, "Coordinates", split, "Vector");
    graph.LinkSurface(split, NodeLibrary::emissivePin, "X");

    auto compiled = graph.Compile();
    ASSERT_TRUE(compiled->valid);
    EXPECT_FALSE(compiled->writesNormal);
    EXPECT_NE(compiled->mainVertexCode.find("MATERIAL_GRAPH_USES_TANGENTS"), std::string::npos);
    EXPECT_NE(compiled->mainFragmentCode.find("MATERIAL_GRAPH_USES_TANGENTS"), std::string::npos);

}

TEST(MaterialGraphEvaluation, PreviewsOfNodesCompile) {

    TestGraph graph;
    auto texture = graph.Add("Texture parameter", "Texture");
    auto sample = graph.Add("Sample texture");
    auto tangent = graph.Add("Object tangent");
    auto varying = graph.Add("Vertex to surface");
    auto comment = graph.Add("Comment");
    graph.Link(texture, "Value", sample, "Texture");
    graph.Link(tangent, "Value", varying, "Value");
    graph.LinkSurface(sample, NodeLibrary::baseColorPin, "Color");
    graph.LinkSurface(varying, NodeLibrary::emissivePin);

    // Values of the vertex stage are calculated in place
    for (auto id : { texture, sample, tangent, varying, graph.surfaceOutput }) {
        auto preview = Compiler::CompilePreview(graph.graph, id);
        EXPECT_TRUE(preview.valid) << id;
        EXPECT_FALSE(preview.fragmentFile.empty()) << id;
    }

    EXPECT_FALSE(Compiler::CompilePreview(graph.graph, comment).valid);

}

TEST(MaterialGraphEvaluation, ParameterLayoutDoesntDependOnTheEvaluatedNode) {

    TestGraph graph;
    auto a = graph.Add("Float parameter", "A");
    auto b = graph.Add("Vector 3 parameter", "B");
    graph.Add("Color parameter", "Unused");
    auto add = graph.Add("Add");
    graph.Link(a, "Value", add, "A");
    graph.LinkSurface(add, NodeLibrary::emissivePin, "Result");
    graph.LinkSurface(b, NodeLibrary::baseColorPin);

    auto first = Compiler::CompilePreview(graph.graph, a);
    auto second = Compiler::CompilePreview(graph.graph, b);
    auto bake = Compiler::CompileBake(graph.graph);

    ASSERT_TRUE(first.valid && second.valid && bake.valid);
    EXPECT_EQ(first.parameterWords, second.parameterWords);
    EXPECT_EQ(first.parameterWords, bake.parameterWords);
    ASSERT_EQ(first.parameters.size(), 3u);
    ASSERT_EQ(bake.parameters.size(), 3u);
    for (size_t i = 0; i < first.parameters.size(); i++)
        EXPECT_EQ(first.parameters[i].offset, bake.parameters[i].offset);

}

TEST(MaterialGraphEvaluation, TimeIsReported) {

    TestGraph graph;
    auto time = graph.Add("Time");
    auto color = graph.Add("Color");
    graph.LinkSurface(color, NodeLibrary::baseColorPin);

    EXPECT_TRUE(Compiler::CompilePreview(graph.graph, time).usesTime);
    EXPECT_FALSE(Compiler::CompilePreview(graph.graph, color).usesTime);

}

TEST(MaterialGraphSerialization, ParameterSettingsAndFramesRoundTrip) {

    TestGraph graph;
    auto parameter = graph.Add("Float parameter", "Glow");
    auto frame = graph.Add("Frame", "Title");
    graph.graph.GetNode(parameter)->range = vec2(0.0f, 10.0f);
    graph.graph.GetNode(parameter)->group = "Surface";
    graph.graph.GetNode(parameter)->order = 3;
    graph.graph.GetNode(frame)->size = vec2(400.0f, 200.0f);
    graph.LinkSurface(parameter, NodeLibrary::roughnessPin);

    json j;
    to_json(j, graph.graph);

    Graph loaded;
    from_json(j, loaded);

    EXPECT_EQ(loaded.GetNode(parameter)->range, vec2(0.0f, 10.0f));
    EXPECT_EQ(loaded.GetNode(parameter)->group, "Surface");
    EXPECT_EQ(loaded.GetNode(parameter)->order, 3);
    EXPECT_EQ(loaded.GetNode(frame)->size, vec2(400.0f, 200.0f));

    // The compiled parameters carry the settings for the materials
    auto compiled = graph.Compile();
    ASSERT_EQ(compiled->parameters.size(), 1u);
    EXPECT_TRUE(compiled->parameters[0].HasRange());
    EXPECT_EQ(compiled->parameters[0].group, "Surface");
    EXPECT_EQ(compiled->parameters[0].order, 3);

}

TEST(MaterialGraphBake, ClearBringsBackTheProperties) {

    Material material;
    material.baseColor = vec3(0.2f);
    material.roughness = 0.1f;
    material.graphBake.bakedState = 1;
    material.graphBake.unbaked.baseColor = vec3(0.7f);
    material.graphBake.unbaked.roughness = 0.9f;
    material.graphBake.unbaked.normalScale = 0.25f;

    Baker::Clear(material);

    EXPECT_EQ(material.baseColor, vec3(0.7f));
    EXPECT_EQ(material.roughness, 0.9f);
    EXPECT_EQ(material.normalScale, 0.25f);
    EXPECT_EQ(material.graphBake.bakedState, 0u);

}

TEST(MaterialGraphBake, ClearWithoutABakeChangesNothing) {

    Material material;
    material.baseColor = vec3(0.2f);
    material.graphBake.unbaked.baseColor = vec3(0.7f);

    Baker::Clear(material);

    EXPECT_EQ(material.baseColor, vec3(0.2f));

}

TEST(MaterialGraphSerialization, UnbakedPropertiesRoundTrip) {

    Material material;
    material.graphBake.bakedState = 42;
    material.graphBake.unbaked.baseColor = vec3(0.7f);
    material.graphBake.unbaked.emissiveIntensity = 3.0f;
    material.graphBake.unbaked.maps["normal"] = "textures/normal.png";

    json j;
    to_json(j, material);

    Material loaded;
    from_json(j, loaded);

    EXPECT_EQ(loaded.graphBake.bakedState, 42u);
    EXPECT_EQ(loaded.graphBake.unbaked.baseColor, vec3(0.7f));
    EXPECT_EQ(loaded.graphBake.unbaked.emissiveIntensity, 3.0f);
    ASSERT_TRUE(loaded.graphBake.unbaked.maps.contains("normal"));
    EXPECT_EQ(loaded.graphBake.unbaked.maps["normal"], "textures/normal.png");

}

static uint32_t AddCode(TestGraph& test, const std::string& code, std::vector<CodeInput> inputs, ValueType output) {

    auto& node = test.graph.AddNode(NodeLibrary::codeType);
    node.code = code;
    node.codeInputs = std::move(inputs);
    node.codeOutputType = output;
    NodeLibrary::UpdateCodeDefinition(node);
    return node.id;

}

TEST(MaterialGraphCompiler, CodeNodesBecomeFunctions) {

    TestGraph test;
    auto color = test.Add("Color");
    auto first = AddCode(test, "return a * b;", { { "a", ValueType::Vec3 }, { "b", ValueType::Float } }, ValueType::Vec3);
    auto second = AddCode(test, "return a * b;", { { "a", ValueType::Vec3 }, { "b", ValueType::Float } }, ValueType::Vec3);
    test.Link(color, "Value", first, "a");
    test.Link(first, "Result", second, "a");
    test.LinkSurface(second, NodeLibrary::baseColorPin, "Result");

    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    const auto& code = compiled->mainFragmentCode;
    EXPECT_TRUE(Contains(code, "(vec3 a, float b) {\nreturn a * b;\n}")) << code;
    // Nodes with the same code share one function, only the stages which use it contain it
    auto define = code.find("#define MGCode");
    ASSERT_NE(define, std::string::npos);
    EXPECT_EQ(code.find("#define MGCode", define + 1), std::string::npos) << code;
    EXPECT_FALSE(Contains(compiled->mainVertexCode, "MGCode"));

}

TEST(MaterialGraphCompiler, InvalidCodeInputsAreReported) {

    TestGraph test;
    auto code = AddCode(test, "return x;", { { "1x", ValueType::Float }, { "x", ValueType::Float }, { "x", ValueType::Float } },
        ValueType::Float);
    test.LinkSurface(code, NodeLibrary::roughnessPin, "Result");
    auto noReturn = AddCode(test, "0.5;", {}, ValueType::Float);
    test.LinkSurface(noReturn, NodeLibrary::metalnessPin, "Result");

    auto compiled = test.Compile();

    EXPECT_FALSE(compiled->valid);
    EXPECT_TRUE(test.HasError(*compiled, "letters, digits and underscores"));
    EXPECT_TRUE(test.HasError(*compiled, "exists twice"));
    EXPECT_TRUE(test.HasError(*compiled, "has to return"));

}

TEST(MaterialGraphSerialization, CodeNodesRoundTrip) {

    TestGraph test;
    auto code = AddCode(test, "return vec2(a, a);", { { "a", ValueType::Float } }, ValueType::Vec2);
    test.LinkSurface(code, NodeLibrary::roughnessPin, "Result");
    auto compiled = test.Compile();
    ASSERT_TRUE(compiled->valid);

    json j;
    to_json(j, test.graph);
    auto loaded = CreateRef<Graph>();
    from_json(json::parse(j.dump()), *loaded);

    auto node = loaded->GetNode(code);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->code, "return vec2(a, a);");
    ASSERT_EQ(node->codeInputs.size(), size_t(1));
    EXPECT_EQ(node->codeInputs.front().type, ValueType::Float);
    EXPECT_EQ(node->codeOutputType, ValueType::Vec2);
    EXPECT_EQ(loaded->Compile()->mainFragmentCode, compiled->mainFragmentCode);

    // Types are stored by name, the same format capture jobs use
    for (const auto& nodeJson : j["nodes"]) {
        if (nodeJson["id"] != code)
            continue;
        EXPECT_EQ(nodeJson["codeOutputType"], "vec2");
        EXPECT_EQ(nodeJson["codeInputs"], json::parse(R"([["a", "float"]])"));
    }

}

TEST(MaterialGraphCompiler, CodeNodesWithManyInputs) {

    std::vector<CodeInput> inputs;
    for (int32_t i = 0; i < 11; i++)
        inputs.push_back({ "a" + std::to_string(i), ValueType::Float });

    TestGraph test;
    auto code = AddCode(test, "return a10;", inputs, ValueType::Float);
    for (int32_t i = 0; i < 11; i++)
        test.graph.GetNode(code)->inputValues["a" + std::to_string(i)] = vec4(float(i) + 0.25f);
    test.LinkSurface(code, NodeLibrary::roughnessPin, "Result");

    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "(0.25, 1.25, 2.25, 3.25, 4.25, 5.25, 6.25, 7.25, 8.25, 9.25, 10.25)"))
        << compiled->mainFragmentCode;

}

TEST(MaterialGraphCompiler, CodeNodesNeedNumberTypes) {

    TestGraph test;
    AddCode(test, "return 0.0;", {}, ValueType::Texture);
    EXPECT_TRUE(test.HasError(*test.Compile(), "The result needs a number or vector type"));

    // Unknown types in files are generic, which is reported as well
    TestGraph saved;
    auto code = AddCode(saved, "return a;", { { "a", ValueType::Float } }, ValueType::Float);
    json j;
    to_json(j, saved.graph);
    for (auto& nodeJson : j["nodes"]) {
        if (nodeJson["id"] != code)
            continue;
        nodeJson["codeOutputType"] = "mat3";
        nodeJson["codeInputs"][0][1] = "sampler2D";
    }

    Graph loaded;
    from_json(j, loaded);
    auto compiled = loaded.Compile();

    EXPECT_FALSE(compiled->valid);
    EXPECT_TRUE(test.HasError(*compiled, "The result needs a number or vector type"));
    EXPECT_TRUE(test.HasError(*compiled, "The input a needs a number or vector type"));

}

TEST(MaterialGraphEvaluation, CodeNodesAreDefinedInEvaluations) {

    TestGraph test;
    auto color = test.Add("Color");
    auto code = AddCode(test, "return a * 0.5;", { { "a", ValueType::Vec3 } }, ValueType::Vec3);
    test.Link(color, "Value", code, "a");
    test.LinkSurface(code, NodeLibrary::baseColorPin, "Result");

    auto preview = Compiler::CompilePreview(test.graph, code);
    auto bake = Compiler::CompileBake(test.graph);

    // The function comes before the one which calls it
    for (const auto& [shader, signature] : { std::pair(preview, "EvaluateMaterialPreview(MaterialSurfaceInput surfaceIn) {"),
        std::pair(bake, "EvaluateMaterialSurface(MaterialSurfaceInput surfaceIn) {") }) {
        ASSERT_TRUE(shader.valid);
        auto file = Loader::ShaderLoader::LoadFile(shader.fragmentFile, VK_SHADER_STAGE_FRAGMENT_BIT).code;
        auto definition = file.find("vec3 MGCode");
        auto caller = file.find(signature);
        ASSERT_NE(caller, std::string::npos) << file;
        EXPECT_LT(definition, caller) << file;
    }

}
