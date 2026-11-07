// Included by the shaders generated for evaluating a part of a graph, see materialgraph/Compiler

layout(location=0) in vec3 positionVS;
layout(location=1) in vec3 normalVS;
layout(location=2) in vec2 texCoordVS;
layout(location=3) in vec4 colorVS;
layout(location=4) in vec4 tangentVS;

#ifdef EVALUATE_PREVIEW
layout(location=0) out vec4 colorFS;
#else
layout(location=0) out vec4 baseColorFS;
layout(location=1) out vec4 normalFS;
layout(location=2) out vec4 roughnessMetalnessAoFS;
layout(location=3) out vec4 emissiveFS;
layout(location=4) out vec4 opacityFS;
#endif

layout(push_constant) uniform constants {
    float time;
    uint invertUVs;
} PushConstants;

void main() {

    vec3 normal = normalize(normalVS);

    // The same frame the geometry pass builds
    mat3 tangentFrame;
    if (dot(tangentVS.xyz, tangentVS.xyz) > 0.0) {
        vec3 tangent = normalize(tangentVS.xyz - normal * dot(tangentVS.xyz, normal));
        float correctionFactor = tangentVS.w * (PushConstants.invertUVs > 0 ? 1.0 : -1.0);
        tangentFrame = mat3(tangent, normalize(correctionFactor * cross(tangent, normal)), normal);
    }
    else {
        tangentFrame = MGCotangentFrame(normal, positionVS, texCoordVS);
    }

    MaterialSurfaceInput surfaceIn = CreateMaterialSurfaceInput(positionVS, normal, positionVS, normal, vec3(0.0),
        tangentFrame, texCoordVS, texCoordVS, colorVS, 1.0, 0u);

    surfaceIn.time = PushConstants.time;
    // Maps are the same from all directions, so they are made looking straight at the surface
    surfaceIn.viewDirection = normal;
    surfaceIn.tangent = tangentFrame[0];
    surfaceIn.modelMatrix = mat4(1.0);

#ifdef EVALUATE_PREVIEW
    vec4 value = EvaluateMaterialPreview(surfaceIn);
    colorFS = vec4(pow(clamp(value.rgb, vec3(0.0), vec3(1.0)), vec3(1.0 / 2.2)), 1.0);
#else
    MaterialSurfaceOutput surface = EvaluateMaterialSurface(surfaceIn);

    // The alpha channels mark the texels the mesh covers
    baseColorFS = vec4(surface.baseColor, 1.0);
    // Texels of degenerate triangles have no direction
    normalFS = vec4(any(isnan(surface.normal)) ? vec3(0.0, 0.0, 1.0) : surface.normal, 1.0);
    roughnessMetalnessAoFS = vec4(surface.roughness, surface.metalness, surface.ao, 1.0);
    emissiveFS = vec4(surface.emissive, 1.0);
    opacityFS = vec4(surface.opacity, 0.0, 0.0, 1.0);
#endif

}
