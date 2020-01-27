layout(location=0)in vec3 vPosition;
layout(location=1)in vec3 vColor;

out vec3 colorVS;

uniform mat4 vMatrix;
uniform mat4 pMatrix;

void main() {

    gl_Position = pMatrix * vMatrix * vec4(vPosition, 1.0);

    colorVS = vColor;

}