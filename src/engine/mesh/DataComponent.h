#ifndef AE_DATACOMPONENT_H
#define AE_DATACOMPONENT_H

#include "../System.h"
#include "../common/Packing.h"
#include "../graphics/Common.h"

#include <type_traits>
#include <cstddef>
#include <algorithm>
#include <cassert>

#include <glm/gtc/type_ptr.hpp>

namespace Atlas {

    namespace Mesh {

        enum class ComponentFormat {
            UnsignedInt = 0,
            UnsignedShort = 1,
            Float = 2,
            HalfFloat = 3,
            PackedFloat = 4,
            PackedColor = 5
        };

        /**
         * Converts mesh data from basic types to
         * more compressed data internally.
         * @tparam T
         */
        template <class T> class DataComponent {

            static_assert(std::is_same_v<T, float> || std::is_same_v<T, vec2> ||
                          std::is_same_v<T, vec3> || std::is_same_v<T, vec4> ||
                          std::is_same_v<T, uint32_t>, "Unsupported data format.");

        public:
            /**
             * Constructs a DataComponent object.
             */
            DataComponent() = default;

            /**
             * Constructs a DataComponent object.
             * @param format The type of the component S should be converted into. See {@link DataComponent.h} for more.
             */
            DataComponent(ComponentFormat format);

            /**
             * Resets the type of the component.
             * @param format The new component type.
             * @note This results in the loss of all data the component contains.
             */
            void SetType(ComponentFormat format);

            /**
             * Returns the type set in the constructor.
             * @return
             * @note The type is equivalent to an OpenGL type.
             */
            int32_t GetType();

            /**
             * Returns the format of the data as a Vulkan format type
             * @return
             */
            VkFormat GetFormat() const;

            /**
             * Sets the data for the component and converts it into data of component type.
             * @param values An vector of values of type S.
             * @note The length of the should be exactly the same as the size set by {@link SetSize}.
             */
            void Set(std::vector<T>& values);

            /**
             * Returns the data in type S which the components holds.
             * @return A pointer to the data.
             */
            std::vector<T>& Get();

            /**
             * Gets the stride between converted elements to form
             * the base type T
             * @return The stride in number of converted elements
             */
            size_t GetStride();

            /**
             * Returns the size of one converted element.
             * @return The size in bytes
             */
            size_t GetElementSize();

            /**
             * Converts the data and returns a byte array.
             * @return Returns the converted data as a byte vector.
             */
            std::vector<std::byte>& GetConverted();

            /**
             * Converts the data and returns a void pointer pointing to the data.
             * @return A void pointer pointing to the converted data.
             */
            void* GetConvertedVoid();

            /**
             * Clears all the data
             */
            void Clear();

            /**
             * Clears the converted data which is created by to free space.
             * @note This makes the previously returned converted data invalid.
             */
            void ClearConverted();

            /**
             *
             * @return
             */
            bool ContainsData();

        private:
            void ConvertData();

            ComponentFormat format;

            std::vector<T> data;
            std::vector<std::byte> converted;

        };

        template <class T>
        DataComponent<T>::DataComponent(ComponentFormat format) : format(format) {


        }

        template <class T>
        void DataComponent<T>::SetType(ComponentFormat format) {

            this->format = format;

        }

        template <class T>
        int32_t DataComponent<T>::GetType() {

            return static_cast<int32_t>(format);

        }

        template<class T>
        VkFormat DataComponent<T>::GetFormat() const {
            if constexpr(std::is_same_v<T, uint32_t>) {
                switch(format) {
                    case ComponentFormat::UnsignedInt: return VK_FORMAT_R32_UINT;
                    case ComponentFormat::UnsignedShort: return VK_FORMAT_R16_UINT;
                    default: assert(0 && "Invalid combination of formats");
                }
            }
            if constexpr(std::is_same_v<T, float>) {
                switch(format) {
                    case ComponentFormat::Float: return VK_FORMAT_R32_SFLOAT;
                    case ComponentFormat::HalfFloat: return VK_FORMAT_R16_SFLOAT;
                    default: assert(0 && "Invalid combination of formats");
                }
            }
            if constexpr(std::is_same_v<T, vec2>) {
                switch(format) {
                    case ComponentFormat::Float: return VK_FORMAT_R32G32_SFLOAT;
                    case ComponentFormat::HalfFloat: return VK_FORMAT_R16G16_SFLOAT;
                    default: assert(0 && "Invalid combination of formats");
                }
            }
            if constexpr(std::is_same_v<T, vec3>) {
                switch(format) {
                    case ComponentFormat::Float: return VK_FORMAT_R32G32B32_SFLOAT;
                    case ComponentFormat::HalfFloat: return VK_FORMAT_R16G16B16_SFLOAT;
                    default: assert(0 && "Invalid combination of formats");
                }
            }
            if constexpr(std::is_same_v<T, vec4>) {
                switch(format) {
                    case ComponentFormat::Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
                    case ComponentFormat::HalfFloat: return VK_FORMAT_R16G16B16A16_SFLOAT;
                    case ComponentFormat::PackedFloat: return VK_FORMAT_A2B10G10R10_SNORM_PACK32;
                    case ComponentFormat::PackedColor: return VK_FORMAT_R8G8B8A8_UNORM;
                    default: assert(0 && "Invalid combination of formats"); 
                }
            }
        }

