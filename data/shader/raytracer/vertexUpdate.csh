#include <structures>

layout (local_size_x = 1, local_size_y = 1) in;

layout (std430, binding = 1) buffer Triangles {
	Triangle data[];
} triangles;

uniform mat4 mMatrix;
uniform int triangleOffset;
uniform int triangleCount;
uniform int xInvocations;

void main() {

	int index = int(gl_GlobalInvocationID.x)
		+ xInvocations * int(gl_GlobalInvocationID.y);
	
	/*
    // We might have more threads than triangles 
	// per compute dispatch
	if (index < triangleCount) {
	
		index += triangleOffset;
	
		Triangle triangle = triangles.data[index];
	
		triangle.v0 = mMatrix * triangle.v0;
		triangle.v1 = mMatrix * triangle.v1;
		triangle.v2 = mMatrix * triangle.v2;
		
		triangle.n0 = normalize(mMatrix * triangle.n0);
		triangle.n1 = normalize(mMatrix * triangle.n1);
		triangle.n2 = normalize(mMatrix * triangle.n2);
		
		triangles.data[index] = triangle;

	}
	*/
	
}