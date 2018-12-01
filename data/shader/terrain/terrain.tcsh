layout(vertices = 16) out;

const int AB = 2;
const int BC = 3;
const int CD = 0;
const int DA = 1;

uniform float tessellationFactor;
uniform float tessellationSlope;
uniform float tessellationShift;
uniform int maxTessellationLevel;

uniform vec3 cameraLocation;

float GetTessLevel(float distance) {

	return clamp(tessellationFactor / pow(distance, tessellationSlope) + tessellationShift, 0.0f, 1.0f);

}
		
void main() {

	if(gl_InvocationID == 0)
	{
            
            vec3 midAB = vec3(gl_in[0].gl_Position + gl_in[3].gl_Position) / 2.0f;
			vec3 midBC = vec3(gl_in[3].gl_Position + gl_in[15].gl_Position) / 2.0f;
			vec3 midCD = vec3(gl_in[15].gl_Position + gl_in[12].gl_Position) / 2.0f;
			vec3 midDA = vec3(gl_in[12].gl_Position + gl_in[0].gl_Position) / 2.0f;	
			
			float distanceAB = distance(cameraLocation, midAB);
            float distanceBC = distance(cameraLocation, midBC);
			float distanceCD = distance(cameraLocation, midCD);
			float distanceDA = distance(cameraLocation, midDA);
			
			gl_TessLevelOuter[AB] = mix(1, maxTessellationLevel, GetTessLevel(distanceAB));
			gl_TessLevelOuter[BC] = mix(1, maxTessellationLevel, GetTessLevel(distanceBC));
			gl_TessLevelOuter[CD] = mix(1, maxTessellationLevel, GetTessLevel(distanceCD));
			gl_TessLevelOuter[DA] = mix(1, maxTessellationLevel, GetTessLevel(distanceDA));
	
			gl_TessLevelInner[0] = (gl_TessLevelOuter[BC] + gl_TessLevelOuter[DA])/4;
			gl_TessLevelInner[1] = (gl_TessLevelOuter[AB] + gl_TessLevelOuter[CD])/4;
		
	}
	
	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
	
}
