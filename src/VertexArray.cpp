#include "VertexArray.h"

uint32_t VertexArray::boundVertexArrayID = 0;

VertexArray::VertexArray() {

	glGenVertexArrays(1, &ID);

}