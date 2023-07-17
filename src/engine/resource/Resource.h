#ifndef AE_RESOURCE_H
#define AE_RESOURCE_H

#include "System.h"

#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <future>

#define RESOURCE_RETENTION_FRAME_COUNT 30

namespace Atlas {

    template<typename T>
    class ResourceHandle;

    template<typename T>
    class ResourceManager;

    enum ResourceOrigin {
        System = 0,
        User = 1
    };

    template<typename T>
    class Resource {
        friend class ResourceHandle<T>;
        friend class ResourceManager<T>;

    public:
        Resource() = default;

        Resource(const std::string& path, Ref<T> data = nullptr) :
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

        template<class ...Args>
        void LoadWithExternalLoader(std::function<Ref<T>(const std::string&, Args...)> loaderFunction, Args... args) {
            data = loaderFunction(path, std::forward<Args>(args)...);
            isLoaded = true;
        }

        void Unload() {
            isLoaded = false;
            data = nullptr;
        }

        ResourceOrigin origin = System;

        const std::string path;
        bool permanent = false;

    private:
        Ref<T> data;

        std::atomic_bool isLoaded = false;
        std::future<void> future;

        int32_t framesToDeletion = RESOURCE_RETENTION_FRAME_COUNT;
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
                resource->future.get();
            }
        }

        inline Ref<T>& Get() const {
            return resource->data;
        }

        inline Ref<Resource<T>>& GetResource() const {
            return resource;
        }

        inline T& operator*() {
            return resource->data.operator*();
        }

        inline T* operator->() {
            return resource->data.operator->();
        }

    private:
        Ref<Resource<T>> resource = nullptr;

    };

}

#endif
