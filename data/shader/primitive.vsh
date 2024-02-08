#include <globals.hsh>

layout(location=0) in vec3 vPosition;
layout(location=1) in vec3 vColor;

layout(location=0) out vec3 colorVS;
layout(location=1) out vec3 ndcCurrentVS;
layout(location=2) out vec3 ndcLastVS;

void main() {

    gl_Position = globalData.pMatrix * globalData.vMatrix * vec4(vPosition, 1.0);

    colorVS = vColor;

    ndcCurrentVS = vec3(gl_Position.xy, gl_Position.w);
    // For moving objects we need the last frames matrix
    vec4 last = globalData.pvMatrixLast * vec4(vPosition, 1.0);
    ndcLastVS = vec3(last.xy, last.w);

}