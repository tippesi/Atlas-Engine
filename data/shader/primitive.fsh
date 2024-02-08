#include <globals.hsh>

layout(location=0) out vec3 colorFS;
layout(location=1) out vec2 velocityFS;

layout(location=0) in vec3 colorVS;
layout(location=1) in vec3 ndcCurrentVS;
layout(location=2) in vec3 ndcLastVS;

void main() {

    colorFS = colorVS;

    vec2 ndcL = ndcLastVS.xy / ndcLastVS.z;
    vec2 ndcC = ndcCurrentVS.xy / ndcCurrentVS.z;

    ndcL -= globalData.jitterLast;
    ndcC -= globalData.jitterCurrent;

    velocityFS = (ndcL - ndcC) * 0.5;

}