#ifdef ALPHA
uniform sampler2D diffuseMap;

in vec2 fTexCoord;
#endif

void main() {
	
#ifdef ALPHA
	float alpha = texture(diffuseMap, fTexCoord).a;
	
	if(alpha < 0.2f)
		discard;
#endif
	
}