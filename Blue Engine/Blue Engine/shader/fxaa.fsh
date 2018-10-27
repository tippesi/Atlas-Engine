in vec2 fTexCoord;

uniform sampler2D diffuseMap;
uniform vec2 framebufferResolution;

uniform float fxaaLumaThreshold;
uniform float fxaaLumaThresholdMin;
uniform bool debug;

out vec3 color;

void main() {
	
	const float fxaaSpanMax = 8.0f;
	const float fxaaReduceMul = 1.0f / 8.0f;
	const float fxaaReduceMin = 1.0f / 128.0f;

    vec3 rgbNW = textureOffset(diffuseMap, fTexCoord, ivec2(-1.0f, -1.0f)).xyz;
    vec3 rgbNE = textureOffset(diffuseMap, fTexCoord, ivec2(1.0f, -1.0f)).xyz;
    vec3 rgbSW = textureOffset(diffuseMap, fTexCoord, ivec2(-1.0f, 1.0f)).xyz;
    vec3 rgbSE = textureOffset(diffuseMap, fTexCoord, ivec2(1.0f, 1.0f)).xyz;
    vec3 rgbM = texture(diffuseMap, fTexCoord).xyz;
        
    const vec3 luma = vec3(0.299f, 0.587f, 0.114f);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
	float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);
        
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
	
	if (lumaMax - lumaMin < max(fxaaLumaThresholdMin, lumaMax * fxaaLumaThreshold)) {
		color = rgbM;
		return;
	}
        
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
        
    float dirReduce = max(
            (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25f * fxaaReduceMul),
            fxaaReduceMin);
          
    float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);
        
    dir = min(vec2( fxaaSpanMax,  fxaaSpanMax),
            max(vec2(-fxaaSpanMax, -fxaaSpanMax),
            dir * rcpDirMin)) / framebufferResolution;
                
    vec3 rgbA = (1.0f / 2.0f) * (
            texture(diffuseMap, fTexCoord.xy + dir * (1.0f / 3.0f - 0.5f)).xyz +
            texture(diffuseMap, fTexCoord.xy + dir * (2.0f / 3.0f - 0.5f)).xyz);
			
    vec3 rgbB = rgbA * (1.0f / 2.0f) + (1.0f / 4.0f) * (
            texture(diffuseMap, fTexCoord.xy + dir * 0.5f).xyz +
            texture(diffuseMap, fTexCoord.xy + dir * -0.5f).xyz);
        
	float lumaB = dot(rgbB, luma);

    if(lumaB < lumaMin || lumaB > lumaMax) {
        color.xyz = rgbA;
    }
	else{
        color.xyz = rgbB;
    }
	
	if (debug) {
		color = vec3(1.0f, 0.0f, 0.0f);
	}
	
}