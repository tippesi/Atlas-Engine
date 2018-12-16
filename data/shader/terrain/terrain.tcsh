layout(vertices = 16) out;

const int AB = 2;
const int BC = 3;
const int CD = 0;
const int DA = 1;

uniform float tessellationFactor;
uniform float tessellationSlope;
uniform float tessellationShift;
uniform float maxTessellationLevel;

uniform vec3 cameraLocation;
uniform vec4 frustumPlanes[6];
uniform float tileScale;

// Nvidia frustum culling
// https://github.com/NVIDIAGameWorks/GraphicsSamples/blob/master/samples/es3aep-kepler/TerrainTessellation/assets/shaders/terrain_control.glsl

float GetTessLevel(float distance) {

	return clamp(tessellationFactor / pow(distance, tessellationSlope) + tessellationShift, 0.0f, 1.0f);

}

bool IsSphereVisible(vec3 center, float radius) {
	return true;
	for (int i = 0; i < 6; i++) {
		if (dot(vec4(center, 1.0f), frustumPlanes[i]) + radius < 0.0f) {
			return false;
		}
	}
	return true;
}
		
void main() {

	if(gl_InvocationID == 0){
	
		vec3 spherePosition = gl_in[0].gl_Position.xyz + vec3(tileScale, 0.0f, tileScale) * 0.5f;
		
		if (IsSphereVisible(spherePosition, 10.0f)) {
	
			vec3 midAB = vec3(gl_in[0].gl_Position + gl_in[1].gl_Position) / 2.0f;
			vec3 midBC = vec3(gl_in[1].gl_Position + gl_in[2].gl_Position) / 2.0f;
			vec3 midCD = vec3(gl_in[2].gl_Position + gl_in[3].gl_Position) / 2.0f;
			vec3 midDA = vec3(gl_in[3].gl_Position + gl_in[0].gl_Position) / 2.0f;
			
			float distanceAB = distance(cameraLocation, midAB);
            float distanceBC = distance(cameraLocation, midBC);
			float distanceCD = distance(cameraLocation, midCD);
			float distanceDA = distance(cameraLocation, midDA);
			
			gl_TessLevelOuter[AB] = mix(1.0f, maxTessellationLevel, GetTessLevel(distanceAB));
			gl_TessLevelOuter[BC] = mix(1.0f, maxTessellationLevel, GetTessLevel(distanceBC));
			gl_TessLevelOuter[CD] = mix(1.0f, maxTessellationLevel, GetTessLevel(distanceCD));
			gl_TessLevelOuter[DA] = mix(1.0f, maxTessellationLevel, GetTessLevel(distanceDA));
	
			gl_TessLevelInner[0] = (gl_TessLevelOuter[BC] + gl_TessLevelOuter[DA]) / 2.0f;
			gl_TessLevelInner[1] = (gl_TessLevelOuter[AB] + gl_TessLevelOuter[CD]) / 2.0f;
		
		}
		else {
			
			gl_TessLevelOuter[AB] = -1.0f;
			gl_TessLevelOuter[BC] = -1.0f;
			gl_TessLevelOuter[CD] = -1.0f;
			gl_TessLevelOuter[DA] = -1.0f;
	
			gl_TessLevelInner[0] = -1.0f;
			gl_TessLevelInner[1] = -1.0f;
			
		}
	}
	
	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
	
}
