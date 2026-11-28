#include <deferred/texture.hsh>
#include <common/opacity.hsh>
#include <common/ign.hsh>
#include <shadowMapping.hsh>

#if defined(OPACITY_MAP) || defined(MATERIAL_GRAPH_SURFACE)
layout(location = 0) in vec2 texCoordVS;
#endif

#ifdef TRANSLUCENT
layout(location = 13) flat in float opacityVS;
#endif

#ifdef MATERIAL_GRAPH_SURFACE
layout(location = 1) in vec3 worldPositionVS;
layout(location = 2) in vec3 worldNormalVS;
layout(location = 3) in vec4 vertexColorsVS;
layout(location = 10) in vec3 objectPositionVS;
layout(location = 11) in vec3 instancePositionVS;
layout(location = 12) in vec3 objectNormalVS;
#endif

void main() {

    float opacity = 1.0;
#ifdef TRANSLUCENT
    opacity = opacityVS;
#endif

#ifdef OPACITY_MAP
    opacity *= SampleOpacity(texCoordVS, PushConstants.textureID, 0.0);
#endif

#ifdef MATERIAL_GRAPH_SURFACE
    MaterialSurfaceInput surfaceInput = CreateMaterialSurfaceInput(objectPositionVS, normalize(objectNormalVS),
        worldPositionVS, normalize(worldNormalVS), instancePositionVS, mat3(1.0), texCoordVS, vec2(0.0),
        vertexColorsVS, gl_FrontFacing ? 1.0 : -1.0, PushConstants.materialGraphOffset);
    opacity *= EvaluateMaterialSurface(surfaceInput).opacity;
#endif

#ifdef TRANSLUCENT
    // A pattern changing every frame lets light through by the opacity, the filtering of the shadow maps and the temporal
    // accumulation average it to a partial shadow. White noise leaves coarser blotches than interleaved gradient noise
    if (opacity <= GetInterleavedGradientNoise(gl_FragCoord.xy, 16u))
        discard;
#else
    if (opacity < opacityCutoff)
        discard;
#endif

}