        template <class T>
        void DataComponent<T>::Set(std::vector<T>& data) {

            this->data = data;

        }

        template <class T>
        std::vector<T>& DataComponent<T>::Get() {

            return data;

        }

        template <class T>
        size_t DataComponent<T>::GetStride() {

            // The size of T is always a multiple of 4 bytes
            return sizeof(T) / 4;

        }

        template <class T>
        size_t DataComponent<T>::GetElementSize() {

            size_t size = 0;
            switch (format) {
                case ComponentFormat::UnsignedInt: size = sizeof(uint32_t) * GetStride(); break;
                case ComponentFormat::UnsignedShort: size = sizeof(uint16_t) * GetStride(); break;
                case ComponentFormat::Float: size = sizeof(float) * GetStride(); break;
                case ComponentFormat::HalfFloat: size = sizeof(float16) * GetStride(); break;
                case ComponentFormat::PackedFloat: size = sizeof(uint32_t); break;
                case ComponentFormat::PackedColor: size = sizeof(uint32_t); break;
            }

            return size;

        }

        template<class T>
        std::vector<std::byte>& DataComponent<T>::GetConverted() {

            ConvertData();
            return converted;

        }

        template <class T>
        void* DataComponent<T>::GetConvertedVoid() {

            ConvertData();
            return static_cast<void*>(converted.data());

        }

        template <class T>
        void DataComponent<T>::Clear() {

            data.clear();
            converted.clear();

        }

        template <class T>
        void DataComponent<T>::ClearConverted() {

            converted.clear();

        }

        template <class T>
        bool DataComponent<T>::ContainsData() {

            return data.size() > 0;

        }

        template<class T>
        void DataComponent<T>::ConvertData() {

            auto stride = GetStride();
            auto sizeInBytes = GetElementSize() * data.size();
            converted.resize(sizeInBytes);

            if (format == ComponentFormat::UnsignedInt) {
                if constexpr(std::is_same_v<T, uint32_t>) {
                    std::memcpy(converted.data(), data.data(), sizeInBytes);
                }
            }
            else if (format == ComponentFormat::UnsignedShort) {
                if constexpr(std::is_same_v<T, uint32_t>) {
                    std::vector<uint16_t> convertedData(data.size() * stride);
                    std::transform(data.begin(), data.end(), convertedData.begin(),
                                   [](auto x) { return uint16_t(x); });
                    std::memcpy(converted.data(), convertedData.data(), sizeInBytes);
                }
            }
            else if (format == ComponentFormat::Float) {
                if constexpr(std::is_same_v<T, float> || std::is_same_v<T, vec2> ||
                    std::is_same_v<T, vec3> || std::is_same_v<T, vec4>) {
                    std::memcpy(converted.data(), data.data(), sizeInBytes);
                }
            }
            else if (format == ComponentFormat::HalfFloat) {
                if constexpr(std::is_same_v<T, float> || std::is_same_v<T, vec2> ||
                    std::is_same_v<T, vec3> || std::is_same_v<T, vec4>) {
                    std::vector<uint16_t> convertedData(data.size() * stride);
                    float* floatData = static_cast<float*>(glm::value_ptr(data.front()));
                    for (size_t i = 0; i < convertedData.size(); i++)
                        convertedData[i] = glm::detail::toFloat16(floatData[i]);
                    std::memcpy(converted.data(), convertedData.data(), sizeInBytes);
                }
            }
            else if (format == ComponentFormat::PackedFloat) {
                if constexpr(std::is_same_v<T, vec4>) {
                    std::vector<uint32_t> convertedData(data.size());
                    std::transform(data.begin(), data.end(), convertedData.begin(),
                                   [](auto x) { return Common::Packing::PackNormalizedFloat3x10_1x2(x); });
                    std::memcpy(converted.data(), convertedData.data(), sizeInBytes);
                }
            }
            else if (format == ComponentFormat::PackedColor) {
                if constexpr(std::is_same_v<T, vec4>) {
                    std::vector<uint32_t> convertedData(data.size());
                    std::transform(data.begin(), data.end(), convertedData.begin(),
                        [](auto x) { return glm::packUnorm4x8(x); });
                    std::memcpy(converted.data(), convertedData.data(), sizeInBytes);
                }
            }

        }

    }

}

#endif