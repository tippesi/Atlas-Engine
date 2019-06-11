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
uniform float heightScale;

// Nvidia frustum culling
// https://github.com/NVIDIAGameWorks/GraphicsSamples/blob/master/samples/es3aep-kepler/TerrainTessellation/assets/shaders/terrain_control.glsl

float GetTessLevel(float distance) {

	return clamp(tessellationFactor / pow(distance, tessellationSlope) + tessellationShift, 0.0, 1.0);

}

bool IsTileVisible(vec3 min, vec3 max) {
	for (int i = 0; i < 6; i++) {
		vec3 normal = frustumPlanes[i].xyz;
		float distance = frustumPlanes[i].w;
		
		vec3 s;
		s.x = normal.x >= 0.0 ? max.x : min.x;
		s.y = normal.y >= 0.0 ? max.y : min.y;
		s.z = normal.z >= 0.0 ? max.z : min.z;
				
		if (distance + dot(normal, s) < 0.0) {
			return false;
		}
	}
	return true;
}
		
void main() {

	if(gl_InvocationID == 0){
	
		vec3 minVec = min(gl_in[0].gl_Position.xyz,
			min(gl_in[1].gl_Position.xyz,
			min(gl_in[2].gl_Position.xyz,
			    gl_in[3].gl_Position.xyz)));
		vec3 maxVec = max(gl_in[0].gl_Position.xyz,
			max(gl_in[1].gl_Position.xyz,
			max(gl_in[2].gl_Position.xyz,
			    gl_in[3].gl_Position.xyz)));
				
		maxVec.y += 2.0;
		
		if (IsTileVisible(minVec, maxVec)) {
	
			vec3 midAB = vec3(gl_in[0].gl_Position + gl_in[1].gl_Position) / 2.0;
			vec3 midBC = vec3(gl_in[1].gl_Position + gl_in[2].gl_Position) / 2.0;
			vec3 midCD = vec3(gl_in[2].gl_Position + gl_in[3].gl_Position) / 2.0;
			vec3 midDA = vec3(gl_in[3].gl_Position + gl_in[0].gl_Position) / 2.0;
			
			float distanceAB = distance(cameraLocation, midAB);
            float distanceBC = distance(cameraLocation, midBC);
			float distanceCD = distance(cameraLocation, midCD);
			float distanceDA = distance(cameraLocation, midDA);
			
			gl_TessLevelOuter[AB] = mix(1.0, maxTessellationLevel, GetTessLevel(distanceAB));
			gl_TessLevelOuter[BC] = mix(1.0, maxTessellationLevel, GetTessLevel(distanceBC));
			gl_TessLevelOuter[CD] = mix(1.0, maxTessellationLevel, GetTessLevel(distanceCD));
			gl_TessLevelOuter[DA] = mix(1.0, maxTessellationLevel, GetTessLevel(distanceDA));
	
			gl_TessLevelInner[0] = (gl_TessLevelOuter[BC] + gl_TessLevelOuter[DA]) / 2.0;
			gl_TessLevelInner[1] = (gl_TessLevelOuter[AB] + gl_TessLevelOuter[CD]) / 2.0;
		
		}
		else {
			
			gl_TessLevelOuter[AB] = -1.0;
			gl_TessLevelOuter[BC] = -1.0;
			gl_TessLevelOuter[CD] = -1.0;
			gl_TessLevelOuter[DA] = -1.0;
	
			gl_TessLevelInner[0] = -1.0;
			gl_TessLevelInner[1] = -1.0;
			
		}
	}
	
	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
	
}
