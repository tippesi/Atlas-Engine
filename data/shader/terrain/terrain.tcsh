#include <../globals.hsh>

layout(vertices = 16) out;

//layout(location=0) flat in uvec4 materialIndicesVS[];
//layout(location=0) flat out uvec4 materialIndicesTC[];

const int AB = 2;
const int BC = 3;
const int CD = 0;
const int DA = 1;

layout (set = 3, binding = 9, std140) uniform UniformBuffer {
    vec4 frustumPlanes[6];

    float heightScale;
    float displacementDistance;

    float tessellationFactor;
    float tessellationSlope;
    float tessellationShift;
    float maxTessellationLevel;
} Uniforms;

layout(push_constant) uniform constants {
    float nodeSideLength;
    float tileScale;
    float patchSize;
    float normalTexelSize;

    float leftLoD;
    float topLoD;
    float rightLoD;
    float bottomLoD;

    vec2 nodeLocation;
} PushConstants;

// Nvidia frustum culling
// https://github.com/NVIDIAGameWorks/GraphicsSamples/blob/master/samples/es3aep-kepler/TerrainTessellation/assets/shaders/terrain_control.glsl

float GetTessLevel(float distance) {

	return clamp(Uniforms.tessellationFactor / pow(distance,
        Uniforms.tessellationSlope) + Uniforms.tessellationShift, 0.0, 1.0);

}

bool IsTileVisible(vec3 min, vec3 max) {
	for (int i = 0; i < 6; i++) {
		vec3 normal = Uniforms.frustumPlanes[i].xyz;
		float distance = Uniforms.frustumPlanes[i].w;
		
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

	if(gl_InvocationID == 0) {
	
		//materialIndicesTC[gl_InvocationID] = materialIndicesVS[0];
	
		vec3 minVec = min(gl_in[0].gl_Position.xyz,
			min(gl_in[1].gl_Position.xyz,
			min(gl_in[2].gl_Position.xyz,
			    gl_in[3].gl_Position.xyz)));
		vec3 maxVec = max(gl_in[0].gl_Position.xyz,
			max(gl_in[1].gl_Position.xyz,
			max(gl_in[2].gl_Position.xyz,
			    gl_in[3].gl_Position.xyz)));
				
		vec3 center = 0.5 * (minVec + maxVec);
		vec3 dir = maxVec - center;
		
		// Displacement is in normal direction
		// To fix culling we simply scale the bounding
		// box of the current tile by a small margin
		maxVec += dir * .25;
		minVec -= dir * .25;

		if (true) {
	
#ifndef DISTANCE
			vec3 midAB = vec3(gl_in[0].gl_Position + gl_in[1].gl_Position) / 2.0;
			vec3 midBC = vec3(gl_in[1].gl_Position + gl_in[2].gl_Position) / 2.0;
			vec3 midCD = vec3(gl_in[2].gl_Position + gl_in[3].gl_Position) / 2.0;
			vec3 midDA = vec3(gl_in[3].gl_Position + gl_in[0].gl_Position) / 2.0;
			
			float distanceAB = distance(globalData.cameraLocation.xyz, midAB);
            float distanceBC = distance(globalData.cameraLocation.xyz, midBC);
			float distanceCD = distance(globalData.cameraLocation.xyz, midCD);
			float distanceDA = distance(globalData.cameraLocation.xyz, midDA);
			
			gl_TessLevelOuter[AB] = mix(1.0, Uniforms.maxTessellationLevel, GetTessLevel(distanceAB));
			gl_TessLevelOuter[BC] = mix(1.0, Uniforms.maxTessellationLevel, GetTessLevel(distanceBC));
			gl_TessLevelOuter[CD] = mix(1.0, Uniforms.maxTessellationLevel, GetTessLevel(distanceCD));
			gl_TessLevelOuter[DA] = mix(1.0, Uniforms.maxTessellationLevel, GetTessLevel(distanceDA));
	
			gl_TessLevelInner[0] = (gl_TessLevelOuter[BC] + gl_TessLevelOuter[DA]) / 2.0;
			gl_TessLevelInner[1] = (gl_TessLevelOuter[AB] + gl_TessLevelOuter[CD]) / 2.0;
#else
			gl_TessLevelOuter[AB] = 1.0;
			gl_TessLevelOuter[BC] = 1.0;
			gl_TessLevelOuter[CD] = 1.0;
			gl_TessLevelOuter[DA] = 1.0;
		
			gl_TessLevelInner[0] = 1.0;
			gl_TessLevelInner[1] = 1.0;
#endif
			
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
