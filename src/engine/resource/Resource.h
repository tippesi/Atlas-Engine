#ifndef AE_RESOURCE_H
#define AE_RESOURCE_H

#include "System.h"

#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <future>

namespace Atlas {

    template<typename T>
    class ResourceHandle;

    template<typename T>
    class ResourceManager;

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

            if constexpr (std::is_constructible<T, const std::string&, Args...>()) {
                data = std::make_shared<T>(path, std::forward<Args>(args)...);
            }
            else {
                data = std::make_shared<T>(std::forward<Args>(args)...);
            }
            
            isLoaded = true;
        }

        void LoadWithExternalLoader(std::function<Ref<T>(const std::string)> loaderFunction) {
            data = loaderFunction(path);
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
        std::future<void> future;
    };

    template<typename T>
    class ResourceHandle {

    public:
        ResourceHandle() = default;

        ResourceHandle(Ref<Resource<T>>& resource) : resource(resource) {}

        inline bool IsLoaded() {
            return resource != nullptr && resource->isLoaded;
        }

        inline void WaitForLoad() {
            if (resource != nullptr) {
                if (!resource->future.valid())
                    return;
                resource->future.wait();
            }
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
