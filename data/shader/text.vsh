layout(location=0)in vec4 vPosition;

out vec2 fTexCoord;

uniform mat4 pMatrix;

void main()
{
    gl_Position = pMatrix* vec4(vPosition.xy, 0.0, 1.0);
    fTexCoord = vPosition.zw;
}