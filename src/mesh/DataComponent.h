#ifndef DATACOMPONENT_H
#define DATACOMPONENT_H

#include "../System.h"
#include "Packing.h"

#include <type_traits>

#define COMPONENT_UNSIGNED_INT GL_UNSIGNED_INT
#define COMPONENT_FLOAT GL_FLOAT
#define COMPONENT_UNSIGNED_SHORT GL_UNSIGNED_SHORT
#define COMPONENT_UNSIGNED_BYTE GL_UNSIGNED_BYTE
#define COMPONENT_HALF_FLOAT GL_HALF_FLOAT
#define COMPONENT_PACKED_FLOAT GL_INT_2_10_10_10_REV

/**
 * Manage the data of the vertex buffer of a mesh
 * @tparam T The type of the input data.
 * @remarks DataComponent is responsible to automatically convert the the of type T
 * into a defined component type. This makes it easy to reduce the amount of data on
 * the GPU, while being easy to program. See {@link DataComponent.h} for the supported component types.
 */
template <class T> class DataComponent {

public:
	/**
	 * Constructs a DataComponent object
	 * @param componentType The type of the component T should be converted into. See {@link DataComponent.h} for more.
	 * @param stride The stride in elements of that specific component.
	 */
	DataComponent(int32_t componentType, int32_t stride);

	/**
	 * Sets the data for the component and converts it into data of component type.
	 * @param values An array of values of type T.
	 * @note The length of the should be exactly the same as the size set by {@link SetSize}.
	 */
	void Set(T* values);

	/**
	 * Returns the data in type T which the components holds.
	 * @return A pointer to the data.
	 */
	T* Get();

	/**
	 * Resets the type of the component.
	 * @param componentType The new component type.
	 * @note This results in the loss of all data the component contains.
	 */
	void SetType(int32_t componentType);

	///
	/// \return
	int32_t GetType();

	///
	/// \param size
	void SetSize(int32_t size);

	///
	/// \return
	int32_t GetStride();

	///
	/// \return
	int32_t GetElementSize();

	///
	/// \return
	void* GetInternal();

	///
	/// \return
	bool ContainsData();

	~DataComponent();

private:
	bool containsData;

	int32_t componentType;
	int32_t stride;
	int32_t size;

	T* data;
	void* internalData;

};


template <class T> 
DataComponent<T>::DataComponent(int32_t componentType, int32_t stride) : stride(stride) {

	containsData = false;

	internalData = nullptr;
	data = nullptr;

	size = 0;

	SetType(componentType);

}

template <class T> 
void DataComponent<T>::Set(T* data) {

	containsData = true;

	delete[] this->data;

	if (componentType == COMPONENT_HALF_FLOAT) {
		int32_t dataSize = stride * size;
		float16_t* internalData = (float16_t*)this->internalData;
		for (int32_t i = 0; i < dataSize; i++) {
			internalData[i] = glm::detail::toFloat16((float)data[i]);
		}
	}
	else if (componentType == COMPONENT_PACKED_FLOAT) {
		uint32_t* internalData = (uint32_t*)this->internalData;
		int32_t dataCounter = 0;
		for (int32_t i = 0; i < size; i++) {
			vec4 vector = vec4(data[dataCounter], data[dataCounter + 1],
				data[dataCounter + 2], data[dataCounter + 3]);
			internalData[i] = packNormalizedFloat_2_10_10_10_REV(vector);
			dataCounter += 4;
		}
	}
	else if (componentType == COMPONENT_UNSIGNED_SHORT && sizeof(uint16_t) <= sizeof(T)) {
		int32_t dataSize = stride * size;
		uint16_t* internalData = (uint16_t*)this->internalData;
		for (int32_t i = 0; i < dataSize; i++) {
			internalData[i] = (uint16_t)data[i];
		}
	}
	else if (componentType == COMPONENT_UNSIGNED_BYTE && sizeof(uint8_t) <= sizeof(T)) {
		int32_t dataSize = stride * size;
		uint8_t* internalData = (uint8_t*)this->internalData;
		for (int32_t i = 0; i < dataSize; i++) {
			internalData[i] = (uint8_t)data[i];
		}
	}
	else {
		this->internalData = data;
	}

	this->data = data;

}

template <class T>
T* DataComponent<T>::Get() {

	return data;

}

