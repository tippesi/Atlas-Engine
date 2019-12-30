layout(location=0)in vec3 vPosition;

out vec3 fTexCoord;
out vec3 ndcCurrent;
out vec3 ndcLast;

uniform mat4 mvpMatrix;
uniform vec3 cameraLocation;
uniform vec3 cameraLocationLast;

uniform mat4 pvMatrixLast;

void main() {
	
    vec4 pos = mvpMatrix * vec4(vPosition + cameraLocation, 1.0);
    gl_Position = pos.xyww;
	
	// Velocity buffer
	ndcCurrent = vec3(gl_Position.xy, gl_Position.w);
	// For moving objects we need the last matrix
	vec4 last = pvMatrixLast * vec4(vPosition + cameraLocationLast, 1.0);
	ndcLast = vec3(last.xy, last.w);
	
    fTexCoord = vPosition;
	
}  