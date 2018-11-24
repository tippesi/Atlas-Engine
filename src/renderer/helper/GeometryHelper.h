#ifndef GEOMETRYHELPER_H
#define GEOMETRYHELPER_H

#include "../../System.h"
#include "../../VertexArray.h"

class GeometryHelper {

public:
	static VertexArray* GenerateRectangleVertexArray();
	
	static VertexArray* GenerateCubeVertexArray();

	static VertexArray* GenerateSphereVertexArray();

private:
	static int8_t rectangleVertices[];

	static float cubeVertices[];

	static float sphereVertices[];

};

#endif