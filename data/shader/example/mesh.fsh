layout (location = 0) out vec4 colorFS;

layout (location = 0) in vec3 normalVS;
layout (location = 1) in vec2 texCoordVS;

// Texture binding, remember that uniform buffer is bound to set0, binding0
layout(set = 0, binding = 1) uniform sampler2D baseColorTexture;

const vec3 lightDirection = -vec3(1.0, 1.0, 0.0);
const vec3 lightColor = vec3(2.0);
const float lightAmbient = 0.2;

void main() {

    // Really simple lighting
    float NdotL = dot(normalize(normalVS), -normalize(lightDirection));
    colorFS = vec4(NdotL * lightColor + lightAmbient * lightColor, 1.0);

    vec2 texCoord = vec2(texCoordVS.x, 1.0 - texCoordVS.y);
    colorFS.rgb *= texture(baseColorTexture, texCoord).rgb;

}