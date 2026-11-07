#include "NodeLibrary.h"

#include <algorithm>
#include <unordered_map>

namespace Atlas::MaterialGraph {

    const std::string NodeLibrary::vertexOutputType = "Vertex output";
    const std::string NodeLibrary::surfaceOutputType = "Surface output";

    const std::string NodeLibrary::positionOffsetPin = "Position offset";
    const std::string NodeLibrary::vertexNormalPin = "Normal";
    const std::string NodeLibrary::baseColorPin = "Base color";
    const std::string NodeLibrary::normalPin = "Normal";
    const std::string NodeLibrary::roughnessPin = "Roughness";
    const std::string NodeLibrary::metalnessPin = "Metalness";
    const std::string NodeLibrary::aoPin = "Ambient occlusion";
    const std::string NodeLibrary::emissivePin = "Emissive";
    const std::string NodeLibrary::opacityPin = "Opacity";

    const InputDefinition* NodeDefinition::FindInput(const std::string& name) const {

        auto iter = std::find_if(inputs.begin(), inputs.end(), [&](const auto& input) { return input.name == name; });
        return iter != inputs.end() ? &*iter : nullptr;

    }

    const OutputDefinition* NodeDefinition::FindOutput(const std::string& name) const {

        auto iter = std::find_if(outputs.begin(), outputs.end(), [&](const auto& output) { return output.name == name; });
        return iter != outputs.end() ? &*iter : nullptr;

    }

    const NodeDefinition* NodeLibrary::Find(const std::string& type) {

        static const auto map = []() {
            std::unordered_map<std::string, const NodeDefinition*> definitionMap;
            for (const auto& definition : GetDefinitions())
                definitionMap[definition.type] = &definition;
            return definitionMap;
            }();

        auto iter = map.find(type);
        return iter != map.end() ? iter->second : nullptr;

    }

    const std::vector<NodeDefinition>& NodeLibrary::GetDefinitions() {

        static const auto definitions = CreateDefinitions();
        return definitions;

    }

