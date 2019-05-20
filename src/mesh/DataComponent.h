#ifndef AE_DATACOMPONENT_H
#define AE_DATACOMPONENT_H

#include "../System.h"
#include "../TypeFormat.h"
#include "Packing.h"

#include <type_traits>

/**
 * T should be uint32_t to support proper conversion
 */
#define AE_COMPONENT_UNSIGNED_INT AE_UINT
/**
 * T should be float to support proper conversion
 */
#define AE_COMPONENT_FLOAT AE_FLOAT
/**
 * T should be uint16_t to support proper conversion
 */
#define AE_COMPONENT_UNSIGNED_SHORT AE_USHORT
/**
 * T should be uint8_t to support proper conversion
 */
#define AE_COMPONENT_UNSIGNED_BYTE AE_UBYTE
/**
 * T should be float16 to support proper conversion
 */
#define AE_COMPONENT_HALF_FLOAT AE_HALF_FLOAT
/**
 * T should be uint32_t to support proper conversion
 */
#define AE_COMPONENT_PACKED_FLOAT AE_INT_2_10_10_10

namespace Atlas {

	namespace Mesh {

		/**
		 * Manage the data of the vertex buffer of a mesh
		 * @tparam S The type of the input data.
		 * @tparam T The type ouf the output data
		 * @note The converted data is internally held by a void pointer. This means that the converted
		 * data isn't converted to T, but to the component type format specified in the constructor. T just tells
		 * the class to return the void pointer as type T. As an example say component type is COMPONENT_UNSIGNED_BYTE
		 * and T is uint32_t. Then each uint32_t would hold 4 converted S.
		 * @remarks DataComponent is responsible to automatically convert the the of type S
		 * into a defined component type. This makes it easy to reduce the amount of data on
 		 * the GPU, while being easy to program. See {@link DataComponent.h} for the supported component types.
 		 */
		template <class S, class T> class DataComponent {

		public:
			/**
			 * Constructs a DataComponent object.
			 */
			DataComponent() {}

			DataComponent(const DataComponent& that);

			/**
             * Constructs a DataComponent object.
             * @param componentType The type of the component S should be converted into. See {@link DataComponent.h} for more.
             * @param stride The stride in elements of that specific component.
             */
			DataComponent(int32_t componentType, int32_t stride);

			~DataComponent();

			DataComponent& operator=(const DataComponent& that);

			/**
             * Sets the data for the component and converts it into data of component type.
             * @param values An array of values of type S.
             * @note The length of the should be exactly the same as the size set by {@link SetSize}.
             */
			void Set(S* values);

			/**
             * Returns the data in type S which the components holds.
             * @return A pointer to the data.
             */
			S* Get();

			/**
             * Returns data in type S which the components holds.
             * @param index The index to the data
             * @return The data S
             */
			S Get(int32_t index);

			/**
             * Resets the type of the component.
             * @param componentType The new component type.
             * @note This results in the loss of all data the component contains.
             */
			void SetType(int32_t componentType);

			/**
             * Returns the type set in the constructor.
             * @return
             * @note The type is equivalent to an OpenGL type.
             */
			int32_t GetType();

			/**
             *
             * @param size
             */
			void SetSize(int32_t size);

			/**
             * Returns the stride set in the constructor
             * @return The stride as an integer
             */
			int32_t GetStride();

			/**
             * Returns the size of one converted element.
             * @return The size as an integer
             */
			int32_t GetElementSize();

			/**
             *
             * @return
             */
			T* GetConverted();

			/**
             *
             * @param index
             * @return
             */
			T GetConverted(int32_t index);

			/**
             *
             * @return
             */
			void* GetConvertedVoid();

			/**
             *
             * @return
             */
			bool ContainsData();

		private:
			void DeleteData();

			void DeepCopy(const DataComponent& that);

			bool containsData = false;

			int32_t componentType = 0;
			int32_t stride = 0;
			int32_t size = 0;

			S* data = nullptr;
			void* convertedData = nullptr;

		};

		template <class S, class T>
		DataComponent<S, T>::DataComponent(const DataComponent<S, T>& that) {

			DeepCopy(that);

		}

		template <class S, class T>
		DataComponent<S, T>::DataComponent(int32_t componentType, int32_t stride) : stride(stride) {

			SetType(componentType);

		}

		template <class S, class T>
		DataComponent<S, T>::~DataComponent() {

			DeleteData();

		}

		template <class S, class T>
		DataComponent<S, T>& DataComponent<S, T>::operator=(const DataComponent<S, T>& that) {

			if (this != &that) {

				DeepCopy(that);

			}

			return *this;

		}

