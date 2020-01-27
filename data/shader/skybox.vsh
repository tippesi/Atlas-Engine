layout(location = 0)in vec3 vPosition;

out vec3 worldPositionVS;
out vec3 texCoordVS;
out vec3 ndcCurrentVS;
out vec3 ndcLastVS;

uniform mat4 mvpMatrix;
uniform mat4 ivMatrix;
uniform mat4 ipMatrix;
uniform vec3 cameraLocation;
uniform vec3 cameraLocationLast;

uniform mat4 pvMatrixLast;

void main() {
	
    vec4 pos = mvpMatrix * vec4(vPosition + cameraLocation, 1.0);
    gl_Position = pos.xyww;
	
	// We need position for fog calculation
	worldPositionVS = vec3(vPosition);
	
	// Velocity buffer
	ndcCurrentVS = vec3(gl_Position.xy, gl_Position.w);
	// For moving objects we need the last matrix
	vec4 last = pvMatrixLast * vec4(vPosition + cameraLocationLast, 1.0);
	ndcLastVS = vec3(last.xy, last.w);
	
    texCoordVS = vPosition;
	
}  