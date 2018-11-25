layout(location=0)in vec3 vPosition;

uniform mat4 vpMatrix;
out float colorGradient;

void main()
{
	
    vec4 pos = vpMatrix * vec4(vPosition * 5.0f, 1.0);
    gl_Position = pos.xyww;
	
    colorGradient = vPosition.y * 0.5f + 0.5f;
	
}