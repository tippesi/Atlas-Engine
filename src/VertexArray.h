#ifndef VERTEXARRAY_H
#define VERTEXARRAY_H

#include "System.h"
#include "VertexBuffer.h"

#include <unordered_map>

class VertexArray {

public:
	VertexArray();

	void AddIndexComponent(VertexBuffer* buffer);
	
	void AddComponent(uint32_t attribArray, VertexBuffer* buffer, bool normalized = false);

	void AddInstancedComponent(uint32_t attribArray, VertexBuffer* buffer, bool normalized = false);

	VertexBuffer* GetIndexComponent();

	VertexBuffer* GetComponent(uint32_t attribArray);

	void Bind();

	void Unbind();

	void ClearContent();

	void DeleteContent();

	~VertexArray();

private:
	uint32_t ID;

	VertexBuffer* indexComponent;

	unordered_map<uint32_t, VertexBuffer*> vertexComponents;

	static uint32_t boundVertexArrayID;

};


#endif