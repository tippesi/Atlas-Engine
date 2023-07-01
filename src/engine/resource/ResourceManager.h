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

            static_assert(std::is_constructible<T, const std::string&, Args...>() ||
                std::is_constructible<T, Args...>(),
                "Resource class needs to implement constructor with provided argument type");

            CheckInitialization();

            {
                std::lock_guard lock(mutex);
                if (resources.contains(path)) {
                    return ResourceHandle<T>(resources[path]);
                }

                resources[path] = std::make_shared<Resource<T>>(path);
            }

            // Load only after mutex is unlocked
            resources[path]->Load(std::forward<Args>(args)...);
            return ResourceHandle<T>(resources[path]);

        }

        template<typename ...Args>
        static ResourceHandle<T> GetResourceWithLoader(
            std::function<T(const std::string, Args...)> loaderFunction,
            const std::string& path, Args&&... args) {

            CheckInitialization();

            {
                std::lock_guard lock(mutex);
                if (resources.contains(path)) {
                    return ResourceHandle<T>(resources[path]);
                }

                resources[path] = std::make_shared<Resource<T>>(path);
            }

            // Load only after mutex is unlocked
            //resources[path]->LoadWithExternalLoader(loaderFunction, std::forward<Args>(args)...);
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
                    return ResourceHandle<T>(resources[path]);
                }

                resources[path] = std::make_shared<Resource<T>>(path);
            }

            // Load only after mutex is unlocked
            resources[path].future = std::async(Resource<T>::Load, 
                resources[path].get(), std::forward<Args>(args)...);
            return ResourceHandle<T>(resources[path]);

        }

        template<typename ...Args>
        static ResourceHandle<T> GetResourceWithLoaderAsync(
            std::function<Ref<T>(const std::string, Args&&... args)> loaderFunction,
            const std::string& path, Args&&... args) {

            CheckInitialization();

            {
                std::lock_guard lock(mutex);
                if (resources.contains(path)) {
                    return ResourceHandle<T>(resources[path]);
                }

                resources[path] = std::make_shared<Resource<T>>(path);
            }

            // Load only after mutex is unlocked
            resources[path].future = std::async(Resource<T>::LoadWithExternalLoader,
                resources[path].get(), loaderFunction, std::forward<Args>(args)...);
            return ResourceHandle<T>(resources[path]);

        }

        ResourceHandle<T> AddResource(const std::string& path, Ref<Resource<T>> resource) {

            std::lock_guard lock(mutex);
            if (resources.contains(path)) {
                return ResourceHandle<T>(resources[path]);
            }

            resources[path] = resource;
            return ResourceHandle<T>(resource);

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

            for (auto& [_, resource] : resources) {
                if (resource.use_count() == 1) {
                    resource.reset();
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