in vec2 fTexCoord;

layout(binding = 0) uniform sampler2D diffuseTexture;

#ifdef BILATERAL
layout(binding = 1) uniform sampler2D depthTexture;
#endif

uniform vec2 blurDirection;

uniform float offset[80];
uniform float weight[80];

uniform int kernelSize;

#ifdef BLUR_RGB
out vec3 color;
#endif
#ifdef BLUR_RG
out vec2 color;
#endif
#ifdef BLUR_R
out float color;
#endif

void main() {

#ifdef BLUR_RGB
    color = texture(diffuseTexture, fTexCoord).rgb * weight[0];
    for(int i = 1; i < kernelSize; i++)
        color += texture(diffuseTexture, fTexCoord + (vec2(offset[i]) * blurDirection)).rgb * weight[i];

    for (int i = 1; i < kernelSize; i++)
        color += texture(diffuseTexture, fTexCoord - (vec2(offset[i]) * blurDirection)).rgb * weight[i];
#endif
#ifdef BLUR_RG
    color = texture(diffuseTexture, fTexCoord).rg * weight[0];
    for(int i = 1; i < kernelSize; i++)
        color += texture(diffuseTexture, fTexCoord + (vec2(offset[i]) * blurDirection)).rg * weight[i];

    for (int i = 1; i < kernelSize; i++)
        color += texture(diffuseTexture, fTexCoord - (vec2(offset[i]) * blurDirection)).rg * weight[i];
#endif
#ifdef BLUR_R
    float centerColor = texture(diffuseTexture, fTexCoord).r;
#ifdef BILATERAL
	float centerDepth = texture(depthTexture, fTexCoord).r;
#endif
	color = centerColor * weight[0];
	float closeness = 1.0f;
	float totalWeight = weight[0];
    for(int i = 1; i < kernelSize; i++) {
        float texColor = texture(diffuseTexture, fTexCoord + (vec2(offset[i]) * blurDirection)).r;
		float currentWeight = weight[i];
#ifdef BILATERAL
		float depth = texture(depthTexture, fTexCoord + (vec2(offset[i]) * blurDirection)).r;
        closeness = max(0.0f, 1.0f - 1000.0f * abs(centerDepth - depth));
		currentWeight *= closeness;
		totalWeight += currentWeight;
#endif
		color += texColor * currentWeight;
    }

    for(int i = 1; i < kernelSize; i++) {
        float texColor = texture(diffuseTexture, fTexCoord - (vec2(offset[i]) * blurDirection)).r;
		float currentWeight = weight[i];
#ifdef BILATERAL
		float depth = texture(depthTexture, fTexCoord - (vec2(offset[i]) * blurDirection)).r;
        closeness = max(0.0f, 1.0f - 1000.0f * abs(centerDepth - depth));
		currentWeight *= closeness;
		totalWeight += currentWeight;
#endif
		color += texColor * currentWeight;
    }
#ifdef BILATERAL
	color /= totalWeight;
#endif
#endif

}