    std::vector<NodeDefinition> NodeLibrary::CreateDefinitions() {

        const auto F = ValueType::Float;
        const auto V2 = ValueType::Vec2;
        const auto V3 = ValueType::Vec3;
        const auto V4 = ValueType::Vec4;
        const auto T = ValueType::Texture;
        const auto G = ValueType::Generic;

        std::vector<NodeDefinition> definitions;

        auto input = [&](const char* type, const char* tooltip, ValueType valueType, const char* expression,
            uint32_t stages = AllStages) {
            definitions.push_back({ .type = type, .category = "Input", .tooltip = tooltip, .stages = stages,
                .outputs = { { "Value", valueType, expression } } });
            };

        input("Time", "Time of the scene in seconds, stands still while the scene is paused", F, "$IN.time");
        input("Texture coordinates", "Texture coordinates of the mesh", V2, "$IN.texCoord");
        input("Object position", "Position in the space of the mesh, without the displacement of the graph", V3, "$IN.position");
        input("World position", "Position in world space", V3, "$IN.worldPosition");
        input("View position", "Position relative to the camera", V3, "$IN.viewPosition", SurfaceStage);
        input("Instance position", "Origin of the instance in world space, e.g. to vary instances", V3, "$IN.instancePosition");
        input("Object normal", "Normal in the space of the mesh", V3, "$IN.normal");
        input("World normal", "Normal in world space", V3, "$IN.worldNormal");
        input("View normal", "Normal in view space", V3, "$IN.viewNormal", SurfaceStage);
        input("Object tangent", "Tangent in the space of the mesh", V3, "$IN.tangent", VertexStage);
        input("View direction", "World space direction from the surface to the camera", V3, "$IN.viewDirection");
        input("Vertex color", "Color of the vertices, white for meshes without colors", V4, "$IN.color");
        input("Camera position", "Location of the camera in world space", V3, "globalData.cameraLocation.xyz");
        input("Wind direction", "Direction of the scene wind in the xz plane", V2, "globalData.windDir");
        input("Wind speed", "Speed of the scene wind", F, "globalData.windSpeed");
        input("Screen position", "Position on the screen from 0 to 1", V2, "$IN.screenCoord", SurfaceStage);
        input("Front facing", "1 on the front of a triangle, -1 on its back", F, "$IN.frontFacing", SurfaceStage);

        auto constant = [&](const char* type, const char* tooltip, ValueType valueType, vec4 value, bool isColor = false) {
            definitions.push_back({ .type = type, .category = "Constant", .tooltip = tooltip, .kind = NodeKind::Constant,
                .outputs = { { "Value", valueType, "" } }, .defaultValue = value, .valueType = valueType, .isColor = isColor });
            };

        constant("Float", "A fixed number, changing it compiles the shaders again", F, vec4(1.0f));
        constant("Vector 2", "A fixed vector, changing it compiles the shaders again", V2, vec4(0.0f));
        constant("Vector 3", "A fixed vector, changing it compiles the shaders again", V3, vec4(0.0f));
        constant("Vector 4", "A fixed vector, changing it compiles the shaders again", V4, vec4(0.0f));
        constant("Color", "A fixed color, changing it compiles the shaders again", V3, vec4(1.0f), true);

        auto parameter = [&](const char* type, const char* tooltip, ValueType valueType, vec4 value, bool isColor = false) {
            definitions.push_back({ .type = type, .category = "Parameter", .tooltip = tooltip, .kind = NodeKind::Parameter,
                .outputs = { { "Value", valueType, "" } }, .defaultValue = value, .valueType = valueType, .isColor = isColor });
            };

        parameter("Float parameter", "A number each material can change without compiling shaders", F, vec4(1.0f));
        parameter("Vector 2 parameter", "A vector each material can change without compiling shaders", V2, vec4(0.0f));
        parameter("Vector 3 parameter", "A vector each material can change without compiling shaders", V3, vec4(0.0f));
        parameter("Vector 4 parameter", "A vector each material can change without compiling shaders", V4, vec4(0.0f));
        parameter("Color parameter", "A color each material can change without compiling shaders", V3, vec4(1.0f), true);
        parameter("Texture parameter", "A texture each material can change. There is no limit on the number of textures", T,
            vec4(1.0f, 0.0f, 0.0f, 0.0f));

        const std::string coordinates = "$IN.texCoord";

        auto texture = [&](const char* type, const char* tooltip, std::vector<InputDefinition> inputs,
            std::vector<OutputDefinition> outputs, uint32_t stages = AllStages) {
            definitions.push_back({ .type = type, .category = "Texture", .tooltip = tooltip, .stages = stages,
                .inputs = std::move(inputs), .outputs = std::move(outputs) });
            };

        texture("Sample texture", "Reads a texture. The vertex stage reads the most detailed level",
            { { "Texture", T }, { "Coordinates", V2, vec4(0.0f), coordinates } },
            { { "Color", V3, "MGSampleTexture($0, $1).rgb" }, { "Red", F, "MGSampleTexture($0, $1).r" },
              { "Green", F, "MGSampleTexture($0, $1).g" }, { "Blue", F, "MGSampleTexture($0, $1).b" },
              { "Alpha", F, "MGSampleTexture($0, $1).a" } });
        texture("Sample texture level", "Reads a level of a texture",
            { { "Texture", T }, { "Coordinates", V2, vec4(0.0f), coordinates }, { "Level", F } },
            { { "Color", V3, "MGSampleTextureLod($0, $1, $2).rgb" }, { "Alpha", F, "MGSampleTextureLod($0, $1, $2).a" } });
        texture("Sample normal map", "Reads a normal map as a tangent space normal",
            { { "Texture", T }, { "Coordinates", V2, vec4(0.0f), coordinates }, { "Strength", F, vec4(1.0f) } },
            { { "Normal", V3, "MGSampleNormal($0, $1, $2)" } });
        texture("Triplanar texture", "Projects a texture along the three axes, which doesn't need texture coordinates",
            { { "Texture", T }, { "Position", V3, vec4(0.0f), "$IN.worldPosition" }, { "Normal", V3, vec4(0.0f), "$IN.worldNormal" },
              { "Scale", F, vec4(1.0f) }, { "Sharpness", F, vec4(4.0f) } },
            { { "Color", V3, "MGSampleTriplanar($0, $1, $2, $3, $4).rgb" } });
        texture("Triplanar normal map", "Projects a normal map along the three axes, which doesn't need texture coordinates or tangents",
            { { "Texture", T }, { "Position", V3, vec4(0.0f), "$IN.worldPosition" }, { "Normal", V3, vec4(0.0f), "$IN.worldNormal" },
              { "Scale", F, vec4(1.0f) }, { "Sharpness", F, vec4(4.0f) }, { "Strength", F, vec4(1.0f) } },
            { { "Normal", V3, "normalize(transpose($IN.tangentFrame) * (mat3(globalData.vMatrix) * MGSampleTriplanarNormal($0, $1, $2, $3, $4, $5)))" },
              { "World normal", V3, "MGSampleTriplanarNormal($0, $1, $2, $3, $4, $5)" } }, SurfaceStage);
        texture("Sample without tiling", "Reads a texture with a random offset per tile, which hides repetitions",
            { { "Texture", T }, { "Coordinates", V2, vec4(0.0f), coordinates }, { "Blend", F, vec4(0.25f) } },
            { { "Color", V3, "MGSampleWithoutTiling($0, $1, $2).rgb" }, { "Alpha", F, "MGSampleWithoutTiling($0, $1, $2).a" } }, SurfaceStage);
        texture("Parallax coordinates", "Moves coordinates along the view direction according to a height texture, surfaces seem to have depth",
            { { "Height", T }, { "Coordinates", V2, vec4(0.0f), coordinates }, { "Scale", F, vec4(0.05f) }, { "Steps", F, vec4(16.0f) } },
            { { "Coordinates", V2, "MGParallax($0, $1, transpose($IN.tangentFrame) * (mat3(globalData.vMatrix) * $IN.viewDirection), $2, $3)" } }, SurfaceStage);
        texture("Texture size", "Width and height of a texture in pixels",
            { { "Texture", T } }, { { "Size", V2, "MGTextureSize($0)" } });

        auto math = [&](const char* type, const char* tooltip, std::vector<InputDefinition> inputs, ValueType outputType,
            const char* expression, const char* category = "Math") {
            definitions.push_back({ .type = type, .category = category, .tooltip = tooltip, .inputs = std::move(inputs),
                .outputs = { { "Result", outputType, expression } } });
            };

        math("Add", "A + B", { { "A", G }, { "B", G } }, G, "$0 + $1");
        math("Subtract", "A - B", { { "A", G }, { "B", G } }, G, "$0 - $1");
        math("Multiply", "A * B", { { "A", G, vec4(1.0f) }, { "B", G, vec4(1.0f) } }, G, "$0 * $1");
        math("Divide", "A / B", { { "A", G, vec4(1.0f) }, { "B", G, vec4(1.0f) } }, G, "$0 / $1");
        math("Power", "A to the power of B", { { "A", G, vec4(1.0f) }, { "B", G, vec4(2.0f) } }, G, "pow($0, $1)");
        math("Minimum", "The smaller value", { { "A", G }, { "B", G } }, G, "min($0, $1)");
        math("Maximum", "The larger value", { { "A", G }, { "B", G } }, G, "max($0, $1)");
        math("Modulo", "Remainder of A / B", { { "A", G }, { "B", G, vec4(1.0f) } }, G, "mod($0, $1)");
        math("Mix", "Blends from A to B", { { "A", G }, { "B", G, vec4(1.0f) }, { "Factor", F, vec4(0.5f) } }, G, "mix($0, $1, $2)");
        math("Clamp", "Limits the value to a range", { { "Value", G }, { "Minimum", G }, { "Maximum", G, vec4(1.0f) } }, G, "clamp($0, $1, $2)");
        math("Saturate", "Limits the value to the range of 0 to 1", { { "Value", G } }, G, "clamp($0, 0.0, 1.0)");
        math("Step", "0 below the edge, 1 above", { { "Edge", F, vec4(0.5f) }, { "Value", G } }, G, "step($0, $1)");
        math("Smooth step", "Smooth transition from 0 to 1 between two edges",
            { { "Edge 0", F }, { "Edge 1", F, vec4(1.0f) }, { "Value", G } }, G, "smoothstep($0, $1, $2)");
        math("Remap", "Maps a range to another one",
            { { "Value", F }, { "From minimum", F }, { "From maximum", F, vec4(1.0f) }, { "To minimum", F }, { "To maximum", F, vec4(1.0f) } },
            F, "MGRemap($0, $1, $2, $3, $4)");
        math("One minus", "1 - value", { { "Value", G } }, G, "1.0 - $0");
        math("Negate", "-value", { { "Value", G } }, G, "-$0");
        math("Absolute", "Absolute value", { { "Value", G } }, G, "abs($0)");
        math("Sign", "-1, 0 or 1", { { "Value", G } }, G, "sign($0)");
        math("Floor", "Rounds down", { { "Value", G } }, G, "floor($0)");
        math("Ceil", "Rounds up", { { "Value", G } }, G, "ceil($0)");
        math("Round", "Rounds to the closest number", { { "Value", G } }, G, "round($0)");
        math("Fraction", "The part after the decimal point", { { "Value", G } }, G, "fract($0)");
        math("Square root", "Square root", { { "Value", G } }, G, "sqrt($0)");
        math("Exponential", "e to the power of the value", { { "Value", G } }, G, "exp($0)");
        math("Sine", "Sine of radians", { { "Value", G } }, G, "sin($0)");
        math("Cosine", "Cosine of radians", { { "Value", G } }, G, "cos($0)");
        math("Tangent", "Tangent of radians", { { "Value", G } }, G, "tan($0)");
        math("Arc tangent", "Angle of a direction in radians", { { "Y", F }, { "X", F, vec4(1.0f) } }, F, "atan($0, $1)");

        math("Dot", "Dot product", { { "A", G }, { "B", G } }, F, "dot($0, $1)", "Vector");
        math("Cross", "Cross product", { { "A", V3 }, { "B", V3 } }, V3, "cross($0, $1)", "Vector");
        math("Length", "Length of a vector", { { "Value", G } }, F, "length($0)", "Vector");
        math("Distance", "Distance between two points", { { "A", G }, { "B", G } }, F, "distance($0, $1)", "Vector");
        math("Normalize", "Vector with a length of 1, zero stays zero", { { "Value", V3 } }, V3, "MGNormalize($0)", "Vector");
        math("Reflect", "Reflects a direction on a normal", { { "Direction", V3 }, { "Normal", V3, vec4(0.0f, 1.0f, 0.0f, 0.0f) } }, V3, "reflect($0, $1)", "Vector");
        math("Rotate around axis", "Rotates a vector",
            { { "Vector", V3 }, { "Axis", V3, vec4(0.0f, 1.0f, 0.0f, 0.0f) }, { "Angle", F } }, V3, "MGRotateAxis($0, $1, $2)", "Vector");
        math("Combine vector 2", "Vector from numbers", { { "X", F }, { "Y", F } }, V2, "vec2($0, $1)", "Vector");
        math("Combine vector 3", "Vector from numbers", { { "X", F }, { "Y", F }, { "Z", F } }, V3, "vec3($0, $1, $2)", "Vector");
        math("Combine vector 4", "Vector from numbers", { { "X", F }, { "Y", F }, { "Z", F }, { "W", F, vec4(1.0f) } }, V4, "vec4($0, $1, $2, $3)", "Vector");
        definitions.push_back({ .type = "Split vector", .category = "Vector", .tooltip = "Components of a vector",
            .inputs = { { "Vector", V4 } },
            .outputs = { { "X", F, "$0.x" }, { "Y", F, "$0.y" }, { "Z", F, "$0.z" }, { "W", F, "$0.w" }, { "XY", V2, "$0.xy" }, { "XYZ", V3, "$0.xyz" } } });

        math("Object to world direction", "Transforms a normal or direction into world space, in the vertex stage",
            { { "Direction", V3 } }, V3, "MGNormalize(mat3($IN.modelMatrix) * $0)", "Transform");
        definitions.back().stages = VertexStage;
        math("World to object direction", "Transforms a direction or offset from world space into the space of the mesh, in the vertex stage",
            { { "Direction", V3 } }, V3, "inverse(mat3($IN.modelMatrix)) * $0", "Transform");
        definitions.back().stages = VertexStage;
        math("World to tangent normal", "Turns a world space normal into the tangent space of the surface output",
            { { "Normal", V3, vec4(0.0f, 1.0f, 0.0f, 0.0f) } }, V3,
            "normalize(transpose($IN.tangentFrame) * (mat3(globalData.vMatrix) * $0))", "Transform");
        definitions.back().stages = SurfaceStage;

        math("Scale coordinates", "Scales and moves coordinates", { { "Coordinates", V2, vec4(0.0f), coordinates },
            { "Scale", V2, vec4(1.0f) }, { "Offset", V2 } }, V2, "$0 * $1 + $2", "Coordinates");
        math("Pan coordinates", "Moves coordinates over time", { { "Coordinates", V2, vec4(0.0f), coordinates },
            { "Speed", V2, vec4(0.1f, 0.0f, 0.0f, 0.0f) } }, V2, "$0 + $1 * $IN.time", "Coordinates");
        math("Rotate coordinates", "Rotates coordinates around a center", { { "Coordinates", V2, vec4(0.0f), coordinates },
            { "Center", V2, vec4(0.5f) }, { "Angle", F } }, V2, "MGRotateCoord($0, $1, $2)", "Coordinates");
        math("Polar coordinates", "Distance from a center and the angle around it from 0 to 1", { { "Coordinates", V2, vec4(0.0f), coordinates },
            { "Center", V2, vec4(0.5f) }, { "Radial scale", F, vec4(1.0f) }, { "Angle scale", F, vec4(1.0f) } }, V2, "MGPolar($0, $1, $2, $3)", "Coordinates");
        math("Flipbook coordinates", "Coordinates of one frame of a texture with a grid of frames", { { "Coordinates", V2, vec4(0.0f), coordinates },
            { "Columns", F, vec4(4.0f) }, { "Rows", F, vec4(4.0f) }, { "Frame", F } }, V2, "MGFlipbook($0, $1, $2, $3)", "Coordinates");
        math("Twirl coordinates", "Rotates coordinates more the further away they are from a center", { { "Coordinates", V2, vec4(0.0f), coordinates },
            { "Center", V2, vec4(0.5f) }, { "Strength", F, vec4(2.0f) } }, V2, "MGTwirl($0, $1, $2)", "Coordinates");
        math("Distort coordinates", "Moves coordinates by noise", { { "Coordinates", V2, vec4(0.0f), coordinates },
            { "Scale", F, vec4(4.0f) }, { "Strength", F, vec4(0.05f) }, { "Seed", F } }, V2, "MGDistort($0, $1, $2, $3)", "Coordinates");
        math("Mirror coordinates", "Repeats coordinates, every second repetition is mirrored, which hides seams",
            { { "Coordinates", V2, vec4(0.0f), coordinates } }, V2, "1.0 - abs(mod($0, 2.0) - 1.0)", "Coordinates");
        definitions.push_back({ .type = "Tile coordinates", .category = "Coordinates", .tooltip = "Splits coordinates into a grid of tiles",
            .inputs = { { "Coordinates", V2, vec4(0.0f), coordinates }, { "Tiles", V2, vec4(4.0f) } },
            .outputs = { { "Coordinates", V2, "fract($0 * $1)" }, { "Tile", V2, "floor($0 * $1)" } } });
        definitions.push_back({ .type = "Planar coordinates", .category = "Coordinates", .tooltip = "Coordinates from a position, which doesn't need texture coordinates",
            .inputs = { { "Position", V3, vec4(0.0f), "$IN.worldPosition" }, { "Scale", F, vec4(1.0f) } },
            .outputs = { { "XZ", V2, "$0.xz * $1" }, { "XY", V2, "$0.xy * $1" }, { "ZY", V2, "$0.zy * $1" } } });

        auto logic = [&](const char* type, const char* tooltip, const char* expression, vec4 valueB = vec4(0.0f)) {
            math(type, tooltip, { { "A", F }, { "B", F, valueB } }, F, expression, "Logic");
            };

        logic("Greater", "1 if A is larger than B, otherwise 0", "float($0 > $1)");
        logic("Greater or equal", "1 if A is at least B, otherwise 0", "float($0 >= $1)");
        logic("Less", "1 if A is smaller than B, otherwise 0", "float($0 < $1)");
        logic("Less or equal", "1 if A is at most B, otherwise 0", "float($0 <= $1)");
        math("Equal", "1 if A and B are closer than the tolerance, otherwise 0", { { "A", F }, { "B", F }, { "Tolerance", F, vec4(0.001f) } }, F,
            "float(abs($0 - $1) <= $2)", "Logic");
        logic("And", "The smaller value, 1 if both of 0 and 1 are 1", "min($0, $1)");
        logic("Or", "The larger value, 1 if one of 0 and 1 is 1", "max($0, $1)");
        logic("Exclusive or", "Difference of the values, 1 if only one of 0 and 1 is 1", "abs($0 - $1)");
        math("Not", "1 for 0 and 0 for 1", { { "Value", F } }, F, "1.0 - $0", "Logic");
        math("If", "True if the condition is above 0.5, otherwise false. Calculates both values, a mix is the same for values from 0 to 1",
            { { "Condition", F }, { "True", G, vec4(1.0f) }, { "False", G } }, G, "$0 > 0.5 ? $1 : $2", "Logic");
        definitions.push_back({ .type = "Static switch", .category = "Logic", .tooltip = "Only compiles the selected input, the nodes behind "
            "the other one don't cost anything. Changing it compiles the shaders again and applies to all materials with the graph",
            .kind = NodeKind::StaticSwitch, .inputs = { { "On", G, vec4(1.0f) }, { "Off", G } }, .outputs = { { "Result", G, "" } },
            .defaultValue = vec4(1.0f) });

        math("Simplex noise", "Smooth noise from -1 to 1", { { "Position", V3, vec4(0.0f), "$IN.position" }, { "Scale", F, vec4(1.0f) } },
            F, "MGSimplex($0 * $1)", "Procedural");
        math("Fractal noise", "Several layers of noise from -1 to 1", { { "Position", V3, vec4(0.0f), "$IN.position" }, { "Scale", F, vec4(1.0f) },
            { "Octaves", F, vec4(4.0f) }, { "Lacunarity", F, vec4(2.0f) }, { "Gain", F, vec4(0.5f) } },
            F, "MGFractalNoise($0 * $1, $2, $3, $4)", "Procedural");
        math("Ridged noise", "Noise with sharp ridges from 0 to 1, e.g. for rocks and veins", { { "Position", V3, vec4(0.0f), "$IN.position" },
            { "Scale", F, vec4(1.0f) }, { "Octaves", F, vec4(4.0f) }, { "Lacunarity", F, vec4(2.0f) }, { "Gain", F, vec4(0.5f) } },
            F, "MGRidgedNoise($0 * $1, $2, $3, $4)", "Procedural");
        definitions.push_back({ .type = "Voronoi", .category = "Procedural", .tooltip = "Cellular pattern, e.g. for cracks and crystals",
            .inputs = { { "Position", V3, vec4(0.0f), "$IN.position" }, { "Scale", F, vec4(1.0f) }, { "Jitter", F, vec4(1.0f) } },
            .outputs = { { "Distance", F, "MGVoronoi($0 * $1, $2).x" }, { "Second distance", F, "MGVoronoi($0 * $1, $2).y" },
                { "Edge", F, "(MGVoronoi($0 * $1, $2).y - MGVoronoi($0 * $1, $2).x)" }, { "Cell", F, "MGVoronoi($0 * $1, $2).z" } } });
        math("Checker", "Alternating cells of 0 and 1", { { "Position", V3, vec4(0.0f), "$IN.position" }, { "Scale", F, vec4(1.0f) } },
            F, "MGChecker($0 * $1)", "Procedural");
        math("Color ramp", "Gradient through three colors", { { "Position", F }, { "Color 0", V3 }, { "Color 1", V3, vec4(0.5f) },
            { "Color 2", V3, vec4(1.0f) }, { "Center", F, vec4(0.5f) } }, V3, "MGRamp($0, $1, $2, $3, $4)", "Procedural");
        math("Hue shift", "Rotates the hue of a color by radians", { { "Color", V3, vec4(1.0f) }, { "Shift", F } }, V3, "MGHueShift($0, $1)", "Procedural");
        math("Fresnel", "Bright where the surface turns away from the camera", { { "Normal", V3, vec4(0.0f), "$IN.worldNormal" }, { "Power", F, vec4(5.0f) } },
            F, "pow(1.0 - clamp(dot(normalize($0), $IN.viewDirection), 0.0, 1.0), $1)", "Procedural");

        math("Normal from height", "Tangent space normal of a height, also for procedural heights without any texture",
            { { "Height", F }, { "Strength", F, vec4(1.0f) } }, V3,
            "MGNormalFromHeight($0, $1, $IN.viewPosition, $IN.viewNormal, $IN.tangentFrame)", "Normal");
        definitions.back().stages = SurfaceStage;
        math("Blend normals", "Combines two tangent space normals, e.g. of several normal maps",
            { { "A", V3, vec4(0.0f, 0.0f, 1.0f, 0.0f) }, { "B", V3, vec4(0.0f, 0.0f, 1.0f, 0.0f) } }, V3, "MGBlendNormals($0, $1)", "Normal");

        definitions.push_back({ .type = "Vertex to surface", .category = "Utility",
            .tooltip = "Calculates a value per vertex and interpolates it for the surface, which is cheaper than doing it per pixel",
            .kind = NodeKind::Varying, .inputs = { { "Value", G } }, .outputs = { { "Value", G, "" } } });
        definitions.push_back({ .type = "Comment", .category = "Utility", .tooltip = "A note in the graph",
            .kind = NodeKind::Comment });
        definitions.push_back({ .type = "Frame", .category = "Utility", .tooltip = "A box with a title around nodes, which move along with it",
            .kind = NodeKind::Frame });
        definitions.push_back({ .type = "Reroute", .category = "Utility", .tooltip = "A point on a link to lead it around other nodes",
            .kind = NodeKind::Reroute, .inputs = { { "Value", G } }, .outputs = { { "Value", G, "$0" } } });

        definitions.push_back({ .type = vertexOutputType, .category = "Output", .tooltip = "Moves the vertices of the mesh. Without a link "
            "the vertices are the ones of the mesh",
            .kind = NodeKind::VertexOutput, .stages = VertexStage,
            .inputs = { { positionOffsetPin, V3 }, { vertexNormalPin, V3, vec4(0.0f), "$IN.normal" } },
            .defaultValue = vec4(0.0f, 0.02f, 0.0f, 0.0f) });
        definitions.push_back({ .type = surfaceOutputType, .category = "Output", .tooltip = "Describes the surface. Without a link "
            "the surface is the one of the regular material properties",
            .kind = NodeKind::SurfaceOutput, .stages = SurfaceStage,
            .inputs = { { baseColorPin, V3, vec4(1.0f) }, { normalPin, V3, vec4(0.0f, 0.0f, 1.0f, 0.0f) }, { roughnessPin, F, vec4(0.8f) },
                { metalnessPin, F }, { aoPin, F, vec4(1.0f) }, { emissivePin, V3 }, { opacityPin, F, vec4(1.0f) } } });

        return definitions;

    }

}