template <class T>
void DataComponent<T>::SetType(int32_t componentType) {

	// We should check if the type is compatible to T
	this->componentType = componentType;

	if (componentType == COMPONENT_HALF_FLOAT) {
		delete[](float16_t*)internalData;
	}
	else if (componentType == COMPONENT_PACKED_FLOAT) {
		delete[](uint32_t*)internalData;
	}
	else if (componentType == COMPONENT_UNSIGNED_SHORT && sizeof(uint16_t) <= sizeof(T)) {
		delete[](uint16_t*)internalData;
	}
	else if (componentType == COMPONENT_UNSIGNED_BYTE && sizeof(uint8_t) <= sizeof(T)) {
		delete[](uint8_t*)internalData;
	}

	if (componentType == COMPONENT_HALF_FLOAT) {
		delete[] data;
		internalData = new float16_t[stride * size];
	}
	else if (componentType == COMPONENT_PACKED_FLOAT) {
		delete[] data;
		internalData = new uint32_t[size];
	}
	else if (componentType == COMPONENT_UNSIGNED_SHORT && sizeof(uint16_t) <= sizeof(T)) {
		delete[] data;
		internalData = new uint16_t[stride * size];
	}
	else if (componentType == COMPONENT_UNSIGNED_BYTE && sizeof(uint8_t) <= sizeof(T)) {
		delete[] data;
		internalData = new uint8_t[stride * size];
	}

}

template <class T>
int32_t DataComponent<T>::GetType() {

	return componentType;

}

template <class T>
void DataComponent<T>::SetSize(int32_t size) {

	this->size = size;

	if (componentType == COMPONENT_HALF_FLOAT) {
		delete[] (float16_t*)internalData;
	}
	else if (componentType == COMPONENT_PACKED_FLOAT) {
		delete[](uint32_t*)internalData;
	}
	else if (componentType == COMPONENT_UNSIGNED_SHORT && sizeof(uint16_t) <= sizeof(T)) {
		delete[](uint16_t*)internalData;
	}
	else if (componentType == COMPONENT_UNSIGNED_BYTE && sizeof(uint8_t) <= sizeof(T)) {
		delete[](uint8_t*)internalData;
	}

	if (componentType == COMPONENT_HALF_FLOAT) {
		delete[] data;
		internalData = new float16_t[stride * size];
	}
	else if (componentType == COMPONENT_PACKED_FLOAT) {
		delete[] data;
		internalData = new uint32_t[size];
	}
	else if (componentType == COMPONENT_UNSIGNED_SHORT && sizeof(uint16_t) <= sizeof(T)) {
		delete[] data;
		internalData = new uint16_t[stride * size];
	}
	else if (componentType == COMPONENT_UNSIGNED_BYTE && sizeof(uint8_t) <= sizeof(T)) {
		delete[] data;
		internalData = new uint8_t[stride * size];
	}

}

template <class T> 
int32_t DataComponent<T>::GetStride() {

	return componentType != COMPONENT_PACKED_FLOAT ? stride : 4;

}

template <class T> 
int32_t DataComponent<T>::GetElementSize() {

	switch (componentType) {
	case COMPONENT_UNSIGNED_INT: return sizeof(uint32_t) * stride;
	case COMPONENT_UNSIGNED_SHORT: return sizeof(uint16_t) * stride;
	case COMPONENT_UNSIGNED_BYTE: return sizeof(uint8_t) * stride;
	case COMPONENT_FLOAT: return sizeof(float) * stride;
	case COMPONENT_HALF_FLOAT: return sizeof(float16_t) * stride;
	case COMPONENT_PACKED_FLOAT: return sizeof(uint32_t);
	}

	return 0;

}

template <class T> 
void* DataComponent<T>::GetInternal() {

	return internalData;

}

template <class T>
bool DataComponent<T>::ContainsData() {

	return containsData;

}

template <class T>
DataComponent<T>::~DataComponent() {

	delete[] data;
	
	if (componentType == COMPONENT_HALF_FLOAT) {
		delete[] (float16_t*)internalData;
	}
	else if (componentType == COMPONENT_PACKED_FLOAT) {
		delete[] (uint32_t*)internalData;
	}
	else if (componentType == COMPONENT_UNSIGNED_SHORT && sizeof(uint16_t) <= sizeof(T)) {
		delete[] (uint16_t*)internalData;
	}
	else if (componentType == COMPONENT_UNSIGNED_BYTE && sizeof(uint8_t) <= sizeof(T)) {
		delete[] (uint8_t*)internalData;
	}

}

#endif