#include <gtest/gtest.h>

#include "materialgraph/Baker.h"
#include "materialgraph/Compiler.h"
#include "materialgraph/MaterialGraph.h"
#include "materialgraph/MaterialGraphSerializer.h"
#include "materialgraph/NodeLibrary.h"
#include "loader/AssetLoader.h"
#include "loader/MaterialGraphLoader.h"
#include "loader/ShaderLoader.h"
#include "mesh/MeshSerializer.h"
#include "resource/ResourceManager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

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

TEST(MaterialGraphCompiler, SecondTexCoordsReachEveryVertexPass) {

    // Pivot Painter reads the element of a vertex from the second set, rasterizer, shadows and ray tracing have to agree
    TestGraph test;
    auto texCoords = test.Add("Texture coordinates 2");
    auto split = test.Add("Split vector");
    test.Link(texCoords, "Value", split, "Vector");
    auto combine = test.Add("Combine vector 3");
    test.Link(split, "X", combine, "X");
    test.LinkOffset(combine, "Result");
    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    EXPECT_TRUE(Contains(compiled->mainVertexCode, "vertexIn.texCoord1"));
    EXPECT_TRUE(Contains(compiled->shadowVertexCode, "vertexIn.texCoord1"));
    EXPECT_TRUE(Contains(compiled->deformCode, "vertexIn.texCoord1"));

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

    // Fragments without a discard keep early depth tests, the vertices cut out the whole draw. Only translucent
    // passes read the opacity of the surface
    EXPECT_FALSE(Contains(compiled->mainFragmentCode, "MATERIAL_GRAPH_OPACITY"));
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "MATERIAL_GRAPH_HAS_OPACITY"));
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

    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "MaterialGraphFractalNoise"));
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "MaterialGraphVoronoi"));
    EXPECT_FALSE(Contains(compiled->mainFragmentCode, "MaterialGraphSimplex"));

    // Shadows don't calculate the color
    EXPECT_FALSE(Contains(compiled->shadowFragmentCode, "MaterialGraphFractalNoise"));
    EXPECT_TRUE(Contains(compiled->shadowFragmentCode, "MaterialGraphVoronoi"));

    EXPECT_TRUE(Contains(compiled->shadowVertexCode, "MaterialGraphSimplex"));
    EXPECT_FALSE(Contains(compiled->shadowVertexCode, "MaterialGraphFractalNoise"));

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
    EXPECT_NE(on->mainFragmentCode.find("MaterialGraphVoronoi"), std::string::npos);
    EXPECT_EQ(on->mainFragmentCode.find("MaterialGraphChecker"), std::string::npos);

    graph.graph.GetNode(staticSwitch)->value.x = 0.0f;

    auto off = graph.Compile();
    ASSERT_TRUE(off->valid);
    EXPECT_EQ(off->mainFragmentCode.find("MaterialGraphVoronoi"), std::string::npos);
    EXPECT_NE(off->mainFragmentCode.find("MaterialGraphChecker"), std::string::npos);
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
    NodeLibrary::UpdateDefinition(node);
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
    auto define = code.find("#define MaterialGraphCode");
    ASSERT_NE(define, std::string::npos);
    EXPECT_EQ(code.find("#define MaterialGraphCode", define + 1), std::string::npos) << code;
    EXPECT_FALSE(Contains(compiled->mainVertexCode, "MaterialGraphCode"));

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

    // Types are stored by name, the same format bridge jobs use
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
        auto definition = file.find("vec3 MaterialGraphCode");
        auto caller = file.find(signature);
        ASSERT_NE(caller, std::string::npos) << file;
        EXPECT_LT(definition, caller) << file;
    }

}

// A subgraph which scales a color and adds a glow parameter
class TestSubgraph {

public:
    TestSubgraph() : graph(CreateRef<Graph>()), handle(graph) {}

    uint32_t AddPin(const std::string& nodeType, const std::string& name, ValueType type) {

        auto& node = graph->AddNode(nodeType);
        node.name = name;
        node.pinType = type;
        NodeLibrary::UpdateDefinition(node);
        return node.id;

    }

    uint32_t AddInput(const std::string& name, ValueType type) {

        return AddPin(NodeLibrary::subgraphInputType, name, type);

    }

    uint32_t AddOutput(const std::string& name, ValueType type) {

        return AddPin(NodeLibrary::subgraphOutputType, name, type);

    }

    uint32_t Add(const std::string& type, const std::string& name = "") {

        auto& node = graph->AddNode(type);
        node.name = name;
        return node.id;

    }

    void Link(uint32_t from, const std::string& fromPin, uint32_t to, const std::string& toPin) {

        EXPECT_TRUE(graph->Connect(from, fromPin, to, toPin)) << fromPin << " -> " << toPin;

    }

    // Multiplies an input with a constant
    static TestSubgraph CreateScale(ValueType type = ValueType::Vec3) {

        TestSubgraph test;
        auto input = test.AddInput("Color", type);
        auto output = test.AddOutput("Result", type);
        auto multiply = test.Add("Multiply");
        test.Link(input, "Value", multiply, "A");
        test.graph->GetNode(multiply)->inputValues["B"] = vec4(0.25f);
        test.Link(multiply, "Result", output, "Value");
        EXPECT_TRUE(test.graph->Compile()->valid);
        return test;

    }

    Ref<Graph> graph;
    ResourceHandle<Graph> handle;

};

static uint32_t AddSubgraph(Graph& graph, const ResourceHandle<Graph>& subgraph) {

    auto id = graph.AddNode(NodeLibrary::subgraphType).id;
    graph.SetSubgraph(id, subgraph);
    return id;

}

