// Evaluates the surface of a material graph for a hit of a ray tracing pipeline, the ray generation stage executes it
// with the hit object of the ray. The shaders generated from graphs include this file after their surface function,
// see materialgraph/Compiler
#extension GL_EXT_ray_tracing : require

#include <graphHit.hsh>

layout(location = 1) rayPayloadInEXT MaterialGraphHit graphHit;

void main() {

    mgHitTextureLevel = graphHit.textureLevel;

    mat4 modelMatrix = mat4(gl_ObjectToWorldEXT);
    mat3 tangentFrame = graphHit.tangentFrame;

    vec3 worldPosition = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
    vec3 objectPosition = gl_ObjectRayOriginEXT + gl_HitTEXT * gl_ObjectRayDirectionEXT;
    vec3 objectNormal = normalize(tangentFrame[2] * mat3(modelMatrix));

    MaterialSurfaceInput surfaceIn = CreateMaterialSurfaceInput(objectPosition, objectNormal, worldPosition,
        tangentFrame[2], vec3(modelMatrix[3]), mat3(globalData.vMatrix) * tangentFrame, graphHit.texCoord, vec2(0.5),
        unpackUnorm4x8(graphHit.color), graphHit.frontFacing, graphHit.parameterOffset);

    // Rays see the surface from their origin, not from the camera
    surfaceIn.viewDirection = -gl_WorldRayDirectionEXT;
    surfaceIn.tangent = normalize(transpose(mat3(modelMatrix)) * tangentFrame[0]);
    surfaceIn.modelMatrix = modelMatrix;

    MaterialSurfaceOutput surface = EvaluateMaterialSurface(surfaceIn);

    graphHit.baseColor = surface.baseColor;
    graphHit.normal = surface.normal;
    graphHit.emissive = surface.emissive;
    graphHit.roughness = surface.roughness;
    graphHit.metalness = surface.metalness;
    graphHit.ao = surface.ao;

}
