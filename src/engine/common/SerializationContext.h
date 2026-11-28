#pragma once

#include "../System.h"
#include "../resource/Resource.h"
#include "SerializationHelper.h"

#include <any>
#include <unordered_map>

namespace Atlas {

    /**
     * Active on the thread that writes or reads a binary scene. Binary scenes store every resource path
     * once in a table and reference it by its index, matrices are stored as raw little endian floats.
     * Without an active context the readable form with paths and arrays is written.
     */
    class SerializationContext {

    public:
        /**
         * Starts writing a binary scene, the table is collected in paths.
         */
        SerializationContext();

        /**
         * Starts reading a binary scene with its resource table.
         */
        explicit SerializationContext(const json& resources);

        SerializationContext(const SerializationContext&) = delete;

        SerializationContext& operator=(const SerializationContext&) = delete;

        ~SerializationContext();

        static void WritePath(json& j, const char* name, const std::string& path);

        static bool ReadPath(const json& j, const char* name, std::string& path);

        /**
         * Reads a resource reference, each resource of the table is only requested once from the resource manager.
         * @param load Requests the resource from a path, e.g. through the resource manager
         */
        template<class T, class Func>
        static void ReadHandle(const json& j, const char* name, ResourceHandle<T>& handle, Func&& load) {

            auto it = j.find(name);
            if (it == j.end())
                return;

            if (it->is_string()) {
                handle = load(it->template get_ref<const std::string&>());
                return;
            }

            ReadHandle(it->template get<size_t>(), handle, load);

        }

        template<class T, class Func>
        static void ReadHandle(size_t index, ResourceHandle<T>& handle, Func&& load) {

            if (!HasIndex(index))
                return;

            auto& cached = active->handles[index];
            if (auto cachedHandle = std::any_cast<ResourceHandle<T>>(&cached)) {
                handle = *cachedHandle;
                return;
            }

            handle = load(active->paths[index]);
            cached = handle;

        }

        // The index of the path in the resource table of the active context
        static uint32_t WriteIndex(const std::string& path);

        static void WriteMatrix(json& j, const char* name, const mat4& matrix);

        // Only writes inside a writing context produce the binary form, a reading context doesn't change them
        static bool IsWriting() { return active && active->writing; }

        std::vector<std::string> paths;

    private:
        // A reading context with an entry for the index, files with invalid indices are reported
        static bool HasIndex(size_t index);

        std::unordered_map<std::string, uint32_t> indices;
        std::vector<std::any> handles;

        SerializationContext* previous;
        bool writing;

        static thread_local SerializationContext* active;

    };

}
