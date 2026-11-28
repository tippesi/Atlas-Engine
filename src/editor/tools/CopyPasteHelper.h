#pragma once

#include "../FileImporter.h"

#include "resource/ResourceManager.h"

#include <typeindex>
#include <functional>

namespace Atlas::Editor {

	class CopyPasteHelper {

	public:
        template<class T>
        static void Copy(T& copy) {

            Copy(&copy, 1);

        }

        template<class T>
        static void CopyMulti(std::vector<T>& copy) {

            Copy(copy.data(), copy.size());

        }

        template<class T>
        static bool AcceptPaste(bool acceptMultiValue = false) {
            
            return typeid(T) == typeInfo && data != nullptr &&
                (elementCount == 1 || acceptMultiValue && elementCount >= 1);
        
        }

        template<class T>
        static void Paste(T& paste) {

            AE_ASSERT(elementCount == 1 && "Can't paste multi value, call PasteMulti() instead");
            Paste(&paste, 1);

        }

        template<class T>
        static void PasteMulti(std::vector<T>& paste) {

            if (paste.size() != elementCount)
                paste.resize(elementCount);

            Paste(paste.data(), elementCount);

        }

        static void Clear() {

            if (data != nullptr) {
                auto ptr = static_cast<char*>(data);
                for (int32_t i = 0; i < elementCount; i++) {
                    destructor(static_cast<void*>(ptr));
                    ptr += elementSize;
                }
                std::free(data);
            }

            data = nullptr;
            elementCount = 0;
            elementSize = 0;

        }

    private:
        template<class T> 
        static void Copy(T* source, size_t count) {

            Clear();

            elementCount = count;
            elementSize = sizeof(T);

            typeInfo = typeid(T);
            data = std::malloc(sizeof(T) * elementCount);
            std::memset(data, 0, sizeof(T) * elementCount);

            T* typeData = static_cast<T*>(data);
            for (size_t i = 0; i < elementCount; i++) {
                new (&typeData[i])T();
                typeData[i] = source[i];
            }

            destructor = [](void* ptr) {
                static_cast<T*>(ptr)->~T();
            };

        }

        template<class T> 
        static void Paste(T* dst, size_t count) {

            T* typeData = static_cast<T*>(data);
            for (size_t i = 0; i < count; i++)
                dst[i] = typeData[i];

        }

        static std::type_index typeInfo;

        static void* data;
        static size_t elementCount;
        static size_t elementSize;
        static std::function<void(void*)> destructor;

	};

}