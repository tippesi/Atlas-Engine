#ifdef OPACITY_MAP
layout(binding = 0) uniform sampler2D opacityMap;

in vec2 texCoordVS;
#endif

void main() {
	
#ifdef OPACITY_MAP
	float opacity = texture(opacityMap, texCoordVS).r;	
	if(opacity < 0.2)
		discard;
#endif
	
}