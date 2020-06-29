layout (location = 0) out vec4 baseColorFS;
layout (location = 2) out vec3 normalFS;
layout (location = 3) out vec3 roughnessMetalnessAoFS;
layout (location = 4) out uint materialIdxFS;
layout (location = 5) out vec2 velocityFS;

layout(binding = 0) uniform sampler2DArray baseColorMap;
layout(binding = 1) uniform sampler2DArray roughnessMetalnessAoMap;
layout(binding = 2) uniform sampler2DArray normalMap;

in vec2 texCoordVS;
in vec3 ndcCurrentVS;
in vec3 ndcLastVS;

#ifdef INTERPOLATION
flat in int index0VS;
flat in int index1VS;
flat in int index2VS;

flat in float weight0VS;
flat in float weight1VS;
flat in float weight2VS;
#else
flat in int indexVS;
#endif

uniform mat4 vMatrix;

uniform float cutoff;
uniform uint materialIdx;

uniform vec2 jitterLast;
uniform vec2 jitterCurrent;

void main() {

    vec4 baseColor;

#ifdef INTERPOLATION
    vec4 baseColor0 = texture(baseColorMap, vec3(texCoordVS, float(index0VS))).rgba;
    vec4 baseColor1 = texture(baseColorMap, vec3(texCoordVS, float(index1VS))).rgba;
    vec4 baseColor2 = texture(baseColorMap, vec3(texCoordVS, float(index2VS))).rgba;

    baseColor = weight0VS * baseColor0 + 
		weight1VS * baseColor1 + 
		  weight2VS * baseColor2;
#else
    baseColor = texture(baseColorMap, vec3(texCoordVS, float(indexVS))).rgba;
#endif

    if (baseColor.a < cutoff)
        discard;

    baseColorFS = vec4(baseColor.rgb, 1.0);

#ifdef INTERPOLATION
    vec3 normal0 = 2.0 * texture(normalMap, vec3(texCoordVS, float(index0VS))).rgb - 1.0;
    vec3 normal1 = 2.0 * texture(normalMap, vec3(texCoordVS, float(index1VS))).rgb - 1.0;
    vec3 normal2 = 2.0 * texture(normalMap, vec3(texCoordVS, float(index2VS))).rgb - 1.0;

    normalFS = weight0VS * normal0 + 
		weight1VS * normal1 + 
		weight2VS * normal2;
#else
	normalFS = 2.0 * texture(normalMap, vec3(texCoordVS, float(indexVS))).rgb - 1.0;
#endif

    normalFS = 0.5 * normalize(vec3(vMatrix * vec4(normalFS, 0.0))) + 0.5;

#ifdef INTERPOLATION
    vec3 matInfo0 = texture(roughnessMetalnessAoMap, vec3(texCoordVS, float(index0VS))).rgb;
    vec3 matInfo1 = texture(roughnessMetalnessAoMap, vec3(texCoordVS, float(index1VS))).rgb;
    vec3 matInfo2 = texture(roughnessMetalnessAoMap, vec3(texCoordVS, float(index2VS))).rgb;

    vec3 matInfo = weight0VS * matInfo0 + 
		weight1VS * matInfo1 + 
		weight2VS * matInfo2;

    roughnessMetalnessAoFS = matInfo;
#else
	roughnessMetalnessAoFS = texture(roughnessMetalnessAoMap, vec3(texCoordVS, float(indexVS))).rgb;
#endif

    // Calculate velocity
	vec2 ndcL = ndcLastVS.xy / ndcLastVS.z;
	vec2 ndcC = ndcCurrentVS.xy / ndcCurrentVS.z;

	ndcL -= jitterLast;
	ndcC -= jitterCurrent;

	velocityFS = (ndcL - ndcC) * 0.5;

    materialIdxFS = materialIdx;

}