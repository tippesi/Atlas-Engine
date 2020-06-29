#include <filtering.hsh>

layout (location = 0) out vec4 outputFS;

in vec3 worldPositionVS;

void main() {

    vec3 N = normalize(worldPositionVS);

    // Use alpha channel for sky visibility
    outputFS = Irradiance(N);

}