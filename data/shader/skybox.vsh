layout(location=0)in vec3 vPosition;

uniform mat4 mvpMatrix;
out vec3 fTexCoord;

void main()
{
	
    vec4 pos = mvpMatrix * vec4(vPosition, 1.0);
    gl_Position = pos.xyww;
	
    fTexCoord = vPosition;
	
}  