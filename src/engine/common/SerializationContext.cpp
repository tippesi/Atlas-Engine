#include "SerializationContext.h"
#include "../Log.h"

namespace Atlas {

    thread_local SerializationContext* SerializationContext::active = nullptr;

    SerializationContext::SerializationContext() : previous(active), writing(true) {

        active = this;

    }

    SerializationContext::SerializationContext(const json& resources) : previous(active), writing(false) {

        resources.get_to(paths);
        handles.resize(paths.size());
        active = this;

    }

    SerializationContext::~SerializationContext() {

        active = previous;

    }

    void SerializationContext::WritePath(json& j, const char* name, const std::string& path) {

        if (!IsWriting()) {
            j[name] = path;
            return;
        }

        j[name] = WriteIndex(path);

    }

    uint32_t SerializationContext::WriteIndex(const std::string& path) {

        AE_ASSERT(IsWriting() && "Resource indices need a writing serialization context");

        auto [it, inserted] = active->indices.try_emplace(path, uint32_t(active->paths.size()));
        if (inserted)
            active->paths.push_back(path);
        return it->second;

    }

    bool SerializationContext::ReadPath(const json& j, const char* name, std::string& path) {

        auto it = j.find(name);
        if (it == j.end())
            return false;

        if (it->is_string()) {
            path = it->get_ref<const std::string&>();
            return true;
        }

        auto index = it->get<size_t>();
        if (!HasIndex(index))
            return false;

        path = active->paths[index];
        return true;

    }

    void SerializationContext::WriteMatrix(json& j, const char* name, const mat4& matrix) {

        if (!IsWriting()) {
            j[name] = matrix;
            return;
        }

        json::binary_t data(std::vector<uint8_t>(sizeof(mat4)));
        std::memcpy(data.data(), &matrix[0][0], sizeof(mat4));
        j[name] = std::move(data);

    }

    bool SerializationContext::HasIndex(size_t index) {

        if (active && !active->writing && index < active->paths.size())
            return true;

        Log::Error("Resource index " + std::to_string(index) + " without a matching resource table entry");
        return false;

    }

}