TEST(MaterialGraphSubgraphs, GenerateTheCodeOfTheirNodes) {

    auto scale = TestSubgraph::CreateScale();

    TestGraph test;
    auto color = test.Add("Color parameter", "Tint");
    auto subgraph = AddSubgraph(test.graph, scale.handle);
    test.Link(color, "Value", subgraph, "Color");
    test.LinkSurface(subgraph, NodeLibrary::baseColorPin, "Result");
    auto compiled = test.Compile();

    TestGraph reference;
    auto referenceColor = reference.Add("Color parameter", "Tint");
    auto multiply = reference.Add("Multiply");
    reference.graph.GetNode(multiply)->inputValues["B"] = vec4(0.25f);
    reference.Link(referenceColor, "Value", multiply, "A");
    reference.LinkSurface(multiply, NodeLibrary::baseColorPin, "Result");
    auto compiledReference = reference.Compile();

    ASSERT_TRUE(compiled->valid) << (compiled->errors.empty() ? "" : compiled->errors.front().message);
    ASSERT_TRUE(compiledReference->valid);
    // Graphs generating the same code share their pipelines
    EXPECT_EQ(compiled->mainFragmentCode, compiledReference->mainFragmentCode);
    EXPECT_EQ(compiled->hash, compiledReference->hash);

    auto bake = Compiler::CompileBake(test.graph);
    auto referenceBake = Compiler::CompileBake(reference.graph);
    ASSERT_TRUE(bake.valid);
    EXPECT_EQ(bake.fragmentFile, referenceBake.fragmentFile);

}

TEST(MaterialGraphSubgraphs, ConvertToTheTypesOfTheirPins) {

    auto scale = TestSubgraph::CreateScale(ValueType::Float);

    TestGraph test;
    auto color = test.Add("Color parameter", "Tint");
    auto subgraph = AddSubgraph(test.graph, scale.handle);
    test.Link(color, "Value", subgraph, "Color");
    test.LinkSurface(subgraph, NodeLibrary::baseColorPin, "Result");
    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid);
    // The color is reduced to a number before the multiplication, the result is a color again
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "float multiplyResult = tintParameter.x * 0.25")) << compiled->mainFragmentCode;
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "surfaceOut.baseColor = vec3(multiplyResult)")) << compiled->mainFragmentCode;

}

TEST(MaterialGraphSubgraphs, InputsHaveDefaults) {

    auto scale = TestSubgraph::CreateScale();
    auto input = NodeLibrary::GetSubgraphPins(scale.graph->nodes, NodeLibrary::subgraphInputType).front()->id;
    scale.graph->GetNode(input)->inputValues["Default"] = vec4(0.5f, 0.75f, 1.0f, 0.0f);
    scale.graph->Compile();

    TestGraph test;
    auto subgraph = AddSubgraph(test.graph, scale.handle);
    test.LinkSurface(subgraph, NodeLibrary::baseColorPin, "Result");
    EXPECT_TRUE(Contains(test.Compile()->mainFragmentCode, "vec3(0.5, 0.75, 1.0)"));

    // The value of the node replaces it
    test.graph.GetNode(subgraph)->inputValues["Color"] = vec4(0.125f);
    EXPECT_TRUE(Contains(test.Compile()->mainFragmentCode, "vec3(0.125, 0.125, 0.125)"));

    // Nodes of the subgraph as the default, the node has no value of its own then
    test.graph.GetNode(subgraph)->inputValues.clear();
    auto normal = scale.Add("World normal");
    scale.Link(normal, "Value", input, "Default");
    scale.graph->Compile();
    auto compiled = test.Compile();
    ASSERT_TRUE(compiled->valid);
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "worldNormalValue * ")) << compiled->mainFragmentCode;
    EXPECT_FALSE(NodeLibrary::Find(*test.graph.GetNode(subgraph))->FindInput("Color")->defaultExpression.empty());

    // A link replaces all of them
    auto color = test.Add("Color parameter", "Tint");
    test.Link(color, "Value", subgraph, "Color");
    compiled = test.Compile();
    ASSERT_TRUE(compiled->valid);
    EXPECT_FALSE(Contains(compiled->mainFragmentCode, "worldNormalValue"));
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "tintParameter * "));

}

TEST(MaterialGraphSubgraphs, EachUseHasItsOwnTextures) {

    // One texture comes from the node, the other one is a parameter all uses share
    TestSubgraph sampler;
    auto texture = sampler.AddInput("Map", ValueType::Texture);
    auto output = sampler.AddOutput("Color", ValueType::Vec3);
    auto detail = sampler.Add("Texture parameter", "Detail");
    auto sample = sampler.Add("Sample texture");
    auto sampleDetail = sampler.Add("Sample texture");
    auto multiply = sampler.Add("Multiply");
    sampler.Link(texture, "Value", sample, "Texture");
    sampler.Link(detail, "Value", sampleDetail, "Texture");
    sampler.Link(sample, "Color", multiply, "A");
    sampler.Link(sampleDetail, "Color", multiply, "B");
    sampler.Link(multiply, "Result", output, "Value");
    auto compiledSubgraph = sampler.graph->Compile();
    ASSERT_TRUE(compiledSubgraph->valid) << compiledSubgraph->errors.front().message;
    ASSERT_EQ(compiledSubgraph->parameters.size(), size_t(1));

    TestGraph test;
    auto first = test.Add("Texture parameter", "First");
    auto second = test.Add("Texture parameter", "Second");
    auto firstUse = AddSubgraph(test.graph, sampler.handle);
    auto secondUse = AddSubgraph(test.graph, sampler.handle);
    test.Link(first, "Value", firstUse, "Map");
    test.Link(second, "Value", secondUse, "Map");
    test.LinkSurface(firstUse, NodeLibrary::baseColorPin, "Color");
    test.LinkSurface(secondUse, NodeLibrary::emissivePin, "Color");
    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid) << compiled->errors.front().message;
    ASSERT_EQ(compiled->parameters.size(), size_t(3));
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "firstParameter"));
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "secondParameter"));
    for (const auto& parameter : compiled->parameters) {
        EXPECT_EQ(parameter.type, ValueType::Texture);
        // Parameters of subgraphs belong to a node using it, the editor shows that one
        EXPECT_NE(test.graph.GetNode(parameter.nodeId), nullptr) << parameter.name;
    }

    // Without a texture the node is reported by the name of its pin
    test.graph.Disconnect(secondUse, "Map");
    compiled = test.Compile();
    EXPECT_FALSE(compiled->valid);
    ASSERT_FALSE(compiled->errors.empty());
    EXPECT_EQ(compiled->errors.front().nodeId, secondUse);
    EXPECT_TRUE(test.HasError(*compiled, "The input Map needs a texture parameter"));

    // Values don't connect to textures
    auto color = test.Add("Color");
    EXPECT_FALSE(test.graph.Connect(color, "Value", secondUse, "Map"));

}

