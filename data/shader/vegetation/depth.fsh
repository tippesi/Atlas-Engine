#ifdef OPACITY_MAP
layout(binding = 1) uniform sampler2D opacityMap;
#endif

in vec3 positionVS;
in vec2 texCoordVS;

void main() {

#ifdef OPACITY_MAP
	float opacity = texture(opacityMap, texCoordVS).r;
	if (opacity < 0.2)
		discard;
#endif
	
}