#ifndef AE_VERTEXARRAY_H
#define AE_VERTEXARRAY_H

#include "../System.h"
#include "IndexBuffer.h"
#include "VertexBuffer.h"

#include <unordered_map>

class VertexArray {

public:

	/**
	 *
	 */
	VertexArray();

	/**
	 *
	 * @param buffer
	 */
	void AddIndexComponent(IndexBuffer* buffer);

	/**
	 *
	 * @param attribArray
	 * @param buffer
	 * @param normalized
	 */
	void AddComponent(uint32_t attribArray, VertexBuffer* buffer, bool normalized = false);

	/**
	 *
	 * @param attribArray
	 * @param buffer
	 * @param normalized
	 */
	void AddInstancedComponent(uint32_t attribArray, VertexBuffer* buffer, bool normalized = false);

	/**
	 *
	 * @return
	 */
	IndexBuffer* GetIndexComponent();

	/**
	 *
	 * @param attribArray
	 * @return
	 */
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

	IndexBuffer* indexComponent;

	std::unordered_map<uint32_t, VertexBuffer*> vertexComponents;

	uint32_t maxAttribArrayCount;

	static uint32_t boundVertexArrayID;

};


#endif