TEST(MaterialGraphSubgraphs, ContainOtherSubgraphs) {

    auto scale = TestSubgraph::CreateScale();

    TestSubgraph outer;
    auto input = outer.AddInput("In", ValueType::Vec3);
    auto output = outer.AddOutput("Out", ValueType::Vec3);
    auto first = AddSubgraph(*outer.graph, scale.handle);
    auto second = AddSubgraph(*outer.graph, scale.handle);
    auto glow = outer.Add("Float parameter", "Glow");
    auto add = outer.Add("Add");
    outer.Link(input, "Value", first, "Color");
    outer.Link(first, "Result", second, "Color");
    outer.Link(second, "Result", add, "A");
    outer.Link(glow, "Value", add, "B");
    outer.Link(add, "Result", output, "Value");
    ASSERT_TRUE(outer.graph->Compile()->valid);

    TestGraph test;
    auto color = test.Add("Color parameter", "Tint");
    auto subgraph = AddSubgraph(test.graph, outer.handle);
    test.Link(color, "Value", subgraph, "In");
    test.LinkSurface(subgraph, NodeLibrary::emissivePin, "Out");
    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid) << compiled->errors.front().message;
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "multiplyResult2 = multiplyResult * ")) << compiled->mainFragmentCode;
    ASSERT_TRUE(compiled->nodeStages.contains(subgraph));
    EXPECT_TRUE(compiled->nodeStages.at(subgraph) & SurfaceStage);
    for (const auto& [nodeId, stages] : compiled->nodeStages)
        EXPECT_NE(test.graph.GetNode(nodeId), nullptr) << "Only nodes of the graph itself have stages";

    EXPECT_TRUE(test.graph.Uses(*scale.graph));
    EXPECT_FALSE(scale.graph->Uses(test.graph));

    // Previews show subgraphs by their first used output
    EXPECT_TRUE(Compiler::CompilePreview(test.graph, subgraph).valid);
    EXPECT_TRUE(Compiler::CompilePreview(*outer.graph, first).valid);
    EXPECT_TRUE(Compiler::CompilePreview(*outer.graph, input).valid);
    EXPECT_TRUE(Compiler::CompilePreview(*outer.graph, output).valid);

}

TEST(MaterialGraphSubgraphs, LinksFollowRenamedPins) {

    TestSubgraph swap;
    auto a = swap.AddInput("A", ValueType::Vec3);
    auto b = swap.AddInput("B", ValueType::Vec3);
    auto output = swap.AddOutput("Result", ValueType::Vec3);
    auto subtract = swap.Add("Subtract");
    swap.Link(a, "Value", subtract, "A");
    swap.Link(b, "Value", subtract, "B");
    swap.Link(subtract, "Result", output, "Value");
    ASSERT_TRUE(swap.graph->Compile()->valid);

    TestGraph test;
    auto color = test.Add("Color parameter", "Tint");
    auto subgraph = AddSubgraph(test.graph, swap.handle);
    test.Link(color, "Value", subgraph, "A");
    test.graph.GetNode(subgraph)->inputValues["B"] = vec4(0.5f);
    test.LinkSurface(subgraph, NodeLibrary::baseColorPin, "Result");
    auto code = test.Compile()->mainFragmentCode;

    // Names that are swapped are the hardest case
    swap.graph->GetNode(a)->name = "B";
    swap.graph->GetNode(b)->name = "A";
    swap.graph->GetNode(output)->name = "Difference";
    ASSERT_TRUE(swap.graph->Compile()->valid);
    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid) << compiled->errors.front().message;
    EXPECT_EQ(compiled->mainFragmentCode, code);
    EXPECT_NE(test.graph.GetInputLink(subgraph, "B"), nullptr);
    EXPECT_EQ(test.graph.GetInputLink(subgraph, "A"), nullptr);
    EXPECT_EQ(test.graph.GetInputLink(test.surfaceOutput, NodeLibrary::baseColorPin)->fromPin, "Difference");

    // Links to a removed pin are kept and reported until they are removed
    swap.graph->RemoveNode(a);
    ASSERT_TRUE(swap.graph->Compile()->valid);
    compiled = test.Compile();
    EXPECT_FALSE(compiled->valid);
    EXPECT_TRUE(test.HasError(*compiled, "has no input B"));
    EXPECT_NE(NodeLibrary::Find(*test.graph.GetNode(subgraph))->FindInput("B"), nullptr);

    test.graph.Disconnect(subgraph, "B");
    EXPECT_TRUE(test.Compile()->valid);
    EXPECT_EQ(NodeLibrary::Find(*test.graph.GetNode(subgraph))->FindInput("B"), nullptr);

}

