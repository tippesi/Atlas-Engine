#ifndef VERTEXARRAY_H
#define VERTEXARRAY_H

#include "System.h"
#include "VertexBuffer.h"

#include <unordered_map>

class VertexArray {

public:
	
	VertexArray();

	///
	/// \param buffer
	void AddIndexComponent(VertexBuffer* buffer);

	///
	/// \param attribArray
	/// \param buffer
	/// \param normalized
	void AddComponent(uint32_t attribArray, VertexBuffer* buffer, bool normalized = false);

	///
	/// \param attribArray
	/// \param buffer
	/// \param normalized
	void AddInstancedComponent(uint32_t attribArray, VertexBuffer* buffer, bool normalized = false);

	///
	/// \return
	VertexBuffer* GetIndexComponent();

	///
	/// \param attribArray
	/// \return
	VertexBuffer* GetComponent(uint32_t attribArray);

	int32_t GetMaxAttributeArrayCount();

	///
	void Bind();

	///
	void Unbind();

	///
	void ClearContent();

	///
	void DeleteContent();

	~VertexArray();

private:
	uint32_t ID;

	VertexBuffer* indexComponent;

	unordered_map<uint32_t, VertexBuffer*> vertexComponents;

	int32_t maxAttribArrayCount;

	static uint32_t boundVertexArrayID;

};


#endif