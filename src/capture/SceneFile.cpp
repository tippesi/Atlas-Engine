#include "SceneFile.h"
#include "Process.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace Atlas::Capture {

    static std::vector<uint8_t> ReadBytes(const fs::path& path) {

        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open())
            throw std::runtime_error("Couldn't open " + path.string());
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {});

    }

    static bool IsJsonFile(const fs::path& path) {

        return path.extension() == ".json";

    }

    // Collects the entities while the parser walks the file, without building the document
    class EntityReader : public nlohmann::json_sax<json> {

    public:
        EntityReader(std::vector<FileEntity>& entities, bool rootIsEntity, bool namesOnly)
            : entities(entities), rootIsEntity(rootIsEntity), namesOnly(namesOnly) {}

        bool null() override { return true; }
        bool boolean(bool) override { return true; }
        bool number_integer(number_integer_t value) override { return Number(float(value)); }
        bool number_unsigned(number_unsigned_t value) override { return Number(float(value)); }
        bool number_float(number_float_t value, const string_t&) override { return Number(float(value)); }
        bool binary(binary_t&) override { return true; }

        bool string(string_t& value) override {

            auto& frame = frames.back();
            if (frame.role == Role::Name && frame.key == "name")
                entities[frame.entity].name = value;
            else if (frame.role == Role::Mesh && frame.key == "resourcePath")
                entities[frame.entity].mesh = value;
            return true;

        }

        bool start_object(std::size_t) override {

            if (frames.empty()) {
                frames.push_back({ rootIsEntity ? Role::Entity : Role::Root, {}, rootIsEntity ? AddEntity(-1) : -1 });
                return true;
            }

            const auto parent = frames.back();
            auto role = Role::Ignored;
            auto entity = parent.entity;
            if (parent.role == Role::Entities) {
                role = Role::Entity;
                entity = AddEntity(parent.entity);
            }
            else if (parent.role == Role::Entity) {
                if (parent.key == "name")
                    role = Role::Name;
                else if (parent.key == "transform" && !namesOnly)
                    role = Role::Transform;
                else if (parent.key == "mesh" && !namesOnly)
                    role = Role::Mesh;
            }
            else if (parent.role == Role::Matrix) {
                // Older files store a matrix as one object per column
                role = Role::Matrix;
            }
            frames.push_back({ role, {}, entity });
            return true;

        }

        bool key(string_t& value) override {

            auto& frame = frames.back();
            frame.key = value;
            if (frame.role == Role::Entity && !namesOnly && value != "id" && value != "entities" && value != "root" && value != "name" &&
                value != "transform")
                entities[frame.entity].components.push_back(value);
            return true;

        }

        bool end_object() override {

            frames.pop_back();
            return true;

        }

        bool start_array(std::size_t) override {

            if (frames.empty()) {
                frames.push_back({ Role::Ignored, {}, -1 });
                return true;
            }

            const auto& parent = frames.back();
            auto role = Role::Ignored;
            if ((parent.role == Role::Root || parent.role == Role::Entity) && parent.key == "entities")
                role = Role::Entities;
            else if ((parent.role == Role::Transform && parent.key == "matrix") || parent.role == Role::Matrix)
                role = Role::Matrix;
            frames.push_back({ role, {}, parent.entity });
            return true;

        }

        bool end_array() override {

            frames.pop_back();
            return true;

        }

        bool parse_error(std::size_t position, const std::string&, const nlohmann::detail::exception& exception) override {

            throw std::runtime_error("Invalid file at byte " + std::to_string(position) + ": " + exception.what());

        }

        std::vector<std::vector<float>> matrices;

    private:
        enum class Role {
            Root,
            Entities,
            Entity,
            Name,
            Transform,
            Matrix,
            Mesh,
            Ignored
        };

        struct Frame {

            Role role;
            std::string key;
            int32_t entity;

        };

        int32_t AddEntity(int32_t parent) {

            entities.push_back({ .parent = parent });
            if (!namesOnly)
                matrices.emplace_back();
            return int32_t(entities.size()) - 1;

        }

        bool Number(float value) {

            const auto& frame = frames.back();
            if (frame.role == Role::Matrix)
                matrices[frame.entity].push_back(value);
            return true;

        }

        std::vector<FileEntity>& entities;
        std::vector<Frame> frames;
        bool rootIsEntity;
        bool namesOnly;

    };

    static std::vector<FileEntity> ReadEntities(const fs::path& path, bool namesOnly) {

        auto bytes = ReadBytes(path);
        std::vector<FileEntity> entities;
        EntityReader reader(entities, path.extension() == ".aeprefab", namesOnly);
        if (IsJsonFile(path))
            json::sax_parse(bytes.begin(), bytes.end(), &reader, json::input_format_t::json, true, true);
        else
            json::sax_parse(bytes.begin(), bytes.end(), &reader, json::input_format_t::msgpack);

        if (namesOnly)
            return entities;

        for (size_t i = 0; i < entities.size(); i++) {
            auto& entity = entities[i];
            const auto& values = reader.matrices[i];
            if (values.size() == 16)
                entity.local = glm::make_mat4(values.data());
            entity.global = entity.parent >= 0 ? entities[entity.parent].global * entity.local : entity.local;
        }
        return entities;

    }

    std::vector<FileEntity> ReadFileEntities(const fs::path& path) {

        return ReadEntities(path, false);

    }

    std::vector<std::string> ReadEntityNames(const fs::path& path) {

        std::error_code error;
        auto size = fs::file_size(path, error);
        // The count is a 128 bit integer on macOS, which std::to_string doesn't take
        auto time = static_cast<int64_t>(fs::last_write_time(path, error).time_since_epoch().count());
        auto key = std::hash<std::string>()(fs::absolute(path).generic_string() + "|" + std::to_string(size) + "|" + std::to_string(time));
        auto cache = GetExecutablePath().parent_path() / ".cache" / "capture-names" / (path.stem().string() + "-" + std::to_string(key) + ".txt");

        std::vector<std::string> names;
        if (std::ifstream stream(cache); stream.is_open()) {
            for (std::string line; std::getline(stream, line);)
                names.push_back(line);
            return names;
        }

        for (auto& entity : ReadEntities(path, true))
            names.push_back(std::move(entity.name));

        fs::create_directories(cache.parent_path(), error);
        // Older caches of the same file are of no use anymore
        for (const auto& entry : fs::directory_iterator(cache.parent_path(), error))
            if (entry.path().filename().string().starts_with(path.stem().string() + "-"))
                fs::remove(entry.path(), error);
        std::string content;
        for (const auto& name : names)
            content += name + "\n";
        WriteFileAtomic(cache, content);
        return names;

    }

    static vec3 ToVector(const json& j) {

        if (j.is_array() && j.size() >= 3)
            return vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
        if (j.is_object())
            return vec3(j.value("x", 0.0f), j.value("y", 0.0f), j.value("z", 0.0f));
        return vec3(0.0f);

    }

    MeshFileInfo ReadMeshFile(const fs::path& path) {

        auto bytes = ReadBytes(path);
        json j;
        try {
            j = json::from_msgpack(bytes);
        }
        catch (const json::exception&) {
            j = json::parse(bytes.begin(), bytes.end());
        }

        MeshFileInfo info;
        info.name = j.value("name", path.stem().string());
        info.impostor = j.value("impostor", std::string());

        const auto& data = j.value("data", json::object());
        if (data.contains("aabb"))
            info.bounds = Volume::AABB(ToVector(data["aabb"]["min"]), ToVector(data["aabb"]["max"]));
        info.vertexCount = data.value("vertexCount", int64_t(0));
        info.indexCount = data.value("indexCount", int64_t(0));
        for (const auto& material : data.value("materials", json::array()))
            info.materials.push_back(material.is_string() ? material.get<std::string>() : material.dump());
        for (const auto& sub : data.value("subData", json::array()))
            info.subData.emplace_back(sub.value("name", std::string()), sub.value("materialIdx", 0));

        for (const auto& [key, value] : j.items())
            if (key != "data" && key != "impostor" && key != "name" && !value.is_object() && !value.is_array())
                info.settings[key] = value;

        return info;

    }

    static size_t EditDistance(const std::string& a, const std::string& b) {

        std::vector<size_t> row(b.size() + 1);
        for (size_t j = 0; j <= b.size(); j++)
            row[j] = j;

        for (size_t i = 1; i <= a.size(); i++) {
            size_t diagonal = row[0];
            row[0] = i;
            for (size_t j = 1; j <= b.size(); j++) {
                auto above = row[j];
                auto same = std::tolower(static_cast<unsigned char>(a[i - 1])) == std::tolower(static_cast<unsigned char>(b[j - 1]));
                row[j] = std::min({ row[j] + 1, row[j - 1] + 1, diagonal + (same ? 0 : 1) });
                diagonal = above;
            }
        }
        return row[b.size()];

    }

    std::vector<std::string> FindClosestNames(const std::vector<std::string>& names, const std::string& text, size_t count) {

        // Wildcards don't count as differences
        std::string pattern;
        std::copy_if(text.begin(), text.end(), std::back_inserter(pattern), [](char c) { return c != '*' && c != '?'; });

        std::vector<std::pair<size_t, std::string>> scored;
        std::vector<std::string> unique = names;
        std::sort(unique.begin(), unique.end());
        unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
        for (const auto& name : unique) {
            // A name that contains the text is as close as it gets
            auto lowerName = name, lowerPattern = pattern;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            auto distance = lowerName.find(lowerPattern) != std::string::npos ? 0 : EditDistance(lowerName, lowerPattern);
            scored.emplace_back(distance, name);
        }
        std::partial_sort(scored.begin(), scored.begin() + std::min(count, scored.size()), scored.end());

        std::vector<std::string> closest;
        for (size_t i = 0; i < std::min(count, scored.size()); i++)
            closest.push_back(scored[i].second);
        return closest;

    }

}
