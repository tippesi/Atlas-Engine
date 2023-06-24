#ifndef AE_RESOURCEMANAGER_H
#define AE_RESOURCEMANAGER_H

#include "System.h"
#include "Resource.h"
#include "common/Hash.h"
#include "events/EventManager.h"

#include <type_traits>
#include <mutex>
#include <unordered_map>

namespace Atlas {

    template<typename T>
    class ResourceManager {

    public:
        template<typename ...Args>
        static ResourceHandle<T> GetResource(const std::string& path, Args&&... args) {

            static_assert(std::is_constructible<T, const std::string&, Args...>(),
                "Resource class needs to implement constructor with provided argument type");

            CheckInitialization();

            {
                std::lock_guard lock(mutex);
                if (resources.contains(path)) {
                    return ResourceHandle<T>(resources[path]);
                }

                auto resource = std::make_shared<Resource<T>>();
                resources[path] = resource;
            }

            // Load only after mutex is unlocked
            resources[path]->Load(path, std::forward<Args>(args)...);
            return ResourceHandle<T>(resources[path]);

        }

    private:
        static std::mutex mutex;
        static std::unordered_map<std::string, Ref<Resource<T>>> resources;

        static std::atomic_bool isInitialized;

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

            for (auto& [path, resource] : resources) {
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

}

#endif