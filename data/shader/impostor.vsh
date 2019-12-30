#include <common/PI>

layout(location=0)in vec2 vPosition;
layout(location=1)in mat4 mMatrix;

out vec2 fTexCoord;
out vec3 ndcCurrent;
out vec3 ndcLast;
flat out int index;

uniform mat4 vMatrix;
uniform mat4 pMatrix;
uniform vec3 cameraLocation;

uniform vec3 right;
uniform vec3 up;

uniform vec3 minVec;
uniform vec3 maxVec;

uniform int views;

uniform mat4 pvMatrixLast;

void main() {

    fTexCoord = 0.5 * vPosition + 0.5;
	
	vec3 pos = mMatrix[3].xyz;
	vec3 dir = pos - cameraLocation;
	dir.y = 0.0;
	
	dir = normalize(dir);
	
	float dot0 = dot(dir, vec3(0.0, 0.0, 1.0));	
	float dot1 = dot(dir, vec3(1.0, 0.0, 0.0));
	
	float angle = 0.0;
	
	if (dot0 >= 0.0 && dot1 >= 0.0) {
		angle = acos(dot0);
	}
	else if (dot0 >= 0.0 && dot1 < 0.0) {
		angle = acos(-dot1) + 1.5 * PI;
	}
	else if (dot0 < 0.0 && dot1 >= 0.0) {
		angle = acos(dot1) + 0.5 * PI;
	}
	else {
		angle = acos(-dot0) + PI;
	}
	
	index = int(floor(angle / (2.0 * PI) * float(views)));
	
	vec2 scale = maxVec.xy - minVec.xy;

    vec2 position = vec2(0.5 * vPosition.x, fTexCoord.y);
    position = position * scale;

    vec4 modelPosition = vec4((up * position.y
        + right * position.x) + vec3(0.0, minVec.y, 0.0), 1.0);	

    gl_Position =  pMatrix * vMatrix * mMatrix * modelPosition;

    ndcCurrent = vec3(gl_Position.xy, gl_Position.w);
	// For moving objects we need the last matrix
    vec4 last = pvMatrixLast * mMatrix * modelPosition;
	ndcLast = vec3(last.xy, last.w);

}