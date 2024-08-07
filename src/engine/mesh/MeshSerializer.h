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
    void to_json(json& j, const DataComponent<T>& p) {

        int format = static_cast<int>(p.format);

        j = json{
            {"format", format},
            {"data", p.data}
        };

    }

    template<class T>
    void from_json(const json& j, DataComponent<T>& p) {

        int format;

        j.at("format").get_to(format);
        j.at("data").get_to(p.data);

        p.format = static_cast<ComponentFormat>(format);

    }

}

namespace Atlas {

    void to_json(json& j, const Material& p);

    void from_json(const json& j, Material& p);

}