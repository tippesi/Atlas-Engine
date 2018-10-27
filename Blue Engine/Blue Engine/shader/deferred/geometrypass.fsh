layout (location = 0) out vec3 diffuse;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 additional;

uniform sampler2D diffuseMap;
uniform sampler2D normalMap;

uniform bool useDiffuseMap;
uniform bool useNormalMap;

#ifdef REFLECTION
uniform samplerCube environmentCube;
uniform mat4 ivMatrix;
#endif

uniform vec3 diffuseColor;
uniform float specularIntensity;
uniform float specularHardness;
uniform float reflectivity;
uniform float brightness;

in vec2 fTexCoord;
in vec3 fNormal;

#ifdef NORMALMAPPING
in mat3 toTangentSpace;
#endif


#if defined(REFLECTION) || defined(FLAT)
in vec3 fPosition;
#endif

void main() {
	
	vec4 textureColor = vec4(diffuseColor, 1.0f);
	
	if(useDiffuseMap)
		textureColor *= texture(diffuseMap, fTexCoord);
	
	
	if(textureColor.a < 0.2f)
		discard;
	
#ifdef FLAT
	// Get the normal of the current polygon (so its flat)
	// We have to do this complicated because some devices dont support dFdx on 3 component vectors
	vec3 fdx = vec3(dFdx(fPosition.x),dFdx(fPosition.y),dFdx(fPosition.z));
	vec3 fdy = vec3(dFdy(fPosition.x),dFdy(fPosition.y),dFdy(fPosition.z));
	normal = cross(fdx, fdy);
#else
	normal = fNormal;
#endif
	
#if defined(NORMALMAPPING) && !defined(FLAT)
	if(useNormalMap)
		normal = normalize(toTangentSpace * (2.0f * texture(normalMap, fTexCoord).rgb - 1.0f));
	else
		normal = normalize(normal);
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