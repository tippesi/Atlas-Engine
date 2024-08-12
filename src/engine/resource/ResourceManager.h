#pragma once

#include "System.h"
#include "Resource.h"
#include "Log.h"
#include "events/EventManager.h"
#include "loader/AssetLoader.h"

#include <type_traits>
#include <mutex>
#include <unordered_map>
#include <future>
#include <typeinfo>

namespace Atlas {

    enum class ResourceTopic {
        ResourceCreate = 0,
        ResourceDestroy
    };

    template<typename T>
    class ResourceManager {

    public:
        static ResourceHandle<T> GetResource(const std::string& path) {

            CheckInitialization();
            
            auto relativePath = GetAssetDirectoryPath(path);
            std::lock_guard lock(mutex);
            if (resources.contains(relativePath)) {
                auto& resource = resources[relativePath];
                resource->framesToDeletion = RESOURCE_RETENTION_FRAME_COUNT;
                return ResourceHandle<T>(resource);
            }

            return ResourceHandle<T>();

        }

        template<typename ...Args>
        static ResourceHandle<T> GetOrLoadResource(const std::string& path, Args&&... args) {

            return GetOrLoadResource(path, System, std::forward<Args>(args)...);

        }

        template<typename ...Args>
        static ResourceHandle<T> GetOrLoadResource(const std::string& path, ResourceOrigin origin, Args&&... args) {

            static_assert(std::is_constructible<T, const std::string&, Args...>() ||
                          std::is_constructible<T, Args...>(),
                "Resource class needs to implement constructor with provided argument type");

            CheckInitialization();

            auto relativePath = GetAssetDirectoryPath(path);
            auto handle = GetHandleOrCreateResourceInternal(relativePath, origin);
            if (!handle.IsValid()) {
                auto resource = GetResourceInternal(relativePath);
                resource->Load(std::forward<Args>(args)...);
                NotifyAllSubscribers(ResourceTopic::ResourceCreate, resource);
                handle = ResourceHandle<T>(resource);
            }

            handle.WaitForLoad();

            return handle;

        }

        template<class ...Args>
        static ResourceHandle<T> GetOrLoadResourceWithLoader(const std::string& path,
            Ref<T> (*loaderFunction)(const std::string&, Args...), Args... args) {

            return GetOrLoadResourceWithLoader(path, System, std::function(loaderFunction), std::forward<Args>(args)...);

        }

        template<class ...Args>
        static ResourceHandle<T> GetOrLoadResourceWithLoader(const std::string& path, ResourceOrigin origin,
            Ref<T> (*loaderFunction)(const std::string&, Args...), Args... args) {

            return GetOrLoadResourceWithLoader(path, origin, std::function(loaderFunction), std::forward<Args>(args)...);

        }

        template<class ...Args>
        static ResourceHandle<T> GetOrLoadResourceWithLoader(const std::string& path,
            std::function<Ref<T>(const std::string&, Args...)> loaderFunction, Args&&... args) {

            return GetOrLoadResourceWithLoader(path, System, loaderFunction, std::forward<Args>(args)...);

        }

        template<class ...Args>
        static ResourceHandle<T> GetOrLoadResourceWithLoader(const std::string& path, ResourceOrigin origin,
            std::function<Ref<T>(const std::string&, Args...)> loaderFunction, Args&&... args) {

            CheckInitialization();

            auto relativePath = GetAssetDirectoryPath(path);
            auto handle = GetHandleOrCreateResourceInternal(relativePath, origin);
            if (!handle.IsValid()) {
                auto resource = GetResourceInternal(relativePath);
                resource->LoadWithExternalLoader(loaderFunction, std::forward<Args>(args)...);
                NotifyAllSubscribers(ResourceTopic::ResourceCreate, resource);
                handle = ResourceHandle<T>(resource);
            }

            handle.WaitForLoad();

            return handle;

        }

        template<typename ...Args>
        static ResourceHandle<T> GetOrLoadResourceAsync(const std::string& path, Args&&... args) {

            return GetOrLoadResourceAsync(path, System, std::forward<Args>(args)...);

        }

        template<typename ...Args>
        static ResourceHandle<T> GetOrLoadResourceAsync(const std::string& path, ResourceOrigin origin, Args&&... args) {

            static_assert(std::is_constructible<T, const std::string&, Args...>() ||
                std::is_constructible<T, Args...>(),
                "Resource class needs to implement constructor with provided argument type");

            CheckInitialization();

            auto relativePath = GetAssetDirectoryPath(path);
            auto handle = GetHandleOrCreateResourceInternal(relativePath, origin);
            if (!handle.IsValid()) {
                auto resource = GetResourceInternal(relativePath);
                resource->future = std::async(std::launch::async,
                    &Resource<T>::template Load<Args...>,
                    resource.get(), std::forward<Args>(args)...);
                NotifyAllSubscribers(ResourceTopic::ResourceCreate, resource);
                handle = ResourceHandle<T>(resource);
            }

            return handle;

        }

