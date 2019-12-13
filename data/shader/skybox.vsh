layout(location=0)in vec3 vPosition;

out vec3 fTexCoord;
out vec3 ndcCurrent;
out vec3 ndcLast;

uniform mat4 mvpMatrix;
uniform mat4 pvMatrixLast;
uniform mat4 pvMatrixCurrent;

void main()
{
	
    vec4 pos = mvpMatrix * vec4(vPosition, 1.0);
    gl_Position = pos.xyww;
	
	// Velocity buffer (calculate with unjittered matrices)
	vec4 current = pvMatrixCurrent * vec4(vPosition, 1.0);
	ndcCurrent = vec3(current.xy, current.w);
	// For moving objects we need the last matrix
	vec4 last = pvMatrixLast * vec4(vPosition, 1.0);
	ndcLast = vec3(last.xy, last.w);
	
    fTexCoord = vPosition;
	
}  