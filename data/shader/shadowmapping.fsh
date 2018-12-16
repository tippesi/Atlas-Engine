#ifdef ALPHA
#ifdef ARRAY_MAP
uniform sampler2DArray arrayMap;
uniform float diffuseMapIndex;
#else
uniform sampler2D diffuseMap;
#endif

in vec2 fTexCoord;
#endif

void main() {
	
#ifdef ALPHA
#ifdef ARRAY_MAP
	float alpha = texture(arrayMap, vec3(fTexCoord, diffuseMapIndex)).a;
#else
	float alpha = texture(diffuseMap, fTexCoord).a;
#endif
	
	if(alpha < 0.2f)
		discard;
#endif
	
}