        template<class ...Args>
        static ResourceHandle<T> GetOrLoadResourceWithLoaderAsync(const std::string& path,
            Ref<T> (*loaderFunction)(const std::string&, Args...), Args... args) {

            return GetOrLoadResourceWithLoaderAsync(path, System, std::function(loaderFunction), std::forward<Args>(args)...);

        }

        template<class ...Args>
        static ResourceHandle<T> GetOrLoadResourceWithLoaderAsync(const std::string& path, ResourceOrigin origin,
            Ref<T> (*loaderFunction)(const std::string&, Args...), Args... args) {

            return GetOrLoadResourceWithLoaderAsync(path, origin, std::function(loaderFunction), std::forward<Args>(args)...);

        }

        template<class ...Args>
        static ResourceHandle<T> GetOrLoadResourceWithLoaderAsync(const std::string& path,
            std::function<Ref<T>(const std::string&, Args...)> loaderFunction, Args... args) {

            return GetResourceWithLoaderAsync(path, System, loaderFunction, std::forward<Args>(args)...);

        }

        template<class ...Args>
        static ResourceHandle<T> GetOrLoadResourceWithLoaderAsync(const std::string& path, ResourceOrigin origin,
            std::function<Ref<T>(const std::string&, Args...)> loaderFunction, Args... args) {

            CheckInitialization();

            auto relativePath = GetAssetDirectoryPath(path);
            auto handle = GetHandleOrCreateResourceInternal(relativePath, origin);
            if (!handle.IsValid()) {
                auto resource = GetResourceInternal(relativePath);
                resource->future = std::async(std::launch::async,
                    &Resource<T>::template LoadWithExternalLoader<Args...>,
                    resource.get(), loaderFunction, std::forward<Args>(args)...);
                NotifyAllSubscribers(ResourceTopic::ResourceCreate, resource);
                handle = ResourceHandle<T>(resource);
            }

            return handle;

        }

        static ResourceHandle<T> AddResource(const std::string& path, Ref<Resource<T>> resource) {

            bool alreadyExisted;
            AddResource(path, resource, alreadyExisted);

        }

        static ResourceHandle<T> AddResource(const std::string& path, Ref<Resource<T>> resource, bool& alreadyExisted) {

            CheckInitialization();

            auto relativePath = GetAssetDirectoryPath(path);
            {
                std::lock_guard lock(mutex);
                if (resources.contains(relativePath)) {
                    auto &resource = resources[relativePath];
                    resource->framesToDeletion = RESOURCE_RETENTION_FRAME_COUNT;
                    alreadyExisted = true;
                    return ResourceHandle<T>(resource);
                }

                alreadyExisted = false;
                resources[relativePath] = resource;
            }

            NotifyAllSubscribers(ResourceTopic::ResourceCreate, resource);
            return ResourceHandle<T>(resource);

        }

        static ResourceHandle<T> AddResource(const std::string& path, Ref<T> data) {

            bool alreadyExisted;
            return AddResource(path, System, data, alreadyExisted);

        }

        static ResourceHandle<T> AddResource(const std::string& path, ResourceOrigin origin, Ref<T> data) {

            bool alreadyExisted;
            return AddResource(path, origin, data, alreadyExisted);

        }

        static ResourceHandle<T> AddResource(const std::string& path, Ref<T> data, bool& alreadyExisted) {

            return AddResource(path, System, data, alreadyExisted);

        }

        static ResourceHandle<T> AddResource(const std::string& path, ResourceOrigin origin,
            Ref<T> data, bool& alreadyExisted) {

            auto resource = CreateRef<Resource<T>>(path, origin, data);
            resource->isLoaded = true;
            return AddResource(path, resource, alreadyExisted);

        }

        static std::vector<ResourceHandle<T>> GetResources() {

            std::vector<ResourceHandle<T>> resourceHandles;

            for (auto& [_, resource] : resources) {
                resourceHandles.emplace_back(resource);
            }

            return resourceHandles;

        }

        static std::vector<ResourceHandle<T>> GetResourcesByOrigin(ResourceOrigin origin) {

            std::vector<ResourceHandle<T>> resourceHandles;

            for (auto& [_, resource] : resources) {
                if (resource->origin != origin)
                    continue;
                resourceHandles.emplace_back(resource);
            }

            return resourceHandles;

        }

        static int32_t Subscribe(const ResourceTopic topic, std::function<void(Ref<Resource<T>>&)> function) {

            std::lock_guard lock(subscriberMutex);

            auto subscribers = GetSubscribers(topic);

            subscribers->emplace_back(ResourceSubscriber<T>{
                .ID = subscriberCount,
                .function = function
            });

            return subscriberCount++;

        }

