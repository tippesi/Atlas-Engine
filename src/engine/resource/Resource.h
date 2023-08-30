#ifndef AE_RESOURCE_H
#define AE_RESOURCE_H

#include "System.h"
#include "../common/Hash.h"

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
        User = 1,
        Custom = 2,
        Other
    };

    template<typename T>
    class Resource {
        friend class ResourceHandle<T>;
        friend class ResourceManager<T>;

    public:
        Resource() = default;

        Resource(const std::string& path, ResourceOrigin origin, Ref<T> data = nullptr) :
            path(path), origin(origin), data(data), isLoaded(data != nullptr) {

            HashCombine(ID, path);

        }

        template<typename ...Args>
        void Load(Args&&... args) {

            try {
                if constexpr (std::is_constructible<T, const std::string&, Args...>()) {
                    data = std::make_shared<T>(path, std::forward<Args>(args)...);
                }
                else {
                    data = std::make_shared<T>(std::forward<Args>(args)...);
                }

                isLoaded = true;
            }
            catch (const std::exception& exception) {
                errorOnLoad = true;
                exceptionOnLoad = exception;
            }
            catch(...) {
                errorOnLoad = true;
                exceptionOnLoad = std::runtime_error("Unknown issue occurred");
            }

        }

        template<class ...Args>
        void LoadWithExternalLoader(std::function<Ref<T>(const std::string&, Args...)> loaderFunction, Args... args) {

            try {
                data = loaderFunction(path, std::forward<Args>(args)...);
                isLoaded = true;
            }
            catch (const std::exception& exception) {
                errorOnLoad = true;
                exceptionOnLoad = exception;
            }
            catch(...) {
                errorOnLoad = true;
                exceptionOnLoad = std::runtime_error("Unknown issue occurred");
            }

        }

        void Unload() {
            isLoaded = false;
            errorOnLoad = false;
            data = nullptr;
        }

        Hash ID = 0;

        ResourceOrigin origin = System;

        const std::string path;
        bool permanent = false;

        bool errorOnLoad = false;
        std::exception exceptionOnLoad;

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

        inline bool IsValid() {
            return resource != nullptr;
        }

        inline bool IsLoaded() {
            return IsValid() && resource->isLoaded;
        }

        inline void WaitForLoad() {
            if (IsValid()) {
                if (!resource->future.valid())
                    return;
                resource->future.wait();
                resource->future.get();
            }
        }

        inline size_t GetID() {
            if (!IsValid()) return 0;

            return resource->ID;
        }

        inline Ref<T>& Get() {
            return resource->data;
        }

        inline const Ref<T>& Get() const {
            return resource->data;
        }

        inline const Ref<Resource<T>>& GetResource() const {
            return resource;
        }

        inline Ref<Resource<T>>& GetResource() {
            return resource;
        }

        inline void Reset() {
            resource = nullptr;
        }

        inline T& operator*() {
            return resource->data.operator*();
        }

        inline T* operator->() const {
            return resource->data.operator->();
        }

        inline T* operator&() const {
            return resource->data.get();
        }

    private:
        Ref<Resource<T>> resource = nullptr;

    };

    template<typename T>
    struct ResourceSubscriber {
        int32_t ID = 0;
        std::function<void(Ref<Resource<T>>&)> function;
    };

}

#endif
