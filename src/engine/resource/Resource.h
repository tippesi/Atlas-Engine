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
    class ResourceManager;

    template<class T>
    struct ResourceLoader {
        std::function<Ref<T>(const std::string, void*)> function;
        void* userData = nullptr;
    };

    template<typename T>
    class Resource {
        friend class ResourceHandle<T>;
        friend class ResourceManager<T>;

    public:
        Resource() = default;

        Resource(const std::string& path) : path(path) {}

        Resource(const std::string& path, Ref<T> data) :
            path(path), data(data), isLoaded(data != nullptr) {}

        template<typename ...Args>
        void Load(Args&&... args) {
            data = std::make_shared<T>(path, std::forward<Args>(args)...);
            isLoaded = true;
        }

        template<typename ...Args>
        void LoadWithExternalLoader(std::function<Ref<T>(const std::string, void*)> loaderFunction,
            Args&&... args) {
            data = loaderFunction(path, std::forward<Args>(args)...);
            isLoaded = true;
        }

        void Unload() {
            isLoaded = false;
            data = nullptr;
        }

    private:
        Ref<T> data;

        std::string path;

        std::atomic_bool isLoaded = false;
    };

    template<typename T>
    class ResourceHandle {

    public:
        ResourceHandle() = default;

        ResourceHandle(Ref<Resource<T>>& resource) : resource(resource) {}

        inline bool IsLoaded() {
            return resource->isLoaded;
        }

        T& operator*() {
            return resource->data.operator*();
        }

        T* operator->() {
            return resource->data.operator->();
        }

    private:
        Ref<Resource<T>> resource = nullptr;

    };

}

#endif
