layout(location=0)in vec2 vPosition;
layout(location=1)in mat4 mMatrix;

out vec2 fTexCoord;
out vec3 ndcCurrent;
out vec3 ndcLast;

uniform mat4 vMatrix;
uniform mat4 pMatrix;

uniform vec3 right;
uniform vec3 up;

uniform vec3 minVec;
uniform vec3 maxVec;

uniform mat4 pvMatrixLast;

void main() {

    fTexCoord = 0.5 * vPosition + 0.5;
	
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