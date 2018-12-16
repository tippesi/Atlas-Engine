#ifndef VERTEXBUFFER_H
#define VERTEXBUFFER_H

#include "System.h"

/**
 * Manages the vertex data flow between the CPU and GPU
 */
class VertexBuffer {

public:
	/**
	 * Constructs a VertexBuffer object.
	 * @param type The type/target of the data, e.g GL_ARRAY_BUFFER
	 * @param dataType The data type of the data, e.g GL_FLOAT
	 * @param stride The number of elements of dataType
	 * @param usage The usage of the buffer which is GL_STATIC_DRAW by default.
	 * @note This is similar to
	 * <a href="https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glBufferData.xhtml">glBufferData</a>.
	 * For all available data types see <a href="https://www.khronos.org/opengl/wiki/OpenGL_Type">Common Enums</a>.
	 * GL_FIXED is not supported.
	 * @remark If you want to pass a 4-component vector you would use GL_FLOAT as the data type
	 * and 4 as the stride. For a 4x4 matrix data type would also be GL_FLOAT, but stride would equal 16.
	 * This is different to all the OpenGL functions, where stride is in bytes.
	 */
	VertexBuffer(uint32_t type,  int32_t dataType, int32_t stride, uint32_t usage = GL_STATIC_DRAW);

	/**
	* Binds the OpenGL vertex buffer object.
	*/
	void Bind();

	/**
	* Unbinds any OpenGL vertex buffer object.
	*/
	void Unbind();

	/**
	* Returns the type of the OpenGL vertex buffer object.
	* @return An integer corresponding to an OpenGL vertex buffer type.
	*/
	uint32_t GetType();

	/**
	* Returns the type of the data which the buffer holds.
	* @return An integer corresponding to an OpenGL data type.
	* @remark For all available data types see https://www.khronos.org/opengl/wiki/OpenGL_Type
	* <a href="https://www.khronos.org/opengl/wiki/OpenGL_Type">Common Enums</a>. GL_FIXED is not supported.
	*/
	int32_t GetDataType();

	/**
	* Returns the stride of each element
	* @return
	* @note Stride is not in bytes but is the number of base elements of type dataType. Have a look at
	* {@link VertexBuffer()} for an example.
	*/
	int32_t GetStride();

	/**
	* Returns the number of elements.
	* @return An integer which equals the number of elements.
	*/
	int32_t GetElementCount();

	/**
	 * Returns the size in bytes of an element
	 * @return An integer which equals the size of an element.
	 */
	int32_t GetElementSize();

	/**
	 * Sets the data of the OpenGL vertex buffer object.
	 * @param data The data as a pointer.
	 * @param length The number of elements in data.
	 */
	void SetData(uint8_t* data, int32_t length);

	/**
	* Sets the data of the OpenGL vertex buffer object.
	* @param data The data as a pointer.
	* @param length The number of elements in data.
	*/
	void SetData(uint16_t* data, int32_t length);

	/**
	* Sets the data of the OpenGL vertex buffer object.
	* @param data The data as a pointer.
	* @param length The number of elements in data.
	*/
	void SetData(uint32_t* data, int32_t length);

	/**
	* Sets the data of the OpenGL vertex buffer object.
	* @param data The data as a pointer.
	* @param length The number of elements in data.
	*/
	void SetData(int8_t* data, int32_t length);

	/**
	* Sets the data of the OpenGL vertex buffer object.
	* @param data The data as a pointer.
	* @param length The number of elements in data.
	*/
	void SetData(int16_t* data, int32_t length);

	/**
	* Sets the data of the OpenGL vertex buffer object.
	* @param data The data as a pointer.
	* @param length The number of elements in data.
	*/
	void SetData(int32_t* data, int32_t length);

	/**
	* Sets the data of the OpenGL vertex buffer object.
	* @param data The data as a pointer.
	* @param length The number of elements in data.
	*/
	void SetData(float* data, int32_t length);

	/**
	* Sets the data of the OpenGL vertex buffer object.
	* @param data The data as a pointer.
	* @param length The number of elements in data.
	*/
	void SetData(vec2* data, int32_t length);

	/**
	* Sets the data of the OpenGL vertex buffer object.
	* @param data The data as a pointer.
	* @param length The number of elements in data.
	*/
	void SetData(vec3* data, int32_t length);

	/**
	* Sets the data of the OpenGL vertex buffer object.
	* @param data The data as a pointer.
	* @param length The number of elements in data.
	*/
	void SetData(vec4* data, int32_t length);

	/**
	* Sets the data of the OpenGL vertex buffer object.
	* @param data The data as a pointer.
	* @param length The number of elements in data.
	*/
	void SetData(mat3* data, int32_t length);

	/**
	* Sets the data of the OpenGL vertex buffer object.
	* @param data The data as a pointer.
	* @param length The number of elements in data.
	*/
	void SetData(mat4* data, int32_t length);

	/**
	 * Sets the data of the OpenGL vertex buffer object.
	 * @param data The data as a pointer.
	 * @param length The number of elements in data.
	 * @param elementSize The size of each element in bytes.
	 * @note length * elementSize should equal the length in bytes of the passed data.
	 */
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