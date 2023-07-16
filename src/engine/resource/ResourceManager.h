#ifndef AE_RESOURCEMANAGER_H
#define AE_RESOURCEMANAGER_H

#include "System.h"
#include "Resource.h"
#include "ResourceMemoryManager.h"
#include "events/EventManager.h"

#include <type_traits>
#include <mutex>
#include <unordered_map>
#include <future>

namespace Atlas {

    template<typename T>
    class ResourceManager {

    public:
        template<typename ...Args>
        static ResourceHandle<T> GetResource(const std::string& path, Args&&... args) {

            return GetResource(path, System, std::forward<Args>(args)...);

        }

        template<typename ...Args>
        static ResourceHandle<T> GetResource(const std::string& path, ResourceOrigin origin, Args&&... args) {

            static_assert(std::is_constructible<T, const std::string&, Args...>() ||
                          std::is_constructible<T, Args...>(),
                "Resource class needs to implement constructor with provided argument type");

            CheckInitialization();

            {
                std::lock_guard lock(mutex);
                if (resources.contains(path)) {
                    auto& resource = resources[path];
                    resource->framesToDeletion = RESOURCE_RETENTION_FRAME_COUNT;
                    return ResourceHandle<T>(resource);
                }

                resources[path] = std::make_shared<Resource<T>>(path);
            }

            // Load only after mutex is unlocked
            resources[path]->Load(std::forward<Args>(args)...);
            return ResourceHandle<T>(resources[path]);

        }

        template<class ...Args>
        static ResourceHandle<T> GetResourceWithLoader(const std::string& path,
            Ref<T> (*loaderFunction)(const std::string&, Args...), Args... args) {

            CheckInitialization();

            {
                std::lock_guard lock(mutex);
                if (resources.contains(path)) {
                    auto& resource = resources[path];
                    resource->framesToDeletion = RESOURCE_RETENTION_FRAME_COUNT;
                    return ResourceHandle<T>(resource);
                }

                resources[path] = std::make_shared<Resource<T>>(path);
            }

            // Load only after mutex is unlocked
            resources[path]->LoadWithExternalLoader(std::function(loaderFunction), std::forward<Args>(args)...);
            return ResourceHandle<T>(resources[path]);

        }

        template<class ...Args>
        static ResourceHandle<T> GetResourceWithLoader(const std::string& path,
            std::function<Ref<T>(const std::string&, Args...)> loaderFunction, Args&&... args) {

            CheckInitialization();

            {
                std::lock_guard lock(mutex);
                if (resources.contains(path)) {
                    auto& resource = resources[path];
                    resource->framesToDeletion = RESOURCE_RETENTION_FRAME_COUNT;
                    return ResourceHandle<T>(resource);
                }

                resources[path] = std::make_shared<Resource<T>>(path);
            }

            // Load only after mutex is unlocked
            resources[path]->LoadWithExternalLoader(loaderFunction, std::forward<Args>(args)...);
            return ResourceHandle<T>(resources[path]);

        }

        template<typename ...Args>
        static ResourceHandle<T> GetResourceAsync(const std::string& path, Args&&... args) {

            static_assert(std::is_constructible<T, const std::string&, Args...>() ||
                std::is_constructible<T, Args...>(),
                "Resource class needs to implement constructor with provided argument type");

            CheckInitialization();

            {
                std::lock_guard lock(mutex);
                if (resources.contains(path)) {
                    auto& resource = resources[path];
                    resource->framesToDeletion = RESOURCE_RETENTION_FRAME_COUNT;
                    return ResourceHandle<T>(resource);
                }

                resources[path] = std::make_shared<Resource<T>>(path);
            }

            // Load only after mutex is unlocked
            resources[path]->future = std::async(std::launch::async, &Resource<T>::Load,
                resources[path].get(), std::forward<Args>(args)...);
            return ResourceHandle<T>(resources[path]);

        }

