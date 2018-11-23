#ifndef VERTEXBUFFER_H
#define VERTEXBUFFER_H

#include "System.h"

class VertexBuffer {

public:
	VertexBuffer(uint32_t type,  int32_t dataType, int32_t stride, uint32_t usage = GL_STATIC_DRAW);

	void SetData(uint8_t* data, int32_t length);

	void SetData(uint16_t* data, int32_t length);

	void SetData(uint32_t* data, int32_t length);

	void SetData(int8_t* data, int32_t length);

	void SetData(int16_t* data, int32_t length);

	void SetData(int32_t* data, int32_t length);

	void SetData(float* data, int32_t length);

	void SetData(vec2* data, int32_t length);

	void SetData(vec3* data, int32_t length);

	void SetData(vec4* data, int32_t length);

	void SetData(void* data, int32_t length, int32_t elementSize);

	void SetSubData(uint8_t* data, int32_t offset, int32_t length);

	void SetSubData(uint16_t* data, int32_t offset, int32_t length);

	void SetSubData(uint32_t* data, int32_t offset, int32_t length);

	void SetSubData(int8_t* data, int32_t offset, int32_t length);

	void SetSubData(int16_t* data, int32_t offset, int32_t length);

	void SetSubData(int32_t* data, int32_t offset, int32_t length);

	void SetSubData(float* data, int32_t offset, int32_t length);

	void SetSubData(vec2* data, int32_t offset, int32_t length);

	void SetSubData(vec3* data, int32_t offset, int32_t length);

	void SetSubData(vec4* data, int32_t offset, int32_t length);

	void SetSubData(void* data, int32_t offset, int32_t length, int32_t elementSize);

	void Bind();

	void Unbind();

	uint32_t GetType();

	int32_t GetDataType();

	int32_t GetStride();

	~VertexBuffer();

private:
	void SetDataInternal(void* data, int32_t length, int32_t elementSize);

	void SetSubDataInternal(void* data, int32_t offset, int32_t length, int32_t elementSize);

	uint32_t ID;

	uint32_t type;
	uint32_t usage;

	int32_t dataType;
	int32_t stride;

	int32_t sizeInBytes;

};


#endif