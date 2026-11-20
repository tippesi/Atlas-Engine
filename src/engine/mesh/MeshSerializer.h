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

    void to_json(json& j, const Joint& p);

    void from_json(const json& j, Joint& p);

    void to_json(json& j, const Skeleton& p);

    void from_json(const json& j, Skeleton& p);

    void to_json(json& j, const ImpostorViewPlane& p);

    void from_json(const json& j, ImpostorViewPlane& p);

    void to_json(json& j, const Impostor& p);

    void from_json(const json& j, Impostor& p);

    void to_json(json& j, const SoftBodyMeshSettings& p);

    void from_json(const json& j, SoftBodyMeshSettings& p);

    template<class T>
    void to_json(json& j, const DataComponent<T>& p, bool binary) {

        int format = static_cast<int>(p.format);

        j = json{
            {"format", format},
        };

        set_binary_json(j, "data", p.data, binary);

    }

    template<class T>
    void from_json(const json& j, DataComponent<T>& p, bool binary) {

        int format;

        j.at("format").get_to(format);
        p.format = static_cast<ComponentFormat>(format);

        try_get_binary_json(j, "data", p.data, binary);

    }

}

namespace Atlas {

    void to_json(json& j, const Material& p);

    void from_json(const json& j, Material& p);

}