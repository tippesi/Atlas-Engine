#ifndef GEOMETRYHELPER_H
#define GEOMETRYHELPER_H

#include "../../System.h"
#include "buffer/VertexArray.h"

class GeometryHelper {

public:
	static VertexArray* GenerateRectangleVertexArray();
	
	static VertexArray* GenerateCubeVertexArray();

	static VertexArray* GenerateSphereVertexArray(uint32_t rings, uint32_t segments);

private:
	static void GenerateSphere(uint32_t rings, uint32_t segments, uint32_t*& indices, vec3*& vertices,
		uint32_t* indexCount, uint32_t* vertexCount);

	static int8_t rectangleVertices[];

	static float cubeVertices[];

};

#endif