TEST(MaterialGraphSubgraphs, ErrorsBelongToTheNodeUsingThem) {

    // Pixel data isn't available for vertices, which only the graph using the subgraph can know
    TestSubgraph screen;
    auto output = screen.AddOutput("Position", ValueType::Vec3);
    auto position = screen.Add("Screen position");
    screen.Link(position, "Value", output, "Value");
    ASSERT_TRUE(screen.graph->Compile()->valid);

    TestGraph test;
    auto subgraph = AddSubgraph(test.graph, screen.handle);
    test.LinkOffset(subgraph, "Position");
    auto compiled = test.Compile();

    EXPECT_FALSE(compiled->valid);
    ASSERT_EQ(compiled->errors.size(), size_t(1));
    EXPECT_EQ(compiled->errors.front().nodeId, subgraph);
    EXPECT_TRUE(test.HasError(*compiled, "isn't available in the vertex stage"));

    // Errors of the subgraph itself are reported as well
    screen.graph->GetNode(output)->name = "";
    EXPECT_FALSE(screen.graph->Compile()->valid);
    compiled = test.Compile();
    EXPECT_FALSE(compiled->valid);
    EXPECT_TRUE(test.HasError(*compiled, "has errors: The pin of a subgraph needs a name"));

    // Subgraphs are no materials and materials no subgraphs
    TestSubgraph invalid;
    invalid.AddInput("A", ValueType::Float);
    EXPECT_TRUE(test.HasError(*invalid.graph->Compile(), "needs an output"));
    invalid.AddOutput("B", ValueType::Float);
    invalid.Add(NodeLibrary::surfaceOutputType);
    EXPECT_TRUE(test.HasError(*invalid.graph->Compile(), "can't have the outputs of a material"));

    TestGraph material;
    material.graph.Compile();
    TestGraph user;
    auto handle = ResourceHandle<Graph>(CreateRef<Graph>(material.graph));
    AddSubgraph(user.graph, handle);
    EXPECT_TRUE(user.HasError(*user.Compile(), "has no inputs or outputs of a subgraph"));
    AddSubgraph(user.graph, ResourceHandle<Graph>());
    EXPECT_TRUE(user.HasError(*user.Compile(), "The node has no subgraph"));

}

TEST(MaterialGraphSubgraphs, CantContainThemselves) {

    auto first = TestSubgraph::CreateScale();
    auto second = TestSubgraph::CreateScale();

    AddSubgraph(*first.graph, second.handle);
    EXPECT_TRUE(first.graph->Compile()->valid);

    AddSubgraph(*second.graph, first.handle);
    auto compiled = second.graph->Compile();
    EXPECT_FALSE(compiled->valid);
    ASSERT_FALSE(compiled->errors.empty());
    EXPECT_TRUE(Contains(compiled->errors.front().message, "contains itself")) << compiled->errors.front().message;

    EXPECT_TRUE(first.graph->Uses(*second.graph));
    EXPECT_TRUE(second.graph->Uses(*first.graph));
    TestGraph other;
    EXPECT_FALSE(first.graph->Uses(other.graph));

    // The handles would keep each other alive
    first.graph->nodes.clear();
    first.graph->Compile();
    second.graph->nodes.clear();
    second.graph->Compile();

}

TEST(MaterialGraphSubgraphs, KeepUniformOpacityAndUnusedWind) {

    TestSubgraph passThrough;
    auto input = passThrough.AddInput("In", ValueType::Float);
    auto output = passThrough.AddOutput("Out", ValueType::Float);
    passThrough.Link(input, "Value", output, "Value");
    ASSERT_TRUE(passThrough.graph->Compile()->valid);

    TestGraph test;
    auto alpha = test.Add("Float parameter", "Alpha");
    auto opacity = AddSubgraph(test.graph, passThrough.handle);
    test.Link(alpha, "Value", opacity, "In");
    test.LinkSurface(opacity, NodeLibrary::opacityPin, "Out");
    // Only leads to a zero
    auto wind = AddSubgraph(test.graph, passThrough.handle);
    test.LinkOffset(wind, "Out");
    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid) << compiled->errors.front().message;
    EXPECT_TRUE(compiled->hasUniformOpacity);
    EXPECT_FALSE(compiled->hasVertexOffset);

}

TEST(MaterialGraphSubgraphs, RoundTrip) {

    auto scale = TestSubgraph::CreateScale();

    // Removing the node with the largest id must not free the id for another pin
    auto removed = scale.AddInput("Removed", ValueType::Float);
    scale.graph->RemoveNode(removed);
    auto nextNodeId = scale.graph->nextNodeId;

    json j;
    to_json(j, *scale.graph);
    Graph loaded;
    from_json(json::parse(j.dump()), loaded);

    EXPECT_EQ(loaded.nextNodeId, nextNodeId);
    auto compiled = loaded.Compile();
    ASSERT_TRUE(compiled->valid);
    EXPECT_TRUE(compiled->IsSubgraph());
    auto pins = NodeLibrary::GetSubgraphPins(loaded.nodes, NodeLibrary::subgraphInputType);
    ASSERT_EQ(pins.size(), size_t(1));
    EXPECT_EQ(pins.front()->name, "Color");
    EXPECT_EQ(pins.front()->pinType, ValueType::Vec3);

    // Files from before have no next id
    j.erase("nextNodeId");
    from_json(j, loaded);
    EXPECT_GT(loaded.nextNodeId, loaded.nodes.back().id);

    auto defaultSubgraph = Graph::CreateDefaultSubgraph();
    EXPECT_TRUE(defaultSubgraph->GetCompiled()->valid);
    EXPECT_TRUE(defaultSubgraph->GetCompiled()->IsSubgraph());

}