        static void Unsubscribe(const ResourceTopic topic, int32_t subscriptionID) {

            std::lock_guard lock(subscriberMutex);

            auto subscribers = GetSubscribers(topic);

            auto item = std::find_if(subscribers->begin(), subscribers->end(),
                [&](ResourceSubscriber<T> subscriber) {
                    return subscriber.ID == subscriptionID;
            });

            if (item != subscribers->end()) {
                subscribers->erase(item);
            }

        }

    private:
        static std::mutex mutex;
        static std::mutex subscriberMutex;

        static std::unordered_map<std::string, Ref<Resource<T>>> resources;

        static std::atomic_bool isInitialized;

        static std::vector<ResourceSubscriber<T>> createSubscribers;
        static std::vector<ResourceSubscriber<T>> destroySubscribers;

        static std::atomic_int subscriberCount;

        static inline void CheckInitialization() {

            if (isInitialized) return;

            bool expected = false;
            if (isInitialized.compare_exchange_strong(expected, true)) {
                Events::EventManager::FrameEventDelegate.Subscribe(
                    ResourceManager<T>::UpdateHandler);
                Events::EventManager::ShutdownEventDelegate.Subscribe(
                    ResourceManager<T>::ShutdownHandler);
            }

        }

        static inline ResourceHandle<T> GetHandleOrCreateResourceInternal(const std::string& path, ResourceOrigin origin) {
            std::lock_guard lock(mutex);
            if (resources.contains(path)) {
                auto& resource = resources[path];
                resource->framesToDeletion = RESOURCE_RETENTION_FRAME_COUNT;
                return ResourceHandle<T>(resource);
            }

            resources[path] = std::make_shared<Resource<T>>(path, origin);
            return ResourceHandle<T>();
        }

        static inline Ref<Resource<T>> GetResourceInternal(const std::string& path) {
            std::lock_guard lock(mutex);
            return resources[path];
        }

        static inline std::string GetAssetDirectoryPath(const std::string& path) {

            auto normalizedPath = Common::Path::Normalize(path);
            return Loader::AssetLoader::GetRelativePath(normalizedPath);

        }

        static void UpdateHandler(Events::FrameEvent event) {

            std::lock_guard lock(mutex);

            for (auto it = resources.begin(); it != resources.end();) {
                auto& resource = it->second;
                // Just one reference (the resource manager), so start countdown for future deletion
                // If resource is accessed in certain time frame we reset the counter
                if (resource.use_count() == 1 && !resource->permanent) {
                    resource->framesToDeletion--;
                }
                else {
                    resource->framesToDeletion = RESOURCE_RETENTION_FRAME_COUNT;
                }

                // Delete if all conditions are met
                if (resource.use_count() == 1 && resource->framesToDeletion == 0) {
                    NotifyAllSubscribers(ResourceTopic::ResourceDestroy, resource);
                    AE_ASSERT(resource.use_count() == 1 &&
                        "Subscribers shouldn't claim ownership of to be deleted resources");
                    resource->Unload();
                    it = resources.erase(it);
                }
                else {
                    ++it;
                }
            }
        }

        static void ShutdownHandler() {
         
            for (const auto& [_, resource] : resources) {
                if (!resource->future.valid())
                    continue;
                resource->future.wait();
                resource->future.get();
            }

            resources.clear();

        }

        static void NotifyAllSubscribers(const ResourceTopic topic, Ref<Resource<T>>& resource) {

            std::lock_guard lock(subscriberMutex);

            auto subscribers = GetSubscribers(topic);

            for (auto& subscriber : *subscribers) {
                subscriber.function(resource);
            }

        }

        static std::vector<ResourceSubscriber<T>>* GetSubscribers(const ResourceTopic topic) {
            std::vector<ResourceSubscriber<T>>* subscribers;

            if (topic == ResourceTopic::ResourceCreate) {
                subscribers = &createSubscribers;
            }
            else {
                subscribers = &destroySubscribers;
            }

            return subscribers;
        }

    };

    template<typename T>
    std::mutex ResourceManager<T>::mutex;

    template<typename T>
    std::mutex ResourceManager<T>::subscriberMutex;

    template<typename T>
    std::unordered_map<std::string, Ref<Resource<T>>> ResourceManager<T>::resources;

    template<typename T>
    std::atomic_bool ResourceManager<T>::isInitialized = false;

    template<typename T>
    std::vector<ResourceSubscriber<T>> ResourceManager<T>::createSubscribers;

    template<typename T>
    std::vector<ResourceSubscriber<T>> ResourceManager<T>::destroySubscribers;

    template<typename T>
    std::atomic_int ResourceManager<T>::subscriberCount = 0;

}