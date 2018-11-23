#ifndef VERTEXARRAY_H
#define VERTEXARRAY_H

#include "System.h"
#include "VertexBuffer.h"

#include <unordered_map>

class VertexArray {

public:
	VertexArray();

	void AddIndexComponent(VertexBuffer* buffer);
	
	void AddComponent(int32_t attribArray, VertexBuffer* buffer);

	void AddInstancedComponent(int32_t attribArray, VertexBuffer* buffer);

	VertexBuffer* GetIndexComponent();

	VertexBuffer* GetComponent(uint32_t attribArray);

	void Bind();

	void Unbind();

private:
	uint32_t ID;

	VertexBuffer* indexComponent;

	unordered_map<int32_t, VertexBuffer*> vertexComponents;

	static uint32_t boundVertexArrayID;

};


#endif