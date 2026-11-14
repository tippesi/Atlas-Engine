#include <deferred/texture.hsh>

#if defined(OPACITY_MAP) || defined(MATERIAL_GRAPH_SURFACE)
layout(location = 0) in vec2 texCoordVS;
#endif

#ifdef MATERIAL_GRAPH_SURFACE
layout(location = 1) in vec3 worldPositionVS;
layout(location = 2) in vec3 worldNormalVS;
layout(location = 3) in vec4 vertexColorsVS;
layout(location = 10) in vec3 objectPositionVS;
layout(location = 11) in vec3 instancePositionVS;
layout(location = 12) in vec3 objectNormalVS;
#endif

layout(push_constant) uniform constants {
    mat4 lightSpaceMatrix;
    uint vegetation;
    uint invertUVs;
    float windTextureLod;
    float windBendScale;
    float windWiggleScale;
    uint textureID;
    uint jointOffset;
    uint jointCount;
    uint instanceOffset;
#ifdef MATERIAL_GRAPH
    uint materialGraphOffset;
#endif
} PushConstants;

void main() {
    
#ifdef OPACITY_MAP
    float opacity = SampleOpacity(texCoordVS, PushConstants.textureID, 0.0);
    if(opacity < 0.2)
        discard;
#endif

#ifdef MATERIAL_GRAPH_SURFACE
    MaterialSurfaceInput surfaceInput = CreateMaterialSurfaceInput(objectPositionVS, normalize(objectNormalVS),
        worldPositionVS, normalize(worldNormalVS), instancePositionVS, mat3(1.0), texCoordVS, vec2(0.0),
        vertexColorsVS, gl_FrontFacing ? 1.0 : -1.0, PushConstants.materialGraphOffset);
    if (EvaluateMaterialSurface(surfaceInput).opacity < 0.2)
        discard;
#endif

}