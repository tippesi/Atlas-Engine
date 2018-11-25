out vec3 fragColor;

in float colorGradient;

void main() {
	
	fragColor = mix(vec3(0.0f, 0.0f, 1.0f), vec3(1.0f, 0.0f, 0.0f), colorGradient);
	
}