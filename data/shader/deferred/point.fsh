#include <../structures.hsh>
#include <../common/convert.hsh>
#include <../common/material.hsh>

in vec3 fTexCoordProj;
in vec3 viewSpacePosition;
out vec4 fragColor;

layout(binding = 0) uniform sampler2D diffuseTexture;
layout(binding = 1) uniform sampler2D normalTexture;
layout(binding = 2) uniform sampler2D geometryNormalTexture;
layout(binding = 3) uniform sampler2D specularTexture;
layout(binding = 4) uniform usampler2D materialIdxTexture;
layout(binding = 5) uniform sampler2D depthTexture;
layout(binding = 6) uniform samplerCubeShadow shadowCubemap;

uniform Light light;
uniform vec3 viewSpaceLightLocation;
uniform mat4 lvMatrix;
uniform mat4 lpMatrix;

uniform bool shadowEnabled;

// Improved filtering: https://kosmonautblog.wordpress.com/2017/03/25/shadow-filtering-for-pointlights/

vec3 sampleOffsetDirections[20] = vec3[]
(
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);

void main() {

    vec2 texCoord = ((fTexCoordProj.xy / fTexCoordProj.z) + 1.0) / 2.0;
    
    float depth = texture(depthTexture, texCoord).r;
    
    vec3 fragPos = ConvertDepthToViewSpace(depth, texCoord);

    if (fragPos.z - viewSpaceLightLocation.z > light.radius)
        discard;

    vec3 fragToLight = viewSpaceLightLocation.xyz - fragPos.xyz;
    float fragToLightDistance = length(fragToLight);

    uint materialIdx = texture(materialIdxTexture, texCoord).r;
    Material material = UnpackMaterial(materialIdx);
    
    vec3 normal = normalize(2.0 * texture(normalTexture, texCoord).rgb - 1.0);
    vec3 reconstructedNormal = normalize(2.0 * texture(geometryNormalTexture, texCoord).rgb - 1.0);

    normal = material.normalMap ? normal : reconstructedNormal;

    vec3 surfaceColor = texture(diffuseTexture, texCoord).rgb;
    
    // Specular properties
    float specularIntensity = 0.0;
    float specularHardness = 1.0;

    if (material.specularMap) {
        vec2 specularProp = texture(specularTexture, texCoord).rg;
        specularIntensity = max(specularProp.r, 0.0);
        specularHardness = max(specularProp.g, 1.0);
    }
    else {
        specularIntensity = material.specularIntensity;
        specularHardness = material.specularHardness;
    }
    
    float shadowFactor = 0.0;
    
    vec3 specular = vec3(0.0);
    vec3 diffuse = vec3(1.0);
    vec3 ambient = vec3(light.ambient * surfaceColor);
    vec3 volumetric = 0.0 * vec3(light.color);

    float occlusionFactor = 1.0;
    
    vec3 lightDir = fragToLight / fragToLightDistance;
    
    vec4 lsPosition = lvMatrix * vec4(fragPos, 1.0);
    vec4 absPosition = abs(lsPosition);
    depth = -max(absPosition.x, max(absPosition.y, absPosition.z));
    vec4 clip = lpMatrix * vec4(0.0, 0.0, depth, 1.0);    
    depth = (clip.z - 0.005) / clip.w * 0.5 + 0.5;
    
    int samples  = 20;
    float diskRadius = 0.0075;

    if (shadowEnabled) {
        for(int i = 0; i < samples; i++) {
            shadowFactor += clamp(texture(shadowCubemap, vec4(surface.P + sampleOffsetDirections[i] * diskRadius, depth)), 0.0, 1.0); 
        }
        shadowFactor /= float(samples + 1);  
    }

    diffuse = max(dot(normal, lightDir), 0.0) * light.color * shadowFactor * surfaceColor;

    fragColor = vec4(max((diffuse + ambient) * (light.radius - fragToLightDistance) / light.radius, 0.0) + volumetric, 1.0);

}




float ComputeScattering(vec3 fragPos) {

    // We also need to consider that we shouldn't use the radius of the sphere when we're inside
    // the volume but should use the distance of the viewSpacePosition to the camera.
    const int sampleCount = 100;
    const float offset = 0.5f;    // Because of geometry getting cut off by near plane (offset = 2 * nearPlane)
    
    float positionLength = length(viewSpacePosition);
    float fragLength = length(fragPos);
    
    vec3 rayDirection = viewSpacePosition / positionLength;
    
    vec3 rayEnd = vec3(viewSpacePosition.xyz) - rayDirection * offset / 2.0f;
    
    vec3 rayStart = positionLength < light.radius - offset / 2.0f ? vec3(0.0f) : 
        vec3(viewSpacePosition.xyz) - rayDirection * (light.radius - offset / 2.0f);
        
    float rayLength = distance(rayStart, rayEnd);

    float stepLength = rayLength / float(sampleCount);
    vec3 stepVector = rayDirection  * stepLength;

    float foginess = 0.0f;
    float scattering = 0.15f;

    vec3 currentPosition = vec3(rayStart);

    for (int i = 0; i < sampleCount; i++) {

        // Normally we should take a shadow map into consideration
        float fragToLightDistance = length(viewSpaceLightLocation - currentPosition);
        float shadowValue = fragToLightDistance < light.radius ? 1.0f : 0.0f;
        shadowValue = fragLength < length(currentPosition) ? 0.0f : shadowValue;

        foginess += max(scattering * shadowValue * (light.radius - fragToLightDistance - 0.25f) / (light.radius- 0.25f), 0.0f);

        currentPosition += stepVector;

    }

    return foginess / float(sampleCount);

}