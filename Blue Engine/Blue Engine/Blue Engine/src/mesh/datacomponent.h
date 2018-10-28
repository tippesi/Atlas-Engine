#ifndef DATACOMPONENT_H
#define DATACOMPONENT_H

#include "../system.h"
#include "packing.h"

#include <type_traits>

#define COMPONENT_UNSIGNED_INT GL_UNSIGNED_INT
#define COMPONENT_FLOAT GL_FLOAT
#define COMPONENT_UNSIGNED_SHORT GL_UNSIGNED_SHORT
#define COMPONENT_UNSIGNED_BYTE GL_UNSIGNED_BYTE
#define COMPONENT_HALF_FLOAT GL_HALF_FLOAT
#define COMPONENT_PACKED_FLOAT GL_INT_2_10_10_10_REV

template <class T> class DataComponent {

public:
	DataComponent(int32_t componentType, int32_t vertexSize);

	void Set(T* values);

	T* Get();

	void SetType(int32_t componentType);

	void SetSize(int32_t size);

	// void SetVertexSize(int32_t vertexSize);

	int32_t GetVertexSize();

	void* GetInternal();

	bool ContainsData();

	~DataComponent();

private:
	bool containsData;
	bool equalDataTypes;

	int32_t componentType;
	int32_t componentTypeSize;
	int32_t vertexSize;
	int32_t size;

	T* data;
	void* internalData;

};


template <class T> DataComponent<T>::DataComponent(int32_t componentType, int32_t vertexSize) : vertexSize(vertexSize) {

	containsData = false;

	internalData = nullptr;
	data = nullptr;

	SetType(componentType);

}

template <class T> void DataComponent<T>::Set(T* data) {

	containsData = true;

	delete this->data;

	if (componentType == COMPONENT_HALF_FLOAT) {
		int32_t dataSize = vertexSize * size;
		float16_t* internalData = (float16_t*)this->internalData;
		for (int32_t i = 0; i < dataSize; i++) {
			internalData[i] = glm::detail::toFloat16(data);
		}
	}
	else if (componentType == COMPONENT_PACKED_FLOAT) {
		uint32_t* internalData = (uint32_t*)this->internalData;
		int32_t dataCounter = 0;
		for (int32_t i = 0; i < size; i++) {
			vec4 vector = vec4(data[dataCounter], data[dataCounter + 1], data[dataCounter + 2], 0.0f);
			internalData[i] = packNormalizedFloat_2_10_10_10_REV(vector);
			dataCounter += 3;
		}
	}
	else {
		this->internalData = data;
	}

	this->data = data;

}

template <class T> T* DataComponent<T>::Get() {

	return data;

}

template <class T> void DataComponent<T>::SetType(int32_t componentType) {

	// We should check if the type is compatible to T
	this->componentType = componentType;

	delete internalData;

	if (componentType == COMPONENT_HALF_FLOAT) {
		delete data;
		internalData = new float16_t[vertexSize * size];
	}
	else if (componentType == COMPONENT_PACKED_FLOAT) {
		delete data;
		internalData = new uint32_t[size];
	}

}

template <class T> void DataComponent<T>::SetSize(int32_t size) {

	this->size = size;

	delete internalData;

	if (componentType == COMPONENT_HALF_FLOAT) {
		delete data;
		internalData = new float16_t[vertexSize * size];
	}
	else if (componentType == COMPONENT_PACKED_FLOAT) {
		delete data;
		internalData = new uint32_t[size];
	}

}

template <class T> int32_t DataComponent<T>::GetVertexSize() {

	return vertexSize;

}

template <class T> void* DataComponent<T>::GetInternal() {

	return internalData;

}

template <class T> bool DataComponent<T>::ContainsData() {

	return containsData;

}

template <class T> DataComponent<T>::~DataComponent() {

	delete T;
	if (internalData != NULL && componentType)
		delete internalData;

}

#endif