TEST(MaterialGraphSubgraphs, LoadFromFiles) {

    namespace fs = std::filesystem;

    auto previousDirectory = Loader::AssetLoader::GetAssetDirectory();
    auto directory = fs::temp_directory_path() / "atlasSubgraphTest";
    fs::remove_all(directory);
    fs::create_directories(directory);
    Loader::AssetLoader::SetAssetDirectory(directory.generic_string());

    auto scale = TestSubgraph::CreateScale();
    Loader::MaterialGraphLoader::SaveGraph(scale.graph, "scale.aematsubgraph");

    auto subgraph = ResourceManager<Graph>::GetOrLoadResourceWithLoader("scale.aematsubgraph", ResourceOrigin::User,
        Loader::MaterialGraphLoader::LoadGraph);
    ASSERT_TRUE(subgraph.IsLoaded());

    TestGraph test;
    auto color = test.Add("Color parameter", "Tint");
    auto id = AddSubgraph(test.graph, subgraph);
    test.Link(color, "Value", id, "Color");
    test.LinkSurface(id, NodeLibrary::baseColorPin, "Result");
    auto code = test.Compile()->mainFragmentCode;

    auto saveGraph = [](const Graph& graph, const std::string& file) {
        Loader::MaterialGraphLoader::SaveGraph(CreateRef<Graph>(graph), file);
        };
    saveGraph(test.graph, "user.aematgraph");

    auto user = ResourceManager<Graph>::GetOrLoadResourceWithLoader("user.aematgraph", ResourceOrigin::User,
        Loader::MaterialGraphLoader::LoadGraph);
    ASSERT_TRUE(user.IsLoaded());
    ASSERT_TRUE(user->GetCompiled()->valid);
    EXPECT_EQ(user->GetCompiled()->mainFragmentCode, code);

    // Changes of the subgraph reach the loaded graphs using it
    subgraph->GetNode(NodeLibrary::GetSubgraphPins(subgraph->nodes, NodeLibrary::subgraphInputType).front()->id)->name = "Tint";
    subgraph->Compile();
    auto users = subgraph->CompileUsers();
    ASSERT_EQ(users.size(), size_t(1));
    EXPECT_EQ(users.front().GetResource()->path, "user.aematgraph");
    EXPECT_TRUE(user->GetCompiled()->valid);
    EXPECT_NE(user->GetInputLink(id, "Tint"), nullptr);

    // A missing file is an error, but the path and the links aren't lost
    saveGraph(test.graph, "missing.aematgraph");
    auto content = json::parse(std::ifstream(directory / "missing.aematgraph"));
    for (auto& node : content["nodes"]) {
        if (node.contains("subgraphPath"))
            node["subgraphPath"] = "notThere.aematsubgraph";
    }
    std::ofstream(directory / "missing.aematgraph") << content.dump();
    auto missing = Loader::MaterialGraphLoader::LoadGraph("missing.aematgraph");
    EXPECT_FALSE(missing->GetCompiled()->valid);
    EXPECT_TRUE(test.HasError(*missing->GetCompiled(), "couldn't be loaded"));
    saveGraph(*missing, "missing.aematgraph");
    auto saved = json::parse(std::ifstream(directory / "missing.aematgraph"));
    EXPECT_EQ(saved["links"], content["links"]);
    EXPECT_TRUE(Contains(saved.dump(), "notThere.aematsubgraph"));

    // Files containing each other fail instead of waiting for each other
    auto addSubgraphTo = [&](const std::string& file, const std::string& subgraphFile) {
        json j = json::parse(std::ifstream(directory / "scale.aematsubgraph"));
        j["nodes"].push_back({ { "id", 100 }, { "type", "Subgraph" }, { "position", vec2(0.0f) }, { "value", vec4(0.0f) },
            { "subgraphPath", subgraphFile } });
        std::ofstream(directory / file) << j.dump();
        };
    addSubgraphTo("a.aematsubgraph", "b.aematsubgraph");
    addSubgraphTo("b.aematsubgraph", "a.aematsubgraph");
    addSubgraphTo("outer.aematsubgraph", "a.aematsubgraph");
    EXPECT_THROW(Loader::MaterialGraphLoader::LoadGraph("a.aematsubgraph"), ResourceLoadException);
    auto outer = Loader::MaterialGraphLoader::LoadGraph("outer.aematsubgraph");
    EXPECT_FALSE(outer->GetCompiled()->valid);

    // Graphs loading at the same time wait for the subgraph they share
    auto shared = TestSubgraph::CreateScale();
    Loader::MaterialGraphLoader::SaveGraph(shared.graph, "shared.aematsubgraph");
    auto content2 = json::parse(std::ifstream(directory / "user.aematgraph"));
    std::vector<std::thread> threads;
    std::vector<ResourceHandle<Graph>> loadedGraphs(16);
    for (size_t i = 0; i < loadedGraphs.size(); i++) {
        auto file = "user" + std::to_string(i) + ".aematgraph";
        for (auto& node : content2["nodes"]) {
            if (node.contains("subgraphPath"))
                node["subgraphPath"] = "shared.aematsubgraph";
        }
        std::ofstream(directory / file) << content2.dump();
        threads.emplace_back([&loadedGraphs, file, i]() {
            loadedGraphs[i] = ResourceManager<Graph>::GetOrLoadResourceWithLoader(file, ResourceOrigin::User,
                Loader::MaterialGraphLoader::LoadGraph);
            });
    }
    for (auto& thread : threads)
        thread.join();
    for (const auto& graph : loadedGraphs) {
        ASSERT_TRUE(graph.IsLoaded());
        EXPECT_TRUE(graph->GetCompiled()->valid);
    }

    Loader::AssetLoader::SetAssetDirectory(previousDirectory.empty() ? "." : previousDirectory);
    fs::remove_all(directory);

}

