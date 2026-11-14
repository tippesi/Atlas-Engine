// Included by the shaders generated for evaluating a part of a graph, see materialgraph/Compiler

layout(location=0) in vec3 positionVS;
layout(location=1) in vec3 normalVS;
layout(location=2) in vec2 texCoordVS;
layout(location=3) in vec4 colorVS;
layout(location=4) in vec4 tangentVS;
#ifdef EVALUATE_IMPOSTOR
layout(location=5) in float depthVS;
#endif

#ifdef EVALUATE_PREVIEW
layout(location=0) out vec4 colorFS;
#elif defined(EVALUATE_IMPOSTOR)
// The layout of the impostor maps, see impostor/generate.fsh
layout(location=0) out vec4 baseColorFS;
layout(location=1) out vec3 normalFS;
layout(location=2) out vec3 roughnessMetalnessAoFS;
layout(location=3) out float depthFS;
#else
layout(location=0) out vec4 baseColorFS;
layout(location=1) out vec4 normalFS;
layout(location=2) out vec4 roughnessMetalnessAoFS;
layout(location=3) out vec4 emissiveFS;
layout(location=4) out vec4 opacityFS;
#endif

layout(push_constant) uniform constants {
#ifdef EVALUATE_IMPOSTOR
    mat4 vMatrix;
#endif
    float time;
    uint invertUVs;
#ifdef EVALUATE_IMPOSTOR
    float distanceToPlaneCenter;
    uint twoSided;
#endif
} PushConstants;

void main() {

    vec3 normal = normalize(normalVS);
    float frontFacing = 1.0;
#ifdef EVALUATE_IMPOSTOR
    frontFacing = gl_FrontFacing ? 1.0 : -1.0;
    normal *= PushConstants.twoSided > 0 ? frontFacing : 1.0;
#endif

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
        tangentFrame, texCoordVS, texCoordVS, colorVS, frontFacing, 0u);

    surfaceIn.time = PushConstants.time;
#ifdef EVALUATE_IMPOSTOR
    vec3 cameraLocation = -transpose(mat3(PushConstants.vMatrix)) * PushConstants.vMatrix[3].xyz;
    surfaceIn.viewDirection = normalize(cameraLocation - positionVS);
#else
    // Maps are the same from all directions, so they are made looking straight at the surface
    surfaceIn.viewDirection = normal;
#endif
    surfaceIn.tangent = tangentFrame[0];
    surfaceIn.modelMatrix = mat4(1.0);

#ifdef EVALUATE_PREVIEW
    vec4 value = EvaluateMaterialPreview(surfaceIn);
    colorFS = vec4(pow(clamp(value.rgb, vec3(0.0), vec3(1.0)), vec3(1.0 / 2.2)), 1.0);
#elif defined(EVALUATE_IMPOSTOR)
    MaterialSurfaceOutput surface = EvaluateMaterialSurface(surfaceIn);
    if (surface.opacity < 0.2)
        discard;

    // Impostors store the normal in the space of the mesh, like the geometry pass writes it
    vec3 surfaceNormal = normalize(tangentFrame * surface.normal);
    surfaceNormal = any(isnan(surfaceNormal)) ? normal : surfaceNormal * (dot(normal, surfaceNormal) < 0.0 ? -1.0 : 1.0);

    baseColorFS = vec4(surface.baseColor, 1.0);
    normalFS = 0.5 * surfaceNormal + 0.5;
    roughnessMetalnessAoFS = vec3(surface.roughness, surface.metalness, surface.ao);
    depthFS = depthVS;
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