		template <class S, class T>
		void DataComponent<S, T>::Set(S* data) {

			containsData = true;

			DeleteData();

			if (componentType == AE_COMPONENT_HALF_FLOAT) {
				int32_t dataSize = stride * size;
				convertedData = new float16[dataSize];
				float16* internalData = (float16*)convertedData;
				for (int32_t i = 0; i < dataSize; i++) {
					internalData[i] = glm::detail::toFloat16((float)data[i]);
				}
			}
			else if (componentType == AE_COMPONENT_PACKED_FLOAT) {
				convertedData = new uint32_t[size];
				uint32_t* internalData = (uint32_t*)convertedData;
				int32_t dataCounter = 0;
				for (int32_t i = 0; i < size; i++) {
					vec4 vector = vec4(data[dataCounter], data[dataCounter + 1],
									   data[dataCounter + 2], data[dataCounter + 3]);
					internalData[i] = packNormalizedFloat_2_10_10_10_REV(vector);
					dataCounter += 4;
				}
			}
			else if (componentType == AE_COMPONENT_UNSIGNED_SHORT && sizeof(uint16_t) <= sizeof(S)) {
				int32_t dataSize = stride * size;
				convertedData = new uint16_t[dataSize];
				uint16_t* internalData = (uint16_t*)convertedData;
				for (int32_t i = 0; i < dataSize; i++) {
					internalData[i] = (uint16_t)data[i];
				}
			}
			else if (componentType == AE_COMPONENT_UNSIGNED_BYTE && sizeof(uint8_t) <= sizeof(S)) {
				int32_t dataSize = stride * size;
				convertedData = new uint8_t[dataSize];
				uint8_t* internalData = (uint8_t*)convertedData;
				for (int32_t i = 0; i < dataSize; i++) {
					internalData[i] = (uint8_t)data[i];
				}
			}
			else {
				convertedData = data;
			}

			this->data = data;

		}

		template <class S, class T>
		S* DataComponent<S, T>::Get() {

			return data;

		}

		template <class S, class T>
		S DataComponent<S, T>::Get(int32_t index) {

			return data[index];

		}

		template <class S, class T>
		void DataComponent<S, T>::SetType(int32_t componentType) {

			DeleteData();

			// We should check if the type is compatible to T
			this->componentType = componentType;

		}

		template <class S, class T>
		int32_t DataComponent<S, T>::GetType() {

			return componentType;

		}

		template <class S, class T>
		void DataComponent<S, T>::SetSize(int32_t size) {

			DeleteData();

			this->size = size;

		}

		template <class S, class T>
		int32_t DataComponent<S, T>::GetStride() {

			return componentType != AE_COMPONENT_PACKED_FLOAT ? stride : 4;

		}

		template <class S, class T>
		int32_t DataComponent<S, T>::GetElementSize() {

			switch (componentType) {
				case AE_COMPONENT_UNSIGNED_INT: return sizeof(uint32_t) * stride;
				case AE_COMPONENT_UNSIGNED_SHORT: return sizeof(uint16_t) * stride;
				case AE_COMPONENT_UNSIGNED_BYTE: return sizeof(uint8_t) * stride;
				case AE_COMPONENT_FLOAT: return sizeof(float) * stride;
				case AE_COMPONENT_HALF_FLOAT: return sizeof(float16) * stride;
				case AE_COMPONENT_PACKED_FLOAT: return sizeof(uint32_t);
			}

			return 0;

		}

		template <class S, class T>
		T* DataComponent<S, T>::GetConverted() {

			return (T*)convertedData;

		}

		template <class S, class T>
		T DataComponent<S, T>::GetConverted(int32_t index) {

			return ((T*)convertedData)[index];

		}

		template <class S, class T>
		void* DataComponent<S, T>::GetConvertedVoid() {

			return convertedData;

		}

		template <class S, class T>
		bool DataComponent<S, T>::ContainsData() {

			return containsData;

		}

		template <class S, class T>
		void DataComponent<S, T>::DeleteData() {

			if (data)
				delete[] data;

			if (componentType == AE_COMPONENT_HALF_FLOAT) {
				delete[](float16*)convertedData;
			}
			else if (componentType == AE_COMPONENT_PACKED_FLOAT) {
				delete[](uint32_t*)convertedData;
			}
			else if (componentType == AE_COMPONENT_UNSIGNED_SHORT && sizeof(uint16_t) <= sizeof(S)) {
				delete[](uint16_t*)convertedData;
			}
			else if (componentType == AE_COMPONENT_UNSIGNED_BYTE && sizeof(uint8_t) <= sizeof(S)) {
				delete[](uint8_t*)convertedData;
			}

			data = nullptr;
			convertedData = nullptr;

		}

		template <class S, class T>
		void DataComponent<S, T>::DeepCopy(const DataComponent<S, T>& that) {

			stride = that.stride;
			containsData = false;

			// Deletes data and convertedData.
			// Creates new convertedData afterwards
			SetType(that.componentType);
			SetSize(that.size);

			if (that.containsData) {
				// We need to duplicate the data
				auto dataSize = stride * size;
				auto copy = new S[dataSize];
				std::memcpy(copy, that.data, dataSize * sizeof(S));
				Set(copy);
			}

		}

	}

}

#endif