        template<class ...Args>
        static ResourceHandle<T> GetResourceWithLoaderAsync(const std::string& path,
            std::function<Ref<T>(const std::string&, Args...)> loaderFunction, Args... args) {

            CheckInitialization();

            {
                std::lock_guard lock(mutex);
                if (resources.contains(path)) {
                    auto& resource = resources[path];
                    resource->framesToDeletion = RESOURCE_RETENTION_FRAME_COUNT;
                    return ResourceHandle<T>(resource);
                }

                resources[path] = std::make_shared<Resource<T>>(path);
            }

            // Load only after mutex is unlocked
            resources[path]->future = std::async(std::launch::async,
                &Resource<T>::template LoadWithExternalLoader<Args...>,
                resources[path].get(), loaderFunction, std::forward<Args>(args)...);
            return ResourceHandle<T>(resources[path]);

        }

        template<class ...Args>
        static ResourceHandle<T> GetResourceWithLoaderAsync(const std::string& path,
            Ref<T> (*loaderFunction)(const std::string&, Args...), Args... args) {

            CheckInitialization();

            {
                std::lock_guard lock(mutex);
                if (resources.contains(path)) {
                    auto& resource = resources[path];
                    resource->framesToDeletion = RESOURCE_RETENTION_FRAME_COUNT;
                    return ResourceHandle<T>(resource);
                }

                resources[path] = std::make_shared<Resource<T>>(path);
            }

            // Load only after mutex is unlocked
            resources[path]->future = std::async(std::launch::async,
                &Resource<T>::template LoadWithExternalLoader<Args...>,
                resources[path].get(), std::function(loaderFunction), std::forward<Args>(args)...);
            return ResourceHandle<T>(resources[path]);

        }

        static ResourceHandle<T> AddResource(const std::string& path, Ref<Resource<T>> resource) {

            std::lock_guard lock(mutex);
            if (resources.contains(path)) {
                auto& resource = resources[path];
                resource->framesToDeletion = RESOURCE_RETENTION_FRAME_COUNT;
                return ResourceHandle<T>(resource);
            }

            resources[path] = resource;
            return ResourceHandle<T>(resource);

        }

        static ResourceHandle<T> AddResource(const std::string& path, Ref<T> data) {

            return AddResource(path, CreateRef<Resource<T>>(path, data));

        }

        std::vector<ResourceHandle<T>> GetResources() {

            std::vector<ResourceHandle<T>> resourceHandles;

            for (auto& [_, resource] : resources) {
                resourceHandles.emplace_back(resource);
            }

            return resourceHandles;

        }

    private:
        static std::mutex mutex;
        static std::unordered_map<std::string, Ref<Resource<T>>> resources;

        static std::atomic_bool isInitialized;

        static ResourceMemoryManager<T> memoryManager;

        static inline void CheckInitialization() {

            if (isInitialized) return;

            bool expected = false;
            if (isInitialized.compare_exchange_strong(expected, true)) {
                Events::EventManager::FrameEventDelegate.Subscribe(
                    ResourceManager<T>::UpdateHandler);
                Events::EventManager::QuitEventDelegate.Subscribe(
                    ResourceManager<T>::ShutdownHandler);
            }

        }

        static void UpdateHandler(Events::FrameEvent event) {

            std::lock_guard lock(mutex);

            for (auto it = resources.begin(); it != resources.end();) {
                auto& resource = it->second;
                // Just one reference (the resource manager), so start countdown for future deletion
                // If resource is accessed in certain time frame we reset the counter
                if (resource->data.use_count() == 1) {
                    resource->framesToDeletion--;
                }
                else {
                    resource->framesToDeletion = RESOURCE_RETENTION_FRAME_COUNT;
                }

                // Delete if all conditions are met
                if (resource->data.use_count() == 1 && resource->framesToDeletion == 0) {
                    it = resources.erase(it);
                }
                else {
                    ++it;
                }
            }
        }

        static void ShutdownHandler() {

            resources.clear();

        }

    };

    template<typename T>
    std::mutex ResourceManager<T>::mutex;

    template<typename T>
    std::unordered_map<std::string, Ref<Resource<T>>> ResourceManager<T>::resources;

    template<typename T>
    std::atomic_bool ResourceManager<T>::isInitialized = false;

    template<typename T>
    ResourceMemoryManager<T> ResourceManager<T>::memoryManager;

}

#endif