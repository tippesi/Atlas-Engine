#pragma once

#include "System.h"
#include "ResourceLoadException.h"
#include "../common/Hash.h"
#include "../common/Path.h"
#include "../loader/AssetLoader.h"
#include "../jobsystem/JobSystem.h"

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
            origin(origin), path(path), data(data), isLoaded(data != nullptr) {

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
            catch (const ResourceLoadException& exception) {
                errorOnLoad = true;
                exceptionOnLoad = exception;
                Log::Error("Exception on load for resource " + path + ": " + std::string(exception.what()));
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
                Log::Error("Exception on load for resource " + path + ": " + std::string(exception.what()));
            }
            catch(...) {
                errorOnLoad = true;
                exceptionOnLoad = std::runtime_error("Unknown issue occurred");
            }

        }

        bool WasModified() const {

            std::filesystem::file_time_type lastModified = std::filesystem::file_time_type::min();
            lastModified = Loader::AssetLoader::GetFileLastModifiedTime(path, lastModified);

            return loadModifiedTime < lastModified;

        }

        void UpdateModifiedTime() {

            loadModifiedTime = Loader::AssetLoader::GetFileLastModifiedTime(path, loadModifiedTime);

        }

        void Unload() {
            isLoaded = false;
            errorOnLoad = false;
            data = nullptr;
        }

        void Swap(Ref<T>& newData) {
            data.swap(newData);
        }

        std::string GetFileName() const {

            return Common::Path::GetFileName(path);

        }

        Hash ID = 0;

        ResourceOrigin origin = System;

        const std::string path;
        bool permanent = false;

        bool errorOnLoad = false;
        std::exception exceptionOnLoad;

        std::filesystem::file_time_type loadModifiedTime;        

    private:
        Ref<T> data;

        std::atomic_bool isLoaded = false;
        JobGroup jobGroup;
        std::shared_future<void> future;

        int32_t framesToDeletion = RESOURCE_RETENTION_FRAME_COUNT;
    };

    template<typename T>
    class ResourceHandle {

    public:
        ResourceHandle() = default;

        ResourceHandle(Ref<Resource<T>>& resource) : resource(resource) {}

        inline bool IsValid() const {
            return resource != nullptr;
        }

        inline bool IsLoaded() const {
            return IsValid() && resource->isLoaded;
        }

        inline void WaitForLoad() {
            if (IsValid()) {
                while (!resource->isLoaded)
                    JobSystem::Wait(resource->jobGroup);                
            }
        }

        inline size_t GetID() const {
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

        inline T* operator->() const {
            return resource->data.operator->();
        }

        inline T* operator&() const {
            return resource->data.get();
        }

        inline bool operator==(const ResourceHandle<T>& that) const {
            return that.resource == resource;
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