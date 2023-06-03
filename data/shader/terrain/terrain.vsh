layout (location = 0) in vec2 vPosition;

layout(set = 3, binding = 0) uniform usampler2D heightField;
layout(set = 3, binding = 2) uniform usampler2D splatMap;

// layout(location=0) flat out uvec4 materialIndicesVS;

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

vec2 stitch(vec2 position) {
	
	// We have 8x8 patches per node
	float nodeSize = 8.0 * PushConstants.patchSize;
	
	if (position.x == 0.0 && PushConstants.leftLoD > 1.0) {
		position.y = floor(position.y / PushConstants.leftLoD)
			* PushConstants.leftLoD;
	}
	else if (position.y == 0.0 && PushConstants.topLoD > 1.0) {
		position.x = floor(position.x / PushConstants.topLoD)
			* PushConstants.topLoD;
	}
	else if (position.x == nodeSize && PushConstants.rightLoD > 1.0) {
		position.y = floor(position.y / PushConstants.rightLoD)
			* PushConstants.rightLoD;
	}
	else if (position.y == nodeSize && PushConstants.bottomLoD > 1.0) {
		position.x = floor(position.x / PushConstants.bottomLoD)
			* PushConstants.bottomLoD;
	}
	
	return position;
	
}

void main() {

	vec2 patchOffset = vec2(gl_InstanceIndex / 8, gl_InstanceIndex % 8);
	
	vec2 localPosition = patchOffset * PushConstants.patchSize + vPosition;
	
	localPosition = stitch(localPosition) * PushConstants.tileScale;
	
	vec2 position = PushConstants.nodeLocation + localPosition;
	
	vec2 texCoords = localPosition;
	texCoords /= PushConstants.nodeSideLength;
	
	float texel = 1.0 / (8.0 * PushConstants.patchSize);
	
	//materialIndicesVS.x = texture(splatMap, texCoords).r;
	//materialIndicesVS.y = texture(splatMap, texCoords + vec2(texel, 0.0)).r;
	//materialIndicesVS.z = texture(splatMap, texCoords + vec2(0.0, texel)).r;
	//materialIndicesVS.w = texture(splatMap, texCoords + vec2(texel, texel)).r;
	
	// The middle of the texel should match the vertex position
	float height = float(texture(heightField, texCoords).r) / 65535.0 * Uniforms.heightScale;
					
	gl_Position =  vec4(position.x, height, position.y, 1.0);
	
}