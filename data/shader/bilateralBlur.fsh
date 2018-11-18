in vec2 fTexCoord;

#ifdef TEXTURE
uniform sampler2D diffuseTexture;
#endif
#ifdef TEXTUREARRAY
uniform sampler2DArray diffuseTexture;
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

vec3 sampleRGB(vec2 texCoord);
vec2 sampleRG(vec2 texCoord);
float sampleR(vec2 texCoord);

void main() {

#ifdef BLUR_RGB
    color = sampleRGB(fTexCoord) * weight[0];
    for(int i = 1; i < kernelSize; i++)
        color += sampleRGB(fTexCoord + (vec2(offset[i]) * blurDirection)) * weight[i];

    for (int i = 1; i < kernelSize; i++)
        color += sampleRGB(fTexCoord - (vec2(offset[i]) * blurDirection)) * weight[i];
#endif
#ifdef BLUR_RG
    color = sampleRG(fTexCoord) * weight[0];
    for(int i = 1; i < kernelSize; i++)
        color += sampleRG(fTexCoord + (vec2(offset[i]) * blurDirection)) * weight[i];

    for (int i = 1; i < kernelSize; i++)
        color += sampleRG(fTexCoord - (vec2(offset[i]) * blurDirection)) * weight[i];
#endif
#ifdef BLUR_R
    float centerColor = sampleR(fTexCoord);
    color = centerColor * weight[0];
    for(int i = 1; i < kernelSize; i++) {
        float texColor = sampleR(fTexCoord + (vec2(offset[i]) * blurDirection));
        float closeness = pow(1.0f - length(texColor - centerColor) / length(vec3(1,1,1)), 2.0f);
        color += texColor * closeness * weight[i];
    }

    for(int i = 1; i < kernelSize; i++) {
        float texColor = sampleR(fTexCoord - (vec2(offset[i]) * blurDirection));
        float closeness = pow(1.0f - length(texColor - centerColor) / length(vec3(1,1,1)), 2.0f);
        color += texColor * closeness * weight[i];
    }
#endif
}

vec3 sampleRGB(vec2 texCoord) {
#ifdef TEXTURE
    return texture(diffuseTexture, texCoord).rgb;
#endif
#ifdef TEXTUREARRAY
    return texture(diffuseTexture, vec3(texCoord, 0)).rgb;
#endif
}

vec2 sampleRG(vec2 texCoord) {
#ifdef TEXTURE
    return texture(diffuseTexture, texCoord).rg;
#endif
#ifdef TEXTUREARRAY
    return texture(diffuseTexture, vec3(texCoord, 0)).rg;
#endif
}

float sampleR(vec2 texCoord){
#ifdef TEXTURE
    return texture(diffuseTexture, texCoord).r;
#endif
#ifdef TEXTUREARRAY
    return texture(diffuseTexture, vec3(texCoord, 0)).r;
#endif
}