layout(location=0)in vec3 vPosition;//Position eines Vertex

uniform mat4 pMatrix;
uniform mat4 vMatrix;
uniform mat4 mMatrix;

out vec3 fTexCoord;

void main()
{
    vec4 pos = pMatrix * vMatrix * mMatrix * vec4(vPosition, 1.0);
    gl_Position = pos.xyww;
	
    fTexCoord = vPosition;
	
}  