TEST(MaterialGraphSubgraphs, ExtractAndDissolveKeepTheCode) {

    TestGraph test;
    auto color = test.Add("Color parameter", "Tint");
    auto strength = test.Add("Float parameter", "Strength");
    auto scale = test.Add("Multiply");
    auto fresnel = test.Add("Fresnel");
    auto glow = test.Add("Multiply");
    test.Link(color, "Value", scale, "A");
    test.Link(strength, "Value", scale, "B");
    test.Link(scale, "Result", glow, "A");
    test.Link(fresnel, "Result", glow, "B");
    test.LinkSurface(scale, NodeLibrary::baseColorPin, "Result");
    test.LinkSurface(glow, NodeLibrary::emissivePin, "Result");
    auto reference = test.Compile();
    ASSERT_TRUE(reference->valid);

    std::vector<Link> inputLinks, outputLinks;
    std::unordered_set<uint32_t> ids = { scale, fresnel, glow };
    auto subgraph = test.graph.ExtractSubgraph(ids, inputLinks, outputLinks);
    ASSERT_TRUE(subgraph->GetCompiled()->valid) << subgraph->GetCompiled()->errors.front().message;
    EXPECT_EQ(inputLinks.size(), size_t(2));
    EXPECT_EQ(outputLinks.size(), size_t(2));

    auto inputs = NodeLibrary::GetSubgraphPins(subgraph->nodes, NodeLibrary::subgraphInputType);
    ASSERT_EQ(inputs.size(), size_t(2));
    EXPECT_EQ(inputs[0]->name, "Tint");
    EXPECT_EQ(inputs[0]->pinType, ValueType::Vec3);
    EXPECT_EQ(inputs[1]->name, "Strength");
    EXPECT_EQ(inputs[1]->pinType, ValueType::Float);
    auto outputs = NodeLibrary::GetSubgraphPins(subgraph->nodes, NodeLibrary::subgraphOutputType);
    ASSERT_EQ(outputs.size(), size_t(2));
    EXPECT_EQ(outputs[0]->name, NodeLibrary::baseColorPin);
    EXPECT_EQ(outputs[1]->name, NodeLibrary::emissivePin);

    auto node = test.graph.ReplaceWithSubgraph(ids, ResourceHandle<Graph>(subgraph), inputLinks, outputLinks);
    EXPECT_EQ(test.graph.GetNode(scale), nullptr);
    auto compiled = test.Compile();
    ASSERT_TRUE(compiled->valid) << compiled->errors.front().message;
    EXPECT_EQ(compiled->mainFragmentCode, reference->mainFragmentCode);

    auto added = test.graph.DissolveSubgraph(node);
    EXPECT_EQ(added.size(), size_t(3));
    EXPECT_EQ(test.graph.GetNode(node), nullptr);
    compiled = test.Compile();
    ASSERT_TRUE(compiled->valid) << compiled->errors.front().message;
    EXPECT_EQ(compiled->mainFragmentCode, reference->mainFragmentCode);

}

TEST(MaterialGraphSubgraphs, DissolveKeepsConversionsAndValues) {

    auto scale = TestSubgraph::CreateScale(ValueType::Float);

    TestGraph test;
    auto color = test.Add("Color parameter", "Tint");
    auto converted = AddSubgraph(test.graph, scale.handle);
    auto valued = AddSubgraph(test.graph, scale.handle);
    test.Link(color, "Value", converted, "Color");
    test.graph.GetNode(valued)->inputValues["Color"] = vec4(0.5f);
    test.LinkSurface(converted, NodeLibrary::baseColorPin, "Result");
    test.LinkSurface(valued, NodeLibrary::roughnessPin, "Result");
    ASSERT_TRUE(test.Compile()->valid);

    test.graph.DissolveSubgraph(converted);
    test.graph.DissolveSubgraph(valued);
    auto compiled = test.Compile();

    ASSERT_TRUE(compiled->valid) << compiled->errors.front().message;
    EXPECT_FALSE(test.graph.Uses(*scale.graph));
    // Only the red channel gets into the multiplication, as before
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "(tintParameter.x)")) << compiled->mainFragmentCode;
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "floatValue * 0.25")) << compiled->mainFragmentCode;

}

TEST(MaterialGraphSubgraphs, PreviewsShowTheirOutputs) {

    TestSubgraph sampler;
    auto texture = sampler.AddInput("Map", ValueType::Texture);
    auto baseColor = sampler.AddOutput(NodeLibrary::baseColorPin, ValueType::Vec3);
    auto roughness = sampler.AddOutput(NodeLibrary::roughnessPin, ValueType::Float);
    auto sample = sampler.Add("Sample texture");
    sampler.Link(texture, "Value", sample, "Texture");
    sampler.Link(sample, "Color", baseColor, "Value");
    sampler.Link(sample, "Red", roughness, "Value");
    ASSERT_TRUE(sampler.graph->Compile()->valid);

    auto preview = Graph::CreateSubgraphPreview(sampler.handle);
    auto compiled = preview->GetCompiled();
    ASSERT_TRUE(compiled->valid) << compiled->errors.front().message;
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "surfaceOut.baseColor = sampleTextureColor")) << compiled->mainFragmentCode;
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "surfaceOut.roughness = sampleTextureRed")) << compiled->mainFragmentCode;

    // Nothing is named like a pin of a material
    auto scale = TestSubgraph::CreateScale();
    compiled = Graph::CreateSubgraphPreview(scale.handle)->GetCompiled();
    ASSERT_TRUE(compiled->valid);
    EXPECT_TRUE(Contains(compiled->mainFragmentCode, "surfaceOut.baseColor = multiplyResult"));

}

TEST(MaterialGraphSubgraphs, DissolveChangesNothingWithErrors) {

    // The defaults of two inputs are each other, the output doesn't need them
    TestSubgraph cycle;
    auto a = cycle.AddInput("A", ValueType::Float);
    auto b = cycle.AddInput("B", ValueType::Float);
    auto output = cycle.AddOutput("Result", ValueType::Float);
    auto unused = cycle.Add("Multiply");
    cycle.Link(a, "Value", b, "Default");
    cycle.Link(b, "Value", a, "Default");
    cycle.Link(a, "Value", unused, "A");
    cycle.graph->GetNode(output)->inputValues["Value"] = vec4(0.5f);
    ASSERT_TRUE(cycle.graph->Compile()->valid);

    TestGraph test;
    auto node = AddSubgraph(test.graph, cycle.handle);
    test.LinkSurface(node, NodeLibrary::roughnessPin, "Result");
    auto before = test.Compile();
    ASSERT_TRUE(before->valid);
    test.graph.DissolveSubgraph(node);
    EXPECT_EQ(test.graph.GetNode(node), nullptr);
    auto after = test.Compile();
    ASSERT_TRUE(after->valid) << after->errors.front().message;
    EXPECT_TRUE(Contains(after->mainFragmentCode, "0.5")) << after->mainFragmentCode;

    // The output needs the cycle
    cycle.Link(a, "Value", output, "Value");
    EXPECT_FALSE(cycle.graph->Compile()->valid);
    auto invalid = AddSubgraph(test.graph, cycle.handle);
    auto nodeCount = test.graph.nodes.size();
    EXPECT_TRUE(test.graph.DissolveSubgraph(invalid).empty());
    EXPECT_NE(test.graph.GetNode(invalid), nullptr);
    EXPECT_EQ(test.graph.nodes.size(), nodeCount);

}

