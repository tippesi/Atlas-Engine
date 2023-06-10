#ifdef OPACITY_MAP
layout(location = 0) in vec2 texCoordVS;
layout(set = 3, binding = 0) uniform sampler2D opacityMap;
#endif

void main() {
    
#ifdef OPACITY_MAP
    float opacity = texture(opacityMap, texCoordVS).r;    
    if(opacity < 0.2)
        discard;
#endif
    
}