layout(location = 0)in vec3 vPosition;

out vec3 worldPositionVS;

uniform mat4 pvMatrix;

void main() {
	
    vec4 pos = pvMatrix * vec4(vPosition, 1.0);
    gl_Position = pos.xyww;
	
	worldPositionVS = vec3(vPosition);
	
}  