layout (location = 0) out vec3 diffuse;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 additional;


uniform sampler2D diffuseMap;

#ifdef NORMALMAPPING
uniform sampler2D normalMap;
#endif

uniform bool useDiffuseMap;
uniform bool useNormalMap;

#ifdef REFLECTION
uniform samplerCube environmentCube;
uniform mat4 ivMatrix;
#endif

const vec3 diffuseColor;
const float specularIntensity;
const float specularHardness;

in vec2 fTexCoord;
in vec3 fNormal;

#ifdef NORMALMAPPING
in mat3 toTangentSpace;
#endif


#if defined(REFLECTION)
in vec3 fPosition;
#endif

void main() {
	
	vec4 textureColor = vec4(diffuseColor, 1.0f);
	
	textureColor *= texture(diffuseMap, fTexCoord);
	
	normal = fNormal;

#if defined(NORMALMAPPING)
	normal = normalize(toTangentSpace * (2.0f * texture(normalMap, fTexCoord).rgb - 1.0f));
#else
	normal = normalize(normal);
#endif

#ifdef REFLECTION
    vec3 R = mat3(ivMatrix) * reflect(normalize(fPosition), normal);
    diffuse = mix(textureColor.rgb, textureLod(environmentCube, R, specularHardness / 50.0f).rgb, reflectivity);
#else
	diffuse = textureColor.rgb;
#endif

	additional = vec2(specularIntensity, specularHardness);
	
	normal = 0.5f * normal + 0.5f;
	
}