TEST(MaterialGraphSubgraphs, DissolveOfPassedOnValues) {

    auto passThrough = ResourceHandle<Graph>(Graph::CreateDefaultSubgraph());

    TestGraph test;
    auto color = test.Add("Color parameter", "Tint");
    auto node = AddSubgraph(test.graph, passThrough);
    test.Link(color, "Value", node, "Value");
    test.LinkSurface(node, NodeLibrary::baseColorPin, "Result");
    auto before = test.Compile();

    EXPECT_TRUE(test.graph.DissolveSubgraph(node).empty());
    EXPECT_EQ(test.graph.GetNode(node), nullptr);
    ASSERT_NE(test.graph.GetInputLink(test.surfaceOutput, NodeLibrary::baseColorPin), nullptr);
    EXPECT_EQ(test.graph.GetInputLink(test.surfaceOutput, NodeLibrary::baseColorPin)->fromNode, color);
    EXPECT_EQ(test.Compile()->mainFragmentCode, before->mainFragmentCode);

}

TEST(MaterialGraphSubgraphs, DissolveKeepsTheTypesOfDefaults) {

    // The order of the links must not matter
    for (auto defaultFirst : { true, false }) {
        TestSubgraph subgraph;
        auto a = subgraph.AddInput("A", ValueType::Vec3);
        auto b = subgraph.AddInput("B", ValueType::Float);
        auto output = subgraph.AddOutput("Result", ValueType::Vec3);
        auto add = subgraph.Add("Add");
        auto multiply = subgraph.Add("Multiply");
        subgraph.graph->GetNode(add)->inputValues["B"] = vec4(0.125f);
        subgraph.graph->GetNode(multiply)->inputValues["B"] = vec4(2.0f);
        auto linkDefault = [&]() { subgraph.Link(add, "Result", b, "Default"); };
        auto linkInput = [&]() { subgraph.Link(a, "Value", add, "A"); };
        auto linkMultiply = [&]() { subgraph.Link(b, "Value", multiply, "A"); };
        if (defaultFirst) {
            linkMultiply();
            linkDefault();
            linkInput();
        }
        else {
            linkInput();
            linkDefault();
            linkMultiply();
        }
        subgraph.Link(multiply, "Result", output, "Value");
        ASSERT_TRUE(subgraph.graph->Compile()->valid);

        TestGraph test;
        auto color = test.Add("Color parameter", "Tint");
        auto node = AddSubgraph(test.graph, subgraph.handle);
        test.Link(color, "Value", node, "A");
        test.LinkSurface(node, NodeLibrary::baseColorPin, "Result");
        ASSERT_TRUE(Contains(test.Compile()->mainFragmentCode, "float multiplyResult"));

        test.graph.DissolveSubgraph(node);
        auto compiled = test.Compile();
        ASSERT_TRUE(compiled->valid) << compiled->errors.front().message;
        EXPECT_TRUE(Contains(compiled->mainFragmentCode, "float multiplyResult")) << compiled->mainFragmentCode;
    }

}

TEST(MaterialGraphSubgraphs, ClicksDontChangeTheLayoutOfEvaluations) {

    auto createParameter = [](const std::string& name, const std::string& type) {
        TestSubgraph test;
        auto output = test.AddOutput("Result", ValueType::Vec3);
        auto parameter = test.Add(type, name);
        test.Link(parameter, "Value", output, "Value");
        EXPECT_TRUE(test.graph->Compile()->valid);
        return test;
        };
    auto first = createParameter("First", "Vector 3 parameter");
    auto second = createParameter("Second", "Float parameter");

    TestGraph test;
    auto firstNode = AddSubgraph(test.graph, first.handle);
    auto secondNode = AddSubgraph(test.graph, second.handle);
    test.LinkSurface(firstNode, NodeLibrary::baseColorPin, "Result");
    test.LinkSurface(secondNode, NodeLibrary::emissivePin, "Result");

    auto getLayout = [&]() {
        std::string layout;
        for (const auto& parameter : Compiler::CompilePreview(test.graph, firstNode).parameters)
            layout += parameter.name + "@" + std::to_string(parameter.offset) + " ";
        return layout;
        };

    // A clicked node moves to the end
    auto layout = getLayout();
    auto iter = std::find_if(test.graph.nodes.begin(), test.graph.nodes.end(), [&](const Node& node) { return node.id == firstNode; });
    std::rotate(iter, iter + 1, test.graph.nodes.end());
    EXPECT_EQ(getLayout(), layout);
    EXPECT_FALSE(layout.empty());

}

TEST(MaterialGraphSubgraphs, EvaluationErrorsBelongToTheNodeUsingThem) {

    TestSubgraph sampler;
    auto map = sampler.AddInput("Map", ValueType::Texture);
    auto output = sampler.AddOutput("Color", ValueType::Vec3);
    auto sample = sampler.Add("Sample texture");
    sampler.Link(map, "Value", sample, "Texture");
    sampler.Link(sample, "Color", output, "Value");
    ASSERT_TRUE(sampler.graph->Compile()->valid);
    sampler.graph->name = "sampler";

    TestGraph test;
    auto node = AddSubgraph(test.graph, sampler.handle);
    test.LinkSurface(node, NodeLibrary::baseColorPin, "Color");

    for (const auto& shader : { Compiler::CompileBake(test.graph), Compiler::CompilePreview(test.graph, node) }) {
        EXPECT_FALSE(shader.valid);
        ASSERT_FALSE(shader.errors.empty());
        EXPECT_EQ(shader.errors.front().nodeId, node);
        EXPECT_TRUE(Contains(shader.errors.front().message, "sampler: The input Map")) << shader.errors.front().message;
    }

}

