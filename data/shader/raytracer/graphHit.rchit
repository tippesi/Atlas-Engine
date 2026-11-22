// Evaluates the surface of a material graph for a hit of a ray tracing pipeline, the ray generation stage executes it
// with the hit object of the ray. The shaders generated from graphs include this file after their surface function,
// see materialgraph/Compiler
#extension GL_EXT_ray_tracing : require

#include <../common/normalencode.hsh>
#include <graphHit.hsh>

layout(location = 1) rayPayloadInEXT MaterialGraphHit graphHit;

// Colors are linear and limited to 8 bits per channel like in the G-buffer, emission and the volume are halves
MaterialGraphHit PackGraphHitOutput(MaterialSurfaceOutput surface) {

    MaterialGraphHit hit;
    hit.data[0] = packUnorm4x8(vec4(surface.baseColor, surface.transmission));
    hit.data[1] = PackGraphHitDirection(surface.normal);
    hit.data[2] = packHalf2x16(surface.emissive.rg);
    hit.data[3] = packHalf2x16(vec2(surface.emissive.b, surface.refractiveIndex));
    hit.data[4] = packUnorm4x8(vec4(surface.roughness, surface.metalness, surface.ao, surface.reflectance));
    hit.data[5] = 0u;
    hit.data[6] = packUnorm4x8(vec4(surface.attenuationColor, surface.opacity));
    hit.data[7] = packHalf2x16(vec2(surface.thickness, surface.attenuationDistance));
    return hit;

}

void main() {

    MaterialGraphHitInput hitInput = UnpackGraphHitInput(graphHit);
    materialGraphHitTextureLevel = hitInput.textureLevel;

    mat4 modelMatrix = mat4(gl_ObjectToWorldEXT);
    mat3 tangentFrame = hitInput.tangentFrame;

    vec3 worldPosition = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
    vec3 objectPosition = gl_ObjectRayOriginEXT + gl_HitTEXT * gl_ObjectRayDirectionEXT;
    vec3 objectNormal = normalize(tangentFrame[2] * mat3(modelMatrix));

    MaterialSurfaceInput surfaceIn = CreateMaterialSurfaceInput(objectPosition, objectNormal, worldPosition,
        tangentFrame[2], vec3(modelMatrix[3]), mat3(globalData.vMatrix) * tangentFrame, hitInput.texCoord, vec2(0.5),
        hitInput.color, hitInput.frontFacing, hitInput.parameterOffset);

    // Rays see the surface from their origin, not from the camera
    surfaceIn.viewDirection = -gl_WorldRayDirectionEXT;
    surfaceIn.tangent = normalize(transpose(mat3(modelMatrix)) * tangentFrame[0]);
    surfaceIn.modelMatrix = modelMatrix;

    MaterialSurfaceOutput surface = EvaluateMaterialSurface(surfaceIn);

    graphHit = PackGraphHitOutput(surface);

}
