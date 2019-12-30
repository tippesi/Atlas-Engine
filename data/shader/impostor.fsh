layout (location = 0) out vec4 diffuse;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 additional;
layout (location = 3) out vec2 velocity;

layout(binding = 0) uniform sampler2DArray diffuseMap;
layout(binding = 1) uniform sampler2DArray normalMap;

in vec2 fTexCoord;
in vec3 ndcCurrent;
in vec3 ndcLast;
flat in int index;

uniform mat4 vMatrix;

uniform vec2 jitterLast;
uniform vec2 jitterCurrent;

void main() {

    vec4 color = texture(diffuseMap, vec3(fTexCoord, float(index))).rgba;

    if (color.a < 0.2)
        discard;

    diffuse = vec4(color.rgb, 1.0);
    normal = 2.0 * texture(normalMap, vec3(fTexCoord, float(index))).rgb - 1.0;
    normal = 0.5 * normalize(vec3(vMatrix * vec4(normal, 0.0))) + 0.5;
    additional = vec2(0.0, 1.0);

    // Calculate velocity
	vec2 ndcL = ndcLast.xy / ndcLast.z;
	vec2 ndcC = ndcCurrent.xy / ndcCurrent.z;

	ndcL -= jitterLast;
	ndcC -= jitterCurrent;

	velocity = (ndcL - ndcC) * 0.5;

}