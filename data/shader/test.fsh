layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in flat uint fragFlags;

layout(location = 0) out vec4 outColor;

layout (std140, binding=0) uniform Matrices
{
    mat4 projection;
    mat4 view;
    uint uniformFlags;
};

#define VOXEL_FLAGS_LIGHTING_ENABLED 1u
#define UNIFORM_FLAG_DRAW_OPAQUE 1u
#define UNIFORM_FLAG_DRAW_TRANSPARENT 2u


void main() {

    vec3 result = fragColor.rgb;

    if (fragColor.a < 1.0){
        // transparent
        if ((uniformFlags & UNIFORM_FLAG_DRAW_TRANSPARENT) == 0u){
            discard;
        }
    } else {
        // opaque
        if ((uniformFlags & UNIFORM_FLAG_DRAW_OPAQUE) == 0u){
            discard;
        }
    }

    if ((fragFlags & VOXEL_FLAGS_LIGHTING_ENABLED) != 0u){
        float ambient = 0.5;
        vec3 lightDirection = vec3(0.816497, -0.408248, 0.408248);// normalized (2, -1, 1)

        float diffuse = max(dot(fragNormal, -lightDirection), 0.0);

        float faceFactor = dot(vec3(0.0, 1.0, 0.0), fragNormal);
        result = (ambient + diffuse + faceFactor * ambient * 0.5) * result;
    }

    outColor = vec4(result, fragColor.a);

}
