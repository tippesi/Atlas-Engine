#include <../common/utility.hsh>
#include <../common/PI.hsh>

#include <common.hsh>
#include <sharedUniforms.hsh>
#include <shoreInteraction.hsh>

layout(location=0) in vec3 vPosition;

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
    
    vec3 fPosition = stitch(vPosition) * PushConstants.nodeSideLength +
        vec3(PushConstants.nodeLocation.x, 0.0, PushConstants.nodeLocation.y)
        + Uniforms.translation.xyz;

    float perlinScale, shoreScaling;
    vec3 normalShoreWave;
    fPosition += GetOceanDisplacement(fPosition, perlinScale, shoreScaling, normalShoreWave);
    
    fPosition = vec3(globalData.vMatrix * vec4(fPosition, 1.0));
    vec4 fClipSpace = globalData.pMatrix * vec4(fPosition, 1.0);
    
    gl_Position = fClipSpace;
    
}