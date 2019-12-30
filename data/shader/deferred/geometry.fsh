layout (location = 0) out vec4 diffuse;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 additional;
layout (location = 3) out vec2 velocity;

#ifdef DIFFUSE_MAP
layout(binding = 0) uniform sampler2D diffuseMap;
#endif
#ifdef NORMAL_MAP
layout(binding = 1) uniform sampler2D normalMap;
#endif
#ifdef SPECULAR_MAP
layout(binding = 2) uniform sampler2D specularMap;
#endif
#ifdef HEIGHT_MAP
layout(binding = 3) uniform sampler2D heightMap;
#endif

in vec2 fTexCoord;
in vec3 fNormal;
in vec3 fPosition;
in vec3 ndcCurrent;
in vec3 ndcLast;

#ifdef NORMAL_MAP
in mat3 toTangentSpace;
#endif


#ifdef REFLECTION
in vec3 fPosition;
#endif

#ifdef REFLECTION
uniform samplerCube environmentCube;
uniform mat4 ivMatrix;
#endif

uniform vec3 diffuseColor;

uniform float specularIntensity;
uniform float specularHardness;

uniform float normalScale;

uniform mat4 vMatrix;

uniform vec2 jitterLast;
uniform vec2 jitterCurrent;

void main() {
	
	vec4 textureColor = vec4(diffuseColor, 1.0);
	
#ifdef DIFFUSE_MAP
#ifdef ARRAY_MAP
	textureColor *= texture(arrayMap, vec3(fTexCoord, diffuseMapIndex));
#else
	textureColor *= texture(diffuseMap, fTexCoord);
#endif
	
	if (textureColor.a < 0.2)
		discard;
#endif

	normal = normalize(fNormal);

#ifdef NORMAL_MAP
#ifdef ARRAY_MAP
	vec3 normalColor = texture(arrayMap, vec3(fTexCoord, normalMapIndex)).rgb;
#else
	vec3 normalColor = texture(normalMap, fTexCoord).rgb;
#endif
	normal = mix(normal, normalize(toTangentSpace * (2.0 * normalColor - 1.0)), normalScale);
#endif

#ifdef REFLECTION
    vec3 R = mat3(ivMatrix) * reflect(normalize(fPosition), normal);
    diffuse = vec4(mix(textureColor.rgb, textureLod(environmentCube, R, specularHardness / 50.0).rgb, reflectivity), 1.0);
#else
	diffuse = vec4(textureColor.rgb, 1.0);
#endif

	additional = vec2(specularIntensity, specularHardness);
	
	normal = 0.5 * normal + 0.5;

	// Calculate velocity
	vec2 ndcL = ndcLast.xy / ndcLast.z;
	vec2 ndcC = ndcCurrent.xy / ndcCurrent.z;

	ndcL -= jitterLast;
	ndcC -= jitterCurrent;

	velocity = (ndcL - ndcC) * vec2(0.5, 0.5);
	
}