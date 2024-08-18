#pragma once

#include "Mesh.h"

#include "common/SerializationHelper.h"

#include <string>
#include <unordered_map>

namespace Atlas::Mesh {

    void to_json(json& j, const Mesh& p);

    void from_json(const json& j, Mesh& p);

    void to_json(json& j, const MeshData& p);

    void from_json(const json& j, MeshData& p);

    void to_json(json& j, const MeshSubData& p);

    void from_json(const json& j, MeshSubData& p);

    void to_json(json& j, const Impostor& p);

    void from_json(const json& j, Impostor& p);

    template<class T>
    void to_json(json& j, const DataComponent<T>& p, bool binary) {

        int format = static_cast<int>(p.format);

        j = json{
            {"format", format},
        };

        std::vector<uint8_t> binaryData;
        if (!p.data.empty()) {
            binaryData.resize(p.data.size() * sizeof(T));
            std::memcpy(binaryData.data(), p.data.data(), binaryData.size());
        }

        if (binary) {            
            j["data"] = json::binary_t(binaryData);
        }
        else {
            j["data"] = binaryData;
        }

    }

    template<class T>
    void from_json(const json& j, DataComponent<T>& p, bool binary) {

        int format;

        j.at("format").get_to(format);
        p.format = static_cast<ComponentFormat>(format);

        std::vector<uint8_t> binaryData;
        if (binary)
            binaryData = j["data"].get_binary();
        else
            j.at("data").get_to(binaryData);
        if (!binaryData.empty()) {
            p.data.resize(binaryData.size() / sizeof(T));
            std::memcpy(p.data.data(), binaryData.data(), binaryData.size());
        }  

    }

}

namespace Atlas {

    void to_json(json& j, const Material& p);

    void from_json(const json& j, Material& p);

}