TEST(MaterialGraphSubgraphs, RenamedPinsReplaceRemovedOnes) {

    TestSubgraph subgraph;
    auto a = subgraph.AddInput("A", ValueType::Vec3);
    auto b = subgraph.AddInput("B", ValueType::Vec3);
    auto output = subgraph.AddOutput("Result", ValueType::Vec3);
    auto add = subgraph.Add("Add");
    subgraph.Link(a, "Value", add, "A");
    subgraph.Link(b, "Value", add, "B");
    subgraph.Link(add, "Result", output, "Value");
    ASSERT_TRUE(subgraph.graph->Compile()->valid);

    TestGraph test;
    auto first = test.Add("Color parameter", "First");
    auto second = test.Add("Color parameter", "Second");
    auto node = AddSubgraph(test.graph, subgraph.handle);
    test.Link(first, "Value", node, "A");
    test.Link(second, "Value", node, "B");
    test.LinkSurface(node, NodeLibrary::baseColorPin, "Result");
    ASSERT_TRUE(test.Compile()->valid);

    subgraph.graph->RemoveNode(b);
    subgraph.graph->GetNode(a)->name = "B";
    // Two pins of the same name are an error, nothing is followed until it is gone
    auto duplicate = subgraph.AddInput("B", ValueType::Vec3);
    EXPECT_FALSE(subgraph.graph->Compile()->valid);
    EXPECT_FALSE(test.Compile()->valid);
    EXPECT_NE(test.graph.GetInputLink(node, "A"), nullptr);
    subgraph.graph->RemoveNode(duplicate);
    ASSERT_TRUE(subgraph.graph->Compile()->valid);

    auto compiled = test.Compile();
    ASSERT_TRUE(compiled->valid) << compiled->errors.front().message;
    ASSERT_NE(test.graph.GetInputLink(node, "B"), nullptr);
    EXPECT_EQ(test.graph.GetInputLink(node, "B")->fromNode, first);
    EXPECT_EQ(std::count_if(test.graph.links.begin(), test.graph.links.end(), [&](const Link& link) { return link.toNode == node; }), 1);

    // Values of pins another subgraph doesn't have aren't kept
    test.graph.GetNode(node)->inputValues["B"] = vec4(1.0f);
    auto scale = TestSubgraph::CreateScale();
    test.graph.SetSubgraph(node, scale.handle);
    EXPECT_TRUE(test.graph.GetNode(node)->inputValues.empty());

}

TEST(MaterialGraphSubgraphs, ExtractNextToUnknownNodes) {

    TestGraph test;
    auto unknown = test.Add("Float");
    auto multiply = test.Add("Multiply");
    test.Link(unknown, "Value", multiply, "A");
    test.LinkSurface(multiply, NodeLibrary::roughnessPin, "Result");
    test.graph.GetNode(unknown)->type = "From a newer version";

    std::vector<Link> inputLinks, outputLinks;
    auto subgraph = test.graph.ExtractSubgraph({ multiply }, inputLinks, outputLinks);
    EXPECT_EQ(inputLinks.size(), size_t(1));
    EXPECT_EQ(outputLinks.size(), size_t(1));

    // Nodes without any link to other ones have no pins
    auto alone = test.Add("Float");
    inputLinks.clear();
    outputLinks.clear();
    EXPECT_FALSE(test.graph.ExtractSubgraph({ alone }, inputLinks, outputLinks)->GetCompiled()->IsSubgraph());

}

TEST(MaterialGraphSubgraphs, VertexValuesHaveAPreview) {

    TestSubgraph wind;
    auto output = wind.AddOutput("Offset", ValueType::Vec3);
    auto direction = wind.Add("World to object direction");
    wind.Link(direction, "Result", output, "Value");
    ASSERT_TRUE(wind.graph->Compile()->valid);

    auto compiled = Graph::CreateSubgraphPreview(wind.handle)->GetCompiled();
    EXPECT_TRUE(compiled->valid) << (compiled->errors.empty() ? "" : compiled->errors.front().message);

    // The texture of an input next to a parameter of the same name
    TestSubgraph sampler;
    auto map = sampler.AddInput("Map", ValueType::Texture);
    auto scale = sampler.Add("Float parameter", "Map");
    auto color = sampler.AddOutput("Color", ValueType::Vec3);
    auto sample = sampler.Add("Sample texture");
    auto multiply = sampler.Add("Multiply");
    sampler.Link(map, "Value", sample, "Texture");
    sampler.Link(sample, "Color", multiply, "A");
    sampler.Link(scale, "Value", multiply, "B");
    sampler.Link(multiply, "Result", color, "Value");
    ASSERT_TRUE(sampler.graph->Compile()->valid);
    compiled = Graph::CreateSubgraphPreview(sampler.handle)->GetCompiled();
    EXPECT_TRUE(compiled->valid) << (compiled->errors.empty() ? "" : compiled->errors.front().message);

}

TEST(MaterialGraphSubgraphs, CompilingWithoutChangesWritesNothing) {

    auto scale = TestSubgraph::CreateScale();

    TestGraph test;
    auto node = AddSubgraph(test.graph, scale.handle);
    test.LinkSurface(node, NodeLibrary::baseColorPin, "Result");
    test.Compile();

    auto definition = test.graph.GetNode(node)->definition;
    test.Compile();
    EXPECT_EQ(test.graph.GetNode(node)->definition, definition);

}
