#ifndef AE_RESOURCE_H
#define AE_RESOURCE_H

#include "System.h"

#include <vector>
#include <mutex>
#include <atomic>

namespace Atlas {

    template<typename T>
    class ResourceHandle;

    template<typename T>
    class Resource {
        friend class ResourceHandle<T>;

    public:
        Resource() = default;

        template<typename ...Args>
        void Load(const std::string& path, Args&&... args) {
            data = std::make_shared<T>(path, std::forward<Args>(args)...);
            isLoaded = true;
        }

        void Unload() {
            isLoaded = false;
            data = nullptr;
        }

    private:
        Ref<T> data;

        std::atomic_bool isLoaded = false;
    };

    template<typename T>
    class ResourceHandle {

    public:
        ResourceHandle(Ref<Resource<T>>& resource) : resource(resource) {}

        inline bool IsLoaded() {
            return resource->isLoaded;
        }

        Ref<T>& operator*() {
            return resource->data;
        }

    private:
        Ref<Resource<T>> resource;

    };

}

#endif
