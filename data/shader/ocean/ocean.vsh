#include <../common/utility.hsh>
#include <../common/PI.hsh>

#include <common.hsh>
#include <sharedUniforms.hsh>
#include <shoreInteraction.hsh>

layout(location=0) in vec3 vPosition;

layout(location=0) out vec4 fClipSpace;
layout(location=1) out vec3 fPosition;
layout(location=2) out vec3 fModelCoord;
layout(location=3) out vec3 fOriginalCoord;
// layout(location=5) out float waterDepth;
layout(location=6) out float shoreScaling;
layout(location=7) out vec3 ndcCurrent;
layout(location=8) out vec3 ndcLast;
#ifdef TERRAIN
layout(location=9) out vec3 normalShoreWave;
#endif
layout(location=10) out float perlinScale;

vec3 stitch(vec3 position) {
    
    // Note: This only works because our grid has a constant size
    // which cannot be changed.
    position.xz *= 128.0;
    
    if (position.x == 0.0 && PushConstants.leftLoD > 1.0) {
        position.z = floor(position.z / PushConstants.leftLoD)
            * PushConstants.leftLoD;
    }
    else if (position.z == 0.0 && PushConstants.topLoD > 1.0) {
        position.x = floor(position.x / PushConstants.topLoD)
            * PushConstants.topLoD;
    }
    else if (position.x == 128.0 && PushConstants.rightLoD > 1.0) {
        position.z = floor(position.z / PushConstants.rightLoD)
            * PushConstants.rightLoD;
    }
    else if (position.z == 128.0 && PushConstants.bottomLoD > 1.0) {
        position.x = floor(position.x / PushConstants.bottomLoD)
            * PushConstants.bottomLoD;
    }
    
    position.xz /= 128.0;
    
    return position;
    
}

void main() {
    
    fPosition = stitch(vPosition) * PushConstants.nodeSideLength +
        vec3(PushConstants.nodeLocation.x, 0.0, PushConstants.nodeLocation.y)
        + Uniforms.translation.xyz;

    fOriginalCoord = fPosition;
    
#ifndef TERRAIN
    vec3 normalShoreWave;
#endif   
    float distanceToCamera = distance(fOriginalCoord.xyz, globalData.cameraLocation.xyz);
    fPosition += GetOceanDisplacement(fPosition, distanceToCamera, perlinScale, shoreScaling, normalShoreWave);
    fModelCoord = fPosition;
    
    fPosition = vec3(globalData.vMatrix * vec4(fPosition, 1.0));
    fClipSpace = globalData.pMatrix * vec4(fPosition, 1.0);

    ndcCurrent = vec3(fClipSpace.xy, fClipSpace.w);
    // For moving objects we need the last matrix
    vec4 last = globalData.pvMatrixLast * vec4(fModelCoord, 1.0);
    ndcLast = vec3(last.xy, last.w);
    
    gl_Position = fClipSpace;
    
}